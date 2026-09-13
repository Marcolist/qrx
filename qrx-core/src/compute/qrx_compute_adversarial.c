#include "compute/qrx_compute_adversarial.h"
#include <openssl/evp.h>
#include <string.h>

static int nz(const char*s,size_t n){return s&&s[0]&&memchr(s,'\0',n+1)!=NULL;}
static int hex64_or_empty(const char*s){if(!s||!s[0])return 1;if(strlen(s)!=64)return 0;for(size_t i=0;i<64;i++){char c=s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;}return 1;}
static int bps(uint32_t v){return v<=10000u;}
static uint32_t clamp_bps(uint64_t v){return v>10000u?10000u:(uint32_t)v;}
static int fld(EVP_MD_CTX*x,const void*p,size_t n){uint64_t z=(uint64_t)n;if(EVP_DigestUpdate(x,&z,sizeof(z))!=1)return -1;if(n&&EVP_DigestUpdate(x,p,n)!=1)return -1;return 0;}
static int u32(EVP_MD_CTX*x,uint32_t v){return fld(x,&v,sizeof(v));}
static int u64(EVP_MD_CTX*x,uint64_t v){return fld(x,&v,sizeof(v));}

int qrx_compute_adversarial_evidence_validate(const QrxComputeAdversarialEvidence*e){
    if(!e||e->version!=QRX_COMPUTE_ADVERSARIAL_VERSION||!nz(e->scenario_id,QRX_COMPUTE_ADVERSARIAL_ID_MAX)||e->attack<QRX_ADV_FAKE_COMPUTE||e->attack>QRX_ADV_POD_FAILURE)return -1;
    if(!nz(e->provider_id,QRX_RESOURCE_PROVIDER_ID_MAX))return -2;
    if(!hex64_or_empty(e->expected_model_commitment)||!hex64_or_empty(e->observed_model_commitment)||!hex64_or_empty(e->expected_result_commitment)||!hex64_or_empty(e->observed_result_commitment))return -3;
    if(!bps(e->timeout_bps)||!bps(e->queue_wait_bps)||!bps(e->cache_hit_claim_bps)||!bps(e->cache_hit_verified_bps)||!bps(e->unavailable_peer_bps)||!bps(e->unavailable_pod_bps))return -4;
    if(e->matching_verifiers>e->verifier_count)return -5;
    switch(e->attack){
        case QRX_ADV_FAKE_COMPUTE: if(!e->claimed_compute_atoms)return -6; break;
        case QRX_ADV_RESULT_MISMATCH: if(!e->expected_result_commitment[0]||!e->observed_result_commitment[0])return -7; break;
        case QRX_ADV_MODEL_RUNTIME_MISMATCH: if((!e->expected_model_commitment[0]||!e->observed_model_commitment[0])&&(!nz(e->expected_runtime,QRX_POUC_MAX_RUNTIME-1)||!nz(e->observed_runtime,QRX_POUC_MAX_RUNTIME-1)))return -8; break;
        case QRX_ADV_VERIFIER_COLLUSION: if(e->verifier_count<2)return -9; break;
        case QRX_ADV_SYBIL_PROVIDER_SET: if(!e->distinct_provider_count)return -10; break;
        case QRX_ADV_CACHE_FRAUD: break;
        case QRX_ADV_SLOW_WORKER: if(!e->timeout_bps)return -11; break;
        case QRX_ADV_QUEUE_STARVATION: if(!e->queue_wait_bps)return -12; break;
        case QRX_ADV_WAN_PARTITION: if(!e->unavailable_peer_bps)return -13; break;
        case QRX_ADV_POD_FAILURE: if(!e->unavailable_pod_bps)return -14; break;
        default:return -15;
    }
    return 0;
}

