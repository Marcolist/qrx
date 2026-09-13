#include "compute/qrx_pouc_consensus.h"
#include "compute/qrx_pouc_verifier.h"
#include "compute/qrx_pouc_pipeline.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#define POOL_KEY "consensus:compute:escrow_pool"
#define READY_PREFIX "consensus:compute:readiness:"
#define CHALLENGE_PREFIX "consensus:compute:challenge:"
#define ADV_PREFIX "consensus:compute:adversarial:"
#define SLASH_PREFIX "consensus:compute:slash_candidate:"
#define JAIL_PREFIX "consensus:compute:jail_candidate:"

static int hex64(const char*s){if(!s||strlen(s)!=64)return 0;for(size_t i=0;i<64;i++){char c=s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;}return 1;}
static int text_safe(const char*s){return s&&s[0]&&!strchr(s,';')&&!strchr(s,'|')&&!strchr(s,'\n')&&!strchr(s,'\r');}
static int get_u64_strict(const char*s,uint64_t*out){if(!s||!*s||!out||*s=='-')return -1;errno=0;char*e=NULL;unsigned long long v=strtoull(s,&e,10);if(errno||!e||*e)return -1;*out=(uint64_t)v;return 0;}
static int get_u32_strict(const char*s,uint32_t*out){uint64_t v=0;if(get_u64_strict(s,&v)||v>UINT32_MAX)return -1;*out=(uint32_t)v;return 0;}
static char *field(const char*p,const char*k){if(!p||!k)return NULL;size_t kl=strlen(k);const char*s=p;while(*s){const char*e=strchr(s,';');size_t n=e?(size_t)(e-s):strlen(s);if(n>kl+1&&!strncmp(s,k,kl)&&s[kl]=='='){size_t v=n-kl-1;char*r=(char*)malloc(v+1);if(!r)return NULL;memcpy(r,s+kl+1,v);r[v]=0;return r;}if(!e)break;s=e+1;}return NULL;}
static int db_u64(QrxDB*db,const char*k,uint64_t*out){char b[128];if(!db||!k||!out||qrxdb_get(db,k,b,sizeof(b)))return -1;return get_u64_strict(b,out);}
static int batch_u64(QrxDBBatch*b,const char*k,uint64_t v){char x[64];snprintf(x,sizeof(x),"%llu",(unsigned long long)v);return qrxdb_batch_put(b,k,x);}

int qrx_pouc_consensus_stage_readiness(QrxDBBatch*b,const QrxPoucMainnetReadiness*r){
    if(!b||qrx_pouc_mainnet_readiness_validate(r))return -1;
    static const char*names[]={"verification_stable","rewards_simulated","fasttrack_fair","standard_tasks_protected","fasttrack_dev_share_transparent","model_integrity_verified","provider_fraud_controlled","resource_globe_privacy_checked","server_independence_verified"};
    for(size_t i=0;i<sizeof(names)/sizeof(names[0]);i++){char k[160];snprintf(k,sizeof(k),"%s%s",READY_PREFIX,names[i]);if(qrxdb_batch_put(b,k,"1"))return -2;}return 0;
}
int qrx_pouc_consensus_stage_challenge(QrxDBBatch*b,const char rc[65],uint32_t status){if(!b||!hex64(rc)||(status<QRX_POUC_CHALLENGE_PENDING||status>QRX_POUC_CHALLENGE_FAILED))return -1;char k[180];snprintf(k,sizeof(k),"%s%s",CHALLENGE_PREFIX,rc);const char*v=status==QRX_POUC_CHALLENGE_PENDING?"PENDING":status==QRX_POUC_CHALLENGE_PASSED?"PASSED":"FAILED";return qrxdb_batch_put(b,k,v);}

