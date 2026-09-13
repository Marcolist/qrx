#include "compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 QrxAuraAgentManifest a={0};a.version=QRX_AURA_RUNTIME_VERSION;strcpy(a.agent_id,"aura");strcpy(a.owner,"qrx1owner");strcpy(a.model_id,"test-model");strcpy(a.model_version,"1");a.capability_mask=QRX_AURA_CAP_ALLOWED_V1;a.max_steps=32;a.max_job_fee_atoms=500;a.max_session_fee_atoms=2000;a.require_user_approval_for_code_execution=1;
 QrxAuraSession s;assert(qrx_aura_session_open(&a,"session-1",1500,&s)==0);
 QrxAuraConversation c;assert(qrx_aura_conversation_open(&a,&s,"conv-1","Test","qrxdrive:encrypted-context",1000,&c)==0);assert(c.encrypted_context==1);
 QrxAuraToolCall call;QrxAuraChatMessage m;assert(qrx_aura_chat_begin_assistant(&c,"m2","m1","qrxdrive:prompt-1","qrxdrive:reply-1",400,&call,&m)==0);assert(call.tool_type==QRX_AURA_TOOL_MODEL_INFERENCE);assert(qrx_aura_tool_call_validate(&a,&s,&call)==0);
 assert(qrx_aura_chat_stream_advance(&m,1)==0);assert(qrx_aura_chat_stream_advance(&m,1)!=0);assert(qrx_aura_chat_add_artifact(&m,"qrxdrive:artifact-1")==0);assert(qrx_aura_chat_complete(&c,&m,275,"job:abc")==0);assert(c.spent_atoms==275);
 char h1[65],h2[65];assert(qrx_aura_chat_message_commitment(&m,h1)==0);assert(qrx_aura_chat_message_commitment(&m,h2)==0);assert(!strcmp(h1,h2));
 QrxAuraConversation b;assert(qrx_aura_chat_branch(&c,&m,"conv-branch",&b)==0);assert(!strcmp(b.conversation_id,"conv-branch"));assert(b.spent_atoms==0);
 QrxAuraChatMessage m3;assert(qrx_aura_chat_begin_assistant(&c,"m3","m2","qrxdrive:p2","qrxdrive:r2",300,&call,&m3)==0);assert(qrx_aura_chat_cancel(&m3)==0);
 puts("compute phase135 aura chat: PASS");return 0;
}
