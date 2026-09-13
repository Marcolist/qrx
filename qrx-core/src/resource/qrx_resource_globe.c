#include "resource/qrx_resource_globe.h"
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>

static int nonempty(const char *s,size_t n){return s&&memchr(s,'\0',n)&&s[0];}
static uint32_t clamp_bps(uint32_t x){return x>10000u?10000u:x;}
static uint32_t clamp100(uint64_t x){return x>100u?100u:(uint32_t)x;}
static int same_provider_region(const QrxResourceGlobeObservation *a,const QrxResourceGlobeObservation *b){return !strcmp(a->region,b->region)&&!strcmp(a->provider_id,b->provider_id);}

int qrx_resource_globe_observation_validate(const QrxResourceGlobeObservation *o){
    if(!o||o->version!=QRX_RESOURCE_GLOBE_VERSION)return -1;
    if(!nonempty(o->provider_id,sizeof(o->provider_id))||!nonempty(o->region,sizeof(o->region)))return -1;
    if(!o->layer_mask||(o->layer_mask&~QRX_GLOBE_LAYER_ALL))return -1;
    if(o->utilization_bps>10000u||o->reliability_bps>10000u||o->demand_bps>10000u)return -1;
    /* Region is deliberately coarse metadata. Coordinates/addresses have no field in this ABI. */
    return 0;
}

static uint32_t scarcity_score(uint32_t demand,uint32_t util,uint32_t reliability,uint64_t providers){
    uint64_t pneed=providers>=10?0:(10-providers)*1000u;
    uint64_t rel_pen=10000u-clamp_bps(reliability);
    return clamp100(((uint64_t)clamp_bps(demand)*45u+(uint64_t)clamp_bps(util)*25u+pneed*20u+rel_pen*10u)/10000u);
}

QrxGlobeLatencyClass qrx_resource_globe_latency_class(uint32_t ms){if(!ms)return QRX_GLOBE_LATENCY_UNKNOWN;if(ms<=5)return QRX_GLOBE_LATENCY_LOCAL;if(ms<=25)return QRX_GLOBE_LATENCY_LOW;if(ms<=80)return QRX_GLOBE_LATENCY_MEDIUM;return QRX_GLOBE_LATENCY_HIGH;}
QrxGlobeDemandClass qrx_resource_globe_demand_class(uint32_t bps){if(!bps)return QRX_GLOBE_DEMAND_UNKNOWN;if(bps<3000)return QRX_GLOBE_DEMAND_LOW;if(bps<7000)return QRX_GLOBE_DEMAND_BALANCED;if(bps<9000)return QRX_GLOBE_DEMAND_HIGH;return QRX_GLOBE_DEMAND_CRITICAL;}
QrxGlobeOpportunityClass qrx_resource_globe_opportunity_class(uint32_t s){if(!s)return QRX_GLOBE_OPPORTUNITY_NONE;if(s<25)return QRX_GLOBE_OPPORTUNITY_LOW;if(s<50)return QRX_GLOBE_OPPORTUNITY_MEDIUM;if(s<75)return QRX_GLOBE_OPPORTUNITY_HIGH;return QRX_GLOBE_OPPORTUNITY_EXTREME;}
QrxGlobeCapacityClass qrx_resource_globe_capacity_class(const QrxResourceGlobeCell *c){
    if(!c)return QRX_GLOBE_CAPACITY_NONE;
    uint64_t signals=0;
    if(c->storage_free_bytes)signals++;
    if(c->compute_ncu_milli)signals++;
    if(c->ai_milli_tokens_per_second)signals++;
    if(c->model_cache_free_bytes)signals++;
    if(c->network_egress_mbps)signals++;
    if(!signals)return QRX_GLOBE_CAPACITY_NONE;
    if(c->provider_count>=50||signals>=5)return QRX_GLOBE_CAPACITY_EXTREME;
    if(c->provider_count>=20||signals>=4)return QRX_GLOBE_CAPACITY_HIGH;
    if(c->provider_count>=8||signals>=3)return QRX_GLOBE_CAPACITY_MEDIUM;
    return QRX_GLOBE_CAPACITY_LOW;
}

