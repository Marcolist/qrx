/*
 * QRX Upscaler core.
 *
 * Copyright (c) 2026 QRX contributors.
 * SPDX-License-Identifier: MIT
 */

#include "apps/qrx_upscaler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#  include <windows.h>
#  include <direct.h>
#  define QRX_UPS_SEP '\\'
#else
#  include <dirent.h>
#  include <unistd.h>
#  define QRX_UPS_SEP '/'
#endif

#ifdef QRX_HAVE_PNG
#  include <png.h>
#endif

/* ------------------------------------------------------------------ */
/* diagnostics                                                         */
/* ------------------------------------------------------------------ */

const char *qrx_ups_strerror(int status) {
    switch (status) {
        case QRX_UPS_OK:              return "ok";
        case QRX_UPS_ERR_ARG:         return "invalid argument";
        case QRX_UPS_ERR_IO:          return "i/o error";
        case QRX_UPS_ERR_FORMAT:      return "unsupported or malformed image";
        case QRX_UPS_ERR_LIMIT:       return "image exceeds configured safety limit";
        case QRX_UPS_ERR_OOM:         return "out of memory";
        case QRX_UPS_ERR_UNSUPPORTED: return "feature not built in";
        case QRX_UPS_ERR_PATH:        return "unsafe path rejected";
        default:                      return "unknown error";
    }
}

const char *qrx_ups_filter_name(QrxUpsFilter f) {
    switch (f) {
        case QRX_UPS_NEAREST:  return "nearest";
        case QRX_UPS_BILINEAR: return "bilinear";
        case QRX_UPS_BICUBIC:  return "bicubic";
        case QRX_UPS_LANCZOS3: return "lanczos3";
        default:               return "unknown";
    }
}

int qrx_ups_filter_from_name(const char *name, QrxUpsFilter *out) {
    if (!name || !out) return QRX_UPS_ERR_ARG;
    if (!strcmp(name, "nearest"))  { *out = QRX_UPS_NEAREST;  return QRX_UPS_OK; }
    if (!strcmp(name, "bilinear")) { *out = QRX_UPS_BILINEAR; return QRX_UPS_OK; }
    if (!strcmp(name, "bicubic"))  { *out = QRX_UPS_BICUBIC;  return QRX_UPS_OK; }
    if (!strcmp(name, "lanczos3") || !strcmp(name, "lanczos")) {
        *out = QRX_UPS_LANCZOS3; return QRX_UPS_OK;
    }
    return QRX_UPS_ERR_ARG;
}

/* ------------------------------------------------------------------ */
/* image lifecycle                                                     */
/* ------------------------------------------------------------------ */

static int dims_ok(int32_t w, int32_t h, int32_t ch) {
    if (w <= 0 || h <= 0) return 0;
    if (ch <= 0 || ch > QRX_UPS_MAX_CHANNELS) return 0;
    if (w > QRX_UPS_MAX_DIM || h > QRX_UPS_MAX_DIM) return 0;
    /* 64-bit product, so the limit check itself cannot overflow. */
    if ((int64_t)w * (int64_t)h > (int64_t)QRX_UPS_MAX_PIXELS) return 0;
    return 1;
}

int qrx_ups_image_alloc(QrxUpsImage *img, int32_t w, int32_t h, int32_t ch) {
    if (!img) return QRX_UPS_ERR_ARG;
    memset(img, 0, sizeof(*img));
    if (!dims_ok(w, h, ch)) return QRX_UPS_ERR_LIMIT;
    size_t n = (size_t)w * (size_t)h * (size_t)ch;
    img->pixels = (uint8_t *)calloc(n, 1);
    if (!img->pixels) return QRX_UPS_ERR_OOM;
    img->width = w; img->height = h; img->channels = ch;
    return QRX_UPS_OK;
}

void qrx_ups_image_free(QrxUpsImage *img) {
    if (!img) return;
    free(img->pixels);
    img->pixels = NULL;
    img->width = img->height = img->channels = 0;
}

/* ------------------------------------------------------------------ */
/* PPM / PGM (built in, no external dependency)                        */
/* ------------------------------------------------------------------ */

