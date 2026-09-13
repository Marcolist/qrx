#include "resource/qrx_task_globe.h"
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>

static int bounded(const char *s,size_t n){return s&&memchr(s,'\0',n)!=NULL;}
static int valid_stage(QrxTaskGlobeStage s){return s>=QRX_TASK_GLOBE_STAGE_SUBMITTED&&s<=QRX_TASK_GLOBE_STAGE_FAILED;}

int qrx_task_globe_event_validate(const QrxTaskGlobeEvent *e){
    if(!e||e->version!=QRX_TASK_GLOBE_VERSION)return -1;
    if(!bounded(e->task_id,sizeof(e->task_id))||!e->task_id[0])return -1;
    if(!bounded(e->region,sizeof(e->region))||!bounded(e->pod_id,sizeof(e->pod_id))||
       !bounded(e->provider_id,sizeof(e->provider_id))||!bounded(e->expert_group,sizeof(e->expert_group)))return -1;
    if(!valid_stage(e->stage)||(e->flags&~QRX_TASK_GLOBE_FLAG_ALL))return -1;
    if((e->stage==QRX_TASK_GLOBE_STAGE_COMPUTE||e->stage==QRX_TASK_GLOBE_STAGE_EXPERT)&&
       (!e->region[0]||!e->provider_id[0]))return -1;
    if(e->stage==QRX_TASK_GLOBE_STAGE_EXPERT&&(!e->expert_group[0]||e->expert_count==0))return -1;
    return 0;
}

static int cmp_event(const void *a,const void *b){
    const QrxTaskGlobeEvent *x=(const QrxTaskGlobeEvent*)a,*y=(const QrxTaskGlobeEvent*)b;
    if(x->sequence<y->sequence)return -1;if(x->sequence>y->sequence)return 1;
    if(x->timestamp_ms<y->timestamp_ms)return -1;if(x->timestamp_ms>y->timestamp_ms)return 1;
    return strcmp(x->task_id,y->task_id);
}
static int seen_str(char (*arr)[QRX_TASK_GLOBE_PROVIDER_ID_MAX+1],size_t n,const char*s){
    for(size_t i=0;i<n;i++)if(!strcmp(arr[i],s))return 1;return 0;
}
static int seen_region(char (*arr)[QRX_GLOBE_REGION_MAX+1],size_t n,const char*s){for(size_t i=0;i<n;i++)if(!strcmp(arr[i],s))return 1;return 0;}
static int seen_pod(char (*arr)[QRX_TASK_GLOBE_POD_ID_MAX+1],size_t n,const char*s){for(size_t i=0;i<n;i++)if(!strcmp(arr[i],s))return 1;return 0;}
static int seen_expert(char (*arr)[QRX_TASK_GLOBE_EXPERT_GROUP_MAX+1],size_t n,const char*s){for(size_t i=0;i<n;i++)if(!strcmp(arr[i],s))return 1;return 0;}
static uint32_t progress_for(QrxTaskGlobeStage s){
    switch(s){case QRX_TASK_GLOBE_STAGE_SUBMITTED:return 500;case QRX_TASK_GLOBE_STAGE_SCHEDULED:return 2000;case QRX_TASK_GLOBE_STAGE_COMPUTE:return 5000;case QRX_TASK_GLOBE_STAGE_EXPERT:return 8000;case QRX_TASK_GLOBE_STAGE_RESULT:return 10000;case QRX_TASK_GLOBE_STAGE_FAILED:return 10000;default:return 0;}
}

