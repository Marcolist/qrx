#pragma once
#include <stdint.h>
#include "compute/qrx_compute.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_OS_COMPUTE_VERSION 1u
#define QRX_OS_COMPUTE_REF_MAX 129u
#define QRX_OS_COMPUTE_ID_MAX 129u

#define QRX_OS_SERVICE_WALLET  (1u<<0)
#define QRX_OS_SERVICE_DRIVE   (1u<<1)
#define QRX_OS_SERVICE_COMPUTE (1u<<2)
#define QRX_OS_SERVICE_CHAIN   (1u<<3)
#define QRX_OS_SERVICE_ALL (QRX_OS_SERVICE_WALLET|QRX_OS_SERVICE_DRIVE|QRX_OS_SERVICE_COMPUTE|QRX_OS_SERVICE_CHAIN)

typedef enum {
    QRX_OS_APP_CHAT=1,
    QRX_OS_APP_CODING=2,
    QRX_OS_APP_DOCUMENTS=3,
    QRX_OS_APP_RESEARCH=4,
    QRX_OS_APP_RAG=5,
    QRX_OS_APP_BUILD_TASKS=6,
    QRX_OS_APP_AI_AGENTS=7,
    QRX_OS_APP_BATCH_COMPUTE=8
} QrxOsComputeApp;

typedef enum {
    QRX_OS_RUN_DRAFT=1,
    QRX_OS_RUN_READY=2,
    QRX_OS_RUN_SUBMITTED=3,
    QRX_OS_RUN_RUNNING=4,
    QRX_OS_RUN_COMPLETED=5,
    QRX_OS_RUN_FAILED=6,
    QRX_OS_RUN_CANCELLED=7
} QrxOsComputeRunState;

typedef struct {
    uint32_t version;
    QrxOsComputeApp app;
    char workspace_id[QRX_OS_COMPUTE_ID_MAX];
    char wallet_identity[QRX_AURA_MAX_OWNER];
    char drive_namespace_ref[QRX_OS_COMPUTE_REF_MAX];
    char payment_escrow_ref[QRX_OS_COMPUTE_REF_MAX];
    char settlement_ref[QRX_OS_COMPUTE_REF_MAX];
    uint32_t service_mask;
    uint64_t max_total_fee_atoms;
    uint64_t expiry_height;
    uint8_t private_pq;
} QrxOsComputeWorkspace;

typedef struct {
    uint32_t version;
    char run_id[QRX_OS_COMPUTE_ID_MAX];
    char workspace_commitment[65];
    char graph_commitment[65];
    char output_artifact_commitment[65];
    QrxOsComputeRunState state;
    uint64_t charged_fee_atoms;
    uint64_t submitted_height;
    uint64_t settled_height;
} QrxOsComputeRun;

uint32_t qrx_os_compute_required_services(QrxOsComputeApp app);
int qrx_os_compute_workspace_validate(const QrxOsComputeWorkspace *workspace);
int qrx_os_compute_workspace_commitment(const QrxOsComputeWorkspace *workspace, char out_hex[65]);
int qrx_os_compute_bind_graph(const QrxOsComputeWorkspace *workspace, const QrxComputeJobGraph *graph, QrxOsComputeRun *run);
int qrx_os_compute_submit(QrxOsComputeRun *run, uint64_t height);
int qrx_os_compute_mark_running(QrxOsComputeRun *run);
int qrx_os_compute_complete(const QrxOsComputeWorkspace *workspace, const QrxComputeJobGraph *graph, QrxOsComputeRun *run, const QrxAuraArtifact *artifact, uint64_t charged_fee_atoms, uint64_t settled_height);
int qrx_os_compute_fail(QrxOsComputeRun *run);
int qrx_os_compute_cancel(QrxOsComputeRun *run);
int qrx_os_compute_run_commitment(const QrxOsComputeRun *run, char out_hex[65]);

#ifdef __cplusplus
}
#endif