static int adv_encode(const QrxComputeAdversarialVerdict*v,char*out,size_t n){if(!v||!out||v->version!=QRX_COMPUTE_ADVERSARIAL_VERSION||!hex64(v->evidence_commitment))return -1;int z=snprintf(out,n,"%u|%u|%u|%u|%u|%u|%u|%u|%u|%u|%s",v->version,(unsigned)v->attack,(unsigned)v->severity,v->action_mask,v->risk_bps,v->challenge_bps,(unsigned)v->detected,(unsigned)v->block_new_work,(unsigned)v->payout_allowed,(unsigned)v->consensus_slash_applied,v->evidence_commitment);return z>0&&(size_t)z<n?0:-1;}
static int adv_decode(const char*s,QrxComputeAdversarialVerdict*v){if(!s||!v)return -1;memset(v,0,sizeof(*v));unsigned ver=0,a=0,se=0,de=0,bl=0,pa=0,sl=0;char ec[65]={0};if(sscanf(s,"%u|%u|%u|%u|%u|%u|%u|%u|%u|%u|%64s",&ver,&a,&se,&v->action_mask,&v->risk_bps,&v->challenge_bps,&de,&bl,&pa,&sl,ec)!=11||!hex64(ec))return -1;v->version=ver;v->attack=(QrxComputeAdversarialAttack)a;v->severity=(QrxComputeAdversarialSeverity)se;v->detected=(uint8_t)de;v->block_new_work=(uint8_t)bl;v->payout_allowed=(uint8_t)pa;v->consensus_slash_applied=(uint8_t)sl;snprintf(v->evidence_commitment,65,"%s",ec);return v->version==QRX_COMPUTE_ADVERSARIAL_VERSION?0:-1;}
int qrx_pouc_consensus_stage_adversarial(QrxDBBatch*b,const char rc[65],const QrxComputeAdversarialVerdict*v){if(!b||!hex64(rc)||!v)return -1;char k[180],x[512];if(adv_encode(v,x,sizeof(x)))return -2;snprintf(k,sizeof(k),"%s%s",ADV_PREFIX,rc);return qrxdb_batch_put(b,k,x);}
static int load_adversarial(QrxDB*db,const char rc[65],QrxComputeAdversarialVerdict*v){char k[180],x[512];snprintf(k,sizeof(k),"%s%s",ADV_PREFIX,rc);if(qrxdb_get(db,k,x,sizeof(x)))return 1;return adv_decode(x,v);}

int qrx_pouc_consensus_stage_escrow_lock(QrxDB*db,QrxDBBatch*b,const QrxComputeEscrow*e){
    if(!db||!b||!e||(e->state!=QRX_COMPUTE_ESCROW_LOCKED&&e->state!=QRX_COMPUTE_ESCROW_ASSIGNED))return -1;
    uint64_t pool=0;char x[128];if(qrxdb_get(db,POOL_KEY,x,sizeof(x))==0){if(get_u64_strict(x,&pool))return -2;}if(UINT64_MAX-pool<e->locked_atoms)return -3;
    if(qrx_pouc_journal_stage_escrow(b,e)||batch_u64(b,POOL_KEY,pool+e->locked_atoms))return -4;return 0;
}

