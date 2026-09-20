/*
 * qrx-upscaler - command line front end.
 *
 * Copyright (c) 2026 QRX contributors.
 * SPDX-License-Identifier: MIT
 *
 * Licence boundary
 * ----------------
 * Image decoding, upscaling and encoding are implemented here or on top of
 * libpng/zlib, all permissive. No GPL or LGPL component is linked or bundled.
 *
 * Video demux/mux needs a codec stack that QRX deliberately does not ship.
 * The `video` subcommand therefore drives an *external* media tool that the
 * operator installs and points at explicitly via QRX_UPSCALER_MEDIA_TOOL.
 * It is launched as a separate process with an argument vector - never a
 * shell command string - so a hostile filename cannot inject arguments, and
 * the licence of that tool never propagates into QRX.
 */

#include "apps/qrx_upscaler.h"
#include "apps/qrx_upscaler_capabilities.h"
#include "apps/qrx_upscaler_ai.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#  include <direct.h>
#  include <process.h>
#  define qrx_mkdir(p) _mkdir(p)
#else
#  include <unistd.h>
#  include <sys/wait.h>
#  define qrx_mkdir(p) mkdir((p), 0755)
#endif

static void usage(void) {
    fputs(
     "qrx-upscaler - deterministic image and frame upscaling\n"
     "\n"
     "  qrx-upscaler image  <in> <out> [--scale N] [--filter F]\n"
     "  qrx-upscaler batch  <in-dir> <out-dir> [--scale N] [--filter F]\n"
     "                      [--shard I --shards N]\n"
     "  qrx-upscaler video  <in> <out> [--scale N] [--filter F] [--work DIR]\n"
     "  qrx-upscaler ai-image <in> <out> --scale 2|4 [--tile N] [--model ID]\n"
     "  qrx-upscaler ai-status\n"
     "  qrx-upscaler verify-model <manifest.qrxmodel>\n"
     "  qrx-upscaler capabilities\n"
     "  qrx-upscaler selftest\n"
     "\n"
     "  --scale N     integer factor, 1..8 (default 2)\n"
     "  --filter F    nearest | bilinear | bicubic | lanczos3 (default lanczos3)\n"
     "  --shard I     process only frames where index %% shards == I\n"
     "  --shards N    number of parallel workers sharing one batch\n"
     "\n"
     "Formats: PPM/PGM always; PNG when built with QRX_HAVE_PNG.\n"
     "\n"
     "video requires an external media tool for demux/mux. QRX does not bundle\n"
     "one, because the usual choices are GPL/LGPL. Set QRX_UPSCALER_MEDIA_TOOL\n"
     "to its absolute path. Frame upscaling itself never leaves this program.\n",
     stderr);
}

static int parse_int(const char *s, int lo, int hi, int *out) {
    if (!s || !*s) return -1;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || (end && *end) || v < lo || v > hi) return -1;
    *out = (int)v;
    return 0;
}

typedef struct {
    int scale;
    QrxUpsFilter filter;
    int shard, shards;
    const char *work;
} Opts;

static int parse_opts(int argc, char **argv, int from, Opts *o) {
    o->scale = 2; o->filter = QRX_UPS_LANCZOS3;
    o->shard = 0; o->shards = 1; o->work = NULL;
    for (int i = from; i < argc; i++) {
        if (!strcmp(argv[i], "--scale") && i + 1 < argc) {
            if (parse_int(argv[++i], 1, QRX_UPS_MAX_SCALE, &o->scale)) return -1;
        } else if (!strcmp(argv[i], "--filter") && i + 1 < argc) {
            if (qrx_ups_filter_from_name(argv[++i], &o->filter) != QRX_UPS_OK) return -1;
        } else if (!strcmp(argv[i], "--shard") && i + 1 < argc) {
            if (parse_int(argv[++i], 0, 4095, &o->shard)) return -1;
        } else if (!strcmp(argv[i], "--shards") && i + 1 < argc) {
            if (parse_int(argv[++i], 1, 4096, &o->shards)) return -1;
        } else if (!strcmp(argv[i], "--work") && i + 1 < argc) {
            o->work = argv[++i];
        } else {
            fprintf(stderr, "qrx-upscaler: unknown option %s\n", argv[i]);
            return -1;
        }
    }
    if (o->shard >= o->shards) { fprintf(stderr, "qrx-upscaler: --shard must be < --shards\n"); return -1; }
    return 0;
}