int qrx_compute_adversarial_evidence_commitment(const QrxComputeAdversarialEvidence*e,char out[65]){
    if(qrx_compute_adversarial_evidence_validate(e)||!out)return -1;
    EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=x!=NULL;const char dom[]="QRX/COMPUTE/ADVERSARIAL-EVIDENCE/V1";
    if(ok&&EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;
#define F(p,n) do{if(ok&&fld(x,(p),(n)))ok=0;}while(0)
#define U32(v) do{if(ok&&u32(x,(uint32_t)(v)))ok=0;}while(0)
#define U64(v) do{if(ok&&u64(x,(uint64_t)(v)))ok=0;}while(0)
    F(dom,sizeof(dom)-1);F(e->scenario_id,strlen(e->scenario_id));U32(e->attack);F(e->provider_id,strlen(e->provider_id));
    F(e->expected_model_commitment,strlen(e->expected_model_commitment));F(e->observed_model_commitment,strlen(e->observed_model_commitment));F(e->expected_runtime,strlen(e->expected_runtime));F(e->observed_runtime,strlen(e->observed_runtime));F(e->expected_result_commitment,strlen(e->expected_result_commitment));F(e->observed_result_commitment,strlen(e->observed_result_commitment));
    U64(e->claimed_compute_atoms);U64(e->measured_compute_atoms);U32(e->verifier_count);U32(e->matching_verifiers);U32(e->distinct_operator_count);U32(e->distinct_provider_count);U32(e->timeout_bps);U32(e->queue_wait_bps);U32(e->cache_hit_claim_bps);U32(e->cache_hit_verified_bps);U32(e->unavailable_peer_bps);U32(e->unavailable_pod_bps);
#undef F
#undef U32
#undef U64
    if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -2;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;
}

static void detected(QrxComputeAdversarialVerdict*v,uint32_t risk,QrxComputeAdversarialSeverity sev,uint32_t actions,uint32_t challenge,uint8_t block,uint8_t payout){v->detected=1;v->risk_bps=clamp_bps(risk);v->severity=sev;v->action_mask=actions;v->challenge_bps=clamp_bps(challenge);v->block_new_work=block;v->payout_allowed=payout;}

int qrx_compute_adversarial_evaluate(const QrxComputeAdversarialEvidence*e,QrxComputeAdversarialVerdict*v){
    if(qrx_compute_adversarial_evidence_validate(e)||!v)return -1;memset(v,0,sizeof(*v));v->version=QRX_COMPUTE_ADVERSARIAL_VERSION;v->attack=e->attack;v->severity=QRX_ADV_SEVERITY_INFO;v->payout_allowed=1;if(qrx_compute_adversarial_evidence_commitment(e,v->evidence_commitment))return -2;
    switch(e->attack){
        case QRX_ADV_FAKE_COMPUTE:{
            uint64_t deficit=e->claimed_compute_atoms>e->measured_compute_atoms?e->claimed_compute_atoms-e->measured_compute_atoms:0;uint32_t r=e->claimed_compute_atoms?(uint32_t)((deficit>e->claimed_compute_atoms?e->claimed_compute_atoms:deficit)*10000ULL/e->claimed_compute_atoms):0;
            if(r>=500)detected(v,r,r>=5000?QRX_ADV_SEVERITY_CRITICAL:QRX_ADV_SEVERITY_HIGH,QRX_ADV_ACTION_CHALLENGE|QRX_ADV_ACTION_REJECT_RESULT|QRX_ADV_ACTION_FREEZE_PAYOUT|QRX_ADV_ACTION_QUARANTINE|QRX_ADV_ACTION_SLASH_CANDIDATE,10000,1,0);break;}
        case QRX_ADV_RESULT_MISMATCH: if(strcmp(e->expected_result_commitment,e->observed_result_commitment))detected(v,10000,QRX_ADV_SEVERITY_CRITICAL,QRX_ADV_ACTION_CHALLENGE|QRX_ADV_ACTION_REJECT_RESULT|QRX_ADV_ACTION_FREEZE_PAYOUT|QRX_ADV_ACTION_QUARANTINE|QRX_ADV_ACTION_RETRY_REDUNDANT|QRX_ADV_ACTION_SLASH_CANDIDATE,10000,1,0);break;
        case QRX_ADV_MODEL_RUNTIME_MISMATCH:{int mm=e->expected_model_commitment[0]&&strcmp(e->expected_model_commitment,e->observed_model_commitment);int rm=nz(e->expected_runtime,QRX_POUC_MAX_RUNTIME-1)&&strcmp(e->expected_runtime,e->observed_runtime);if(mm||rm)detected(v,9000,QRX_ADV_SEVERITY_CRITICAL,QRX_ADV_ACTION_REJECT_RESULT|QRX_ADV_ACTION_FREEZE_PAYOUT|QRX_ADV_ACTION_QUARANTINE|QRX_ADV_ACTION_RETRY_REDUNDANT,10000,1,0);break;}
        case QRX_ADV_VERIFIER_COLLUSION:{uint32_t op=e->distinct_operator_count?e->distinct_operator_count:1;uint32_t concentration=(e->verifier_count>op)?(uint32_t)(((uint64_t)(e->verifier_count-op)*10000ULL)/e->verifier_count):0;uint32_t agreement=e->verifier_count?(uint32_t)(((uint64_t)e->matching_verifiers*10000ULL)/e->verifier_count):0;if(concentration>=5000&&agreement>=6667)detected(v,(concentration+agreement)/2,QRX_ADV_SEVERITY_CRITICAL,QRX_ADV_ACTION_CHALLENGE|QRX_ADV_ACTION_FREEZE_PAYOUT|QRX_ADV_ACTION_QUARANTINE|QRX_ADV_ACTION_RETRY_REDUNDANT,10000,1,0);break;}
        case QRX_ADV_SYBIL_PROVIDER_SET:{uint32_t operators=e->distinct_operator_count?e->distinct_operator_count:1;uint32_t ratio=e->distinct_provider_count>operators?(uint32_t)(((uint64_t)(e->distinct_provider_count-operators)*10000ULL)/e->distinct_provider_count):0;if(e->distinct_provider_count>=3&&ratio>=5000)detected(v,ratio,QRX_ADV_SEVERITY_HIGH,QRX_ADV_ACTION_CHALLENGE|QRX_ADV_ACTION_QUARANTINE|QRX_ADV_ACTION_REROUTE,5000,1,1);break;}
        case QRX_ADV_CACHE_FRAUD:{uint32_t gap=e->cache_hit_claim_bps>e->cache_hit_verified_bps?e->cache_hit_claim_bps-e->cache_hit_verified_bps:0;if(gap>=1000)detected(v,gap,gap>=5000?QRX_ADV_SEVERITY_HIGH:QRX_ADV_SEVERITY_MEDIUM,QRX_ADV_ACTION_CHALLENGE|QRX_ADV_ACTION_REROUTE|(gap>=5000?QRX_ADV_ACTION_FREEZE_PAYOUT:0),gap>=5000?10000:3000,gap>=5000,gap<5000);break;}
        case QRX_ADV_SLOW_WORKER: if(e->timeout_bps>=1000)detected(v,e->timeout_bps,e->timeout_bps>=5000?QRX_ADV_SEVERITY_HIGH:QRX_ADV_SEVERITY_MEDIUM,QRX_ADV_ACTION_REROUTE|QRX_ADV_ACTION_DRAIN_PROVIDER|QRX_ADV_ACTION_RETRY_REDUNDANT,1000,e->timeout_bps>=5000,1);break;
        case QRX_ADV_QUEUE_STARVATION: if(e->queue_wait_bps>=2000)detected(v,e->queue_wait_bps,e->queue_wait_bps>=7000?QRX_ADV_SEVERITY_HIGH:QRX_ADV_SEVERITY_MEDIUM,QRX_ADV_ACTION_REROUTE|QRX_ADV_ACTION_DRAIN_PROVIDER,500,e->queue_wait_bps>=7000,1);break;
        case QRX_ADV_WAN_PARTITION: if(e->unavailable_peer_bps>=2000)detected(v,e->unavailable_peer_bps,e->unavailable_peer_bps>=7000?QRX_ADV_SEVERITY_CRITICAL:QRX_ADV_SEVERITY_HIGH,QRX_ADV_ACTION_REROUTE|QRX_ADV_ACTION_RETRY_REDUNDANT|QRX_ADV_ACTION_DRAIN_PROVIDER,1000,e->unavailable_peer_bps>=7000,1);break;
        case QRX_ADV_POD_FAILURE: if(e->unavailable_pod_bps>=2000)detected(v,e->unavailable_pod_bps,e->unavailable_pod_bps>=7000?QRX_ADV_SEVERITY_CRITICAL:QRX_ADV_SEVERITY_HIGH,QRX_ADV_ACTION_REROUTE|QRX_ADV_ACTION_RETRY_REDUNDANT|QRX_ADV_ACTION_DRAIN_PROVIDER,2000,e->unavailable_pod_bps>=7000,1);break;
        default:return -3;
    }
    /* 0.0.9.26 only produces slash candidates/evidence. Consensus slashing is
       deliberately not performed by this testnet evaluator. */
    v->consensus_slash_applied=0;return 0;
}