static int receipt_from_payload(const char*p,QrxPoucReceipt*r,uint32_t*vc,uint32_t*mv,uint64_t*nonce){
    if(!p||!r||!vc||!mv||!nonce)return -1;memset(r,0,sizeof(*r));r->version=QRX_POUC_VERSION;
    char *graph=field(p,"graph_commitment"),*node=field(p,"node_id"),*provider=field(p,"provider_id"),*runtime=field(p,"runtime_id"),*input=field(p,"input_commitment"),*model=field(p,"model_commitment"),*params=field(p,"execution_params_commitment"),*result=field(p,"result_commitment"),*atoms=field(p,"verified_compute_atoms"),*started=field(p,"started_height"),*completed=field(p,"completed_height"),*mode=field(p,"verification_mode"),*verifiers=field(p,"verifier_count"),*matching=field(p,"matching_verifiers"),*sn=field(p,"settlement_nonce");
    int ok=graph&&node&&provider&&runtime&&input&&model&&params&&result&&atoms&&started&&completed&&mode&&verifiers&&matching&&sn;
    uint32_t nd=0,md=0;if(ok)ok=hex64(graph)&&hex64(input)&&hex64(model)&&hex64(params)&&hex64(result)&&text_safe(provider)&&text_safe(runtime)&&!get_u32_strict(node,&nd)&&!get_u64_strict(atoms,&r->verified_compute_atoms)&&!get_u64_strict(started,&r->started_height)&&!get_u64_strict(completed,&r->completed_height)&&!get_u32_strict(mode,&md)&&!get_u32_strict(verifiers,vc)&&!get_u32_strict(matching,mv)&&!get_u64_strict(sn,nonce)&&*nonce>0;
    if(ok){r->node_id=nd;r->verification_mode=md;snprintf(r->graph_commitment,65,"%s",graph);snprintf(r->provider_id,sizeof(r->provider_id),"%s",provider);snprintf(r->runtime_id,sizeof(r->runtime_id),"%s",runtime);snprintf(r->input_commitment,65,"%s",input);snprintf(r->model_commitment,65,"%s",model);snprintf(r->execution_params_commitment,65,"%s",params);snprintf(r->result_commitment,65,"%s",result);ok=qrx_pouc_receipt_validate(r)==0;}
    free(graph);free(node);free(provider);free(runtime);free(input);free(model);free(params);free(result);free(atoms);free(started);free(completed);free(mode);free(verifiers);free(matching);free(sn);return ok?0:-2;
}

static int load_challenge(QrxDB*db,const char rc[65],QrxPoucSettlementRequest*r){
    if(!r->verification.challenge_required){r->challenge_finalized=1;r->challenge_passed=1;return 0;}char k[180],v[32];snprintf(k,sizeof(k),"%s%s",CHALLENGE_PREFIX,rc);if(qrxdb_get(db,k,v,sizeof(v))){r->challenge_finalized=0;r->challenge_passed=0;return 0;}if(!strcmp(v,"PENDING")){r->challenge_finalized=0;r->challenge_passed=0;return 0;}if(!strcmp(v,"PASSED")){r->challenge_finalized=1;r->challenge_passed=1;return 0;}if(!strcmp(v,"FAILED")){r->challenge_finalized=1;r->challenge_passed=0;return 0;}return -1;
}

static int transition_copy(const QrxPoucSettlementRequest*r,const QrxPoucSettlementDecision*d,const QrxComputeEscrow*before,QrxComputeEscrow*after){*after=*before;if(d->outcome==QRX_POUC_SETTLEMENT_PAYOUT)return qrx_pouc_mainnet_apply_payout(d,after,r->current_height);if(d->outcome==QRX_POUC_SETTLEMENT_REJECTED)return qrx_compute_escrow_refund(after,r->current_height);return d->outcome==QRX_POUC_SETTLEMENT_FROZEN?0:-1;}