/* Reads one ASCII header token, skipping whitespace and # comments. */
static int pnm_token(FILE *f, char *buf, size_t cap) {
    size_t n = 0;
    int c;
    for (;;) {
        c = fgetc(f);
        if (c == EOF) return -1;
        if (c == '#') { while (c != '\n' && c != EOF) c = fgetc(f); continue; }
        if (!isspace(c)) break;
    }
    while (c != EOF && !isspace(c)) {
        if (n + 1 >= cap) return -1;
        buf[n++] = (char)c;
        c = fgetc(f);
    }
    buf[n] = 0;
    return n ? 0 : -1;
}

static int pnm_load(FILE *f, QrxUpsImage *out) {
    char tok[32];
    if (pnm_token(f, tok, sizeof(tok)) != 0) return QRX_UPS_ERR_FORMAT;
    int channels;
    if (!strcmp(tok, "P6")) channels = 3;
    else if (!strcmp(tok, "P5")) channels = 1;
    else return QRX_UPS_ERR_FORMAT;

    long w = 0, h = 0, maxv = 0;
    char *end = NULL;
    if (pnm_token(f, tok, sizeof(tok)) != 0) return QRX_UPS_ERR_FORMAT;
    w = strtol(tok, &end, 10); if (end == tok || *end) return QRX_UPS_ERR_FORMAT;
    if (pnm_token(f, tok, sizeof(tok)) != 0) return QRX_UPS_ERR_FORMAT;
    h = strtol(tok, &end, 10); if (end == tok || *end) return QRX_UPS_ERR_FORMAT;
    if (pnm_token(f, tok, sizeof(tok)) != 0) return QRX_UPS_ERR_FORMAT;
    maxv = strtol(tok, &end, 10); if (end == tok || *end) return QRX_UPS_ERR_FORMAT;
    if (maxv != 255) return QRX_UPS_ERR_UNSUPPORTED;   /* 8 bit only */
    if (w <= 0 || h <= 0 || w > INT32_MAX || h > INT32_MAX) return QRX_UPS_ERR_FORMAT;
    if (!dims_ok((int32_t)w, (int32_t)h, channels)) return QRX_UPS_ERR_LIMIT;

    int rc = qrx_ups_image_alloc(out, (int32_t)w, (int32_t)h, channels);
    if (rc != QRX_UPS_OK) return rc;

    size_t need = (size_t)w * (size_t)h * (size_t)channels;
    if (fread(out->pixels, 1, need, f) != need) {
        qrx_ups_image_free(out);
        return QRX_UPS_ERR_FORMAT;     /* truncated frame is not tolerated */
    }
    return QRX_UPS_OK;
}

static int pnm_save(const char *path, const QrxUpsImage *img) {
    if (img->channels != 1 && img->channels != 3) return QRX_UPS_ERR_UNSUPPORTED;
    FILE *f = fopen(path, "wb");
    if (!f) return QRX_UPS_ERR_IO;
    if (fprintf(f, "%s\n%d %d\n255\n", img->channels == 3 ? "P6" : "P5",
                (int)img->width, (int)img->height) < 0) {
        fclose(f); return QRX_UPS_ERR_IO;
    }
    size_t n = (size_t)img->width * (size_t)img->height * (size_t)img->channels;
    int ok = fwrite(img->pixels, 1, n, f) == n;
    if (fflush(f) != 0) ok = 0;
    if (fclose(f) != 0) ok = 0;
    return ok ? QRX_UPS_OK : QRX_UPS_ERR_IO;
}

/* ------------------------------------------------------------------ */
/* PNG (optional, libpng + zlib, both permissive)                      */
/* ------------------------------------------------------------------ */

