#pragma once
#include "qrxdb.h"
#include "resource/qrx_storage_consensus.h"
#include "storage/qrx_storage_fs.h"
#include "storage/qrx_merkle.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_STORAGE_REPAIR_MAX 64

typedef struct {
    char contract_id[129];
    uint32_t shard_index;
    QrxStorageAssignmentRecord assignment;
    uint64_t due_height;
} QrxStorageRepairStartCandidate;

typedef struct {
    char contract_id[129];
    uint32_t shard_index;
    QrxStorageRepairRecord repair;
    int needs_reconstruction;
    int needs_accept;
    int proof_due;
    uint64_t proof_height;
    uint64_t proof_epoch;
    uint64_t leaf_index;
} QrxStorageReplacementWork;

int qrx_storage_repair_collect_starts(QrxDB *db,uint64_t current_height,QrxStorageRepairStartCandidate *out,size_t cap,size_t *count_out);
int qrx_storage_repair_collect_replacement(QrxDB *db,const char *provider,uint64_t current_height,QrxStorageFs *fs,QrxStorageReplacementWork *out,size_t cap,size_t *count_out);
int qrx_storage_repair_start_payload(const QrxStorageRepairStartCandidate *c,char **payload_out);
int qrx_storage_repair_accept_payload(const QrxStorageReplacementWork *w,char **payload_out);
int qrx_storage_repair_build_complete_proof(QrxDB *db,QrxStorageFs *fs,const QrxStorageReplacementWork *w,unsigned char **leaf_out,size_t *leaf_len_out,QrxMerkleProof *proof_out);
int qrx_storage_repair_complete_payload(const QrxStorageReplacementWork *w,const unsigned char *leaf,size_t leaf_len,const QrxMerkleProof *proof,char **payload_out);
#ifdef __cplusplus
}
#endif