int qrx_pouc_consensus_prepare(QrxDB*db,const char*chain_id,const char*genesis_hash,const char*protocol_version,const char*tx_type,const char*from,const char*to,uint64_t amount,const char*payload,int activation_ready,uint64_t h,QrxPoucConsensusEffect*out){
    (void)from;if(!db||!out||!tx_type||strcmp(tx_type,"POUC_SETTLEMENT")||amount!=0||!chain_id||!genesis_hash||!protocol_version||!payload||!to||!h||!activation_ready)return -1;memset(out,0,sizeof(*out));out->version=QRX_POUC_CONSENSUS_VERSION;
    QrxPoucSettlementRequest*r=&out->request;r->version=QRX_POUC_MAINNET_VERSION;r->chain.version=QRX_POUC_MAINNET_VERSION;snprintf(r->chain.chain_id,sizeof(r->chain.chain_id),"%s",chain_id);snprintf(r->chain.genesis_hash,sizeof(r->chain.genesis_hash),"%s",genesis_hash);snprintf(r->chain.protocol_version,sizeof(r->chain.protocol_version),"%s",protocol_version);if(qrx_pouc_chain_binding_validate(&r->chain))return -2;
    /* A threshold-signed COMPUTE_POUC_V1 protocol activation is the canonical
       attestation that the 0.0.9.27 readiness predicates were satisfied. */
    memset(&r->readiness,1,sizeof(r->readiness));r->readiness.version=QRX_POUC_MAINNET_VERSION;
    uint32_t vc_tx=0,mv_tx=0;if(receipt_from_payload(payload,&r->receipt,&vc_tx,&mv_tx,&r->settlement_nonce))return -3;if(strcmp(to,r->receipt.provider_id))return -4;if(qrx_pouc_journal_get_escrow(db,r->receipt.graph_commitment,&out->escrow_before))return -5;if(out->escrow_before.state!=QRX_COMPUTE_ESCROW_ASSIGNED||strcmp(out->escrow_before.graph_commitment,r->receipt.graph_commitment))return -6;
    /* 0.0.9.30: any escrow created through the public Compute pipeline must
       settle against its canonical assignment, receipt and verifier aggregate.
       A missing assignment is accepted only as legacy pre-0.0.9.30 state: no
       public post-activation transaction can create an ASSIGNED escrow without
       COMPUTE_ASSIGN, so this preserves upgrade compatibility without opening a
       new bypass. */
    QrxPoucAssignment asg;int asgrc=qrx_pouc_pipeline_get_assignment(db,r->receipt.graph_commitment,&asg);
    if(asgrc==0){
        if(strcmp(asg.owner,out->escrow_before.owner)||strcmp(asg.provider,r->receipt.provider_id)||asg.node_id!=r->receipt.node_id||asg.verification_mode!=r->receipt.verification_mode||strcmp(asg.runtime_id,r->receipt.runtime_id)||strcmp(asg.model_commitment,r->receipt.model_commitment))return -8;
        char canonical_rc[65];if(qrx_pouc_pipeline_get_receipt_for_graph(db,r->receipt.graph_commitment,canonical_rc))return -9;QrxPoucReceipt canonical_receipt;if(qrx_pouc_pipeline_get_receipt(db,canonical_rc,&canonical_receipt))return -10;char supplied_rc[65];if(qrx_pouc_receipt_commitment(&r->receipt,supplied_rc)||strcmp(supplied_rc,canonical_rc))return -11;
        QrxPoucVerificationAggregate agg;if(qrx_pouc_pipeline_get_verification(db,canonical_rc,&agg)||!agg.finalized||agg.verifier_count!=agg.target_verifiers)return -12;if(vc_tx!=agg.verifier_count||mv_tx!=agg.matching_verifiers)return -13;
        r->receipt=canonical_receipt;if(qrx_pouc_verify_result(&r->receipt,agg.verifier_count,agg.matching_verifiers,&r->verification))return -14;
    }else if(asgrc==-2){
        if(qrx_pouc_verify_result(&r->receipt,vc_tx,mv_tx,&r->verification))return -14;
    }else return -7;
    if(load_challenge(db,r->verification.receipt_commitment,r))return -15;QrxComputeAdversarialVerdict av;int arc=load_adversarial(db,r->verification.receipt_commitment,&av);if(arc<0)return -16;if(arc==0){r->has_adversarial_verdict=1;r->adversarial=av;}r->current_height=h;
    if(qrx_pouc_mainnet_decide(r,&out->escrow_before,&out->decision))return -10;if(transition_copy(r,&out->decision,&out->escrow_before,&out->escrow_after))return -11;
    snprintf(out->provider,sizeof(out->provider),"%s",r->receipt.provider_id);snprintf(out->owner,sizeof(out->owner),"%s",out->escrow_before.owner);
    if(out->escrow_before.verification_reward_spent_atoms>out->escrow_before.locked_atoms)return -12;
    uint64_t remaining_locked=out->escrow_before.locked_atoms-out->escrow_before.verification_reward_spent_atoms;
    if(out->decision.outcome==QRX_POUC_SETTLEMENT_PAYOUT){
        if(out->escrow_after.fasttrack_dev_atoms>out->escrow_after.fasttrack_fee_atoms ||
           out->escrow_after.fasttrack_net_atoms>out->escrow_after.fasttrack_fee_atoms-out->escrow_after.fasttrack_dev_atoms)return -13;
        uint64_t fasttrack_provider=out->escrow_after.fasttrack_fee_atoms-out->escrow_after.fasttrack_dev_atoms-out->escrow_after.fasttrack_net_atoms;
        if(UINT64_MAX-out->escrow_after.actual_compute_atoms<fasttrack_provider)return -13;
        out->provider_credit_atoms=out->escrow_after.actual_compute_atoms+fasttrack_provider;
        out->owner_credit_atoms=out->escrow_after.refund_atoms;out->development_credit_atoms=out->escrow_after.fasttrack_dev_atoms;out->network_credit_atoms=out->escrow_after.fasttrack_net_atoms;out->escrow_pool_debit_atoms=remaining_locked;
    }
    else if(out->decision.outcome==QRX_POUC_SETTLEMENT_REJECTED){out->owner_credit_atoms=out->escrow_after.refund_atoms;out->escrow_pool_debit_atoms=remaining_locked;}
    if(out->escrow_pool_debit_atoms){uint64_t pool=0;if(db_u64(db,POOL_KEY,&pool)||pool<out->escrow_pool_debit_atoms)return -12;}
    if(out->provider_credit_atoms>LLONG_MAX||out->owner_credit_atoms>LLONG_MAX||out->development_credit_atoms>LLONG_MAX||out->network_credit_atoms>LLONG_MAX||out->escrow_pool_debit_atoms>LLONG_MAX)return -13;return 0;
}