#ifdef QRX_HAVE_PNG
static int png_load(FILE *f, QrxUpsImage *out) {
    unsigned char sig[8];
    if (fread(sig, 1, 8, f) != 8 || png_sig_cmp(sig, 0, 8)) return QRX_UPS_ERR_FORMAT;

    png_structp p = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!p) return QRX_UPS_ERR_OOM;
    png_infop info = png_create_info_struct(p);
    if (!info) { png_destroy_read_struct(&p, NULL, NULL); return QRX_UPS_ERR_OOM; }

    png_bytep *rows = NULL;
    int rc = QRX_UPS_ERR_FORMAT;
    if (setjmp(png_jmpbuf(p))) {      /* libpng error path, never exits */
        free(rows);
        qrx_ups_image_free(out);
        png_destroy_read_struct(&p, &info, NULL);
        return rc;
    }

    png_init_io(p, f);
    png_set_sig_bytes(p, 8);
    /* Bound the decoder: a crafted PNG must not be able to request a
     * multi-gigabyte allocation before we ever see its dimensions. */
    png_set_user_limits(p, QRX_UPS_MAX_DIM, QRX_UPS_MAX_DIM);
    png_read_info(p, info);

    png_uint_32 w = png_get_image_width(p, info);
    png_uint_32 h = png_get_image_height(p, info);
    int bit_depth = png_get_bit_depth(p, info);
    int color = png_get_color_type(p, info);

    if (bit_depth == 16) png_set_strip_16(p);
    if (color == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(p);
    if (color == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(p);
    if (png_get_valid(p, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(p);
    png_read_update_info(p, info);

    color = png_get_color_type(p, info);
    int ch = (color == PNG_COLOR_TYPE_GRAY)       ? 1 :
             (color == PNG_COLOR_TYPE_GRAY_ALPHA) ? 2 :
             (color == PNG_COLOR_TYPE_RGB)        ? 3 : 4;
    if (ch == 2) { rc = QRX_UPS_ERR_UNSUPPORTED; png_longjmp(p, 1); }
    if (!dims_ok((int32_t)w, (int32_t)h, ch)) { rc = QRX_UPS_ERR_LIMIT; png_longjmp(p, 1); }

    rc = qrx_ups_image_alloc(out, (int32_t)w, (int32_t)h, ch);
    if (rc != QRX_UPS_OK) png_longjmp(p, 1);
    rc = QRX_UPS_ERR_FORMAT;

    rows = (png_bytep *)malloc(sizeof(png_bytep) * h);
    if (!rows) { rc = QRX_UPS_ERR_OOM; png_longjmp(p, 1); }
    for (png_uint_32 y = 0; y < h; y++)
        rows[y] = out->pixels + (size_t)y * (size_t)w * (size_t)ch;

    png_read_image(p, rows);
    png_read_end(p, NULL);
    free(rows);
    png_destroy_read_struct(&p, &info, NULL);
    return QRX_UPS_OK;
}

static int png_save(const char *path, const QrxUpsImage *img) {
    /* volatile: read after setjmp, so it must survive a longjmp. */
    volatile int color;
    if (img->channels == 1)      color = PNG_COLOR_TYPE_GRAY;
    else if (img->channels == 3) color = PNG_COLOR_TYPE_RGB;
    else if (img->channels == 4) color = PNG_COLOR_TYPE_RGBA;
    else return QRX_UPS_ERR_UNSUPPORTED;

    FILE *f = fopen(path, "wb");
    if (!f) return QRX_UPS_ERR_IO;

    png_structp p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!p) { fclose(f); return QRX_UPS_ERR_OOM; }
    png_infop info = png_create_info_struct(p);
    if (!info) { png_destroy_write_struct(&p, NULL); fclose(f); return QRX_UPS_ERR_OOM; }

    png_bytep *rows = NULL;
    if (setjmp(png_jmpbuf(p))) {
        free(rows);
        png_destroy_write_struct(&p, &info);
        fclose(f);
        return QRX_UPS_ERR_IO;
    }

    png_init_io(p, f);
    png_set_IHDR(p, info, (png_uint_32)img->width, (png_uint_32)img->height, 8, (int)color,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    /* Fixed compression settings: PNG bytes must be reproducible across hosts
     * so that two providers upscaling the same frame agree bit for bit. */
    png_set_compression_level(p, 6);
    png_set_filter(p, 0, PNG_FILTER_NONE);
    png_write_info(p, info);

    rows = (png_bytep *)malloc(sizeof(png_bytep) * (size_t)img->height);
    if (!rows) png_longjmp(p, 1);
    for (int32_t y = 0; y < img->height; y++)
        rows[y] = img->pixels + (size_t)y * (size_t)img->width * (size_t)img->channels;

    png_write_image(p, rows);
    png_write_end(p, NULL);
    free(rows);
    png_destroy_write_struct(&p, &info);
    if (fclose(f) != 0) return QRX_UPS_ERR_IO;
    return QRX_UPS_OK;
}
#endif /* QRX_HAVE_PNG */

/* ------------------------------------------------------------------ */
/* dispatch by content (load) and extension (save)                     */
/* ------------------------------------------------------------------ */

static const char *ext_of(const char *path) {
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, QRX_UPS_SEP);
    if (!dot || (slash && dot < slash)) return "";
    return dot + 1;
}

static int ext_is(const char *path, const char *want) {
    const char *e = ext_of(path);
    size_t n = strlen(want);
    if (strlen(e) != n) return 0;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)e[i]) != want[i]) return 0;
    return 1;
}