/* ---------------------------------------------------------------- */
/* external media tool, argv only, never a shell                     */
/* ---------------------------------------------------------------- */

static int run_tool(const char *tool, char *const argv[]) {
#ifdef _WIN32
    intptr_t rc = _spawnv(_P_WAIT, tool, (const char *const *)argv);
    return rc == 0 ? 0 : -1;
#else
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execv(tool, argv);
        _exit(127);
    }
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) return -1;
    return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
#endif
}

static const char *media_tool(void) {
    const char *t = getenv("QRX_UPSCALER_MEDIA_TOOL");
    if (!t || !*t) return NULL;
    /* Absolute path only: never resolve through PATH, so the tool that runs is
     * the one the operator chose and not something planted earlier in PATH. */
#ifdef _WIN32
    if (!((t[0] && t[1] == ':') || t[0] == '\\')) return NULL;
#else
    if (t[0] != '/') return NULL;
#endif
    struct stat st;
    if (stat(t, &st) != 0) return NULL;
    return t;
}

static int cmd_video(const char *in, const char *out, Opts *o) {
    const char *tool = media_tool();
    if (!tool) {
        fputs("qrx-upscaler: video needs an external media tool.\n"
              "  QRX does not bundle one: the common demuxers are GPL/LGPL and\n"
              "  QRX ships only permissively licensed components.\n"
              "  Install one yourself and set QRX_UPSCALER_MEDIA_TOOL to its\n"
              "  absolute path, then re-run. Image and batch modes work without it.\n",
              stderr);
        return 3;
    }

    char work[1024];
    snprintf(work, sizeof(work), "%s", o->work ? o->work : "./qrx-upscaler-work");
    char raw[1200], up[1200];
    snprintf(raw, sizeof(raw), "%s%cframes-in", work, '/');
    snprintf(up, sizeof(up), "%s%cframes-out", work, '/');
    qrx_mkdir(work); qrx_mkdir(raw); qrx_mkdir(up);

    char pattern_in[1300], pattern_out[1300], scale_arg[32];
    snprintf(pattern_in, sizeof(pattern_in), "%s/frame-%%06d.png", raw);
    snprintf(pattern_out, sizeof(pattern_out), "%s/frame-%%06d.png", up);
    snprintf(scale_arg, sizeof(scale_arg), "%d", o->scale);

    /* 1. demux to frames */
    {
        char *av[] = { (char *)tool, (char *)"-nostdin", (char *)"-y",
                       (char *)"-i", (char *)in, pattern_in, NULL };
        if (run_tool(tool, av) != 0) {
            fprintf(stderr, "qrx-upscaler: frame extraction failed\n");
            return 4;
        }
    }

    /* 2. upscale every frame, in this process, deterministically */
    QrxUpsBatchStats st;
    int rc = qrx_ups_upscale_batch(raw, up, o->scale, o->filter,
                                   o->shard, o->shards, &st);
    fprintf(stderr, "qrx-upscaler: %zu/%zu frames upscaled (%s x%d)\n",
            st.files_done, st.files_total, qrx_ups_filter_name(o->filter), o->scale);
    if (rc != QRX_UPS_OK) return 5;

    /* 3. remux, carrying the original audio across unchanged */
    {
        char *av[] = { (char *)tool, (char *)"-nostdin", (char *)"-y",
                       (char *)"-i", pattern_out,
                       (char *)"-i", (char *)in,
                       (char *)"-map", (char *)"0:v:0",
                       (char *)"-map", (char *)"1:a?",
                       (char *)"-c:a", (char *)"copy",
                       (char *)out, NULL };
        if (run_tool(tool, av) != 0) {
            fprintf(stderr, "qrx-upscaler: remux failed\n");
            return 6;
        }
    }
    return 0;
}