int qrx_pouc_consensus_stage(QrxDB*db,QrxDBBatch*b,const QrxPoucConsensusEffect*e,const char*txid,uint64_t h){
    if(!db||!b||!e||e->version!=QRX_POUC_CONSENSUS_VERSION||!txid||!*txid||h!=e->request.current_height)return -1;QrxComputeEscrow after;if(qrx_pouc_journal_stage(db,b,&e->request,&e->decision,&e->escrow_before,&after))return -2;if(memcmp(&after,&e->escrow_after,sizeof(after)))return -3;
    if(e->escrow_pool_debit_atoms){uint64_t pool=0;if(db_u64(db,POOL_KEY,&pool)||pool<e->escrow_pool_debit_atoms)return -4;if(batch_u64(b,POOL_KEY,pool-e->escrow_pool_debit_atoms))return -5;}
    if(e->decision.slash_candidate||e->decision.jail_candidate){uint64_t slashed=0,jail_until=0;if(qrx_pouc_stage_consensus_penalty(db,b,e->provider,e->decision.receipt_commitment,h,e->decision.slash_candidate,e->decision.jail_candidate,&slashed,&jail_until))return -6;char val[896];snprintf(val,sizeof(val),"height=%llu|provider=%s|receipt=%s|decision=%s|replay=%s|txid=%s|slash=%u|jail=%u|slashed_atoms=%llu|jail_until=%llu",(unsigned long long)h,e->provider,e->decision.receipt_commitment,e->decision.decision_commitment,e->decision.replay_key,txid,(unsigned)e->decision.slash_candidate,(unsigned)e->decision.jail_candidate,(unsigned long long)slashed,(unsigned long long)jail_until);if(e->decision.slash_candidate){char k[512];snprintf(k,sizeof(k),"%s%s:%s",SLASH_PREFIX,e->provider,e->decision.replay_key);if(qrxdb_batch_put(b,k,val))return -7;}if(e->decision.jail_candidate){char k[512];snprintf(k,sizeof(k),"%s%s:%s",JAIL_PREFIX,e->provider,e->decision.replay_key);if(qrxdb_batch_put(b,k,val))return -8;}}
    return 0;
}