int qrx_ups_image_load(const char *path, QrxUpsImage *out) {
    if (!path || !out) return QRX_UPS_ERR_ARG;
    memset(out, 0, sizeof(*out));
    FILE *f = fopen(path, "rb");
    if (!f) return QRX_UPS_ERR_IO;

    unsigned char magic[8];
    size_t got = fread(magic, 1, sizeof(magic), f);
    if (got < 2) { fclose(f); return QRX_UPS_ERR_FORMAT; }
    rewind(f);

    int rc;
    if (got >= 8 && magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G') {
#ifdef QRX_HAVE_PNG
        rc = png_load(f, out);
#else
        rc = QRX_UPS_ERR_UNSUPPORTED;
#endif
    } else if (magic[0] == 'P' && (magic[1] == '5' || magic[1] == '6')) {
        rc = pnm_load(f, out);
    } else {
        rc = QRX_UPS_ERR_FORMAT;
    }
    fclose(f);
    return rc;
}

int qrx_ups_image_save(const char *path, const QrxUpsImage *img) {
    if (!path || !img || !img->pixels) return QRX_UPS_ERR_ARG;
    if (ext_is(path, "png")) {
#ifdef QRX_HAVE_PNG
        return png_save(path, img);
#else
        return QRX_UPS_ERR_UNSUPPORTED;
#endif
    }
    if (ext_is(path, "ppm") || ext_is(path, "pgm") || ext_is(path, "pnm"))
        return pnm_save(path, img);
    return QRX_UPS_ERR_UNSUPPORTED;
}

/* ------------------------------------------------------------------ */
/* resampling kernels, fixed point                                     */
/* ------------------------------------------------------------------ */

/*
 * Kernels are evaluated in double precision once, at table-build time, and
 * immediately quantised to integers that are forced to sum to QRX_UPS_ONE.
 * The pixel loop itself is pure integer arithmetic, so the produced bytes do
 * not depend on FPU mode, compiler fast-math flags or instruction selection.
 */

static double sinc_pi(double x) {
    if (x == 0.0) return 1.0;
    const double pi = 3.14159265358979323846;
    double a = pi * x;
    return sin(a) / a;
}

