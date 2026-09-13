#include "compute/qrx_compute.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void hex64(char out[65], char c){for(int i=0;i<64;i++)out[i]=c;out[64]=0;}

int main(void){
    QrxAuraArtifact a={0}; a.version=QRX_AURA_ARTIFACT_VERSION; strcpy(a.artifact_id,"src-1"); strcpy(a.owner,"qrx1owner"); strcpy(a.name,"main.c"); strcpy(a.mime_type,"text/x-c"); strcpy(a.extension,".c"); a.type=QRX_AURA_ARTIFACT_CODE; strcpy(a.drive_ref,"qrxdrive:artifact:main-c"); hex64(a.content_commitment,'a'); a.size_bytes=42; a.private_pq=1; a.immutable=1;
    CHECK(qrx_aura_artifact_validate(&a)==0);
    QrxAuraProject p={0}; p.version=QRX_AURA_PROJECT_VERSION; strcpy(p.project_id,"proj-1"); strcpy(p.owner,"qrx1owner"); strcpy(p.name,"demo"); strcpy(p.root_ref,"qrxdrive:project:demo"); p.private_pq=1; CHECK(qrx_aura_project_add_file(&p,"src/main.c",&a)==0); CHECK(qrx_aura_project_validate(&p)==0);

    QrxAuraCodeWorkspace w={0}; CHECK(qrx_aura_code_workspace_init(&p,"ws-1","qrxdrive:project:demo",1000,&w)==0);
    CHECK(qrx_aura_code_workspace_add_stage(&w,QRX_AURA_CODE_STAGE_GENERATE,"Generate patch","qrxdrive:project:demo","qrxdrive:artifact:patch","qrx-codegen-v1",200,60000,1024*1024)==0);
    CHECK(qrx_aura_code_workspace_add_stage(&w,QRX_AURA_CODE_STAGE_COMPILE,"Compile","qrxdrive:artifact:patch","qrxdrive:artifact:buildlog","qrx-sandbox-cmake-v1",300,120000,4*1024*1024)==0);
    CHECK(qrx_aura_code_workspace_add_stage(&w,QRX_AURA_CODE_STAGE_TEST,"Test","qrxdrive:artifact:patch","qrxdrive:artifact:testlog","qrx-sandbox-ctest-v1",300,120000,4*1024*1024)==0);
    CHECK(w.stages[0].requires_user_approval==0); CHECK(w.stages[1].requires_user_approval==1);
    CHECK(qrx_aura_code_workspace_approve(&w,0)==-2); CHECK(qrx_aura_code_workspace_approve(&w,1)==0);
    char c1[65],c2[65]; CHECK(qrx_aura_code_workspace_commitment(&w,c1)==0); CHECK(qrx_aura_code_workspace_commitment(&w,c2)==0); CHECK(strcmp(c1,c2)==0);

    QrxAuraConversation conv={0}; conv.version=QRX_AURA_CHAT_VERSION; strcpy(conv.conversation_id,"conv"); strcpy(conv.owner,"qrx1owner"); strcpy(conv.session_id,"sess"); conv.max_conversation_fee_atoms=1000; conv.encrypted_context=1; CHECK(qrx_aura_conversation_validate(&conv)==0);
    QrxAuraToolCall call={0}; CHECK(qrx_aura_code_stage_prepare_tool_call(&w,&conv,1,"compile-call",&call)==0); CHECK(call.tool_type==QRX_AURA_TOOL_COMPILE); CHECK(call.user_approved==1); CHECK(call.requested_fee_atoms==300);

    QrxAuraCodeWorkspace over={0}; CHECK(qrx_aura_code_workspace_init(&p,"ws-2","qrxdrive:project:demo",100,&over)==0); CHECK(qrx_aura_code_workspace_add_stage(&over,QRX_AURA_CODE_STAGE_GENERATE,"too expensive","qrxdrive:project:demo","qrxdrive:x","qrx-codegen-v1",101,1000,1000)==-2);
    puts("compute_phase137_aura_code_workspace PASS"); return 0;
}
