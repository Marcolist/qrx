#include "resource/qrx_resource_provider_runtime.h"
#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>

static int nz(const char *s,size_t n){return s&&memchr(s,'\0',n+1)&&s[0];}
static int hex64(const char*s){if(!s||strlen(s)!=64)return 0;for(size_t i=0;i<64;i++){char c=s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;}return 1;}
static int plan_valid(const QrxResourceProviderPlan*p){return p&&p->version==QRX_RESOURCE_PROVIDER_VERSION&&nz(p->region,QRX_GLOBE_REGION_MAX)&&p->enabled_mask&&(p->enabled_mask&~QRX_PROVIDER_ENABLE_ALL)==0;}
static int plan_fits_reserved(const QrxResourceProviderRuntime*r,const QrxResourceProviderPlan*p){
    if(!r||!p)return 0;
    if(r->reserved_storage_bytes>p->storage_bytes)return 0;
    if(r->reserved_model_cache_bytes>p->model_cache_bytes)return 0;
    if(r->reserved_compute_threads>p->compute_threads)return 0;
    if(r->reserved_network_egress_mbps>p->network_egress_mbps)return 0;
    if(r->active_jobs&&(r->active_plan.enabled_mask&~p->enabled_mask))return 0;
    return 1;
}
static int copy_plan(QrxResourceProviderRuntime*r,const QrxResourceProviderPlan*p,char out[65]){
    char c[65];if(qrx_resource_provider_plan_commitment(p,c))return -1;r->active_plan=*p;memcpy(r->active_plan_commitment,c,65);if(out)memcpy(out,c,65);return 0;
}

int qrx_resource_provider_wallet_approval_validate(const QrxResourceProviderWalletApproval*a,const char*provider_id,const char*network,const char plan_commitment[65],uint64_t h){
    if(!a||a->version!=QRX_PROVIDER_APPROVAL_VERSION||!a->approved||!nz(a->approval_id,QRX_PROVIDER_APPROVAL_ID_MAX)||!nz(a->provider_id,QRX_RESOURCE_PROVIDER_ID_MAX)||!nz(a->network,QRX_PROVIDER_NETWORK_MAX)||!hex64(a->plan_commitment)||!provider_id||!network||!plan_commitment)return -1;
    if(strcmp(a->provider_id,provider_id)||strcmp(a->network,network)||strcmp(a->plan_commitment,plan_commitment))return -2;
    if(a->approved_at_height>h)return -3;
    if(a->expires_at_height&&h>a->expires_at_height)return -4;
    return 0;
}

int qrx_resource_provider_runtime_init(QrxResourceProviderRuntime*r,const char*provider_id,const char*network,const QrxResourceProviderPlan*p){
    if(!r||!nz(provider_id,QRX_RESOURCE_PROVIDER_ID_MAX)||!nz(network,QRX_PROVIDER_NETWORK_MAX)||!plan_valid(p))return -1;
    memset(r,0,sizeof(*r));r->version=QRX_PROVIDER_RUNTIME_VERSION;r->status=QRX_PROVIDER_RUNTIME_CONFIGURED;snprintf(r->provider_id,sizeof(r->provider_id),"%s",provider_id);snprintf(r->network,sizeof(r->network),"%s",network);if(copy_plan(r,p,NULL)){memset(r,0,sizeof(*r));return -2;}return 0;
}
int qrx_resource_provider_runtime_mark_ready(QrxResourceProviderRuntime*r){if(!r||r->version!=QRX_PROVIDER_RUNTIME_VERSION||r->status!=QRX_PROVIDER_RUNTIME_CONFIGURED)return -1;r->status=QRX_PROVIDER_RUNTIME_READY;return 0;}