static double kernel_weight(QrxUpsFilter f, double x) {
    if (x < 0) x = -x;
    switch (f) {
        case QRX_UPS_NEAREST:
            return x < 0.5 ? 1.0 : 0.0;
        case QRX_UPS_BILINEAR:
            return x < 1.0 ? (1.0 - x) : 0.0;
        case QRX_UPS_BICUBIC: {
            /* Mitchell-Netravali, B = C = 1/3. */
            const double B = 1.0 / 3.0, C = 1.0 / 3.0;
            double x2 = x * x, x3 = x2 * x;
            if (x < 1.0)
                return ((12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 + (6 - 2 * B)) / 6.0;
            if (x < 2.0)
                return ((-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 + (-12 * B - 48 * C) * x + (8 * B + 24 * C)) / 6.0;
            return 0.0;
        }
        case QRX_UPS_LANCZOS3:
            if (x < 3.0) return sinc_pi(x) * sinc_pi(x / 3.0);
            return 0.0;
        default:
            return 0.0;
    }
}

static double kernel_support(QrxUpsFilter f) {
    switch (f) {
        case QRX_UPS_NEAREST:  return 0.5;
        case QRX_UPS_BILINEAR: return 1.0;
        case QRX_UPS_BICUBIC:  return 2.0;
        case QRX_UPS_LANCZOS3: return 3.0;
        default:               return 1.0;
    }
}

typedef struct {
    int32_t  taps;       /* weights per output sample                      */
    int32_t *first;      /* first source index per output sample           */
    int32_t *w;          /* taps * out_n fixed-point weights               */
} QrxUpsTable;

static void table_free(QrxUpsTable *t) {
    if (!t) return;
    free(t->first); free(t->w);
    t->first = NULL; t->w = NULL; t->taps = 0;
}

/*
 * Build the resampling table for one axis. Pure upscaling, so the kernel is
 * not widened: support stays at its base radius.
 */
static int table_build(QrxUpsTable *t, int32_t in_n, int32_t out_n, QrxUpsFilter f) {
    memset(t, 0, sizeof(*t));
    double support = kernel_support(f);
    double scale = (double)out_n / (double)in_n;
    int32_t taps = (int32_t)(2.0 * support + 2.0);
    if (taps < 1) taps = 1;

    t->taps = taps;
    t->first = (int32_t *)malloc(sizeof(int32_t) * (size_t)out_n);
    t->w = (int32_t *)malloc(sizeof(int32_t) * (size_t)out_n * (size_t)taps);
    if (!t->first || !t->w) { table_free(t); return QRX_UPS_ERR_OOM; }

    for (int32_t o = 0; o < out_n; o++) {
        /* Centre of output sample o mapped back into source coordinates. */
        double center = ((double)o + 0.5) / scale - 0.5;
        int32_t start = (int32_t)(center - support + 0.5);
        double raw[16];
        double sum = 0.0;
        if (taps > (int32_t)(sizeof(raw) / sizeof(raw[0]))) { table_free(t); return QRX_UPS_ERR_ARG; }

        for (int32_t k = 0; k < taps; k++) {
            double d = center - (double)(start + k);
            double weight = kernel_weight(f, d);
            raw[k] = weight;
            sum += weight;
        }
        if (sum == 0.0) { raw[0] = 1.0; sum = 1.0; }

        /* Quantise and force the row to sum to exactly QRX_UPS_ONE, so no
         * rounding drift can brighten or darken the image. */
        int32_t acc = 0, max_k = 0;
        int32_t maxv = INT32_MIN;
        for (int32_t k = 0; k < taps; k++) {
            double norm = raw[k] / sum;
            int32_t q = (int32_t)(norm * (double)QRX_UPS_ONE + (norm >= 0 ? 0.5 : -0.5));
            t->w[(size_t)o * (size_t)taps + (size_t)k] = q;
            acc += q;
            if (q > maxv) { maxv = q; max_k = k; }
        }
        t->w[(size_t)o * (size_t)taps + (size_t)max_k] += (QRX_UPS_ONE - acc);
        t->first[o] = start;
    }
    return QRX_UPS_OK;
}

static inline int32_t clamp_index(int32_t i, int32_t n) {
    if (i < 0) return 0;
    if (i >= n) return n - 1;
    return i;
}

static inline uint8_t clamp_u8(int64_t v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

int qrx_ups_upscale(const QrxUpsImage *src, int scale, QrxUpsFilter filter,
                    QrxUpsImage *dst) {
    if (!src || !dst || !src->pixels) return QRX_UPS_ERR_ARG;
    if (scale < 1 || scale > QRX_UPS_MAX_SCALE) return QRX_UPS_ERR_ARG;
    if (!dims_ok(src->width, src->height, src->channels)) return QRX_UPS_ERR_LIMIT;

    int64_t ow = (int64_t)src->width * scale;
    int64_t oh = (int64_t)src->height * scale;
    if (ow > QRX_UPS_MAX_DIM || oh > QRX_UPS_MAX_DIM) return QRX_UPS_ERR_LIMIT;
    if (ow * oh > (int64_t)QRX_UPS_MAX_PIXELS) return QRX_UPS_ERR_LIMIT;

    int ch = src->channels;
    int rc = qrx_ups_image_alloc(dst, (int32_t)ow, (int32_t)oh, ch);
    if (rc != QRX_UPS_OK) return rc;

    if (scale == 1) {
        memcpy(dst->pixels, src->pixels,
               (size_t)src->width * (size_t)src->height * (size_t)ch);
        return QRX_UPS_OK;
    }

    QrxUpsTable hx, vy;
    rc = table_build(&hx, src->width, (int32_t)ow, filter);
    if (rc != QRX_UPS_OK) { qrx_ups_image_free(dst); return rc; }
    rc = table_build(&vy, src->height, (int32_t)oh, filter);
    if (rc != QRX_UPS_OK) { table_free(&hx); qrx_ups_image_free(dst); return rc; }

    /* Horizontal pass into a 32-bit intermediate (src->height rows, ow cols). */
    int32_t *tmp = (int32_t *)malloc(sizeof(int32_t) * (size_t)src->height * (size_t)ow * (size_t)ch);
    if (!tmp) { table_free(&hx); table_free(&vy); qrx_ups_image_free(dst); return QRX_UPS_ERR_OOM; }

    for (int32_t y = 0; y < src->height; y++) {
        const uint8_t *srow = src->pixels + (size_t)y * (size_t)src->width * (size_t)ch;
        int32_t *trow = tmp + (size_t)y * (size_t)ow * (size_t)ch;
        for (int32_t x = 0; x < (int32_t)ow; x++) {
            const int32_t *wr = hx.w + (size_t)x * (size_t)hx.taps;
            int32_t f0 = hx.first[x];
            for (int c = 0; c < ch; c++) {
                int64_t acc = 0;
                for (int32_t k = 0; k < hx.taps; k++) {
                    int32_t si = clamp_index(f0 + k, src->width);
                    acc += (int64_t)wr[k] * (int64_t)srow[(size_t)si * (size_t)ch + (size_t)c];
                }
                trow[(size_t)x * (size_t)ch + (size_t)c] = (int32_t)acc;   /* still Q16 */
            }
        }
    }

    /* Vertical pass into the 8-bit destination. */
    for (int32_t y = 0; y < (int32_t)oh; y++) {
        const int32_t *wr = vy.w + (size_t)y * (size_t)vy.taps;
        int32_t f0 = vy.first[y];
        uint8_t *drow = dst->pixels + (size_t)y * (size_t)ow * (size_t)ch;
        for (int32_t x = 0; x < (int32_t)ow; x++) {
            for (int c = 0; c < ch; c++) {
                int64_t acc = 0;
                for (int32_t k = 0; k < vy.taps; k++) {
                    int32_t si = clamp_index(f0 + k, src->height);
                    acc += (int64_t)wr[k] *
                           (int64_t)tmp[((size_t)si * (size_t)ow + (size_t)x) * (size_t)ch + (size_t)c];
                }
                /* Two Q16 multiplies accumulated: shift back by 32, rounding
                 * half up in a sign-symmetric way. */
                int64_t v = (acc + ((int64_t)1 << 31)) >> 32;
                drow[(size_t)x * (size_t)ch + (size_t)c] = clamp_u8(v);
            }
        }
    }

    free(tmp);
    table_free(&hx);
    table_free(&vy);
    return QRX_UPS_OK;
}

int qrx_ups_upscale_file(const char *in_path, const char *out_path,
                         int scale, QrxUpsFilter filter) {
    QrxUpsImage src, dst;
    int rc = qrx_ups_image_load(in_path, &src);
    if (rc != QRX_UPS_OK) return rc;
    rc = qrx_ups_upscale(&src, scale, filter, &dst);
    qrx_ups_image_free(&src);
    if (rc != QRX_UPS_OK) return rc;
    rc = qrx_ups_image_save(out_path, &dst);
    qrx_ups_image_free(&dst);
    return rc;
}

/* ------------------------------------------------------------------ */
/* batch                                                               */
/* ------------------------------------------------------------------ */

/* A frame name is used to build an output path, so it is untrusted input.
 * Only a plain file name is accepted: no separators, no parent references,
 * no drive letters, no leading dash. */
static int safe_leaf_name(const char *name) {
    if (!name || !*name) return 0;
    if (!strcmp(name, ".") || !strcmp(name, "..")) return 0;
    if (name[0] == '-') return 0;
    for (const char *p = name; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':') return 0;
        if ((unsigned char)*p < 0x20) return 0;
    }
    if (strstr(name, "..")) return 0;
    return 1;
}

typedef struct { char **items; size_t count; } NameList;

static void namelist_free(NameList *l) {
    for (size_t i = 0; i < l->count; i++) free(l->items[i]);
    free(l->items);
    l->items = NULL; l->count = 0;
}

static int namelist_push(NameList *l, const char *s) {
    char **g = (char **)realloc(l->items, (l->count + 1) * sizeof(char *));
    if (!g) return -1;
    l->items = g;
    l->items[l->count] = strdup(s);
    if (!l->items[l->count]) return -1;
    l->count++;
    return 0;
}

static int name_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int is_frame_name(const char *n) {
    return ext_is(n, "png") || ext_is(n, "ppm") || ext_is(n, "pgm") || ext_is(n, "pnm");
}

/* Native enumeration. No shell, symlinks skipped, deterministic order. */
static int list_frames(const char *dir, NameList *out) {
    out->items = NULL; out->count = 0;
#ifdef _WIN32
    char pattern[1400];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return QRX_UPS_ERR_IO;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (!safe_leaf_name(fd.cFileName) || !is_frame_name(fd.cFileName)) continue;
        if (out->count >= QRX_UPS_MAX_BATCH_FILES) { FindClose(h); namelist_free(out); return QRX_UPS_ERR_LIMIT; }
        if (namelist_push(out, fd.cFileName) != 0) { FindClose(h); namelist_free(out); return QRX_UPS_ERR_OOM; }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    if (!d) return QRX_UPS_ERR_IO;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!safe_leaf_name(e->d_name) || !is_frame_name(e->d_name)) continue;
        char full[1400];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat st;
        if (lstat(full, &st) != 0) continue;      /* lstat: do not follow links */
        if (!S_ISREG(st.st_mode)) continue;
        if (out->count >= QRX_UPS_MAX_BATCH_FILES) { closedir(d); namelist_free(out); return QRX_UPS_ERR_LIMIT; }
        if (namelist_push(out, e->d_name) != 0) { closedir(d); namelist_free(out); return QRX_UPS_ERR_OOM; }
    }
    closedir(d);
#endif
    if (out->count > 1) qsort(out->items, out->count, sizeof(char *), name_cmp);
    return QRX_UPS_OK;
}

