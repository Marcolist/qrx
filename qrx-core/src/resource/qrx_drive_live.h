#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char contract_id[129];
    char owner[160];
    char profile[16];
    uint64_t logical_bytes;
    uint64_t shard_bytes;
    uint32_t shard_count;
    uint32_t required_shards;
    uint32_t healthy_shards;
    uint32_t degraded_shards;
    uint32_t repairing_shards;
    uint32_t missing_shards;
    uint64_t start_height;
    uint64_t end_height;
    uint32_t status;
    char first_object_id[65];
} QrxDriveFileSnapshot;

typedef struct {
    uint32_t shard_index;
    uint32_t state;
    char provider_id[129];
    char region[96];
    char object_id[65];
    uint64_t physical_bytes;
    uint8_t region_public;
} QrxDriveShardRoute;

int qrx_drive_live_list(const char *chain_dir,const char *owner_filter,QrxDriveFileSnapshot **out,size_t *out_count);
int qrx_drive_live_routes(const char *chain_dir,const char *contract_id,size_t privacy_min_providers,
                          QrxDriveShardRoute **out,size_t *out_count,QrxDriveFileSnapshot *file_out);
void qrx_drive_live_files_free(QrxDriveFileSnapshot *p);
void qrx_drive_live_routes_free(QrxDriveShardRoute *p);
#ifdef __cplusplus
}
#endif
