#pragma once
#include <stddef.h>
#include <stdint.h>
#include "compute/qrx_aura_model_distribution.h"
#include "compute/qrx_aura_remote_dispatch.h"
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_AURA_AUTOPLACEMENT_VERSION 1u
#define QRX_AURA_REPAIR_MAX_ITEMS 256u

typedef enum {
    QRX_AURA_REPAIR_UNDER_REPLICATED=1,
    QRX_AURA_REPAIR_REGION_GAP=2,
    QRX_AURA_REPAIR_HOT_EXPERT=3
} QrxAuraRepairReason;

typedef struct {
    uint32_t asset_index;
    uint32_t demand_bps;
    uint64_t activation_count;
    uint32_t observed_latency_ms;
} QrxAuraModelDemandStat;

typedef struct {
    QrxAuraPodCapacity capacity;
    uint8_t asset_bitmap[QRX_AURA_MODEL_AVAILABILITY_BITS];
} QrxAuraPlacementCandidate;

typedef struct {
    uint32_t asset_index;
    QrxAuraRepairReason reason;
    char source_provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    char source_endpoint[QRX_AURA_MODEL_PROVIDER_ENDPOINT_MAX];
    char target_provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    char target_pod_id[QRX_MOE_MAX_POD_ID];
    char target_region[QRX_GLOBE_REGION_MAX+1];
    uint64_t bytes;
    uint32_t priority_score;
} QrxAuraReplicationRepairItem;

typedef struct {
    uint32_t version;
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char manifest_root[65];
    QrxAuraReplicationRepairItem items[QRX_AURA_REPAIR_MAX_ITEMS];
    uint32_t item_count;
    uint64_t transfer_bytes;
} QrxAuraReplicationRepairPlan;

int qrx_aura_model_repair_plan(const QrxAuraModelCatalogAnnouncement *catalog_entry,
                               const QrxAuraModelProviderIndex *index,uint64_t current_height,
                               const QrxAuraPlacementCandidate *candidates,size_t candidate_count,
                               const QrxAuraModelDemandStat *demand,size_t demand_count,
                               QrxAuraReplicationRepairPlan *out);

typedef int (*QrxAuraReplicationTransferFn)(void *ctx,const QrxAuraReplicationRepairItem *item,
                                            const QrxAuraModelAsset *asset);
typedef struct {
    uint32_t attempted;
    uint32_t succeeded;
    uint32_t failed;
    uint64_t bytes_succeeded;
} QrxAuraReplicationExecutionStats;

/* Execute a deterministic repair plan. The transfer adapter must return success
 * only after the target has verified the content-addressed asset. */
int qrx_aura_model_repair_execute(const QrxAuraReplicationRepairPlan *plan,const QrxAuraModelManifest *manifest,
                                  size_t max_items,QrxAuraReplicationTransferFn transfer,void *transfer_ctx,
                                  QrxAuraReplicationExecutionStats *stats_out);

typedef struct {
    QrxAuraDriveModelFetchContext *drive;
    const char *local_provider_id;
    const char *local_pod_id;
} QrxAuraReplicationDriveTargetContext;

/* Target-side adapter: fetch the planned asset from QRX Drive/CAS into this
 * provider's local verified model cache. */
int qrx_aura_model_repair_drive_target_transfer(void *ctx,const QrxAuraReplicationRepairItem *item,
                                                 const QrxAuraModelAsset *asset);
#ifdef __cplusplus
}
#endif