int qrx_ups_upscale_batch(const char *in_dir, const char *out_dir,
                          int scale, QrxUpsFilter filter,
                          int shard_index, int shard_count,
                          QrxUpsBatchStats *stats) {
    if (!in_dir || !out_dir) return QRX_UPS_ERR_ARG;
    if (shard_count < 1 || shard_index < 0 || shard_index >= shard_count) return QRX_UPS_ERR_ARG;
    if (stats) memset(stats, 0, sizeof(*stats));

    NameList names;
    int rc = list_frames(in_dir, &names);
    if (rc != QRX_UPS_OK) return rc;
    if (stats) stats->files_total = names.count;

    for (size_t i = 0; i < names.count; i++) {
        if ((int)(i % (size_t)shard_count) != shard_index) continue;
        char ip[1400], op[1400];
        snprintf(ip, sizeof(ip), "%s%c%s", in_dir, QRX_UPS_SEP, names.items[i]);
        snprintf(op, sizeof(op), "%s%c%s", out_dir, QRX_UPS_SEP, names.items[i]);
        int r = qrx_ups_upscale_file(ip, op, scale, filter);
        if (r == QRX_UPS_OK) { if (stats) stats->files_done++; }
        else {
            if (stats) stats->files_failed++;
            fprintf(stderr, "qrx-upscaler: %s: %s\n", names.items[i], qrx_ups_strerror(r));
        }
    }
    namelist_free(&names);
    if (stats && stats->files_failed) return QRX_UPS_ERR_IO;
    return QRX_UPS_OK;
}