static int cmd_ai_status(void) {
    QrxUpsAiStatus a; if(qrx_ups_ai_status(&a)) return 1;
    printf("{\n  \"backend\": \"%s\",\n  \"runtime_installed\": %s,\n  \"runtime_verified\": %s,\n  \"runtime_path\": \"%s\",\n  \"model_dir\": \"%s\",\n  \"model_x2_installed\": %s,\n  \"model_x2_verified\": %s,\n  \"model_x2_id\": \"%s\",\n  \"model_x4_installed\": %s,\n  \"model_x4_verified\": %s,\n  \"model_x4_id\": \"%s\",\n  \"ai_ready\": %s,\n  \"status\": \"%s\"\n}\n",
      a.backend,a.runtime_installed?"true":"false",a.runtime_verified?"true":"false",a.runtime_path,a.model_dir,
      a.model_x2_installed?"true":"false",a.model_x2_verified?"true":"false",a.model_x2_id,
      a.model_x4_installed?"true":"false",a.model_x4_verified?"true":"false",a.model_x4_id,
      a.ai_ready?"true":"false",a.status); return 0;
}
static int cmd_verify_model(const char*p){char e[256];int rc=qrx_ups_ai_verify_manifest(p,e,sizeof(e));if(rc){fprintf(stderr,"qrx-upscaler: model verification failed: %s\n",e);return 2;}puts("model_verified=true");return 0;}
static int cmd_ai_image(int argc,char**argv){if(argc<5){usage();return 2;}QrxUpsCapabilities caps;if(qrx_ups_capabilities_probe(&caps)!=0||!caps.ai_runtime_ready){fputs("qrx-upscaler: local AI disabled: no usable Vulkan/MoltenVK accelerator on this client.\n",stderr);return 8;}int scale=0;unsigned tile=0;const char*model=NULL;for(int i=4;i<argc;i++){if(!strcmp(argv[i],"--scale")&&i+1<argc){if(parse_int(argv[++i],2,4,&scale)||!(scale==2||scale==4))return 2;}else if(!strcmp(argv[i],"--tile")&&i+1<argc){int x=0;if(parse_int(argv[++i],0,2048,&x))return 2;tile=(unsigned)x;}else if(!strcmp(argv[i],"--model")&&i+1<argc)model=argv[++i];else{fprintf(stderr,"qrx-upscaler: unknown AI option %s\n",argv[i]);return 2;}}if(!scale){fputs("qrx-upscaler: ai-image requires --scale 2 or 4\n",stderr);return 2;}if(tile==0){QrxUpsCapabilities c;if(qrx_ups_capabilities_probe(&c)==0)tile=c.recommended_tile;}int rc=qrx_ups_ai_run_image(argv[2],argv[3],scale,tile,model);if(rc){fprintf(stderr,"qrx-upscaler: AI inference unavailable/failed (code %d). Run 'qrx-upscaler ai-status'.\n",rc);return 7;}return 0;}

/* ---------------------------------------------------------------- */
/* selftest: proves the pixel path works and is deterministic        */
/* ---------------------------------------------------------------- */

