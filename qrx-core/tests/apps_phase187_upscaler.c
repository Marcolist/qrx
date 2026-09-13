/*
 * Phase 187 - QRX Upscaler regression test.
 *
 * SPDX-License-Identifier: MIT
 *
 * Covers the properties the accelerated (Proof-of-Useful-Compute) mode depends
 * on, not just "does it produce an image":
 *   - correct output geometry,
 *   - bit-exact determinism across repeated runs,
 *   - no rounding drift (a constant image must survive untouched),
 *   - sharded execution produces exactly the same bytes as a single run,
 *   - safety limits reject rather than clamp,
 *   - untrusted frame names cannot escape the output directory,
 *   - symlinks and directories are not treated as frames.
 */

#include "apps/qrx_upscaler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#  include <direct.h>
#  define mk(p) _mkdir(p)
#else
#  include <unistd.h>
#  define mk(p) mkdir((p), 0755)
#endif

static int failures = 0;

static void check(int cond, const char *what) {
    printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) failures++;
}

static void fill_pattern(QrxUpsImage *img, int seed) {
    for (int32_t y = 0; y < img->height; y++)
        for (int32_t x = 0; x < img->width; x++) {
            uint8_t *p = img->pixels + ((size_t)y * img->width + x) * img->channels;
            for (int c = 0; c < img->channels; c++)
                p[c] = (uint8_t)((x * 7 + y * 13 + c * 29 + seed * 31) & 0xFF);
        }
}

static int write_frame(const char *path, int w, int h, int seed) {
    QrxUpsImage img;
    if (qrx_ups_image_alloc(&img, w, h, 3) != QRX_UPS_OK) return -1;
    fill_pattern(&img, seed);
    int rc = qrx_ups_image_save(path, &img);
    qrx_ups_image_free(&img);
    return rc == QRX_UPS_OK ? 0 : -1;
}

static long file_bytes(const char *path, unsigned char **out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return -1; }
    unsigned char *b = (unsigned char *)malloc((size_t)n ? (size_t)n : 1);
    if (!b) { fclose(f); return -1; }
    if (n && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return -1; }
    fclose(f);
    *out = b;
    return n;
}

