#include "compute/qrx_pouc_reorg.h"
#include "compute/qrx_pouc_liveness.h"
#include "compute/qrx_pouc_journal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void hx(char x[65],char c){for(int i=0;i<64;i++)x[i]=c;x[64]=0;}
static uint64_t getu(QrxDB*d,const char*k){char b[128];assert(!qrxdb_get(d,k,b,sizeof(b)));return strtoull(b,NULL,10);}
static void putu(QrxDB*d,const char*k,uint64_t v){char b[64];snprintf(b,sizeof(b),"%llu",(unsigned long long)v);assert(!qrxdb_put(d,k,b));}
static void stage_reward(QrxDB*d,QrxComputeEscrow*esc,uint64_t reward,uint64_t h,const char*txid,uint8_t role){
    QrxPoucPipelineEffect e;memset(&e,0,sizeof(e));e.version=1;e.op=role==1?QRX_POUC_PIPELINE_VERIFY:QRX_POUC_PIPELINE_CHALLENGE;e.reward_credit_atoms=reward;e.reward_role=role;snprintf(e.verifier,sizeof(e.verifier),"validator-A");hx(e.receipt_commitment,'a');e.escrow_before=*esc;e.escrow_after=*esc;e.escrow_after.verification_reward_spent_atoms+=reward;
    uint64_t bal=getu(d,"acct:balance:validator-A"),pool=getu(d,"consensus:compute:escrow_pool");assert(pool>=reward);
    QrxDBBatch b;assert(!qrxdb_batch_begin(d,&b));char n[64];snprintf(n,sizeof(n),"%llu",(unsigned long long)(bal+reward));assert(!qrxdb_batch_put(&b,"acct:balance:validator-A",n));snprintf(n,sizeof(n),"%llu",(unsigned long long)(pool-reward));assert(!qrxdb_batch_put(&b,"consensus:compute:escrow_pool",n));assert(!qrx_pouc_journal_stage_escrow(&b,&e.escrow_after));assert(!qrx_pouc_reward_journal_stage(d,&b,&e,txid,h));assert(!qrxdb_batch_commit(&b));*esc=e.escrow_after;
}
int main(void){
    char dir[]="/tmp/qrx-pouc-158-XXXXXX";assert(mkdtemp(dir));QrxDB db;assert(!qrxdb_init(&db,dir));
    QrxPoucLivenessParams p={QRX_POUC_LIVENESS_PARAMS_VERSION,3,144};assert(!qrx_pouc_liveness_params_validate(&p));QrxDBBatch pb;assert(!qrxdb_batch_begin(&db,&pb));assert(!qrx_pouc_liveness_params_stage(&pb,&p));assert(!qrxdb_batch_commit(&pb));QrxPoucLivenessParams got;assert(!qrx_pouc_liveness_params_get(&db,&got));assert(got.verifier_miss_threshold==3&&got.verifier_miss_jail_blocks==144);QrxPoucLivenessParams bad={QRX_POUC_LIVENESS_PARAMS_VERSION,0,1};assert(qrx_pouc_liveness_params_validate(&bad)!=0);
    putu(&db,"acct:balance:validator-A",1000);putu(&db,"consensus:compute:escrow_pool",10000);
    QrxComputeEscrow esc;memset(&esc,0,sizeof(esc));esc.market_version=QRX_COMPUTE_MARKET_VERSION;hx(esc.graph_commitment,'b');snprintf(esc.owner,sizeof(esc.owner),"owner-A");esc.max_compute_atoms=5000;esc.verification_reward_atoms=100;esc.locked_atoms=5100;esc.state=QRX_COMPUTE_ESCROW_ASSIGNED;QrxDBBatch eb;assert(!qrxdb_batch_begin(&db,&eb));assert(!qrx_pouc_journal_stage_escrow(&eb,&esc));assert(!qrxdb_batch_commit(&eb));
    stage_reward(&db,&esc,20,20,"reward-tx-1",1);stage_reward(&db,&esc,10,21,"reward-tx-2",2);assert(getu(&db,"acct:balance:validator-A")==1030);assert(getu(&db,"consensus:compute:escrow_pool")==9970);QrxComputeEscrow pe;assert(!qrx_pouc_journal_get_escrow(&db,esc.graph_commitment,&pe)&&pe.verification_reward_spent_atoms==30);
    assert(!qrxdb_close(&db));assert(!qrxdb_init(&db,dir));QrxPoucCanonicalReorgReport rr;assert(!qrx_pouc_canonical_reorg_hook(&db,19,&rr));assert(rr.reward_reverted==2&&rr.settlement_reverted==0);assert(getu(&db,"acct:balance:validator-A")==1000);assert(getu(&db,"consensus:compute:escrow_pool")==10000);assert(!qrx_pouc_journal_get_escrow(&db,esc.graph_commitment,&pe)&&pe.verification_reward_spent_atoms==0);
    QrxPoucCanonicalReorgReport rr2;assert(!qrx_pouc_canonical_reorg_hook(&db,19,&rr2)&&rr2.reward_reverted==0);
    assert(!qrxdb_close(&db));char cmd[1200];snprintf(cmd,sizeof(cmd),"rm -rf '%s'",dir);system(cmd);puts("compute phase 158 canonical reorg/reward journal/liveness governance: PASS");return 0;
}