int qrx_resource_provider_market_register(QrxResourceProviderRuntime*r,const QrxResourceProviderWalletApproval*a,uint64_t h,QrxResourceProviderMarketRegistration*out){
    if(!r||!out||r->version!=QRX_PROVIDER_RUNTIME_VERSION||(r->status!=QRX_PROVIDER_RUNTIME_READY&&r->status!=QRX_PROVIDER_RUNTIME_REGISTERED))return -1;
    if(r->active_plan.requires_wallet_approval&&qrx_resource_provider_wallet_approval_validate(a,r->provider_id,r->network,r->active_plan_commitment,h))return -2;
    memset(out,0,sizeof(*out));out->version=QRX_PROVIDER_MARKET_REG_VERSION;snprintf(out->provider_id,sizeof(out->provider_id),"%s",r->provider_id);snprintf(out->network,sizeof(out->network),"%s",r->network);memcpy(out->plan_commitment,r->active_plan_commitment,65);out->registration_revision=++r->market_revision;out->enabled_mask=r->active_plan.enabled_mask;out->storage_bytes=r->active_plan.storage_bytes;out->model_cache_bytes=r->active_plan.model_cache_bytes;out->compute_threads=r->active_plan.compute_threads;out->network_egress_mbps=r->active_plan.network_egress_mbps;out->accelerator_enabled=r->active_plan.accelerator_enabled;out->cuda_enabled=r->active_plan.cuda_enabled;out->metal_mlx_enabled=r->active_plan.metal_mlx_enabled;out->wallet_approved=r->active_plan.requires_wallet_approval?1:0;r->status=QRX_PROVIDER_RUNTIME_REGISTERED;return 0;
}
int qrx_resource_provider_runtime_start_serving(QrxResourceProviderRuntime*r){if(!r||r->version!=QRX_PROVIDER_RUNTIME_VERSION||r->status!=QRX_PROVIDER_RUNTIME_REGISTERED)return -1;r->status=QRX_PROVIDER_RUNTIME_SERVING;return 0;}

int qrx_resource_provider_runtime_reserve(QrxResourceProviderRuntime*r,uint64_t s,uint64_t c,uint32_t t,uint64_t n){
    if(!r||r->version!=QRX_PROVIDER_RUNTIME_VERSION||r->status!=QRX_PROVIDER_RUNTIME_SERVING)return -1;
    if(s>r->active_plan.storage_bytes-r->reserved_storage_bytes||c>r->active_plan.model_cache_bytes-r->reserved_model_cache_bytes||t>r->active_plan.compute_threads-r->reserved_compute_threads||n>r->active_plan.network_egress_mbps-r->reserved_network_egress_mbps)return -2;
    r->reserved_storage_bytes+=s;r->reserved_model_cache_bytes+=c;r->reserved_compute_threads+=t;r->reserved_network_egress_mbps+=n;r->active_jobs++;return 0;
}
int qrx_resource_provider_runtime_release(QrxResourceProviderRuntime*r,uint64_t s,uint64_t c,uint32_t t,uint64_t n){
    if(!r||r->version!=QRX_PROVIDER_RUNTIME_VERSION||(r->status!=QRX_PROVIDER_RUNTIME_SERVING&&r->status!=QRX_PROVIDER_RUNTIME_DRAINING)||!r->active_jobs)return -1;
    if(s>r->reserved_storage_bytes||c>r->reserved_model_cache_bytes||t>r->reserved_compute_threads||n>r->reserved_network_egress_mbps)return -2;
    r->reserved_storage_bytes-=s;r->reserved_model_cache_bytes-=c;r->reserved_compute_threads-=t;r->reserved_network_egress_mbps-=n;r->active_jobs--;if(r->has_pending_plan)qrx_resource_provider_runtime_try_apply_pending(r);return 0;
}