int qrx_task_globe_build_frames(const QrxTaskGlobeEvent *events,size_t count,size_t privacy_min_providers,QrxTaskGlobeFrame **out,size_t *n_out){
    if(!events||!count||!out||!n_out||count>QRX_TASK_GLOBE_MAX_FRAMES)return -1;
    if(!privacy_min_providers)privacy_min_providers=QRX_GLOBE_DEFAULT_PRIVACY_MIN_PROVIDERS;
    for(size_t i=0;i<count;i++)if(qrx_task_globe_event_validate(&events[i]))return -1;
    QrxTaskGlobeEvent *sorted=(QrxTaskGlobeEvent*)malloc(count*sizeof(*sorted));
    QrxTaskGlobeFrame *frames=(QrxTaskGlobeFrame*)calloc(count,sizeof(*frames));
    char (*providers)[QRX_TASK_GLOBE_PROVIDER_ID_MAX+1]=calloc(count,sizeof(*providers));
    char (*regions)[QRX_GLOBE_REGION_MAX+1]=calloc(count,sizeof(*regions));
    char (*pods)[QRX_TASK_GLOBE_POD_ID_MAX+1]=calloc(count,sizeof(*pods));
    char (*experts)[QRX_TASK_GLOBE_EXPERT_GROUP_MAX+1]=calloc(count,sizeof(*experts));
    if(!sorted||!frames||!providers||!regions||!pods||!experts){free(sorted);free(frames);free(providers);free(regions);free(pods);free(experts);return -1;}
    memcpy(sorted,events,count*sizeof(*sorted));qsort(sorted,count,sizeof(*sorted),cmp_event);
    const char *task=sorted[0].task_id;size_t pc=0,rc=0,poc=0,ec=0;uint32_t expert_total=0;uint32_t flags=0;uint64_t last_seq=0;QrxTaskGlobeStage last_stage=QRX_TASK_GLOBE_STAGE_SUBMITTED;
    for(size_t i=0;i<count;i++){
        QrxTaskGlobeEvent *e=&sorted[i];
        if(strcmp(e->task_id,task)){free(sorted);free(frames);free(providers);free(regions);free(pods);free(experts);return -2;}
        if(i&&e->sequence<=last_seq){free(sorted);free(frames);free(providers);free(regions);free(pods);free(experts);return -3;}
        if(i&&last_stage==QRX_TASK_GLOBE_STAGE_RESULT){free(sorted);free(frames);free(providers);free(regions);free(pods);free(experts);return -4;}
        if(i&&last_stage==QRX_TASK_GLOBE_STAGE_FAILED){free(sorted);free(frames);free(providers);free(regions);free(pods);free(experts);return -4;}
        if(e->provider_id[0]&&!seen_str(providers,pc,e->provider_id)){strncpy(providers[pc++],e->provider_id,QRX_TASK_GLOBE_PROVIDER_ID_MAX);}
        if(e->region[0]&&!seen_region(regions,rc,e->region)){strncpy(regions[rc++],e->region,QRX_GLOBE_REGION_MAX);}
        if(e->pod_id[0]&&!seen_pod(pods,poc,e->pod_id)){strncpy(pods[poc++],e->pod_id,QRX_TASK_GLOBE_POD_ID_MAX);}
        if(e->expert_group[0]&&!seen_expert(experts,ec,e->expert_group)){strncpy(experts[ec++],e->expert_group,QRX_TASK_GLOBE_EXPERT_GROUP_MAX);expert_total+=e->expert_count;}
        flags|=e->flags;
        QrxTaskGlobeFrame *f=&frames[i];f->version=QRX_TASK_GLOBE_VERSION;strncpy(f->task_id,task,QRX_TASK_GLOBE_TASK_ID_MAX);f->sequence=e->sequence;f->timestamp_ms=e->timestamp_ms;f->stage=e->stage;f->flags=flags;
        /* Privacy gate: counts derived from sparse provider sets are suppressed until threshold. */
        if(pc>=privacy_min_providers){f->compute_provider_count=(uint32_t)pc;f->region_count=(uint32_t)rc;f->pod_count=(uint32_t)poc;f->expert_group_count=(uint32_t)ec;f->expert_count=expert_total;}
        f->progress_bps=progress_for(e->stage);f->fasttrack_on=(flags&QRX_TASK_GLOBE_FLAG_FASTTRACK)!=0;f->terminal=(e->stage==QRX_TASK_GLOBE_STAGE_RESULT||e->stage==QRX_TASK_GLOBE_STAGE_FAILED);f->success=e->stage==QRX_TASK_GLOBE_STAGE_RESULT;
        last_seq=e->sequence;last_stage=e->stage;
    }
    free(sorted);free(providers);free(regions);free(pods);free(experts);*out=frames;*n_out=count;return 0;
}

void qrx_task_globe_free_frames(QrxTaskGlobeFrame *f){free(f);}
static int upd(EVP_MD_CTX*x,const void*p,size_t n){return EVP_DigestUpdate(x,p,n)==1?0:-1;}
int qrx_task_globe_frame_commitment(const QrxTaskGlobeFrame*f,char out[65]){
    if(!f||!out||f->version!=QRX_TASK_GLOBE_VERSION||!f->task_id[0])return -1;
    EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/TASK-GLOBE/FRAME/V1";
    if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;
#define U(v) do{if(ok&&upd(x,&(v),sizeof(v)))ok=0;}while(0)
    if(ok&&upd(x,dom,sizeof(dom)-1))ok=0;if(ok&&upd(x,f->task_id,strlen(f->task_id)))ok=0;
    U(f->sequence);U(f->timestamp_ms);U(f->stage);U(f->flags);U(f->compute_provider_count);U(f->region_count);U(f->pod_count);U(f->expert_group_count);U(f->expert_count);U(f->progress_bps);U(f->fasttrack_on);U(f->terminal);U(f->success);
#undef U
    if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;
}
