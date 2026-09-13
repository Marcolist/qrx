#include "storage/qrx_postor.h"
#include <openssl/evp.h>
#include <string.h>

static void put_u64be(uint8_t o[8],uint64_t v){for(int i=7;i>=0;--i){o[i]=(uint8_t)(v&255);v>>=8;}}
static void put_u32be(uint8_t o[4],uint32_t v){for(int i=3;i>=0;--i){o[i]=(uint8_t)(v&255);v>>=8;}}
int qrx_postor_derive_challenge(const char *cid,const char *pid,uint32_t shard,uint64_t epoch,uint64_t leaves,const uint8_t block[64],QrxPoStorChallenge *out){
    static const char d[]="QRX-POSTOR-CHALLENGE-V1"; if(!cid||!*cid||strlen(cid)>=129||!pid||!*pid||strlen(pid)>=129||!leaves||!block||!out)return -1;EVP_MD_CTX *ctx=EVP_MD_CTX_new();uint8_t h[64],eb[8],lb[8],sb[4];unsigned int n=0;if(!ctx)return -1;put_u64be(eb,epoch);put_u64be(lb,leaves);put_u32be(sb,shard);
    int ok=EVP_DigestInit_ex(ctx,EVP_sha3_512(),NULL)==1&&EVP_DigestUpdate(ctx,d,sizeof(d)-1)==1&&EVP_DigestUpdate(ctx,cid,strlen(cid))==1&&EVP_DigestUpdate(ctx,pid,strlen(pid))==1&&EVP_DigestUpdate(ctx,sb,4)==1&&EVP_DigestUpdate(ctx,eb,8)==1&&EVP_DigestUpdate(ctx,lb,8)==1&&EVP_DigestUpdate(ctx,block,64)==1&&EVP_DigestFinal_ex(ctx,h,&n)==1&&n==64;EVP_MD_CTX_free(ctx);if(!ok)return -1;uint64_t v=0;for(int i=0;i<8;++i)v=(v<<8)|h[i];memset(out,0,sizeof(*out));strcpy(out->contract_id,cid);strcpy(out->provider_id,pid);out->shard_index=shard;out->epoch=epoch;out->leaf_index=v%leaves;return 0;
}
int qrx_postor_verify(const QrxPoStorChallenge *c,const uint8_t *leaf,size_t len,const QrxMerkleProof *p,const uint8_t root[64]){if(!c||(!leaf&&len)||!p||!root||p->leaf_index!=c->leaf_index)return -1;uint8_t h[64];if(qrx_merkle_leaf_hash(c->leaf_index,leaf,len,h)!=0)return -1;return qrx_merkle_verify_proof(h,p,root);}