int qrx_resource_globe_build(const QrxResourceGlobeObservation *obs,size_t count,size_t privacy,QrxResourceGlobeCell **out,size_t *outn){
    if(!out||!outn||(count&&!obs))return -1;*out=NULL;*outn=0;if(!privacy)privacy=QRX_GLOBE_DEFAULT_PRIVACY_MIN_PROVIDERS;
    for(size_t i=0;i<count;i++)if(qrx_resource_globe_observation_validate(&obs[i]))return -1;
    QrxResourceGlobeCell *c=calloc(count?count:1,sizeof(*c));if(!c)return -1;size_t cn=0;
    for(size_t i=0;i<count;i++){
        size_t j=0;for(;j<cn;j++)if(!strcmp(c[j].region,obs[i].region))break;
        if(j==cn){c[j].version=QRX_RESOURCE_GLOBE_VERSION;strncpy(c[j].region,obs[i].region,QRX_GLOBE_REGION_MAX);cn++;}
        QrxResourceGlobeCell *x=&c[j];
        int unique=1;for(size_t k=0;k<i;k++)if(same_provider_region(&obs[k],&obs[i])){unique=0;break;}
        if(unique)x->provider_count++;
        x->layer_mask|=obs[i].layer_mask;
        x->storage_free_bytes+=obs[i].storage_free_bytes;
        x->compute_ncu_milli+=obs[i].compute_ncu_milli;
        x->ai_milli_tokens_per_second+=obs[i].ai_milli_tokens_per_second;
        x->model_cache_free_bytes+=obs[i].model_cache_free_bytes;
        x->network_egress_mbps+=obs[i].network_egress_mbps;
        x->avg_latency_ms+=obs[i].latency_ms;
        x->avg_utilization_bps+=obs[i].utilization_bps;
        x->avg_reliability_bps+=obs[i].reliability_bps;
        x->avg_demand_bps+=obs[i].demand_bps;
    }
    for(size_t j=0;j<cn;j++){
        QrxResourceGlobeCell *x=&c[j];uint64_t samples=0;for(size_t i=0;i<count;i++)if(!strcmp(x->region,obs[i].region))samples++;
        if(samples){x->avg_latency_ms/=(uint32_t)samples;x->avg_utilization_bps/=(uint32_t)samples;x->avg_reliability_bps/=(uint32_t)samples;x->avg_demand_bps/=(uint32_t)samples;}
        x->publicly_visible=x->provider_count>=privacy;
        uint32_t base=scarcity_score(x->avg_demand_bps,x->avg_utilization_bps,x->avg_reliability_bps,x->provider_count);
        x->storage_opportunity_score=(x->layer_mask&QRX_GLOBE_LAYER_STORAGE)?base:0;
        x->compute_opportunity_score=(x->layer_mask&QRX_GLOBE_LAYER_COMPUTE)?base:0;
        x->ai_opportunity_score=(x->layer_mask&QRX_GLOBE_LAYER_AI)?base:0;
        x->model_cache_opportunity_score=(x->layer_mask&QRX_GLOBE_LAYER_MODEL_CACHE)?base:0;
        x->network_opportunity_score=(x->layer_mask&QRX_GLOBE_LAYER_NETWORK)?base:0;
        uint64_t sum=0,n=0;uint32_t scores[5]={x->storage_opportunity_score,x->compute_opportunity_score,x->ai_opportunity_score,x->model_cache_opportunity_score,x->network_opportunity_score};
        uint32_t bits[5]={QRX_GLOBE_LAYER_STORAGE,QRX_GLOBE_LAYER_COMPUTE,QRX_GLOBE_LAYER_AI,QRX_GLOBE_LAYER_MODEL_CACHE,QRX_GLOBE_LAYER_NETWORK};
        for(int z=0;z<5;z++)if(x->layer_mask&bits[z]){sum+=scores[z];n++;}x->opportunity_score=n?(uint32_t)(sum/n):0;
        x->layer_mask|=QRX_GLOBE_LAYER_OPPORTUNITY;
        x->capacity_class=qrx_resource_globe_capacity_class(x);x->latency_class=qrx_resource_globe_latency_class(x->avg_latency_ms);x->demand_class=qrx_resource_globe_demand_class(x->avg_demand_bps);x->opportunity_class=qrx_resource_globe_opportunity_class(x->opportunity_score);
    }
    *out=c;*outn=cn;return 0;
}
void qrx_resource_globe_free(QrxResourceGlobeCell *c){free(c);}

static int upd(EVP_MD_CTX*x,const void*p,size_t n){return EVP_DigestUpdate(x,p,n)==1?0:-1;}
int qrx_resource_globe_cell_commitment(const QrxResourceGlobeCell *c,char out[65]){
    if(!c||!out||c->version!=QRX_RESOURCE_GLOBE_VERSION||!nonempty(c->region,sizeof(c->region)))return -1;
    EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/RESOURCE-GLOBE/CELL/V1";
    if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;
#define U(v) do{if(ok&&upd(x,&(v),sizeof(v)))ok=0;}while(0)
    if(ok&&upd(x,dom,sizeof(dom)-1))ok=0;if(ok&&upd(x,c->region,strlen(c->region)))ok=0;
    U(c->layer_mask);U(c->provider_count);U(c->storage_free_bytes);U(c->compute_ncu_milli);U(c->ai_milli_tokens_per_second);U(c->model_cache_free_bytes);U(c->network_egress_mbps);U(c->avg_latency_ms);U(c->avg_utilization_bps);U(c->avg_reliability_bps);U(c->avg_demand_bps);U(c->opportunity_score);U(c->capacity_class);U(c->latency_class);U(c->demand_class);U(c->opportunity_class);U(c->publicly_visible);
#undef U
    if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hex[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hex[h[i]>>4];out[i*2+1]=hex[h[i]&15];}out[64]=0;return 0;
}
