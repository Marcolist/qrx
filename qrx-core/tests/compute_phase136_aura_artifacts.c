#include "compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 QrxAuraArtifact a={0};a.version=QRX_AURA_ARTIFACT_VERSION;snprintf(a.artifact_id,sizeof(a.artifact_id),"artifact-1");snprintf(a.owner,sizeof(a.owner),"qrx:user1");snprintf(a.name,sizeof(a.name),"report.xlsx");snprintf(a.mime_type,sizeof(a.mime_type),"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");snprintf(a.extension,sizeof(a.extension),".xlsx");a.type=QRX_AURA_ARTIFACT_SPREADSHEET;snprintf(a.drive_ref,sizeof(a.drive_ref),"qrxdrive:artifact-1-v1");snprintf(a.source_job_ref,sizeof(a.source_job_ref),"job:artifact-build-1");memset(a.content_commitment,'a',64);a.content_commitment[64]=0;a.size_bytes=4096;a.private_pq=1;a.immutable=1;assert(qrx_aura_artifact_validate(&a)==0);
 char ah[65];assert(qrx_aura_artifact_commitment(&a,ah)==0);assert(strlen(ah)==64);
 QrxAuraProject p={0};p.version=QRX_AURA_PROJECT_VERSION;snprintf(p.project_id,sizeof(p.project_id),"project-1");snprintf(p.owner,sizeof(p.owner),"qrx:user1");snprintf(p.name,sizeof(p.name),"Quarterly report");snprintf(p.root_ref,sizeof(p.root_ref),"qrxdrive:projects/project-1");p.private_pq=1;assert(qrx_aura_project_add_file(&p,"reports/Q3/report.xlsx",&a)==0);assert(qrx_aura_project_add_file(&p,"reports/Q3/report.xlsx",&a)==-2);assert(qrx_aura_project_add_file(&p,"../escape.xlsx",&a)!=0);assert(qrx_aura_project_validate(&p)==0);char ph[65];assert(qrx_aura_project_commitment(&p,ph)==0);
 QrxAuraConversation c={0};c.version=QRX_AURA_CHAT_VERSION;snprintf(c.conversation_id,sizeof(c.conversation_id),"conv1");snprintf(c.owner,sizeof(c.owner),"qrx:user1");snprintf(c.session_id,sizeof(c.session_id),"session1");c.max_conversation_fee_atoms=1000;c.encrypted_context=1;assert(qrx_aura_conversation_validate(&c)==0);
 QrxAuraToolCall call;assert(qrx_aura_artifact_prepare_write_call(&c,&a,"write-art-1","qrxdrive:artifact-input",100,&call)==0);assert(call.tool_type==QRX_AURA_TOOL_WRITE_ARTIFACT);assert(!strcmp(call.output_ref,a.drive_ref));
 QrxAuraChatMessage m={0};m.version=QRX_AURA_CHAT_VERSION;snprintf(m.message_id,sizeof(m.message_id),"m1");snprintf(m.conversation_id,sizeof(m.conversation_id),"conv1");m.role=QRX_AURA_CHAT_ROLE_ASSISTANT;m.state=QRX_AURA_CHAT_MSG_COMPLETE;snprintf(m.content_ref,sizeof(m.content_ref),"qrxdrive:reply");assert(qrx_aura_artifact_attach_to_chat(&m,&a)==0);assert(m.artifact_count==1);
 puts("compute_phase136_aura_artifacts: PASS");return 0;
}