static int cmd_capabilities(void) {
    QrxUpsCapabilities c; QrxUpsAiStatus a;
    if (qrx_ups_capabilities_probe(&c) != 0 || qrx_ups_ai_status(&a) != 0) {
        fputs("qrx-upscaler: capability probe failed\n", stderr);
        return 1;
    }
    printf("{\n"
           "  \"version\": %u,\n"
           "  \"device_model\": \"%s\",\n"
           "  \"apple_silicon_generation\": \"%s\",\n"
           "  \"apple_silicon_variant\": \"%s\",\n"
           "  \"os\": \"%s\",\n"
           "  \"arch\": \"%s\",\n"
           "  \"memory_bytes\": %llu,\n"
           "  \"hardware_profile\": \"%s\",\n"
           "  \"ai_support\": \"%s\",\n"
           "  \"vulkan_loader_present\": %s,\n"
           "  \"render_device_present\": %s,\n"
           "  \"native_metal_present\": %s,\n"
           "  \"metal_version_major\": %u,\n"
           "  \"vulkan_mode\": \"%s\",\n"
           "  \"accelerator\": \"%s\",\n"
           "  \"ai_runtime_ready\": %s,\n"
           "  \"runtime_installed\": %s,\n"
           "  \"runtime_verified\": %s,\n"
           "  \"model_x2_verified\": %s,\n"
           "  \"model_x4_verified\": %s,\n"
           "  \"ai_ready\": %s,\n"
           "  \"ai_backend\": \"%s\",\n"
           "  \"model_dir\": \"%s\",\n"
           "  \"classical_available\": %s,\n"
           "  \"recommended_tile\": %u,\n"
           "  \"recommended_threads\": %u,\n"
           "  \"status\": \"%s\"\n"
           "}\n",
           c.version,c.device_model,c.apple_silicon_generation,c.apple_silicon_variant,c.os,c.arch,
           (unsigned long long)c.memory_bytes,
           qrx_ups_hardware_profile_name(c.hardware_profile),
           qrx_ups_ai_support_name(c.ai_support),
           c.vulkan_loader_present?"true":"false",
           c.render_device_present?"true":"false",
           c.native_metal_present?"true":"false",
           c.metal_version_major,c.vulkan_mode,c.accelerator,
           c.ai_runtime_ready?"true":"false",
           a.runtime_installed?"true":"false",a.runtime_verified?"true":"false",a.model_x2_verified?"true":"false",a.model_x4_verified?"true":"false",(a.ai_ready&&c.ai_runtime_ready)?"true":"false",a.backend,a.model_dir,
           c.classical_available?"true":"false",
           c.recommended_tile,c.recommended_threads,c.status);
    return 0;
}

