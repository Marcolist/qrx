#include "compute/qrx_aura_model_autoplacement.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static uint32_t demand_for(const QrxAuraModelDemandStat*d,size_t n,uint32_t asset){for(size_t i=0;i<n;i++)if(d[i].asset_index==asset)return d[i].demand_bps<=10000?d[i].demand_bps:10000;return 0;}
static int bitmap_has(const uint8_t*b,uint32_t i){return b?((b[i>>3]>>(i&7u))&1u):0;}
static int provider_has(const QrxAuraModelProviderIndex*idx,const QrxAuraModelManifest*m,const char*root,uint64_t h,const char*provider,const char*pod,uint32_t asset){for(uint32_t i=0;i<idx->count;i++){const QrxAuraModelAvailabilityAnnouncement*a=&idx->entries[i].announcement;if(a->valid_from_height>h||a->valid_until_height<h)continue;if(strcmp(a->provider_id,provider)||strcmp(a->pod_id,pod)||strcmp(a->model_id,m->model_id)||strcmp(a->model_version,m->model_version)||strcmp(a->manifest_root,root))continue;if(qrx_aura_model_availability_has_asset(a,asset))return 1;}return 0;}
static int region_has_asset(const QrxAuraModelProviderIndex*idx,const QrxAuraModelManifest*m,const char*root,uint64_t h,const char*region,uint32_t asset){for(uint32_t i=0;i<idx->count;i++){const QrxAuraModelAvailabilityAnnouncement*a=&idx->entries[i].announcement;if(a->valid_from_height<=h&&a->valid_until_height>=h&&!strcmp(a->region,region)&&!strcmp(a->model_id,m->model_id)&&!strcmp(a->model_version,m->model_version)&&!strcmp(a->manifest_root,root)&&qrx_aura_model_availability_has_asset(a,asset))return 1;}return 0;}
static uint64_t candidate_score(const QrxAuraPlacementCandidate*c,const QrxAuraModelAsset*a,int new_region,uint32_t demand){const QrxAuraPodCapacity*p=&c->capacity;if(qrx_aura_pod_capacity_validate(p)||!p->available||!(p->role_mask&(QRX_AURA_POD_ROLE_MODEL_CACHE|QRX_AURA_POD_ROLE_INFERENCE))||p->model_cache_free_bytes<a->bytes)return 0;uint64_t s=(uint64_t)p->reliability_bps*1000u+(uint64_t)p->network_egress_mbps*100u+(uint64_t)(10000u-p->utilization_bps)*10u+demand;if(new_region)s+=10000000u;if(p->latency_ms<10000u)s+=10000u-p->latency_ms;return s;}
static const QrxAuraModelAvailabilityAnnouncement*best_source(const QrxAuraModelProviderIndex*idx,const QrxAuraModelManifest*m,const char*root,uint64_t h,uint32_t asset){const QrxAuraModelAvailabilityAnnouncement*best=NULL;uint64_t bs=0;for(uint32_t i=0;i<idx->count;i++){const QrxAuraModelAvailabilityAnnouncement*a=&idx->entries[i].announcement;if(a->valid_from_height>h||a->valid_until_height<h||strcmp(a->model_id,m->model_id)||strcmp(a->model_version,m->model_version)||strcmp(a->manifest_root,root)||!qrx_aura_model_availability_has_asset(a,asset))continue;uint64_t s=(uint64_t)a->reliability_bps*1000u+(uint64_t)a->bandwidth_mbps*100u+(a->latency_ms<10000u?10000u-a->latency_ms:0u);if(!best||s>bs||(s==bs&&strcmp(a->provider_id,best->provider_id)<0)){best=a;bs=s;}}return best;}
int qrx_aura_model_repair_plan(const QrxAuraModelCatalogAnnouncement*c,const QrxAuraModelProviderIndex*idx,uint64_t h,const QrxAuraPlacementCandidate*cand,size_t cn,const QrxAuraModelDemandStat*d,size_t dn,QrxAuraReplicationRepairPlan*out){if(!c||!idx||!out||(!cand&&cn)||(!d&&dn)||qrx_aura_model_manifest_validate(&c->manifest))return-1;char root[65];if(qrx_aura_model_manifest_commitment(&c->manifest,root)||strcmp(root,c->manifest_root))return-1;memset(out,0,sizeof(*out));out->version=1;snprintf(out->model_id,sizeof(out->model_id),"%s",c->manifest.model_id);snprintf(out->model_version,sizeof(out->model_version),"%s",c->manifest.model_version);snprintf(out->manifest_root,65,"%s",root);
    for(uint32_t ai=0;ai<c->manifest.asset_count&&out->item_count<QRX_AURA_REPAIR_MAX_ITEMS;ai++){
        const QrxAuraModelAsset*a=&c->manifest.assets[ai];uint32_t dem=demand_for(d,dn,ai);QrxAuraModelAssetHealth health;if(qrx_aura_model_asset_health(&c->manifest,idx,h,ai,dem,c,&health))continue;int hot=a->kind==QRX_AURA_ASSET_EXPERT_PACK&&dem>=7000u;int need=health.deficit>0||health.region_replicas<QRX_AURA_MODEL_MIN_REGIONS||hot;if(!need)continue;const QrxAuraModelAvailabilityAnnouncement*src=best_source(idx,&c->manifest,root,h,ai);if(!src)continue;const QrxAuraPlacementCandidate*best=NULL;uint64_t bestscore=0;int best_new_region=0;for(size_t x=0;x<cn;x++){const QrxAuraPlacementCandidate*pc=&cand[x];if(bitmap_has(pc->asset_bitmap,ai)||provider_has(idx,&c->manifest,root,h,pc->capacity.provider_id,pc->capacity.pod_id,ai))continue;int nr=!region_has_asset(idx,&c->manifest,root,h,pc->capacity.region,ai);uint64_t sc=candidate_score(pc,a,nr,dem);if(!sc)continue;if(!best||sc>bestscore||(sc==bestscore&&strcmp(pc->capacity.provider_id,best->capacity.provider_id)<0)){best=pc;bestscore=sc;best_new_region=nr;}}if(!best)continue;QrxAuraReplicationRepairItem*it=&out->items[out->item_count++];it->asset_index=ai;if(hot)it->reason=QRX_AURA_REPAIR_HOT_EXPERT;else if(health.region_replicas<QRX_AURA_MODEL_MIN_REGIONS&&best_new_region)it->reason=QRX_AURA_REPAIR_REGION_GAP;else it->reason=QRX_AURA_REPAIR_UNDER_REPLICATED;snprintf(it->source_provider_id,sizeof(it->source_provider_id),"%s",src->provider_id);snprintf(it->source_endpoint,sizeof(it->source_endpoint),"%s",src->endpoint);snprintf(it->target_provider_id,sizeof(it->target_provider_id),"%s",best->capacity.provider_id);snprintf(it->target_pod_id,sizeof(it->target_pod_id),"%s",best->capacity.pod_id);snprintf(it->target_region,sizeof(it->target_region),"%s",best->capacity.region);it->bytes=a->bytes;uint64_t pr=(uint64_t)health.priority_score+(uint64_t)dem*10u+(best_new_region?50000u:0u)+(hot?100000u:0u);it->priority_score=pr>UINT32_MAX?UINT32_MAX:(uint32_t)pr;if(UINT64_MAX-out->transfer_bytes<it->bytes)out->transfer_bytes=UINT64_MAX;else out->transfer_bytes+=it->bytes;
    }
    /* Stable highest-priority first. */for(uint32_t i=1;i<out->item_count;i++){QrxAuraReplicationRepairItem v=out->items[i];uint32_t j=i;while(j>0&&(out->items[j-1].priority_score<v.priority_score||(out->items[j-1].priority_score==v.priority_score&&out->items[j-1].asset_index>v.asset_index))){out->items[j]=out->items[j-1];j--;}out->items[j]=v;}return 0;}


