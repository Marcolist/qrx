#include "resource/qrx_compute_opportunity.h"
#include <string.h>
#include <openssl/evp.h>

#define GIB ((uint64_t)1024u*1024u*1024u)

static uint64_t min64(uint64_t a,uint64_t b){return a<b?a:b;}
static uint32_t min32(uint32_t a,uint32_t b){return a<b?a:b;}
static int bounded_string(const char*s,size_t n){return s&&memchr(s,'\0',n)!=NULL;}

int qrx_compute_opportunity_host_validate(const QrxOpportunityHostProfile *h){
    if(!h||h->version!=QRX_COMPUTE_OPPORTUNITY_VERSION)return -1;
    if(h->resource_mask&~QRX_OPP_HOST_ALL)return -1;
    if(h->cpu_threads_available>h->cpu_threads_total)return -1;
    if(!bounded_string(h->accelerator_name,sizeof(h->accelerator_name)))return -1;
    if((h->resource_mask&QRX_OPP_HOST_CUDA)&&!(h->resource_mask&QRX_OPP_HOST_AI_ACCELERATOR))return -1;
    if((h->resource_mask&QRX_OPP_HOST_METAL_MLX)&&!(h->resource_mask&QRX_OPP_HOST_AI_ACCELERATOR))return -1;
    return 0;
}

static uint64_t storage_target(uint32_t s){
    if(s<25)return 0;if(s<50)return 256u*GIB;if(s<75)return 1024u*GIB;if(s<90)return 2u*1024u*GIB;return 4u*1024u*GIB;
}
static uint64_t cache_target(uint32_t s){
    if(s<25)return 0;if(s<50)return 64u*GIB;if(s<75)return 256u*GIB;if(s<90)return 500u*GIB;return 1024u*GIB;
}
static uint32_t thread_target(uint32_t s){
    if(s<25)return 0;if(s<50)return 2;if(s<75)return 4;if(s<90)return 8;return 12;
}
static uint64_t network_target(uint32_t s){
    if(s<25)return 0;if(s<50)return 100;if(s<75)return 500;if(s<90)return 1000;return 2500;
}
static uint32_t utilization_estimate(const QrxResourceGlobeCell*c){
    /* Demand dominates; current utilization and scarcity raise the planning estimate. */
    uint64_t v=(uint64_t)c->avg_demand_bps*60u+(uint64_t)c->avg_utilization_bps*25u+(uint64_t)c->opportunity_score*100u*15u;
    v/=100u;
    return (uint32_t)(v>10000u?10000u:v);
}

static QrxOpportunityAction primary(const QrxComputeOpportunityRecommendation*r,const QrxOpportunityHostProfile*h){
    uint32_t best=0;QrxOpportunityAction a=QRX_OPP_ACTION_NONE;
#define PICK(enabled,score,act) do{if((enabled)&&(score)>best){best=(score);a=(act);}}while(0)
    PICK((h->resource_mask&QRX_OPP_HOST_STORAGE)&&r->recommended_storage_bytes,r->storage_score,QRX_OPP_ACTION_STORAGE);
    PICK((h->resource_mask&QRX_OPP_HOST_COMPUTE)&&r->recommended_compute_threads,r->compute_score,QRX_OPP_ACTION_COMPUTE);
    PICK((h->resource_mask&QRX_OPP_HOST_AI_ACCELERATOR)&&r->recommend_ai_accelerator,r->ai_score,QRX_OPP_ACTION_AI_ACCELERATOR);
    PICK((h->resource_mask&QRX_OPP_HOST_MODEL_CACHE)&&r->recommended_model_cache_bytes,r->model_cache_score,QRX_OPP_ACTION_MODEL_CACHE);
    PICK((h->resource_mask&QRX_OPP_HOST_NETWORK)&&r->recommended_network_egress_mbps,r->network_score,QRX_OPP_ACTION_NETWORK);
#undef PICK
    return a;
}

int qrx_compute_opportunity_recommend(const QrxResourceGlobeCell*c,const QrxOpportunityHostProfile*h,QrxComputeOpportunityRecommendation*out){
    if(!c||!h||!out||c->version!=QRX_RESOURCE_GLOBE_VERSION||qrx_compute_opportunity_host_validate(h))return -1;
    if(!c->publicly_visible||!c->region[0])return -2; /* do not expose/recommend from privacy-sparse public cells */
    memset(out,0,sizeof(*out));out->version=QRX_COMPUTE_OPPORTUNITY_VERSION;
    strncpy(out->region,c->region,QRX_GLOBE_REGION_MAX);
    out->storage_score=c->storage_opportunity_score;
    out->compute_score=c->compute_opportunity_score;
    out->ai_score=c->ai_opportunity_score;
    out->model_cache_score=c->model_cache_opportunity_score;
    out->network_score=c->network_opportunity_score;
    out->composite_score=c->opportunity_score;
    out->expected_utilization_bps=utilization_estimate(c);
    if(h->resource_mask&QRX_OPP_HOST_STORAGE)out->recommended_storage_bytes=min64(h->storage_available_bytes,storage_target(out->storage_score));
    if(h->resource_mask&QRX_OPP_HOST_MODEL_CACHE)out->recommended_model_cache_bytes=min64(h->model_cache_available_bytes,cache_target(out->model_cache_score));
    if(h->resource_mask&QRX_OPP_HOST_COMPUTE)out->recommended_compute_threads=min32(h->cpu_threads_available,thread_target(out->compute_score));
    if(h->resource_mask&QRX_OPP_HOST_NETWORK)out->recommended_network_egress_mbps=min64(h->network_egress_mbps,network_target(out->network_score));
    if((h->resource_mask&QRX_OPP_HOST_AI_ACCELERATOR)&&out->ai_score>=50u&&h->accelerator_memory_bytes){
        out->recommend_ai_accelerator=1;
        out->recommend_cuda=(h->resource_mask&QRX_OPP_HOST_CUDA)!=0;
        out->recommend_metal_mlx=(h->resource_mask&QRX_OPP_HOST_METAL_MLX)!=0;
    }
    out->primary_action=primary(out,h);return 0;
}

static int upd(EVP_MD_CTX*x,const void*p,size_t n){return EVP_DigestUpdate(x,p,n)==1?0:-1;}
int qrx_compute_opportunity_commitment(const QrxComputeOpportunityRecommendation*r,char out[65]){
    if(!r||!out||r->version!=QRX_COMPUTE_OPPORTUNITY_VERSION||!r->region[0])return -1;
    EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/OPPORTUNITY/RECOMMENDATION/V1";
    if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;
#define U(v) do{if(ok&&upd(x,&(v),sizeof(v)))ok=0;}while(0)
    if(ok&&upd(x,dom,sizeof(dom)-1))ok=0;if(ok&&upd(x,r->region,strlen(r->region)))ok=0;
    U(r->storage_score);U(r->compute_score);U(r->ai_score);U(r->model_cache_score);U(r->network_score);U(r->composite_score);U(r->expected_utilization_bps);U(r->recommended_storage_bytes);U(r->recommended_model_cache_bytes);U(r->recommended_compute_threads);U(r->recommended_network_egress_mbps);U(r->recommend_ai_accelerator);U(r->recommend_cuda);U(r->recommend_metal_mlx);U(r->primary_action);
#undef U
    if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;
}
