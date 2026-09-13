#include "compute/qrx_pouc_journal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void h(char*x,char c){for(int i=0;i<64;i++)x[i]=c;x[64]=0;}
static QrxPoucMainnetReadiness ready(void){QrxPoucMainnetReadiness r={0};r.version=1;r.verification_stable=r.rewards_simulated=r.fasttrack_fair=r.standard_tasks_protected=r.fasttrack_dev_share_transparent=r.model_integrity_verified=r.provider_fraud_controlled=r.resource_globe_privacy_checked=r.server_independence_verified=1;return r;}
static void setup_receipt(QrxPoucReceipt*r,const char*gc,uint32_t mode){memset(r,0,sizeof(*r));r->version=1;r->node_id=7;snprintf(r->provider_id,sizeof(r->provider_id),"provider-phase153");snprintf(r->runtime_id,sizeof(r->runtime_id),"qrx-ai-v1");snprintf(r->graph_commitment,sizeof(r->graph_commitment),"%s",gc);h(r->input_commitment,'b');h(r->model_commitment,'c');h(r->execution_params_commitment,'d');h(r->result_commitment,'e');r->verified_compute_atoms=270000000;r->started_height=100;r->completed_height=110;r->verification_mode=mode;}
static int setup_graph(QrxComputeJobGraph*g){memset(g,0,sizeof(*g));g->protocol_version=QRX_COMPUTE_JOB_PROTOCOL_VERSION;snprintf(g->graph_id,sizeof(g->graph_id),"phase153-job");snprintf(g->owner,sizeof(g->owner),"qrx-owner");g->nonce=153;g->max_total_fee_atoms=400000000;g->expiry_height=5000;g->node_count=1;QrxComputeJobNode*n=&g->nodes[0];n->node_id=1;n->job_type=QRX_JOB_AI_INFERENCE;snprintf(n->runtime_id,sizeof(n->runtime_id),"qrx-ai-v1");snprintf(n->model_id,sizeof(n->model_id),"qrx/model");snprintf(n->model_version,sizeof(n->model_version),"1");snprintf(n->input_ref,sizeof(n->input_ref),"drive:prompt");snprintf(n->output_ref,sizeof(n->output_ref),"drive:answer");n->min_memory_bytes=1024;n->max_runtime_ms=60000;n->max_output_bytes=1024;n->max_fee_atoms=400000000;n->capability_mask=QRX_JOB_CAP_READ_INPUTS|QRX_JOB_CAP_MODEL_INFERENCE|QRX_JOB_CAP_STREAM_OUTPUT;return qrx_compute_job_graph_validate(g);}
int main(void){
 char tmp[]="/tmp/qrx-pouc-journal-XXXXXX";if(!mkdtemp(tmp))return 1;
 QrxDB db;if(qrxdb_init(&db,tmp))return 2;
 QrxComputeJobGraph g;if(setup_graph(&g))return 3;QrxComputeEscrow e;if(qrx_compute_escrow_lock(&e,&g,0,10)||qrx_compute_escrow_mark_assigned(&e))return 4;
 QrxPoucSettlementRequest r;memset(&r,0,sizeof(r));r.version=1;r.chain.version=1;snprintf(r.chain.chain_id,sizeof(r.chain.chain_id),"qrx-mainnet");h(r.chain.genesis_hash,'2');snprintf(r.chain.protocol_version,sizeof(r.chain.protocol_version),"9");r.readiness=ready();setup_receipt(&r.receipt,e.graph_commitment,QRX_POUC_VERIFY_SPOT);if(qrx_pouc_verify_result(&r.receipt,1,1,&r.verification))return 5;r.settlement_nonce=77;r.current_height=120;
 QrxPoucSettlementDecision d;if(qrx_pouc_mainnet_decide(&r,&e,&d)||d.outcome!=QRX_POUC_SETTLEMENT_FROZEN)return 6;
 if(qrx_pouc_journal_apply(&db,&r,&d,&e))return 7;if(qrx_pouc_journal_replay_consumed(&db,d.replay_key)!=1)return 8;
 QrxPoucJournalEntry je;if(qrx_pouc_journal_get(&db,d.replay_key,&je)||je.state!=QRX_POUC_JOURNAL_FROZEN)return 9;char replay[65];snprintf(replay,sizeof(replay),"%s",d.replay_key);
 if(qrxdb_close(&db)||qrxdb_init(&db,tmp))return 10;if(qrx_pouc_journal_get(&db,replay,&je)||je.state!=QRX_POUC_JOURNAL_FROZEN)return 11;
 QrxComputeEscrow persisted;if(qrx_pouc_journal_get_escrow(&db,e.graph_commitment,&persisted)||persisted.state!=QRX_COMPUTE_ESCROW_ASSIGNED)return 12;e=persisted;
 r.challenge_finalized=1;r.challenge_passed=1;r.current_height=125;if(qrx_pouc_mainnet_decide(&r,&e,&d)||strcmp(d.replay_key,replay)||d.outcome!=QRX_POUC_SETTLEMENT_PAYOUT)return 13;
 if(qrx_pouc_journal_apply(&db,&r,&d,&e)||e.state!=QRX_COMPUTE_ESCROW_SETTLED||e.actual_compute_atoms!=270000000)return 14;
 if(qrx_pouc_journal_replay_consumed(&db,replay)!=1)return 15;
 QrxComputeEscrow dup;if(qrx_compute_escrow_lock(&dup,&g,0,10)||qrx_compute_escrow_mark_assigned(&dup))return 16;if(qrx_pouc_journal_apply(&db,&r,&d,&dup)!=-3)return 17;
 if(qrxdb_close(&db)||qrxdb_init(&db,tmp))return 18;if(qrx_pouc_journal_get(&db,replay,&je)||je.state!=QRX_POUC_JOURNAL_PAYOUT)return 19;if(qrx_pouc_journal_get_escrow(&db,e.graph_commitment,&persisted)||persisted.state!=QRX_COMPUTE_ESCROW_SETTLED)return 20;
 uint32_t reverted=0;if(qrx_pouc_journal_revert_above_height(&db,122,&reverted)||reverted!=1)return 21;if(qrx_pouc_journal_get(&db,replay,&je)||je.state!=QRX_POUC_JOURNAL_FROZEN||qrx_pouc_journal_replay_consumed(&db,replay)!=1)return 22;if(qrx_pouc_journal_get_escrow(&db,e.graph_commitment,&persisted)||persisted.state!=QRX_COMPUTE_ESCROW_ASSIGNED)return 23;
 if(qrx_pouc_journal_revert_above_height(&db,110,&reverted)||reverted!=1)return 24;if(qrx_pouc_journal_get(&db,replay,&je)||je.state!=QRX_POUC_JOURNAL_REVERTED||qrx_pouc_journal_replay_consumed(&db,replay)!=0)return 25;
 e=persisted;r.current_height=126;if(qrx_pouc_mainnet_decide(&r,&e,&d)||strcmp(d.replay_key,replay))return 26;if(qrx_pouc_journal_apply(&db,&r,&d,&e)||e.state!=QRX_COMPUTE_ESCROW_SETTLED)return 27;
 if(qrxdb_close(&db))return 28;char cmd[1200];snprintf(cmd,sizeof(cmd),"rm -rf '%s'",tmp);system(cmd);puts("compute phase 153 durable PoUC journal: PASS");return 0;
}
