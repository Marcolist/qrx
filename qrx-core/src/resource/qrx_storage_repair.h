#pragma once
#include "resource/qrx_storage_placement.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { QRX_ASSIGN_PENDING=1, QRX_ASSIGN_ACTIVE=2, QRX_ASSIGN_DEGRADED=3, QRX_ASSIGN_REPAIRING=4, QRX_ASSIGN_REPLACED=5 } QrxStorageAssignmentState;
typedef struct {char contract_id[129];char provider_id[129];uint32_t shard_index;uint64_t physical_bytes;QrxStorageAssignmentState state;uint64_t last_proof_epoch;} QrxStorageAssignment;
typedef struct {size_t replacement_candidate_index;uint32_t shard_index;uint64_t physical_bytes;} QrxStorageRepairPlan;
int qrx_storage_mark_degraded(QrxStorageAssignment *a,uint64_t missed_epoch);
int qrx_storage_plan_repair(const QrxStorageAssignment *all,size_t assignment_count,size_t failed_index,const QrxStoragePlacementCandidate *candidates,size_t candidate_count,uint64_t min_bond_atoms,const uint8_t randomness[64],QrxStorageRepairPlan *out);
int qrx_storage_begin_repair(QrxStorageAssignment *failed,QrxStorageAssignment *replacement,const QrxStoragePlacementCandidate *candidate,const QrxStorageRepairPlan *plan);
int qrx_storage_complete_repair(QrxStorageAssignment *failed,QrxStorageAssignment *replacement,int replacement_postor_verified);
#ifdef __cplusplus
}
#endif
