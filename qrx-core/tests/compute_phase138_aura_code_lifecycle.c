#include "compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void fillhex(char x[65], char c){for(int i=0;i<64;i++)x[i]=c;x[64]=0;}
int main(void){
    QrxAuraArtifact a={0};a.version=QRX_AURA_ARTIFACT_VERSION;strcpy(a.artifact_id,"a1");strcpy(a.owner,"qrx:user1");strcpy(a.name,"source.zip");strcpy(a.mime_type,"application/zip");strcpy(a.extension,".zip");a.type=QRX_AURA_ARTIFACT_ARCHIVE;strcpy(a.drive_ref,"qrxdrive:source");strcpy(a.source_job_ref,"job:seed");fillhex(a.content_commitment,'a');a.size_bytes=10;a.private_pq=1;a.immutable=1;assert(qrx_aura_artifact_validate(&a)==0);
    QrxAuraProject p={0};p.version=QRX_AURA_PROJECT_VERSION;strcpy(p.project_id,"p1");strcpy(p.owner,"qrx:user1");strcpy(p.name,"demo");strcpy(p.root_ref,"qrxdrive:project");p.private_pq=1;assert(qrx_aura_project_add_file(&p,"src/source.zip",&a)==0);
    QrxAuraCodeWorkspace w;assert(qrx_aura_code_workspace_init(&p,"ws1","qrxdrive:project-v1",1000,&w)==0);assert(qrx_aura_code_workspace_add_stage(&w,QRX_AURA_CODE_STAGE_GENERATE,"generate","qrxdrive:project-v1","qrxdrive:patch","aura-code",200,1000,10000)==0);assert(qrx_aura_code_workspace_add_stage(&w,QRX_AURA_CODE_STAGE_COMPILE,"compile","qrxdrive:patch","qrxdrive:build","sandbox-build",300,1000,10000)==0);assert(qrx_aura_code_workspace_add_stage(&w,QRX_AURA_CODE_STAGE_TEST,"test","qrxdrive:build","qrxdrive:test","sandbox-test",300,1000,10000)==0);assert(qrx_aura_code_workspace_approve(&w,1)==0);
    QrxAuraCodeExecution e;assert(qrx_aura_code_execution_init(&w,"exec1",&e)==0);char h[65];fillhex(h,'b');
    assert(qrx_aura_code_execution_start_stage(&w,&e,0,100)==0);assert(qrx_aura_code_execution_record_result(&w,&e,0,QRX_AURA_CODE_STAGE_PASSED,"qrxdrive:patch","qrxdrive:genlog",h,"patch generated",0,100,150)==0);
    assert(qrx_aura_code_execution_start_stage(&w,&e,2,160)==-2);
    assert(qrx_aura_code_execution_start_stage(&w,&e,1,160)==0);assert(qrx_aura_code_execution_record_result(&w,&e,1,QRX_AURA_CODE_STAGE_PASSED,"qrxdrive:build","qrxdrive:buildlog",h,"build passed",0,220,200)==0);
    assert(qrx_aura_code_execution_start_stage(&w,&e,2,210)==0);assert(qrx_aura_code_execution_record_result(&w,&e,2,QRX_AURA_CODE_STAGE_PASSED,"qrxdrive:test","qrxdrive:testlog",h,"tests passed",0,240,260)==0);
    assert(e.total_charged_atoms==560);
    assert(qrx_aura_code_execution_set_reports(&e,"qrxdrive:patch","qrxdrive:diff","qrxdrive:build-report","qrxdrive:test-report")==0);
    assert(qrx_aura_code_execution_finalize(&w,&e)==0);assert(qrx_aura_code_execution_user_approve(&e,1)==0);char c1[65],c2[65];assert(qrx_aura_code_execution_commitment(&w,&e,c1)==0);assert(qrx_aura_code_execution_commitment(&w,&e,c2)==0);assert(!strcmp(c1,c2));assert(qrx_aura_code_execution_validate(&w,&e)==0);
    puts("compute_phase138_aura_code_lifecycle: PASS");return 0;
}
