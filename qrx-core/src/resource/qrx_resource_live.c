#include "resource/qrx_resource_live.h"
#include "resource/qrx_storage_repair.h"
#include "qrxdb.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { QrxStorageAtlasProvider *v; size_t n,cap; } ProvVec;
typedef struct { QrxResourceWorkloadStats w; } WorkCtx;
static char *dupval(const char*v,uint32_t n){char*s=(char*)malloc((size_t)n+1);if(!s)return NULL;memcpy(s,v,n);s[n]=0;return s;}
static int split(char*s,char**a,int max){int n=0;char*sv=NULL;for(char*x=strtok_r(s,"|",&sv);x&&n<max;x=strtok_r(NULL,"|",&sv))a[n++]=x;return n;}
static int provider_cb(const char*k,const char*v,uint32_t vl,void*x){
    const char *pre="storage/provider/"; ProvVec*c=(ProvVec*)x; if(strncmp(k,pre,strlen(pre)))return 0;
    char*t=dupval(v,vl); if(!t)return -1; char*a[17]; int n=split(t,a,17); if(n!=17||strcmp(a[0],"1")){free(t);return 0;}
    if(c->n==c->cap){size_t nc=c->cap?c->cap*2:16;void*p=realloc(c->v,nc*sizeof(*c->v));if(!p){free(t);return -1;}c->v=p;c->cap=nc;}
    QrxStorageAtlasProvider*p=&c->v[c->n++];memset(p,0,sizeof(*p));snprintf(p->provider_id,sizeof(p->provider_id),"%s",k+strlen(pre));snprintf(p->capacity.provider_id,sizeof(p->capacity.provider_id),"%s",p->provider_id);
    p->capacity.proven_bytes=strtoull(a[4],NULL,10);p->capacity.physical_eligible_bytes=p->capacity.proven_bytes;p->capacity.configured_bytes=p->capacity.proven_bytes;
    p->capacity.allocated_physical_bytes=strtoull(a[5],NULL,10);p->capacity.availability_bps=(uint32_t)strtoul(a[7],NULL,10);p->capacity.proof_success_bps=(uint32_t)strtoul(a[8],NULL,10);
    if(strcmp(a[14],"-")&&strcmp(a[15],"-")){snprintf(p->asn,sizeof(p->asn),"%s",a[14]);snprintf(p->region,sizeof(p->region),"%s",a[15]);p->network_attested=1;}
    free(t);return 0;
}
static int contract_cb(const char*k,const char*v,uint32_t vl,void*x){(void)k;WorkCtx*c=(WorkCtx*)x;char*t=dupval(v,vl);if(!t)return -1;char*a[18];int n=split(t,a,18);if(n==18&&!strcmp(a[0],"1")){unsigned st=(unsigned)strtoul(a[17],NULL,10);if(st==1){c->w.active_contracts++;c->w.logical_user_bytes+=strtoull(a[3],NULL,10);c->w.purchased_logical_bytes+=strtoull(a[3],NULL,10);}}free(t);return 0;}
static int assignment_cb(const char*k,const char*v,uint32_t vl,void*x){(void)k;WorkCtx*c=(WorkCtx*)x;char*t=dupval(v,vl);if(!t)return -1;char*a[11];int n=split(t,a,11);if(n==11&&!strcmp(a[0],"1")){unsigned st=(unsigned)strtoul(a[2],NULL,10);if(st==QRX_ASSIGN_ACTIVE)c->w.healthy_shards++;else if(st==QRX_ASSIGN_DEGRADED)c->w.degraded_shards++;else if(st==QRX_ASSIGN_REPAIRING)c->w.repairing_shards++;}free(t);return 0;}
static int count_cb(const char*k,const char*v,uint32_t vl,void*x){(void)k;(void)v;(void)vl;(*(uint64_t*)x)++;return 0;}
int qrx_resource_live_snapshot(const char*chain_dir,size_t privacy_min_providers,QrxResourceDashboardSnapshot*out,QrxStorageAtlasCell**cells_out,size_t*cell_count_out){
    if(!chain_dir||!out||!cells_out||!cell_count_out)return -1;QrxDB db;if(qrxdb_init(&db,chain_dir))return -1;ProvVec pv={0};WorkCtx wc={0};int rc=0;
    if(qrxdb_scan_prefix(&db,"storage/provider/",provider_cb,&pv)||qrxdb_scan_prefix(&db,"storage/contract/",contract_cb,&wc)||qrxdb_scan_prefix(&db,"storage/assignment/",assignment_cb,&wc))rc=-1;
    uint64_t domains=0;if(!rc&&qrxdb_scan_prefix(&db,"qrxnet/domain/",count_cb,&domains))rc=-1;wc.w.active_domains=domains;
    /* PUBLIC_SIGNED site/request/cache counters are reserved in the dashboard schema; until those authoritative keys exist they stay zero rather than being fabricated. */
    QrxStorageRedundancyProfile profile={"STANDARD",10,4,0};
    if(!rc)rc=qrx_resource_dashboard_build(pv.v,pv.n,&wc.w,&profile,privacy_min_providers,out,cells_out,cell_count_out);
    free(pv.v);qrxdb_close(&db);return rc;
}
