#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_STORAGE_TRANSPORT_VERSION 2
#define QRX_STORAGE_MAX_FETCH_SOURCES 32
#define QRX_STORAGE_STANDARD_DATA_SHARDS 10
#define QRX_STORAGE_STANDARD_PARITY_SHARDS 4
#define QRX_STORAGE_STANDARD_TOTAL_SHARDS 14
#define QRX_STORAGE_DEFAULT_FETCH_RANGE (128U * 1024U)

typedef struct {uint32_t shard_index;uint32_t expected_latency_ms;uint64_t throughput_bps;uint32_t reliability_bps;} QrxShardSource;
typedef struct {uint32_t ordered_shards[QRX_STORAGE_MAX_FETCH_SOURCES];size_t source_count;size_t initial_parallel;size_t required_successes;size_t hedge_after_failures;int cancel_remaining_after_k;} QrxShardFetchPlan;
typedef struct {uint8_t object_id[64];uint64_t offset;uint32_t length;uint32_t shard_index;} QrxShardRangeRequest;

/* Provider-aware downloader. fetch_range returns 0 and an allocated buffer on
 * success. The downloader owns/free()s that buffer. Short successful reads are
 * legal and are resumed at the next byte. verify_shard is optional and runs on
 * a complete shard before it is admitted to reconstruction. */
typedef struct {
    const char *provider_id;
    const char *endpoint;
    const char *object_id_hex; /* authoritative per-shard CAS id */
    QrxShardSource source;
} QrxShardProviderSource;
typedef int (*QrxShardFetchRangeFn)(void *ctx,const QrxShardProviderSource *source,
                                    const uint8_t object_id[64],uint64_t offset,size_t length,
                                    uint8_t **out,size_t *out_len);
typedef int (*QrxShardVerifyFn)(void *ctx,uint32_t shard_index,const uint8_t *data,size_t data_len);
typedef struct {
    size_t required_successes;
    size_t initial_parallel;
    size_t hedge_extra;
    size_t range_bytes;
    uint32_t hedge_delay_ms;
    size_t max_range_retries;
} QrxMultiFetchOptions;
typedef struct {
    size_t sources_started;
    size_t sources_completed;
    size_t sources_failed;
    size_t successful_shards;
    size_t hedges_started;
    size_t resumed_ranges;
    size_t cancelled_sources;
    uint64_t bytes_received;
} QrxMultiFetchStats;

int qrx_storage_fetch_plan(const QrxShardSource *sources,size_t source_count,size_t required_successes,size_t hedge_extra,QrxShardFetchPlan *out);
int qrx_storage_range_serialize(const QrxShardRangeRequest *r,uint8_t out[88]);
int qrx_storage_range_parse(const uint8_t in[88],QrxShardRangeRequest *r);
int qrx_storage_multi_provider_fetch(const QrxShardProviderSource *sources,size_t source_count,
                                     const uint8_t object_id[64],size_t shard_size,
                                     unsigned data_shards,unsigned parity_shards,size_t original_size,
                                     const QrxMultiFetchOptions *options,QrxShardFetchRangeFn fetch_range,
                                     QrxShardVerifyFn verify_shard,void *ctx,
                                     uint8_t **data_out,size_t *data_len_out,QrxMultiFetchStats *stats_out);
#ifdef __cplusplus
}
#endif

/* Memory-constant restore: fetches bounded ranges, reconstructs one stripe at
 * a time, and writes each data-shard stripe to its final file offset. Peak
 * erasure payload memory is O((k+m)*stripe_bytes), independent of file size. */
int qrx_storage_stream_reconstruct_to_file(const QrxShardProviderSource *sources,size_t source_count,size_t shard_size,unsigned data_shards,unsigned parity_shards,size_t original_size,size_t stripe_bytes,QrxShardFetchRangeFn fetch_range,void *ctx,const char *output_path,QrxMultiFetchStats *stats_out);

/* Rebuild exactly one missing erasure shard from any k healthy sources. The
 * output is streamed stripe-by-stripe and is accepted only when its SHA3-256
 * CAS id matches expected_object_id_hex. Source shards are also CAS-verified. */
int qrx_storage_stream_reconstruct_shard_to_file(const QrxShardProviderSource *sources,size_t source_count,
                                                  size_t shard_size,unsigned data_shards,unsigned parity_shards,
                                                  uint32_t target_shard,size_t stripe_bytes,
                                                  QrxShardFetchRangeFn fetch_range,void *ctx,
                                                  const char *expected_object_id_hex,const char *output_path,
                                                  QrxMultiFetchStats *stats_out);
