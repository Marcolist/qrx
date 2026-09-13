#include "resource/qrx_resource_provider.h"
#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>

static int nz(const char*s,size_t n){return s&&memchr(s,'\0',n+1)&&s[0];}
static uint64_t min64(uint64_t a,uint64_t b){return a<b?a:b;}
static uint32_t min32(uint32_t a,uint32_t b){return a<b?a:b;}

int qrx_resource_provider_policy_validate(const QrxResourceProviderPolicy*p,const QrxOpportunityHostProfile*h){
    if(!p||!h||p->version!=QRX_RESOURCE_PROVIDER_VERSION||qrx_compute_opportunity_host_validate(h))return -1;
    if(!nz(p->provider_id,QRX_RESOURCE_PROVIDER_ID_MAX)||!p->enabled_mask||(p->enabled_mask&~QRX_PROVIDER_ENABLE_ALL))return -2;
    if((p->enabled_mask&QRX_PROVIDER_ENABLE_STORAGE)&&(!p->max_storage_bytes||!(h->resource_mask&QRX_OPP_HOST_STORAGE)))return -3;
    if((p->enabled_mask&QRX_PROVIDER_ENABLE_MODEL_CACHE)&&(!p->max_model_cache_bytes||!(h->resource_mask&QRX_OPP_HOST_MODEL_CACHE)))return -4;
    if((p->enabled_mask&QRX_PROVIDER_ENABLE_COMPUTE)&&(!p->max_compute_threads||!(h->resource_mask&QRX_OPP_HOST_COMPUTE)))return -5;
    if((p->enabled_mask&QRX_PROVIDER_ENABLE_NETWORK)&&(!p->max_network_egress_mbps||!(h->resource_mask&QRX_OPP_HOST_NETWORK)))return -6;
    if((p->enabled_mask&QRX_PROVIDER_ENABLE_AI)&&(!p->allow_accelerator||!(h->resource_mask&QRX_OPP_HOST_AI_ACCELERATOR)))return -7;
    if(p->allow_cuda&&(!(p->enabled_mask&QRX_PROVIDER_ENABLE_AI)||!(h->resource_mask&QRX_OPP_HOST_CUDA)))return -8;
    if(p->allow_metal_mlx&&(!(p->enabled_mask&QRX_PROVIDER_ENABLE_AI)||!(h->resource_mask&QRX_OPP_HOST_METAL_MLX)))return -9;
    if(p->max_storage_bytes>h->storage_available_bytes||p->max_model_cache_bytes>h->model_cache_available_bytes||p->max_compute_threads>h->cpu_threads_available||p->max_network_egress_mbps>h->network_egress_mbps)return -10;
    return 0;
}

int qrx_resource_provider_plan(const QrxResourceProviderPolicy*p,const QrxOpportunityHostProfile*h,const QrxComputeOpportunityRecommendation*r,QrxResourceProviderPlan*out){
    if(qrx_resource_provider_policy_validate(p,h)||!r||!out||r->version!=QRX_COMPUTE_OPPORTUNITY_VERSION||!nz(r->region,QRX_GLOBE_REGION_MAX))return -1;
    memset(out,0,sizeof(*out));out->version=QRX_RESOURCE_PROVIDER_VERSION;snprintf(out->region,sizeof(out->region),"%s",r->region);
    out->requires_wallet_approval=p->require_wallet_approval?1:0;out->primary_action=(uint32_t)r->primary_action;out->opportunity_score=r->composite_score;out->expected_utilization_bps=r->expected_utilization_bps;
    if((p->enabled_mask&QRX_PROVIDER_ENABLE_STORAGE)&&r->storage_score){out->storage_bytes=min64(p->max_storage_bytes,r->recommended_storage_bytes);if(out->storage_bytes)out->enabled_mask|=QRX_PROVIDER_ENABLE_STORAGE;}
    if((p->enabled_mask&QRX_PROVIDER_ENABLE_MODEL_CACHE)&&r->model_cache_score){out->model_cache_bytes=min64(p->max_model_cache_bytes,r->recommended_model_cache_bytes);if(out->model_cache_bytes)out->enabled_mask|=QRX_PROVIDER_ENABLE_MODEL_CACHE;}
    if((p->enabled_mask&QRX_PROVIDER_ENABLE_COMPUTE)&&r->compute_score){out->compute_threads=min32(p->max_compute_threads,r->recommended_compute_threads);if(out->compute_threads)out->enabled_mask|=QRX_PROVIDER_ENABLE_COMPUTE;}
    if((p->enabled_mask&QRX_PROVIDER_ENABLE_NETWORK)&&r->network_score){out->network_egress_mbps=min64(p->max_network_egress_mbps,r->recommended_network_egress_mbps);if(out->network_egress_mbps)out->enabled_mask|=QRX_PROVIDER_ENABLE_NETWORK;}
    if((p->enabled_mask&QRX_PROVIDER_ENABLE_AI)&&r->ai_score&&p->allow_accelerator&&r->recommend_ai_accelerator){out->accelerator_enabled=1;out->cuda_enabled=(p->allow_cuda&&r->recommend_cuda)?1:0;out->metal_mlx_enabled=(p->allow_metal_mlx&&r->recommend_metal_mlx)?1:0;out->enabled_mask|=QRX_PROVIDER_ENABLE_AI;}
    if(!out->enabled_mask)return -2;
    return 0;
}

static int fld(EVP_MD_CTX*x,const void*p,size_t n){uint64_t z=(uint64_t)n;return EVP_DigestUpdate(x,&z,sizeof(z))==1&&(!n||EVP_DigestUpdate(x,p,n)==1)?0:-1;}
#define U32(v) do{uint32_t z=(uint32_t)(v);if(ok&&fld(x,&z,sizeof(z)))ok=0;}while(0)
#define U64(v) do{uint64_t z=(uint64_t)(v);if(ok&&fld(x,&z,sizeof(z)))ok=0;}while(0)
int qrx_resource_provider_plan_commitment(const QrxResourceProviderPlan*p,char out[65]){
    if(!p||!out||p->version!=QRX_RESOURCE_PROVIDER_VERSION||!nz(p->region,QRX_GLOBE_REGION_MAX)||!p->enabled_mask)return -1;
    EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=x!=NULL;const char dom[]="QRX/RESOURCE-PROVIDER/PLAN/V1";
    if(ok&&EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&fld(x,dom,sizeof(dom)-1))ok=0;if(ok&&fld(x,p->region,strlen(p->region)))ok=0;
    U32(p->enabled_mask);U64(p->storage_bytes);U64(p->model_cache_bytes);U32(p->compute_threads);U64(p->network_egress_mbps);U32(p->accelerator_enabled);U32(p->cuda_enabled);U32(p->metal_mlx_enabled);U32(p->requires_wallet_approval);U32(p->primary_action);U32(p->opportunity_score);U32(p->expected_utilization_bps);
    if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -2;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;
}
#undef U32
#undef U64
