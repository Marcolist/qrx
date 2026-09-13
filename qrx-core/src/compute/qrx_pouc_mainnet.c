#include "compute/qrx_pouc_mainnet.h"
#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>

static int nz(const char*s,size_t n){return s&&s[0]&&memchr(s,'\0',n+1)!=NULL;}
static int hex64(const char*s){if(!s||strlen(s)!=64)return 0;for(size_t i=0;i<64;i++){char c=s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;}return 1;}
static int upd(EVP_MD_CTX*x,const void*p,size_t n){uint64_t z=(uint64_t)n;return EVP_DigestUpdate(x,&z,sizeof(z))==1&&(!n||EVP_DigestUpdate(x,p,n)==1)?0:-1;}
static int digest_finish(EVP_MD_CTX*x,char out[65]){unsigned char h[32];unsigned int n=0;if(EVP_DigestFinal_ex(x,h,&n)!=1||n!=32)return -1;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[2*i]=hx[h[i]>>4];out[2*i+1]=hx[h[i]&15];}out[64]=0;return 0;}

int qrx_pouc_mainnet_readiness_validate(const QrxPoucMainnetReadiness*r){
 if(!r||r->version!=QRX_POUC_MAINNET_VERSION)return -1;
 return (r->verification_stable&&r->rewards_simulated&&r->fasttrack_fair&&r->standard_tasks_protected&&r->fasttrack_dev_share_transparent&&r->model_integrity_verified&&r->provider_fraud_controlled&&r->resource_globe_privacy_checked&&r->server_independence_verified)?0:-2;
}
int qrx_pouc_chain_binding_validate(const QrxPoucChainBinding*b){
 if(!b||b->version!=QRX_POUC_MAINNET_VERSION||!nz(b->chain_id,QRX_POUC_MAINNET_CHAIN_ID_MAX)||!hex64(b->genesis_hash)||!nz(b->protocol_version,QRX_POUC_MAINNET_PROTOCOL_MAX))return -1;return 0;
}
static int replay_key(const QrxPoucSettlementRequest*r,const char*receipt,char out[65]){
 EVP_MD_CTX*x=EVP_MD_CTX_new();if(!x)return -1;int ok=EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)==1;const char dom[]="QRX/POUC/SETTLEMENT-REPLAY/V1";
#define U(p,n) do{if(ok&&upd(x,(p),(n)))ok=0;}while(0)
 U(dom,sizeof(dom)-1);U(r->chain.chain_id,strlen(r->chain.chain_id));U(r->chain.genesis_hash,64);U(r->chain.protocol_version,strlen(r->chain.protocol_version));U(receipt,64);U(&r->settlement_nonce,sizeof(r->settlement_nonce));
#undef U
 int rc=ok?digest_finish(x,out):-1;EVP_MD_CTX_free(x);return rc;
}
static int decision_commit(const QrxPoucSettlementRequest*r,const QrxPoucSettlementDecision*d,char out[65]){
 EVP_MD_CTX*x=EVP_MD_CTX_new();if(!x)return -1;int ok=EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)==1;const char dom[]="QRX/POUC/MAINNET-SETTLEMENT/V1";
#define U(p,n) do{if(ok&&upd(x,(p),(n)))ok=0;}while(0)
 U(dom,sizeof(dom)-1);U(r->chain.chain_id,strlen(r->chain.chain_id));U(r->chain.genesis_hash,64);U(r->chain.protocol_version,strlen(r->chain.protocol_version));U(d->receipt_commitment,64);U(d->replay_key,64);U(&d->outcome,sizeof(d->outcome));U(&d->activation_ready,sizeof(d->activation_ready));U(&d->payout_allowed,sizeof(d->payout_allowed));U(&d->slash_candidate,sizeof(d->slash_candidate));U(&d->jail_candidate,sizeof(d->jail_candidate));U(&d->payout_compute_atoms,sizeof(d->payout_compute_atoms));U(&d->refund_atoms,sizeof(d->refund_atoms));U(&r->current_height,sizeof(r->current_height));
