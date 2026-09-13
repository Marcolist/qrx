#include "resource/qrx_resource_dashboard.h"
#include <string.h>
#include <stdlib.h>

static uint32_t clamp_bps(uint64_t v){ return v>10000?10000:(uint32_t)v; }

int qrx_resource_dashboard_build(const QrxStorageAtlasProvider *providers,size_t provider_count,
                                 const QrxResourceWorkloadStats *w,
                                 const QrxStorageRedundancyProfile *profile,
                                 size_t privacy_min_providers,
                                 QrxResourceDashboardSnapshot *out,
                                 QrxStorageAtlasCell **cells_out,size_t *cell_count_out){
    if(!out||!cells_out||!cell_count_out||!profile||(provider_count&&!providers)) return -1;
    memset(out,0,sizeof(*out)); *cells_out=NULL; *cell_count_out=0;
    QrxStorageProviderCapacity *pc=NULL;
    if(provider_count){ pc=(QrxStorageProviderCapacity*)calloc(provider_count,sizeof(*pc)); if(!pc) return -1; }
    uint64_t avail_sum=0,proof_sum=0,attested=0;
    char asns[256][64]; size_t asn_n=0;
    for(size_t i=0;i<provider_count;i++){
        pc[i]=providers[i].capacity;
        avail_sum+=providers[i].capacity.availability_bps;
        proof_sum+=providers[i].capacity.proof_success_bps;
        if(providers[i].network_attested){
            attested++;
            if(providers[i].asn[0]){ int seen=0; for(size_t j=0;j<asn_n;j++) if(!strcmp(asns[j],providers[i].asn)){seen=1;break;} if(!seen&&asn_n<256){strncpy(asns[asn_n],providers[i].asn,63);asns[asn_n][63]='\0';asn_n++;} }
        }
    }
    const QrxResourceWorkloadStats z={0}; if(!w) w=&z;
    if(qrx_storage_capacity_aggregate(pc,provider_count,w->logical_user_bytes,w->healthy_shards,w->degraded_shards,w->repairing_shards,profile,&out->storage)!=0){free(pc);return -1;}
    free(pc);
    out->avg_availability_bps=provider_count?(uint32_t)(avail_sum/provider_count):0;
    out->avg_proof_success_bps=provider_count?(uint32_t)(proof_sum/provider_count):0;
    out->serving_providers=attested;
    out->independent_asns=asn_n;
    out->provider_diversity_bps=provider_count?clamp_bps((attested*6000/provider_count)+(asn_n*4000/provider_count)):0;
    out->health_score=qrx_storage_network_health_score(&out->storage,out->avg_proof_success_bps,out->provider_diversity_bps);
    out->demand_factor_bps=qrx_storage_dynamic_demand_factor_bps(&out->storage);
    out->active_contracts=w->active_contracts; out->purchased_logical_bytes=w->purchased_logical_bytes;
    out->active_domains=w->active_domains; out->hosted_sites=w->hosted_sites; out->website_logical_bytes=w->website_logical_bytes; out->website_requests_24h=w->website_requests_24h;
    out->website_cache_hit_bps=w->website_requests_24h?clamp_bps(w->website_cache_hits_24h*10000/w->website_requests_24h):0;
    if(qrx_storage_atlas_build(providers,provider_count,privacy_min_providers,cells_out,cell_count_out)!=0) return -1;
    for(size_t i=0;i<*cell_count_out;i++){ if((*cells_out)[i].publicly_visible) out->visible_regions++; else out->hidden_regions++; }
    return 0;
}
