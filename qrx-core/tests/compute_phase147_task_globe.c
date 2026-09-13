#include "resource/qrx_task_globe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static QrxTaskGlobeEvent ev(uint64_t seq,QrxTaskGlobeStage s,const char *provider,const char *region,const char *pod,const char *expert,uint32_t cnt){
    QrxTaskGlobeEvent e;memset(&e,0,sizeof(e));e.version=QRX_TASK_GLOBE_VERSION;strcpy(e.task_id,"task-aura-001");e.sequence=seq;e.timestamp_ms=1000+seq;e.stage=s;e.flags=QRX_TASK_GLOBE_FLAG_FASTTRACK;
    if(provider)strncpy(e.provider_id,provider,QRX_TASK_GLOBE_PROVIDER_ID_MAX);if(region)strncpy(e.region,region,QRX_GLOBE_REGION_MAX);if(pod)strncpy(e.pod_id,pod,QRX_TASK_GLOBE_POD_ID_MAX);if(expert)strncpy(e.expert_group,expert,QRX_TASK_GLOBE_EXPERT_GROUP_MAX);e.expert_count=cnt;return e;
}
int main(void){
    QrxTaskGlobeEvent e[7];
    e[0]=ev(1,QRX_TASK_GLOBE_STAGE_SUBMITTED,NULL,NULL,NULL,NULL,0);
    e[1]=ev(2,QRX_TASK_GLOBE_STAGE_SCHEDULED,NULL,NULL,NULL,NULL,0);
    e[2]=ev(3,QRX_TASK_GLOBE_STAGE_COMPUTE,"p1","EU-WEST","pod-a",NULL,0);
    e[3]=ev(4,QRX_TASK_GLOBE_STAGE_EXPERT,"p2","EU-CENTRAL","pod-b","eg-1",8);
    e[4]=ev(5,QRX_TASK_GLOBE_STAGE_EXPERT,"p3","EU-NORTH","pod-c","eg-2",8);
    e[5]=ev(6,QRX_TASK_GLOBE_STAGE_EXPERT,"p4","EU-WEST","pod-a","eg-3",4);
    e[6]=ev(7,QRX_TASK_GLOBE_STAGE_RESULT,NULL,NULL,NULL,NULL,0);
    for(int i=0;i<7;i++)assert(qrx_task_globe_event_validate(&e[i])==0);
    QrxTaskGlobeFrame *f=NULL;size_t n=0;assert(qrx_task_globe_build_frames(e,7,3,&f,&n)==0&&n==7);
    assert(f[2].compute_provider_count==0); /* 1 provider: privacy suppressed */
    assert(f[3].compute_provider_count==0); /* 2 providers: still suppressed */
    assert(f[4].compute_provider_count==3&&f[4].region_count==3&&f[4].pod_count==3);
    assert(f[5].compute_provider_count==4&&f[5].expert_group_count==3&&f[5].expert_count==20);
    assert(f[6].terminal==1&&f[6].success==1&&f[6].progress_bps==10000&&f[6].fasttrack_on==1);
    char a[65],b[65];assert(qrx_task_globe_frame_commitment(&f[6],a)==0);assert(qrx_task_globe_frame_commitment(&f[6],b)==0);assert(!strcmp(a,b));
    qrx_task_globe_free_frames(f);
    /* Reject mixed tasks and post-terminal events. */
    QrxTaskGlobeEvent bad[2]={ev(1,QRX_TASK_GLOBE_STAGE_SUBMITTED,NULL,NULL,NULL,NULL,0),ev(2,QRX_TASK_GLOBE_STAGE_RESULT,NULL,NULL,NULL,NULL,0)};strcpy(bad[1].task_id,"other");assert(qrx_task_globe_build_frames(bad,2,3,&f,&n)==-2);
    QrxTaskGlobeEvent post[3]={ev(1,QRX_TASK_GLOBE_STAGE_SUBMITTED,NULL,NULL,NULL,NULL,0),ev(2,QRX_TASK_GLOBE_STAGE_RESULT,NULL,NULL,NULL,NULL,0),ev(3,QRX_TASK_GLOBE_STAGE_COMPUTE,"p1","EU","pod",NULL,0)};assert(qrx_task_globe_build_frames(post,3,3,&f,&n)==-4);
    puts("compute_phase147_task_globe: ok");return 0;
}