int main(void) {
    printf("phase187: QRX Upscaler\n");

    /* --- geometry, determinism, drift -------------------------------- */
    QrxUpsImage src, a, b;
    if (qrx_ups_image_alloc(&src, 11, 9, 3) != QRX_UPS_OK) { puts("alloc failed"); return 1; }
    fill_pattern(&src, 1);

    check(qrx_ups_upscale(&src, 4, QRX_UPS_LANCZOS3, &a) == QRX_UPS_OK, "lanczos3 x4 succeeds");
    check(a.width == 44 && a.height == 36 && a.channels == 3, "output geometry is exactly scale x input");

    check(qrx_ups_upscale(&src, 4, QRX_UPS_LANCZOS3, &b) == QRX_UPS_OK, "second run succeeds");
    check(a.pixels && b.pixels &&
          memcmp(a.pixels, b.pixels, (size_t)a.width * a.height * 3) == 0,
          "repeated upscale is bit-exact (required for PoUC verification)");
    qrx_ups_image_free(&b);
    qrx_ups_image_free(&a);

    for (int f = QRX_UPS_NEAREST; f <= QRX_UPS_LANCZOS3; f++) {
        QrxUpsImage flat, out;
        qrx_ups_image_alloc(&flat, 8, 8, 3);
        memset(flat.pixels, 200, (size_t)8 * 8 * 3);
        int ok = qrx_ups_upscale(&flat, 3, (QrxUpsFilter)f, &out) == QRX_UPS_OK;
        if (ok) {
            for (size_t i = 0; i < (size_t)out.width * out.height * 3; i++)
                if (out.pixels[i] != 200) { ok = 0; break; }
            qrx_ups_image_free(&out);
        }
        char msg[128];
        snprintf(msg, sizeof(msg), "%s preserves a constant image exactly",
                 qrx_ups_filter_name((QrxUpsFilter)f));
        check(ok, msg);
        qrx_ups_image_free(&flat);
    }

    /* --- safety limits ----------------------------------------------- */
    QrxUpsImage dummy, oversized;
    oversized.width = QRX_UPS_MAX_DIM;
    oversized.height = QRX_UPS_MAX_DIM;
    oversized.channels = 3;
    oversized.pixels = src.pixels;   /* limit check runs before any access */
    check(qrx_ups_upscale(&oversized, 2, QRX_UPS_LANCZOS3, &dummy) == QRX_UPS_ERR_LIMIT,
          "oversized output rejected, not clamped");
    check(qrx_ups_upscale(&src, 0, QRX_UPS_LANCZOS3, &dummy) == QRX_UPS_ERR_ARG,
          "scale 0 rejected");
    check(qrx_ups_upscale(&src, QRX_UPS_MAX_SCALE + 1, QRX_UPS_LANCZOS3, &dummy) == QRX_UPS_ERR_ARG,
          "scale above maximum rejected");
    check(qrx_ups_upscale(NULL, 2, QRX_UPS_LANCZOS3, &dummy) == QRX_UPS_ERR_ARG,
          "NULL source rejected");

    /* --- batch: sharding must match a single run --------------------- */
    const char *in_dir = "phase187_in";
    const char *single = "phase187_single";
    const char *sharded = "phase187_sharded";
    mk(in_dir); mk(single); mk(sharded);

    int frames = 9;
    int wrote = 1;
    for (int i = 0; i < frames; i++) {
        char p[256];
        snprintf(p, sizeof(p), "%s/frame-%04d.ppm", in_dir, i);
        if (write_frame(p, 10, 8, i) != 0) wrote = 0;
    }
    /* Decoys that must be ignored. */
    {
        char p[256];
        snprintf(p, sizeof(p), "%s/notes.txt", in_dir);
        FILE *f = fopen(p, "wb"); if (f) { fputs("not an image", f); fclose(f); }
        snprintf(p, sizeof(p), "%s/..escape.ppm", in_dir);
        write_frame(p, 4, 4, 99);
    }
    check(wrote, "test frames written");

    QrxUpsBatchStats s1, s2;
    check(qrx_ups_upscale_batch(in_dir, single, 2, QRX_UPS_BICUBIC, 0, 1, &s1) == QRX_UPS_OK,
          "single-worker batch succeeds");
    check(s1.files_total == (size_t)frames,
          "non-image and unsafe names excluded from the batch");

    int shards = 3, all_ok = 1;
    size_t done = 0;
    for (int i = 0; i < shards; i++) {
        if (qrx_ups_upscale_batch(in_dir, sharded, 2, QRX_UPS_BICUBIC, i, shards, &s2) != QRX_UPS_OK)
            all_ok = 0;
        done += s2.files_done;
    }
    check(all_ok, "sharded batch succeeds");
    check(done == (size_t)frames, "shards together cover every frame exactly once");

    int identical = 1;
    for (int i = 0; i < frames && identical; i++) {
        char p1[256], p2[256];
        unsigned char *b1 = NULL, *b2 = NULL;
        snprintf(p1, sizeof(p1), "%s/frame-%04d.ppm", single, i);
        snprintf(p2, sizeof(p2), "%s/frame-%04d.ppm", sharded, i);
        long n1 = file_bytes(p1, &b1), n2 = file_bytes(p2, &b2);
        if (n1 < 0 || n2 < 0 || n1 != n2 || memcmp(b1, b2, (size_t)n1) != 0) identical = 0;
        free(b1); free(b2);
    }
    check(identical, "sharded output is byte-identical to the single-worker run");

    /* the rejected name must not have produced any output */
    {
        char p[256];
        snprintf(p, sizeof(p), "%s/..escape.ppm", single);
        FILE *f = fopen(p, "rb");
        check(f == NULL, "unsafe frame name produced no output file");
        if (f) fclose(f);
    }

    check(qrx_ups_upscale_batch(in_dir, single, 2, QRX_UPS_BICUBIC, 3, 3, NULL) == QRX_UPS_ERR_ARG,
          "shard index outside shard count rejected");

    qrx_ups_image_free(&src);

    printf("phase187: %s (%d failure(s))\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
