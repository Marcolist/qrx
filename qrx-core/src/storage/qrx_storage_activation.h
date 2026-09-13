#pragma once
#include "qrxdb.h"
#include "storage/qrx_storage_fs.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_STORAGE_ACTIVATION_MAX_READY 64

typedef struct {
    char contract_id[129];
    uint32_t shard_index;
    char object_id[65];
} QrxStorageReadyAssignment;

/* Verifies that the local provider really has the exact PENDING shard committed
 * by consensus: CAS id, physical byte size and PoStor Merkle root/leaf count. */
int qrx_storage_assignment_local_ready(QrxDB *db,QrxStorageFs *fs,const char *provider,
                                       const char *contract_id,uint32_t shard_index);
/* Collects ready PENDING assignments for the local provider. No chain state is
 * changed here; the provider must still sign STORAGE_ASSIGN_ACCEPT on-chain. */
int qrx_storage_collect_ready_assignments(QrxDB *db,QrxStorageFs *fs,const char *provider,
                                          QrxStorageReadyAssignment *out,size_t cap,size_t *count_out);
#ifdef __cplusplus
}
#endif
