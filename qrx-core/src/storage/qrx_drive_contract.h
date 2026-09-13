#pragma once
#include "storage/qrx_drive_prepare.h"
#include "resource/qrx_storage_consensus.h"
#include "qrxdb.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_DRIVE_CONTRACT_DEFAULT_EPOCHS 30ULL
#define QRX_DRIVE_CONTRACT_EPOCH_BLOCKS QRX_STORAGE_PROOF_WINDOW_BLOCKS
#define QRX_DRIVE_CONTRACT_DEFAULT_RATE_ATOMS 10000ULL

typedef enum {
 QRX_DRIVE_CONTRACT_INVALID=0,
 QRX_DRIVE_CONTRACT_CREATE=1,
 QRX_DRIVE_CONTRACT_ASSIGN=2,
 QRX_DRIVE_CONTRACT_READY_UPLOAD=3
} QrxDriveContractStepKind;

typedef struct {
 QrxDriveContractStepKind kind;
 char contract_id[129];
 char tx_type[48];
 char to[160];
 char payload[1600];
 uint64_t amount_atoms;
 uint64_t end_height;
 uint64_t base_atoms_per_gib_epoch;
 uint32_t shard_index;
 uint32_t assignments_present;
 uint32_t assignments_required;
} QrxDriveContractStep;

int qrx_drive_contract_id(const char *owner,const QrxDrivePreparedUpload *p,char out[129]);
int qrx_drive_contract_quote(const QrxDrivePreparedUpload *p,uint64_t epochs,uint64_t base_atoms_per_gib_epoch,uint64_t *contract_atoms_out);
int qrx_drive_contract_next_step(QrxDB *db,const char *owner,const QrxDrivePreparedUpload *p,uint64_t current_height,uint64_t epochs,uint64_t base_atoms_per_gib_epoch,QrxDriveContractStep *out);
#ifdef __cplusplus
}
#endif
