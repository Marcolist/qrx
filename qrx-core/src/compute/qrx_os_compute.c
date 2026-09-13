#include "compute/qrx_os_compute.h"
#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

static int nonempty_bounded(const char *s,size_t n){return s&&s[0]&&memchr(s,'\0',n)!=NULL;}
static int hex64(const char*s){if(!s||strlen(s)!=64)return 0;for(size_t i=0;i<64;i++){char c=s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;}return 1;}
static int digest_field(EVP_MD_CTX*x,const void*p,size_t n){uint64_t len=(uint64_t)n;unsigned char b[8];for(int i=0;i<8;i++)b[i]=(unsigned char)(len>>(56-8*i));return EVP_DigestUpdate(x,b,8)!=1||EVP_DigestUpdate(x,p,n)!=1;}
static int digest_u32(EVP_MD_CTX*x,uint32_t v){unsigned char b[4]={(unsigned char)(v>>24),(unsigned char)(v>>16),(unsigned char)(v>>8),(unsigned char)v};return EVP_DigestUpdate(x,b,4)!=1;}
static int digest_u64(EVP_MD_CTX*x,uint64_t v){unsigned char b[8];for(int i=0;i<8;i++)b[i]=(unsigned char)(v>>(56-8*i));return EVP_DigestUpdate(x,b,8)!=1;}
static int finish_hex(EVP_MD_CTX*x,char out[65]){unsigned char h[32];unsigned int n=0;static const char z[]="0123456789abcdef";if(EVP_DigestFinal_ex(x,h,&n)!=1||n!=32)return -1;for(int i=0;i<32;i++){out[i*2]=z[h[i]>>4];out[i*2+1]=z[h[i]&15];}out[64]=0;return 0;}

