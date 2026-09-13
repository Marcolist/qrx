/*
 * QRX Upscaler - deterministic image/frame upscaling core.
 *
 * Copyright (c) 2026 QRX contributors.
 * SPDX-License-Identifier: MIT
 *
 * Dependencies are deliberately limited to permissive, non-copyleft licences:
 *   - zlib    (zlib licence)      optional, only pulled in through libpng
 *   - libpng  (libpng/zlib-style) optional, enabled with QRX_HAVE_PNG
 * PPM/PGM support is built in and needs no external library at all, so the
 * upscaler always builds even with no image library present.
 *
 * Determinism
 * -----------
 * All resampling uses fixed-point integer arithmetic with weights normalised
 * so each kernel sums to exactly QRX_UPS_ONE. Floating point is never used in
 * the pixel path. This matters beyond reproducibility: in accelerated mode a
 * frame batch is a Proof-of-Useful-Compute job, and independent providers must
 * produce byte-identical output for a result to be verifiable at all.
 */

#ifndef QRX_UPSCALER_H
#define QRX_UPSCALER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed-point scale for resampling weights: 1.0 == 1 << 16. */
#define QRX_UPS_ONE 65536

/* Hard safety limits. Image and video containers are untrusted input; a
 * decoder or a hostile manifest must not be able to make the process allocate
 * without bound. Exceeding any limit is an error, never a clamp. */
#define QRX_UPS_MAX_DIM         16384          /* per side, input or output   */
#define QRX_UPS_MAX_PIXELS      67108864       /* 64 MPix, input or output    */
#define QRX_UPS_MAX_CHANNELS    4
#define QRX_UPS_MAX_SCALE       8
#define QRX_UPS_MAX_BATCH_FILES 200000         /* frames in one batch         */

typedef enum {
    QRX_UPS_NEAREST = 0,   /* exact pixel replication, no interpolation       */
    QRX_UPS_BILINEAR,      /* cheap, soft                                     */
    QRX_UPS_BICUBIC,       /* Mitchell-Netravali B=1/3 C=1/3                  */
    QRX_UPS_LANCZOS3       /* default: sharpest of the classical kernels      */
} QrxUpsFilter;

typedef enum {
    QRX_UPS_OK = 0,
    QRX_UPS_ERR_ARG = -1,
    QRX_UPS_ERR_IO = -2,
    QRX_UPS_ERR_FORMAT = -3,
    QRX_UPS_ERR_LIMIT = -4,
    QRX_UPS_ERR_OOM = -5,
    QRX_UPS_ERR_UNSUPPORTED = -6,
    QRX_UPS_ERR_PATH = -7          /* traversal / unsafe path rejected        */
} QrxUpsStatus;

typedef struct {
    int32_t  width;
    int32_t  height;
    int32_t  channels;             /* 1 = gray, 3 = RGB, 4 = RGBA             */
    uint8_t *pixels;               /* width * height * channels, 8 bit        */
} QrxUpsImage;

const char *qrx_ups_strerror(int status);
const char *qrx_ups_filter_name(QrxUpsFilter f);
int         qrx_ups_filter_from_name(const char *name, QrxUpsFilter *out);

/* Image lifecycle. */
int  qrx_ups_image_alloc(QrxUpsImage *img, int32_t w, int32_t h, int32_t ch);
void qrx_ups_image_free(QrxUpsImage *img);

/* Format-detecting load/save. PNG requires QRX_HAVE_PNG, PPM/PGM always work.
 * Format is chosen from the file contents on load and from the extension on
 * save; never from attacker-supplied metadata. */
int qrx_ups_image_load(const char *path, QrxUpsImage *out);
int qrx_ups_image_save(const char *path, const QrxUpsImage *img);

/* Core operation: integer-scale upscale of `src` into `dst`. */
int qrx_ups_upscale(const QrxUpsImage *src, int scale, QrxUpsFilter filter,
                    QrxUpsImage *dst);

/* Single file convenience wrapper. */
int qrx_ups_upscale_file(const char *in_path, const char *out_path,
                         int scale, QrxUpsFilter filter);

typedef struct {
    size_t files_total;
    size_t files_done;
    size_t files_failed;
} QrxUpsBatchStats;

/* Batch a directory of frames. Entries are enumerated natively (no shell),
 * symlinks are ignored, and the order is deterministic so that frame N always
 * maps to the same output regardless of filesystem order. `shard_index` and
 * `shard_count` let several workers split one batch without coordination:
 * worker i processes frames where (index % shard_count) == shard_index. */
int qrx_ups_upscale_batch(const char *in_dir, const char *out_dir,
                          int scale, QrxUpsFilter filter,
                          int shard_index, int shard_count,
                          QrxUpsBatchStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* QRX_UPSCALER_H */
