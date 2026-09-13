#include "resource/qrx_capacity_proof.h"
#include <assert.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>
int main(void){const size_t n=32,sz=4096;uint8_t *slots=malloc(n*sz);assert(slots);assert(RAND_bytes(slots,(int)(n*sz))==1);QrxCapacityCommitment c;assert(qrx_capacity_commit("provider-A",7,slots,n,sz,&c)==0);uint8_t block[64];assert(RAND_bytes(block,64)==1);QrxCapacityChallengeProof p;assert(qrx_capacity_build_challenge_proof(&c,slots,n,sz,block,&p)==0);assert(p.slot_index<n);assert(qrx_capacity_verify_challenge(&c,block,slots+p.slot_index*sz,sz,&p)==0);slots[p.slot_index*sz]^=1;assert(qrx_capacity_verify_challenge(&c,block,slots+p.slot_index*sz,sz,&p)!=0);free(slots);return 0;}
