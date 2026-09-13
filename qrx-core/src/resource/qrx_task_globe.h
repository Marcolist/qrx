#pragma once
#include <stddef.h>
#include <stdint.h>
#include "resource/qrx_resource_globe.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_TASK_GLOBE_VERSION 1u
#define QRX_TASK_GLOBE_TASK_ID_MAX 95u
#define QRX_TASK_GLOBE_POD_ID_MAX 95u
#define QRX_TASK_GLOBE_EXPERT_GROUP_MAX 95u
#define QRX_TASK_GLOBE_PROVIDER_ID_MAX 127u
#define QRX_TASK_GLOBE_MAX_FRAMES 1024u

#define QRX_TASK_GLOBE_FLAG_FASTTRACK (1u<<0)
#define QRX_TASK_GLOBE_FLAG_PRIVATE   (1u<<1)
#define QRX_TASK_GLOBE_FLAG_ALL       ((1u<<2)-1u)

typedef enum {
    QRX_TASK_GLOBE_STAGE_SUBMITTED = 0,
    QRX_TASK_GLOBE_STAGE_SCHEDULED,
    QRX_TASK_GLOBE_STAGE_COMPUTE,
    QRX_TASK_GLOBE_STAGE_EXPERT,
    QRX_TASK_GLOBE_STAGE_RESULT,
    QRX_TASK_GLOBE_STAGE_FAILED
} QrxTaskGlobeStage;

typedef struct {
    uint32_t version;
    char task_id[QRX_TASK_GLOBE_TASK_ID_MAX+1];
    uint64_t sequence;
    uint64_t timestamp_ms;
    QrxTaskGlobeStage stage;
    uint32_t flags;
    char region[QRX_GLOBE_REGION_MAX+1];       /* coarse region only */
    char pod_id[QRX_TASK_GLOBE_POD_ID_MAX+1]; /* internal aggregation key */
    char provider_id[QRX_TASK_GLOBE_PROVIDER_ID_MAX+1]; /* internal only */
    char expert_group[QRX_TASK_GLOBE_EXPERT_GROUP_MAX+1];
    uint32_t expert_count;
} QrxTaskGlobeEvent;

typedef struct {
    uint32_t version;
    char task_id[QRX_TASK_GLOBE_TASK_ID_MAX+1];
    uint64_t sequence;
    uint64_t timestamp_ms;
    QrxTaskGlobeStage stage;
    uint32_t flags;
    uint32_t compute_provider_count;
    uint32_t region_count;
    uint32_t pod_count;
    uint32_t expert_group_count;
    uint32_t expert_count;
    uint32_t progress_bps;
    int fasttrack_on;
    int terminal;
    int success;
    /* Deliberately no provider IDs, node IDs, IPs, hostnames or coordinates. */
} QrxTaskGlobeFrame;

int qrx_task_globe_event_validate(const QrxTaskGlobeEvent *event);
int qrx_task_globe_build_frames(const QrxTaskGlobeEvent *events, size_t count,
                                size_t privacy_min_providers,
                                QrxTaskGlobeFrame **frames_out, size_t *frame_count_out);
void qrx_task_globe_free_frames(QrxTaskGlobeFrame *frames);
int qrx_task_globe_frame_commitment(const QrxTaskGlobeFrame *frame, char out_hex[65]);

#ifdef __cplusplus
}
#endif