#undef U
 int rc=ok?digest_finish(x,out):-1;EVP_MD_CTX_free(x);return rc;
}
int qrx_pouc_mainnet_decide(const QrxPoucSettlementRequest*r,const QrxComputeEscrow*e,QrxPoucSettlementDecision*d){
 if(!r||!e||!d||r->version!=QRX_POUC_MAINNET_VERSION||!r->settlement_nonce||!r->current_height)return -1;
 if(qrx_pouc_chain_binding_validate(&r->chain)||qrx_pouc_mainnet_readiness_validate(&r->readiness))return -2;
 if(qrx_pouc_receipt_validate(&r->receipt)||e->state!=QRX_COMPUTE_ESCROW_ASSIGNED||strcmp(e->graph_commitment,r->receipt.graph_commitment))return -3;
 memset(d,0,sizeof(*d));d->version=QRX_POUC_MAINNET_VERSION;d->activation_ready=1;
 if(qrx_pouc_receipt_commitment(&r->receipt,d->receipt_commitment))return -4;
 if(strcmp(d->receipt_commitment,r->verification.receipt_commitment)||r->verification.matching_verifiers>r->verification.verifier_count)return -5;
 if(replay_key(r,d->receipt_commitment,d->replay_key))return -6;
 uint8_t reject=0,freeze=0,slash=0,jail=0;
 if(r->has_adversarial_verdict){
   const QrxComputeAdversarialVerdict*v=&r->adversarial;
   if(v->version!=QRX_COMPUTE_ADVERSARIAL_VERSION)return -7;
   if(v->detected&&(v->action_mask&QRX_ADV_ACTION_REJECT_RESULT))reject=1;
   if(v->detected&&(v->action_mask&QRX_ADV_ACTION_FREEZE_PAYOUT))freeze=1;
   if(v->detected&&(v->action_mask&QRX_ADV_ACTION_SLASH_CANDIDATE)){slash=1;jail=1;}
 }
 if(!r->verification.accepted)reject=1;
 if(r->verification.challenge_required){if(!r->challenge_finalized)freeze=1;else if(!r->challenge_passed)reject=1;}
 if(e->verification_reward_spent_atoms>e->verification_reward_atoms||e->verification_reward_spent_atoms>e->locked_atoms)return -8;
 if(reject){d->outcome=QRX_POUC_SETTLEMENT_REJECTED;d->payout_allowed=0;d->slash_candidate=slash;d->jail_candidate=jail;d->refund_atoms=e->locked_atoms-e->verification_reward_spent_atoms;}
 else if(freeze){d->outcome=QRX_POUC_SETTLEMENT_FROZEN;d->payout_allowed=0;d->slash_candidate=slash;d->jail_candidate=jail;}
 else {if(r->receipt.verified_compute_atoms>e->max_compute_atoms)return -8;d->outcome=QRX_POUC_SETTLEMENT_PAYOUT;d->payout_allowed=1;d->payout_compute_atoms=r->receipt.verified_compute_atoms;d->refund_atoms=(e->max_compute_atoms-r->receipt.verified_compute_atoms)+(e->verification_reward_atoms-e->verification_reward_spent_atoms);}
 if(decision_commit(r,d,d->decision_commitment))return -9;return 0;
}
int qrx_pouc_mainnet_apply_payout(const QrxPoucSettlementDecision*d,QrxComputeEscrow*e,uint64_t h){if(!d||!e||d->version!=QRX_POUC_MAINNET_VERSION||d->outcome!=QRX_POUC_SETTLEMENT_PAYOUT||!d->payout_allowed)return -1;return qrx_compute_escrow_settle(e,d->payout_compute_atoms,h);}
int qrx_pouc_replay_guard_init(QrxPoucReplayGuard*g){if(!g)return -1;memset(g,0,sizeof(*g));g->version=QRX_POUC_MAINNET_VERSION;return 0;}
int qrx_pouc_replay_guard_accept(QrxPoucReplayGuard*g,const QrxPoucSettlementDecision*d){if(!g||!d||g->version!=QRX_POUC_MAINNET_VERSION||d->version!=QRX_POUC_MAINNET_VERSION||!hex64(d->replay_key))return -1;for(uint32_t i=0;i<g->count;i++)if(!strcmp(g->replay_keys[i],d->replay_key))return -2;if(g->count>=QRX_POUC_MAINNET_REPLAY_SLOTS)return -3;snprintf(g->replay_keys[g->count++],65,"%s",d->replay_key);return 0;}
const char*qrx_pouc_settlement_outcome_name(QrxPoucSettlementOutcome o){switch(o){case QRX_POUC_SETTLEMENT_REJECTED:return "REJECTED";case QRX_POUC_SETTLEMENT_FROZEN:return "FROZEN";case QRX_POUC_SETTLEMENT_PAYOUT:return "PAYOUT";default:return "UNKNOWN";}}
