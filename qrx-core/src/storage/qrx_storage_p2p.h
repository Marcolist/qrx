#pragma once
#include "qrxdb.h"
#include "storage/qrx_storage_fs.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_STORAGE_P2P_MAX_RANGE (128U * 1024U)
int qrx_storage_p2p_read_authorized(QrxDB *db,QrxStorageFs *fs,const char *local_provider_id,
                                    const char *contract_id,uint32_t shard_index,const char *object_id,
                                    uint64_t offset,size_t length,unsigned char **out,size_t *out_len);
#ifdef __cplusplus
}
#endif
int qrx_storage_p2p_write_authorized_file(QrxDB *db,QrxStorageFs *fs,const char *local_provider_id,
                                          const char *contract_id,uint32_t shard_index,const char *object_id,
                                          const char *source_path);
