#include "compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static QrxAuraAgentManifest manifest(void) {
    QrxAuraAgentManifest m; memset(&m,0,sizeof(m));
    m.version=QRX_AURA_RUNTIME_VERSION;
    strcpy(m.agent_id,"aura.default"); strcpy(m.owner,"qrx1owner");
    strcpy(m.model_id,"qrx/kimi-k3"); strcpy(m.model_version,"0.0.9-target");
    m.capability_mask=QRX_AURA_CAP_CHAT|QRX_AURA_CAP_MODEL_INFERENCE|QRX_AURA_CAP_READ_ARTIFACT|QRX_AURA_CAP_WRITE_ARTIFACT|QRX_AURA_CAP_SUBMIT_JOB|QRX_AURA_CAP_STREAM_RESULT|QRX_AURA_CAP_RESEARCH|QRX_AURA_CAP_CODE;
    m.max_steps=32; m.max_job_fee_atoms=400000000ULL; m.max_session_fee_atoms=1000000000ULL;
    m.require_user_approval_for_code_execution=1; return m;
}
static QrxAuraToolCall call(QrxAuraToolType t) {
    QrxAuraToolCall c; memset(&c,0,sizeof(c)); c.version=QRX_AURA_RUNTIME_VERSION;
    strcpy(c.call_id,"call-1"); strcpy(c.session_id,"session-1"); strcpy(c.input_ref,"drive:input:abc"); strcpy(c.output_ref,"drive:output:def"); strcpy(c.runtime_id,"qrx-ai-runtime-v1");
    c.tool_type=t; c.requested_fee_atoms=100000000ULL; c.max_runtime_ms=60000; c.max_output_bytes=4*1024*1024; return c;
}
int main(void) {
    QrxAuraAgentManifest m=manifest(); assert(qrx_aura_agent_manifest_validate(&m)==0);
    char h1[65],h2[65]; assert(qrx_aura_agent_manifest_commitment(&m,h1)==0); assert(qrx_aura_agent_manifest_commitment(&m,h2)==0); assert(strcmp(h1,h2)==0);
    QrxAuraSession s; assert(qrx_aura_session_open(&m,"session-1",500000000ULL,&s)==0); assert(s.state==QRX_AURA_SESSION_OPEN);
    QrxAuraToolCall c=call(QRX_AURA_TOOL_MODEL_INFERENCE); assert(qrx_aura_tool_call_validate(&m,&s,&c)==0);
    QrxComputeJobNode n; assert(qrx_aura_tool_call_to_job_node(&m,&s,&c,1,&n)==0); assert(n.job_type==QRX_JOB_AI_INFERENCE); assert(n.capability_mask&QRX_JOB_CAP_MODEL_INFERENCE); assert(strcmp(n.model_id,m.model_id)==0);
    QrxAuraToolCall exec=call(QRX_AURA_TOOL_CODE_EXECUTION); strcpy(exec.call_id,"call-exec"); assert(qrx_aura_tool_call_validate(&m,&s,&exec)==-4); exec.user_approved=1; assert(qrx_aura_tool_call_validate(&m,&s,&exec)==0); assert(qrx_aura_tool_call_to_job_node(&m,&s,&exec,2,&n)==0); assert(n.capability_mask&QRX_JOB_CAP_SANDBOX_EXEC);
    QrxComputeSandboxPolicy p; assert(qrx_compute_sandbox_policy_default(&n,&p)==0); assert(p.wallet_keys_visible==0 && p.host_secrets_visible==0 && p.host_fs_visible==0 && p.unrestricted_network==0);
    assert(qrx_aura_session_record_step(&m,&s)==0 && s.step_count==1);
    assert(qrx_aura_session_charge(&s,400000000ULL)==0); assert(s.state==QRX_AURA_SESSION_BUDGET_WARNING);
    c.requested_fee_atoms=200000000ULL; assert(qrx_aura_tool_call_validate(&m,&s,&c)==-2); /* remaining budget only 0.1 QUB-equivalent atoms */
    assert(qrx_aura_session_charge(&s,500000000ULL)==0); assert(s.state==QRX_AURA_SESSION_PAUSED); assert(qrx_aura_session_complete(&s)!=0); assert(qrx_aura_session_cancel(&s)==0);
    QrxAuraAgentManifest weak=m; weak.capability_mask&=~QRX_AURA_CAP_CODE; QrxAuraSession sw; assert(qrx_aura_session_open(&weak,"session-1",500000000ULL,&sw)==0); assert(qrx_aura_tool_call_validate(&weak,&sw,&exec)==-3);
    puts("compute_phase134_aura_runtime: PASS"); return 0;
}
