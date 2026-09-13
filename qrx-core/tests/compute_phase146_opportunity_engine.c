#include "resource/qrx_compute_opportunity.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
    QrxResourceGlobeCell c;memset(&c,0,sizeof(c));c.version=QRX_RESOURCE_GLOBE_VERSION;strcpy(c.region,"EU-WEST-COARSE");c.publicly_visible=1;
    c.storage_opportunity_score=82;c.compute_opportunity_score=87;c.ai_opportunity_score=95;c.model_cache_opportunity_score=95;c.network_opportunity_score=70;c.opportunity_score=86;c.avg_demand_bps=9300;c.avg_utilization_bps=8200;
    QrxOpportunityHostProfile h;memset(&h,0,sizeof(h));h.version=QRX_COMPUTE_OPPORTUNITY_VERSION;h.resource_mask=QRX_OPP_HOST_STORAGE|QRX_OPP_HOST_COMPUTE|QRX_OPP_HOST_AI_ACCELERATOR|QRX_OPP_HOST_MODEL_CACHE|QRX_OPP_HOST_NETWORK|QRX_OPP_HOST_CUDA;h.cpu_threads_total=16;h.cpu_threads_available=12;h.storage_available_bytes=3ULL*1024*1024*1024*1024;h.model_cache_available_bytes=600ULL*1024*1024*1024;h.accelerator_memory_bytes=24ULL*1024*1024*1024;h.network_egress_mbps=1000;strcpy(h.accelerator_name,"Tesla P40");
    assert(qrx_compute_opportunity_host_validate(&h)==0);
    QrxComputeOpportunityRecommendation r;assert(qrx_compute_opportunity_recommend(&c,&h,&r)==0);
    assert(r.storage_score==82&&r.compute_score==87&&r.ai_score==95&&r.model_cache_score==95);
    assert(r.recommended_storage_bytes==2ULL*1024*1024*1024*1024);
    assert(r.recommended_model_cache_bytes==600ULL*1024*1024*1024); /* host clamp below 1 TiB target */
    assert(r.recommended_compute_threads==8);
    assert(r.recommended_network_egress_mbps==500);
    assert(r.recommend_ai_accelerator==1&&r.recommend_cuda==1&&r.recommend_metal_mlx==0);
    assert(r.primary_action==QRX_OPP_ACTION_AI_ACCELERATOR||r.primary_action==QRX_OPP_ACTION_MODEL_CACHE);
    assert(r.expected_utilization_bps>8000&&r.expected_utilization_bps<=10000);
    char a[65],b[65];assert(qrx_compute_opportunity_commitment(&r,a)==0);assert(qrx_compute_opportunity_commitment(&r,b)==0);assert(strcmp(a,b)==0);
    c.publicly_visible=0;assert(qrx_compute_opportunity_recommend(&c,&h,&r)==-2);
    h.resource_mask=QRX_OPP_HOST_CUDA;assert(qrx_compute_opportunity_host_validate(&h)==-1);
    puts("compute_phase146_opportunity_engine: ok");return 0;
}
