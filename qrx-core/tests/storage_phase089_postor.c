#include "storage/qrx_postor.h"
#include <assert.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>
int main(void){const size_t leaves=16,sz=1024;uint8_t *data=malloc(leaves*sz),*hash=malloc(leaves*64);assert(data&&hash);assert(RAND_bytes(data,(int)(leaves*sz))==1);for(size_t i=0;i<leaves;i++)assert(qrx_merkle_leaf_hash(i,data+i*sz,sz,hash+i*64)==0);uint8_t root[64];assert(qrx_merkle_root_from_leaves(hash,leaves,root)==0);uint8_t block[64];assert(RAND_bytes(block,64)==1);QrxPoStorChallenge c;assert(qrx_postor_derive_challenge("contract-1","provider-1",3,44,leaves,block,&c)==0);QrxMerkleProof p;assert(qrx_merkle_build_proof_from_leaves(hash,leaves,c.leaf_index,&p)==0);assert(qrx_postor_verify(&c,data+c.leaf_index*sz,sz,&p,root)==0);data[c.leaf_index*sz]^=1;assert(qrx_postor_verify(&c,data+c.leaf_index*sz,sz,&p,root)!=0);free(hash);free(data);return 0;}
