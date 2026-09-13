#include "resource/qrx_resource_provider.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 QrxOpportunityHostProfile h={0};h.version=QRX_COMPUTE_OPPORTUNITY_VERSION;h.resource_mask=QRX_OPP_HOST_STORAGE|QRX_OPP_HOST_COMPUTE|QRX_OPP_HOST_AI_ACCELERATOR|QRX_OPP_HOST_MODEL_CACHE|QRX_OPP_HOST_NETWORK|QRX_OPP_HOST_CUDA;h.cpu_threads_total=16;h.cpu_threads_available=12;h.storage_available_bytes=3ULL<<40;h.model_cache_available_bytes=600ULL<<30;h.accelerator_memory_bytes=24ULL<<30;h.network_egress_mbps=1000;strcpy(h.accelerator_name,"Tesla P40");
 QrxComputeOpportunityRecommendation r={0};r.version=QRX_COMPUTE_OPPORTUNITY_VERSION;strcpy(r.region,"EU-WEST-COARSE");r.storage_score=82;r.compute_score=87;r.ai_score=95;r.model_cache_score=95;r.network_score=70;r.composite_score=90;r.expected_utilization_bps=8800;r.recommended_storage_bytes=2ULL<<40;r.recommended_model_cache_bytes=500ULL<<30;r.recommended_compute_threads=8;r.recommended_network_egress_mbps=500;r.recommend_ai_accelerator=1;r.recommend_cuda=1;r.primary_action=QRX_OPP_ACTION_AI_ACCELERATOR;
 QrxResourceProviderPolicy p={0};p.version=QRX_RESOURCE_PROVIDER_VERSION;strcpy(p.provider_id,"wallet:qrx:test-provider");p.enabled_mask=QRX_PROVIDER_ENABLE_ALL;p.max_storage_bytes=1ULL<<40;p.max_model_cache_bytes=300ULL<<30;p.max_compute_threads=6;p.max_network_egress_mbps=250;p.allow_accelerator=1;p.allow_cuda=1;p.require_wallet_approval=1;
 assert(qrx_resource_provider_policy_validate(&p,&h)==0);QrxResourceProviderPlan plan;assert(qrx_resource_provider_plan(&p,&h,&r,&plan)==0);assert(plan.enabled_mask==QRX_PROVIDER_ENABLE_ALL);assert(plan.storage_bytes==(1ULL<<40));assert(plan.model_cache_bytes==(300ULL<<30));assert(plan.compute_threads==6);assert(plan.network_egress_mbps==250);assert(plan.accelerator_enabled&&plan.cuda_enabled&&!plan.metal_mlx_enabled);assert(plan.requires_wallet_approval);char a[65],b[65];assert(qrx_resource_provider_plan_commitment(&plan,a)==0&&qrx_resource_provider_plan_commitment(&plan,b)==0&&!strcmp(a,b));
 p.max_compute_threads=13;assert(qrx_resource_provider_policy_validate(&p,&h)==-10);p.max_compute_threads=6;p.allow_cuda=0;assert(qrx_resource_provider_plan(&p,&h,&r,&plan)==0&&!plan.cuda_enabled);
 puts("compute_phase149_resource_provider_ux: ok");return 0;
}
