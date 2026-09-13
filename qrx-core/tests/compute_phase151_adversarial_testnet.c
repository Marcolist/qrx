#include "compute/qrx_compute_adversarial.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void h(char x[65],char c){for(int i=0;i<64;i++)x[i]=c;x[64]=0;}
static QrxComputeAdversarialEvidence base(QrxComputeAdversarialAttack a){QrxComputeAdversarialEvidence e;memset(&e,0,sizeof(e));e.version=QRX_COMPUTE_ADVERSARIAL_VERSION;e.attack=a;snprintf(e.scenario_id,sizeof(e.scenario_id),"scenario-%u",(unsigned)a);snprintf(e.provider_id,sizeof(e.provider_id),"wallet:qrx:test-provider");return e;}
int main(void){QrxComputeAdversarialEvidence e;QrxComputeAdversarialVerdict v;char c1[65],c2[65];
 e=base(QRX_ADV_FAKE_COMPUTE);e.claimed_compute_atoms=1000;e.measured_compute_atoms=100;assert(!qrx_compute_adversarial_evaluate(&e,&v)&&v.detected&&!v.payout_allowed&&(v.action_mask&QRX_ADV_ACTION_SLASH_CANDIDATE)&&!v.consensus_slash_applied);
 assert(!qrx_compute_adversarial_evidence_commitment(&e,c1)&&!qrx_compute_adversarial_evidence_commitment(&e,c2)&&!strcmp(c1,c2));
 e=base(QRX_ADV_RESULT_MISMATCH);h(e.expected_result_commitment,'a');h(e.observed_result_commitment,'b');assert(!qrx_compute_adversarial_evaluate(&e,&v)&&v.detected&&v.risk_bps==10000&&v.block_new_work);
 e=base(QRX_ADV_MODEL_RUNTIME_MISMATCH);h(e.expected_model_commitment,'c');h(e.observed_model_commitment,'d');strcpy(e.expected_runtime,"cuda-sm61");strcpy(e.observed_runtime,"cpu-fallback");assert(!qrx_compute_adversarial_evaluate(&e,&v)&&v.detected&&!v.payout_allowed);
 e=base(QRX_ADV_VERIFIER_COLLUSION);e.verifier_count=6;e.matching_verifiers=6;e.distinct_operator_count=1;assert(!qrx_compute_adversarial_evaluate(&e,&v)&&v.detected&&(v.action_mask&QRX_ADV_ACTION_RETRY_REDUNDANT));
 e=base(QRX_ADV_SYBIL_PROVIDER_SET);e.distinct_provider_count=10;e.distinct_operator_count=1;assert(!qrx_compute_adversarial_evaluate(&e,&v)&&v.detected&&v.payout_allowed);
 e=base(QRX_ADV_CACHE_FRAUD);e.cache_hit_claim_bps=9000;e.cache_hit_verified_bps=2000;assert(!qrx_compute_adversarial_evaluate(&e,&v)&&v.detected&&!v.payout_allowed);
 e=base(QRX_ADV_SLOW_WORKER);e.timeout_bps=6500;assert(!qrx_compute_adversarial_evaluate(&e,&v)&&v.detected&&v.block_new_work);
 e=base(QRX_ADV_QUEUE_STARVATION);e.queue_wait_bps=7500;assert(!qrx_compute_adversarial_evaluate(&e,&v)&&v.detected&&(v.action_mask&QRX_ADV_ACTION_DRAIN_PROVIDER));
 e=base(QRX_ADV_WAN_PARTITION);e.unavailable_peer_bps=8000;assert(!qrx_compute_adversarial_evaluate(&e,&v)&&v.detected&&(v.action_mask&QRX_ADV_ACTION_REROUTE));
 e=base(QRX_ADV_POD_FAILURE);e.unavailable_pod_bps=9000;assert(!qrx_compute_adversarial_evaluate(&e,&v)&&v.detected&&(v.action_mask&QRX_ADV_ACTION_RETRY_REDUNDANT));
 e=base(QRX_ADV_FAKE_COMPUTE);e.claimed_compute_atoms=1000;e.measured_compute_atoms=990;assert(!qrx_compute_adversarial_evaluate(&e,&v)&&!v.detected&&v.payout_allowed);
 puts("compute_phase151_adversarial_testnet: ok");return 0;}