static int cmd_selftest(void) {
    int failures = 0;
    QrxUpsImage src, a, b;

    if (qrx_ups_image_alloc(&src, 7, 5, 3) != QRX_UPS_OK) { puts("FAIL alloc"); return 1; }
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 7; x++) {
            uint8_t *p = src.pixels + ((size_t)y * 7 + x) * 3;
            p[0] = (uint8_t)(x * 31); p[1] = (uint8_t)(y * 51); p[2] = (uint8_t)((x + y) * 17);
        }

    /* geometry */
    if (qrx_ups_upscale(&src, 3, QRX_UPS_LANCZOS3, &a) != QRX_UPS_OK) { puts("FAIL upscale"); return 1; }
    printf("  [%s] output geometry %dx%d\n",
           (a.width == 21 && a.height == 15) ? "PASS" : "FAIL", a.width, a.height);
    if (a.width != 21 || a.height != 15) failures++;

    /* determinism: identical input must give byte-identical output */
    if (qrx_ups_upscale(&src, 3, QRX_UPS_LANCZOS3, &b) != QRX_UPS_OK) { puts("FAIL upscale2"); return 1; }
    int same = (memcmp(a.pixels, b.pixels, (size_t)a.width * a.height * 3) == 0);
    printf("  [%s] repeated run is byte-identical\n", same ? "PASS" : "FAIL");
    if (!same) failures++;
    qrx_ups_image_free(&b);

    /* nearest must replicate exactly, so corners are preserved */
    QrxUpsImage n;
    if (qrx_ups_upscale(&src, 2, QRX_UPS_NEAREST, &n) == QRX_UPS_OK) {
        int ok = n.pixels[0] == src.pixels[0] && n.pixels[1] == src.pixels[1];
        printf("  [%s] nearest preserves source pixels\n", ok ? "PASS" : "FAIL");
        if (!ok) failures++;
        qrx_ups_image_free(&n);
    } else { puts("  [FAIL] nearest"); failures++; }

    /* a flat image must stay exactly flat: proves kernel weights sum to 1 */
    QrxUpsImage flat, fo;
    qrx_ups_image_alloc(&flat, 6, 6, 3);
    memset(flat.pixels, 137, (size_t)6 * 6 * 3);
    if (qrx_ups_upscale(&flat, 4, QRX_UPS_LANCZOS3, &fo) == QRX_UPS_OK) {
        int ok = 1;
        for (size_t i = 0; i < (size_t)fo.width * fo.height * 3; i++)
            if (fo.pixels[i] != 137) { ok = 0; break; }
        printf("  [%s] constant image is preserved exactly (no rounding drift)\n", ok ? "PASS" : "FAIL");
        if (!ok) failures++;
        qrx_ups_image_free(&fo);
    } else { puts("  [FAIL] flat"); failures++; }
    qrx_ups_image_free(&flat);

    /* safety limits must reject, not clamp */
    QrxUpsImage big, dummy;
    big.width = QRX_UPS_MAX_DIM; big.height = QRX_UPS_MAX_DIM; big.channels = 3;
    big.pixels = src.pixels; /* not dereferenced: the limit check runs first */
    int limited = qrx_ups_upscale(&big, 2, QRX_UPS_LANCZOS3, &dummy) == QRX_UPS_ERR_LIMIT;
    printf("  [%s] oversized request rejected\n", limited ? "PASS" : "FAIL");
    if (!limited) failures++;

    int badscale = qrx_ups_upscale(&src, 99, QRX_UPS_LANCZOS3, &dummy) == QRX_UPS_ERR_ARG;
    printf("  [%s] out-of-range scale rejected\n", badscale ? "PASS" : "FAIL");
    if (!badscale) failures++;

    qrx_ups_image_free(&a);
    qrx_ups_image_free(&src);

    printf("%s\n", failures ? "SELFTEST FAILED" : "SELFTEST PASS");
    return failures ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 2; }

    if (!strcmp(argv[1], "capabilities")) return cmd_capabilities();
    if (!strcmp(argv[1], "ai-status")) return cmd_ai_status();
    if (!strcmp(argv[1], "verify-model") && argc == 3) return cmd_verify_model(argv[2]);
    if (!strcmp(argv[1], "ai-image") && argc >= 4) return cmd_ai_image(argc, argv);
    if (!strcmp(argv[1], "selftest")) return cmd_selftest();

    if (!strcmp(argv[1], "image") && argc >= 4) {
        Opts o;
        if (parse_opts(argc, argv, 4, &o) != 0) return 2;
        int rc = qrx_ups_upscale_file(argv[2], argv[3], o.scale, o.filter);
        if (rc != QRX_UPS_OK) {
            fprintf(stderr, "qrx-upscaler: %s\n", qrx_ups_strerror(rc));
            return 1;
        }
        fprintf(stderr, "qrx-upscaler: %s -> %s (%s x%d)\n",
                argv[2], argv[3], qrx_ups_filter_name(o.filter), o.scale);
        return 0;
    }

    if (!strcmp(argv[1], "batch") && argc >= 4) {
        Opts o;
        if (parse_opts(argc, argv, 4, &o) != 0) return 2;
        qrx_mkdir(argv[3]);
        QrxUpsBatchStats st;
        int rc = qrx_ups_upscale_batch(argv[2], argv[3], o.scale, o.filter,
                                       o.shard, o.shards, &st);
        fprintf(stderr, "qrx-upscaler: %zu/%zu frames upscaled, %zu failed (%s x%d)\n",
                st.files_done, st.files_total, st.files_failed,
                qrx_ups_filter_name(o.filter), o.scale);
        return rc == QRX_UPS_OK ? 0 : 1;
    }

    if (!strcmp(argv[1], "video") && argc >= 4) {
        Opts o;
        if (parse_opts(argc, argv, 4, &o) != 0) return 2;
        return cmd_video(argv[2], argv[3], &o);
    }

    usage();
    return 2;
}