int qrx_aura_model_repair_execute(const QrxAuraReplicationRepairPlan *plan,const QrxAuraModelManifest *manifest,
                                  size_t max_items,QrxAuraReplicationTransferFn transfer,void *transfer_ctx,
                                  QrxAuraReplicationExecutionStats *stats_out){
    if(!plan||!manifest||!transfer||plan->version!=QRX_AURA_AUTOPLACEMENT_VERSION||qrx_aura_model_manifest_validate(manifest))return -1;
    char root[65];
    if(qrx_aura_model_manifest_commitment(manifest,root)||strcmp(root,plan->manifest_root)||strcmp(manifest->model_id,plan->model_id)||strcmp(manifest->model_version,plan->model_version))return -1;
    QrxAuraReplicationExecutionStats st={0};
    size_t limit=plan->item_count;
    if(max_items&&limit>max_items)limit=max_items;
    for(size_t i=0;i<limit;i++){
        const QrxAuraReplicationRepairItem *it=&plan->items[i];
        if(it->asset_index>=manifest->asset_count){st.failed++;st.attempted++;continue;}
        const QrxAuraModelAsset *a=&manifest->assets[it->asset_index];
        if(a->index!=it->asset_index||a->bytes!=it->bytes){st.failed++;st.attempted++;continue;}
        st.attempted++;
        if(!transfer(transfer_ctx,it,a)){
            st.succeeded++;
            if(UINT64_MAX-st.bytes_succeeded<a->bytes)st.bytes_succeeded=UINT64_MAX;else st.bytes_succeeded+=a->bytes;
        }else st.failed++;
    }
    if(stats_out)*stats_out=st;
    return st.failed?1:0;
}

int qrx_aura_model_repair_drive_target_transfer(void *v,const QrxAuraReplicationRepairItem *item,const QrxAuraModelAsset *asset){
    QrxAuraReplicationDriveTargetContext *c=(QrxAuraReplicationDriveTargetContext*)v;
    if(!c||!c->drive||!c->local_provider_id||!c->local_pod_id||!item||!asset)return -1;
    if(strcmp(item->target_provider_id,c->local_provider_id)||strcmp(item->target_pod_id,c->local_pod_id))return -2;
    return qrx_aura_drive_asset_fetch(c->drive,asset->content_root,asset->bytes);
}
