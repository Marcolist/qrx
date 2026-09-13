#pragma once
#include "qrxdb.h"
#include "storage/qrx_storage_fs.h"
#include "storage/qrx_merkle.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    QRX_POSTOR_HEALTH_ACTIVE = 1,
    QRX_POSTOR_HEALTH_PROOF_DUE = 2,
    QRX_POSTOR_HEALTH_DEGRADED = 3,
    QRX_POSTOR_HEALTH_REPAIRING = 4
} QrxPoStorHealth;

typedef struct {
    char contract_id[129];
    uint32_t shard_index;
    char object_id[65];
    uint64_t physical_bytes;
    uint64_t leaf_count;
    uint64_t epoch;
    uint64_t challenge_height;
    uint64_t leaf_index;
    QrxPoStorHealth health;
    int repair_ready;
} QrxPoStorDueAssignment;

int qrx_storage_postor_collect(QrxDB *db,const char *provider,uint64_t current_height,
                               QrxPoStorDueAssignment *out,size_t cap,size_t *count_out);
int qrx_storage_postor_build_local(QrxDB *db,QrxStorageFs *fs,const char *provider,
                                   const QrxPoStorDueAssignment *due,
                                   unsigned char **leaf_out,size_t *leaf_len_out,
                                   QrxMerkleProof *proof_out);
int qrx_storage_postor_payload(const QrxPoStorDueAssignment *due,
                               const unsigned char *leaf,size_t leaf_len,
                               const QrxMerkleProof *proof,char **payload_out);
const char *qrx_storage_postor_health_name(QrxPoStorHealth health);

#ifdef __cplusplus
}
#endif
