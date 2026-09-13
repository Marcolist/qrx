#include "resource/qrx_storage_placement.h"
#include "resource/qrx_resource.h"
#include <openssl/evp.h>
#include <string.h>

static int contains(const char *s,const char **a,size_t n){if(!s||!*s)return 0;for(size_t i=0;i<n;++i)if(a[i]&&!strcmp(s,a[i]))return 1;return 0;}
static uint64_t tie_hash(const char *provider,const uint8_t randomness[64]){static const char d[]="QRX-STORAGE-PLACEMENT-V1";EVP_MD_CTX *c=EVP_MD_CTX_new();uint8_t h[64];unsigned int n=0;uint64_t v=0;if(!c)return 0;if(EVP_DigestInit_ex(c,EVP_sha3_512(),NULL)!=1||EVP_DigestUpdate(c,d,sizeof(d)-1)!=1||EVP_DigestUpdate(c,provider,strlen(provider))!=1||EVP_DigestUpdate(c,randomness,64)!=1||EVP_DigestFinal_ex(c,h,&n)!=1||n!=64){EVP_MD_CTX_free(c);return 0;}EVP_MD_CTX_free(c);for(int i=0;i<8;i++)v=(v<<8)|h[i];return v;}
uint64_t qrx_storage_placement_score(const QrxStoragePlacementCandidate *c,const uint8_t randomness[64]){
    if(!c||!randomness||!c->active||!c->provider_id[0])return 0; uint64_t cap=qrx_resource_capacity_weight(c->proven_free_bytes); uint64_t reliability=((uint64_t)c->availability_bps+c->proof_success_bps)/2; if(reliability>10000)reliability=10000; uint64_t perf=c->performance_bps; if(perf<5000)perf=5000;if(perf>11500)perf=11500;
    uint64_t price_factor=10000; if(c->price_atoms_per_gib_epoch){uint64_t p=c->price_atoms_per_gib_epoch;price_factor=10000ULL*1000000ULL/(1000000ULL+(p>UINT64_MAX/10000?UINT64_MAX:p*10));if(price_factor<1000)price_factor=1000;}
    uint64_t base=cap?cap:1; if(base>UINT64_MAX/10000)base=UINT64_MAX/10000;base=base*reliability/10000;base=base*perf/10000;base=base*price_factor/10000;
    uint64_t t=tie_hash(c->provider_id,randomness)&0xffffu; return (base<<16)^t;
}
int qrx_storage_select_provider(const QrxStoragePlacementCandidate *c,size_t n,const QrxStoragePlacementExclusions *x,uint64_t minfree,uint64_t minbond,const uint8_t rnd[64],size_t *out){
    if(!c||!n||!rnd||!out)return -1;int found=0;uint64_t best=0;size_t bi=0;for(size_t i=0;i<n;++i){const QrxStoragePlacementCandidate *p=&c[i];if(!p->active||p->proven_free_bytes<minfree||p->bond_atoms<minbond||!p->provider_id[0])continue;if(x&&(contains(p->provider_id,x->provider_ids,x->provider_count)||contains(p->operator_id,x->operator_ids,x->operator_count)))continue;if(x&&p->failure_domain_attested&&(contains(p->asn,x->asns,x->asn_count)||contains(p->region,x->regions,x->region_count)))continue;uint64_t s=qrx_storage_placement_score(p,rnd);if(!found||s>best){found=1;best=s;bi=i;}}
    if(!found)return -1;*out=bi;return 0;
}
