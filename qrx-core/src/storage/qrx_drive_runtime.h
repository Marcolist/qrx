#pragma once
#include "storage/qrx_storage_transport.h"
#include "storage/qrx_drive_prepare.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_DRIVE_RUNTIME_MAX_JOBS 64
#define QRX_DRIVE_RUNTIME_MAX_SOURCES QRX_STORAGE_MAX_FETCH_SOURCES

typedef enum { QRX_DRIVE_JOB_EMPTY=0, QRX_DRIVE_JOB_QUEUED=1, QRX_DRIVE_JOB_RUNNING=2, QRX_DRIVE_JOB_PAUSED=3, QRX_DRIVE_JOB_COMPLETED=4, QRX_DRIVE_JOB_FAILED=5, QRX_DRIVE_JOB_CANCELLED=6 } QrxDriveJobState;
typedef struct {
 char transfer_id[129],contract_id[129],direction[16],path[1024],error[192];
 QrxDriveJobState state; uint64_t total_bytes,completed_bytes; uint32_t completed_shards,required_shards,total_shards;
 uint64_t bytes_received; uint32_t sources_started,sources_completed,sources_failed,hedges_started,resumed_ranges,cancelled_sources;
} QrxDriveJobSnapshot;
typedef struct QrxDriveRuntime QrxDriveRuntime;
int qrx_drive_runtime_open(const char *chain_dir,const char *journal_dir,QrxDriveRuntime **out);
void qrx_drive_runtime_close(QrxDriveRuntime *rt);
/* Internal/trusted boundary: callers must pass sources produced by verified discovery + chain assignments. */
int qrx_drive_runtime_set_contract_sources(QrxDriveRuntime *rt,const char *contract_id,const QrxShardProviderSource *sources,size_t count);
int qrx_drive_runtime_start_download(QrxDriveRuntime *rt,const char *contract_id,const char *destination,uint64_t original_size,size_t shard_size,unsigned data_shards,unsigned parity_shards,char transfer_id[129]);
int qrx_drive_runtime_start_upload(QrxDriveRuntime *rt,const char *contract_id,const char *source_path,unsigned data_shards,unsigned parity_shards,char transfer_id[129]);
/* PRIVATE_PQ authoritative path: uploads the exact preflight shard files only. */
int qrx_drive_runtime_start_prepared_upload(QrxDriveRuntime *rt,const char *contract_id,const QrxDrivePreparedUpload *prepared,char transfer_id[129]);
/* PUBLIC_SIGNED authoritative path: same exact-shard semantics as PRIVATE_PQ,
 * but only accepts qrx-net-public-signed-v1 prepared packages. */
int qrx_drive_runtime_start_public_signed_upload(QrxDriveRuntime *rt,const char *contract_id,const QrxDrivePreparedUpload *prepared,char transfer_id[129]);
int qrx_drive_runtime_pause(QrxDriveRuntime *rt,const char *transfer_id);
int qrx_drive_runtime_resume(QrxDriveRuntime *rt,const char *transfer_id);
int qrx_drive_runtime_cancel(QrxDriveRuntime *rt,const char *transfer_id);
int qrx_drive_runtime_get(QrxDriveRuntime *rt,const char *transfer_id,QrxDriveJobSnapshot *out);
size_t qrx_drive_runtime_list(QrxDriveRuntime *rt,QrxDriveJobSnapshot *out,size_t cap);
#ifdef __cplusplus
}
#endif