uint32_t qrx_os_compute_required_services(QrxOsComputeApp app){
    switch(app){
        case QRX_OS_APP_CHAT: return QRX_OS_SERVICE_WALLET|QRX_OS_SERVICE_COMPUTE|QRX_OS_SERVICE_CHAIN;
        case QRX_OS_APP_CODING: case QRX_OS_APP_DOCUMENTS: case QRX_OS_APP_RESEARCH: case QRX_OS_APP_RAG: case QRX_OS_APP_BUILD_TASKS: case QRX_OS_APP_AI_AGENTS: case QRX_OS_APP_BATCH_COMPUTE: return QRX_OS_SERVICE_ALL;
        default:return 0;
    }
}
int qrx_os_compute_workspace_validate(const QrxOsComputeWorkspace*w){
    if(!w||w->version!=QRX_OS_COMPUTE_VERSION||!qrx_os_compute_required_services(w->app))return -1;
    if(!nonempty_bounded(w->workspace_id,sizeof(w->workspace_id))||!nonempty_bounded(w->wallet_identity,sizeof(w->wallet_identity)))return -2;
    uint32_t req=qrx_os_compute_required_services(w->app);if((w->service_mask&req)!=req||(w->service_mask&~QRX_OS_SERVICE_ALL))return -3;
    if((req&QRX_OS_SERVICE_DRIVE)&&!nonempty_bounded(w->drive_namespace_ref,sizeof(w->drive_namespace_ref)))return -4;
    if(!nonempty_bounded(w->payment_escrow_ref,sizeof(w->payment_escrow_ref))||!nonempty_bounded(w->settlement_ref,sizeof(w->settlement_ref)))return -5;
    if(!w->max_total_fee_atoms||!w->expiry_height||w->private_pq>1)return -6;return 0;
}
int qrx_os_compute_workspace_commitment(const QrxOsComputeWorkspace*w,char out[65]){
    if(qrx_os_compute_workspace_validate(w)||!out)return -1;EVP_MD_CTX*x=EVP_MD_CTX_new();int ok=1;const char dom[]="QRX/OS/COMPUTE-WORKSPACE/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;
#define F(v) do{if(ok&&digest_field(x,(v),strlen(v)))ok=0;}while(0)
    if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;F(w->workspace_id);F(w->wallet_identity);F(w->drive_namespace_ref);F(w->payment_escrow_ref);F(w->settlement_ref);
#undef F
    if(ok&&(digest_u32(x,w->version)||digest_u32(x,(uint32_t)w->app)||digest_u32(x,w->service_mask)||digest_u64(x,w->max_total_fee_atoms)||digest_u64(x,w->expiry_height)||digest_u32(x,w->private_pq)))ok=0;int rc=ok?finish_hex(x,out):-1;EVP_MD_CTX_free(x);return rc;
}
static int graph_compatible(const QrxOsComputeWorkspace*w,const QrxComputeJobGraph*g){
    if(qrx_os_compute_workspace_validate(w)||qrx_compute_job_graph_validate(g))return -1;if(strcmp(w->wallet_identity,g->owner))return -2;if(g->max_total_fee_atoms>w->max_total_fee_atoms||g->expiry_height>w->expiry_height)return -3;
    if(w->app==QRX_OS_APP_BUILD_TASKS){int build=0,test=0;for(uint32_t i=0;i<g->node_count;i++){build|=g->nodes[i].job_type==QRX_JOB_COMPILE;test|=g->nodes[i].job_type==QRX_JOB_TEST;}if(!build||!test)return -4;}
    if(w->app==QRX_OS_APP_CHAT){for(uint32_t i=0;i<g->node_count;i++)if(g->nodes[i].capability_mask&QRX_JOB_CAP_WRITE_ARTIFACTS)return -5;}
    return 0;
}
int qrx_os_compute_bind_graph(const QrxOsComputeWorkspace*w,const QrxComputeJobGraph*g,QrxOsComputeRun*r){
    if(!r||graph_compatible(w,g))return -1;memset(r,0,sizeof(*r));r->version=QRX_OS_COMPUTE_VERSION;if(!nonempty_bounded(g->graph_id,sizeof(g->graph_id)))return -2;snprintf(r->run_id,sizeof(r->run_id),"run:%s",g->graph_id);if(qrx_os_compute_workspace_commitment(w,r->workspace_commitment)||qrx_compute_job_graph_commitment(g,r->graph_commitment))return -3;r->state=QRX_OS_RUN_READY;return 0;
}
int qrx_os_compute_submit(QrxOsComputeRun*r,uint64_t h){if(!r||r->version!=QRX_OS_COMPUTE_VERSION||r->state!=QRX_OS_RUN_READY||!h)return -1;r->state=QRX_OS_RUN_SUBMITTED;r->submitted_height=h;return 0;}
int qrx_os_compute_mark_running(QrxOsComputeRun*r){if(!r||r->version!=QRX_OS_COMPUTE_VERSION||r->state!=QRX_OS_RUN_SUBMITTED)return -1;r->state=QRX_OS_RUN_RUNNING;return 0;}
int qrx_os_compute_complete(const QrxOsComputeWorkspace*w,const QrxComputeJobGraph*g,QrxOsComputeRun*r,const QrxAuraArtifact*a,uint64_t fee,uint64_t settled){
    if(!r||graph_compatible(w,g)||qrx_aura_artifact_validate(a)||r->state!=QRX_OS_RUN_RUNNING)return -1;if(strcmp(a->owner,w->wallet_identity)||fee>g->max_total_fee_atoms||fee>w->max_total_fee_atoms||!settled||settled<r->submitted_height)return -2;
    char wc[65],gc[65],ac[65];if(qrx_os_compute_workspace_commitment(w,wc)||qrx_compute_job_graph_commitment(g,gc)||qrx_aura_artifact_commitment(a,ac))return -3;if(strcmp(wc,r->workspace_commitment)||strcmp(gc,r->graph_commitment))return -4;
    snprintf(r->output_artifact_commitment,sizeof(r->output_artifact_commitment),"%s",ac);r->charged_fee_atoms=fee;r->settled_height=settled;r->state=QRX_OS_RUN_COMPLETED;return 0;
}
int qrx_os_compute_fail(QrxOsComputeRun*r){if(!r||r->version!=QRX_OS_COMPUTE_VERSION||(r->state!=QRX_OS_RUN_SUBMITTED&&r->state!=QRX_OS_RUN_RUNNING))return -1;r->state=QRX_OS_RUN_FAILED;return 0;}
int qrx_os_compute_cancel(QrxOsComputeRun*r){if(!r||r->version!=QRX_OS_COMPUTE_VERSION||(r->state!=QRX_OS_RUN_READY&&r->state!=QRX_OS_RUN_SUBMITTED))return -1;r->state=QRX_OS_RUN_CANCELLED;return 0;}
int qrx_os_compute_run_commitment(const QrxOsComputeRun*r,char out[65]){
    if(!r||r->version!=QRX_OS_COMPUTE_VERSION||!out||!nonempty_bounded(r->run_id,sizeof(r->run_id))||!hex64(r->workspace_commitment)||!hex64(r->graph_commitment)||r->state<QRX_OS_RUN_READY||r->state>QRX_OS_RUN_CANCELLED)return -1;if(r->state==QRX_OS_RUN_COMPLETED&&!hex64(r->output_artifact_commitment))return -2;
    EVP_MD_CTX*x=EVP_MD_CTX_new();int ok=1;const char dom[]="QRX/OS/COMPUTE-RUN/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;
#define F(v) do{if(ok&&digest_field(x,(v),strlen(v)))ok=0;}while(0)
    F(r->run_id);F(r->workspace_commitment);F(r->graph_commitment);F(r->output_artifact_commitment);
#undef F
    if(ok&&(digest_u32(x,r->version)||digest_u32(x,(uint32_t)r->state)||digest_u64(x,r->charged_fee_atoms)||digest_u64(x,r->submitted_height)||digest_u64(x,r->settled_height)))ok=0;int rc=ok?finish_hex(x,out):-1;EVP_MD_CTX_free(x);return rc;
}