int qrx_resource_provider_runtime_request_plan(QrxResourceProviderRuntime*r,const QrxResourceProviderPlan*p){
    if(!r||r->version!=QRX_PROVIDER_RUNTIME_VERSION||!plan_valid(p)||r->status==QRX_PROVIDER_RUNTIME_OFFLINE)return -1;
    char c[65];
    if(qrx_resource_provider_plan_commitment(p,c))return -2;
    if(plan_fits_reserved(r,p)){r->active_plan=*p;memcpy(r->active_plan_commitment,c,65);r->has_pending_plan=0;memset(&r->pending_plan,0,sizeof(r->pending_plan));memset(r->pending_plan_commitment,0,sizeof(r->pending_plan_commitment));if(r->status==QRX_PROVIDER_RUNTIME_REGISTERED||r->status==QRX_PROVIDER_RUNTIME_SERVING)r->status=QRX_PROVIDER_RUNTIME_READY;return 0;}
    r->pending_plan=*p;memcpy(r->pending_plan_commitment,c,65);r->has_pending_plan=1;if(r->status==QRX_PROVIDER_RUNTIME_SERVING)r->status=QRX_PROVIDER_RUNTIME_DRAINING;return 1;
}
int qrx_resource_provider_runtime_try_apply_pending(QrxResourceProviderRuntime*r){
    if(!r||r->version!=QRX_PROVIDER_RUNTIME_VERSION)return -1;
    if(!r->has_pending_plan)return 0;
    if(!plan_fits_reserved(r,&r->pending_plan))return 1;
    r->active_plan=r->pending_plan;memcpy(r->active_plan_commitment,r->pending_plan_commitment,65);r->has_pending_plan=0;memset(&r->pending_plan,0,sizeof(r->pending_plan));memset(r->pending_plan_commitment,0,sizeof(r->pending_plan_commitment));r->status=QRX_PROVIDER_RUNTIME_READY;return 0;
}
int qrx_resource_provider_runtime_begin_drain(QrxResourceProviderRuntime*r){if(!r||r->version!=QRX_PROVIDER_RUNTIME_VERSION||(r->status!=QRX_PROVIDER_RUNTIME_SERVING&&r->status!=QRX_PROVIDER_RUNTIME_REGISTERED))return -1;r->status=QRX_PROVIDER_RUNTIME_DRAINING;return 0;}
int qrx_resource_provider_runtime_shutdown(QrxResourceProviderRuntime*r){if(!r||r->version!=QRX_PROVIDER_RUNTIME_VERSION)return -1;if(r->active_jobs||r->reserved_storage_bytes||r->reserved_model_cache_bytes||r->reserved_compute_threads||r->reserved_network_egress_mbps)return -2;if(r->status!=QRX_PROVIDER_RUNTIME_DRAINING&&r->status!=QRX_PROVIDER_RUNTIME_READY&&r->status!=QRX_PROVIDER_RUNTIME_CONFIGURED&&r->status!=QRX_PROVIDER_RUNTIME_REGISTERED)return -3;r->status=QRX_PROVIDER_RUNTIME_OFFLINE;return 0;}

static int fld(EVP_MD_CTX*x,const void*p,size_t n){uint64_t z=(uint64_t)n;return EVP_DigestUpdate(x,&z,sizeof(z))==1&&(!n||EVP_DigestUpdate(x,p,n)==1)?0:-1;}
#define U32(v) do{uint32_t z=(uint32_t)(v);if(ok&&fld(x,&z,sizeof(z)))ok=0;}while(0)
#define U64(v) do{uint64_t z=(uint64_t)(v);if(ok&&fld(x,&z,sizeof(z)))ok=0;}while(0)
int qrx_resource_provider_market_registration_commitment(const QrxResourceProviderMarketRegistration*p,char out[65]){
    if(!p||!out||p->version!=QRX_PROVIDER_MARKET_REG_VERSION||!nz(p->provider_id,QRX_RESOURCE_PROVIDER_ID_MAX)||!nz(p->network,QRX_PROVIDER_NETWORK_MAX)||!hex64(p->plan_commitment)||!p->registration_revision||!p->enabled_mask)return -1;
    EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=x!=NULL;const char dom[]="QRX/RESOURCE-PROVIDER/MARKET-REG/V1";
    if(ok&&EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;
    if(ok&&fld(x,dom,sizeof(dom)-1))ok=0;
    if(ok&&fld(x,p->provider_id,strlen(p->provider_id)))ok=0;
    if(ok&&fld(x,p->network,strlen(p->network)))ok=0;
    if(ok&&fld(x,p->plan_commitment,64))ok=0;
    U64(p->registration_revision);U32(p->enabled_mask);U64(p->storage_bytes);U64(p->model_cache_bytes);U32(p->compute_threads);U64(p->network_egress_mbps);U32(p->accelerator_enabled);U32(p->cuda_enabled);U32(p->metal_mlx_enabled);U32(p->wallet_approved);
    if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;
    EVP_MD_CTX_free(x);
    if(!ok||hn!=32)return -2;
    static const char hx[]="0123456789abcdef";
    for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}
    out[64]=0;
    return 0;
}
#undef U32
#undef U64
