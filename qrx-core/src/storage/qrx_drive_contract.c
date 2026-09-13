#include "storage/qrx_drive_contract.h"
#include "resource/qrx_storage_market.h"
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void hex64(const uint8_t in[64],char out[129]){static const char h[]="0123456789abcdef";for(int i=0;i<64;i++){out[i*2]=h[in[i]>>4];out[i*2+1]=h[in[i]&15];}out[128]=0;}
static void hex32(const uint8_t in[32],char out[65]){static const char h[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=h[in[i]>>4];out[i*2+1]=h[in[i]&15];}out[64]=0;}
static int add_u64(uint64_t a,uint64_t b,uint64_t*out){if(UINT64_MAX-a<b)return-1;*out=a+b;return 0;}
static int mul_u64(uint64_t a,uint64_t b,uint64_t*out){if(a&&b>UINT64_MAX/a)return-1;*out=a*b;return 0;}
int qrx_drive_contract_id(const char*owner,const QrxDrivePreparedUpload*p,char out[129]){
 if(!owner||!*owner||!p||!p->prepare_id[0]||!out)return-1;EVP_MD_CTX*c=EVP_MD_CTX_new();uint8_t h[32];unsigned n=0;static const char d[]="QRX-DRIVE-CONTRACT-V1";
 int ok=c&&EVP_DigestInit_ex(c,EVP_sha3_256(),NULL)==1&&EVP_DigestUpdate(c,d,sizeof(d)-1)==1&&EVP_DigestUpdate(c,owner,strlen(owner))==1&&EVP_DigestUpdate(c,p->prepare_id,strlen(p->prepare_id))==1&&EVP_DigestUpdate(c,p->manifest_hash,64)==1&&EVP_DigestFinal_ex(c,h,&n)==1&&n==32;EVP_MD_CTX_free(c);if(!ok)return-1;char x[65];hex32(h,x);snprintf(out,129,"drv-%s",x);return 0;
}
int qrx_drive_contract_quote(const QrxDrivePreparedUpload*p,uint64_t epochs,uint64_t rate,uint64_t*out){if(!p||!epochs||!rate||!out||!p->total_shards||!p->shard_bytes)return-1;QrxStorageRewardInput ri={p->shard_bytes,1,rate,10000,10000,11500};uint64_t per=qrx_storage_provider_reward(&ri),all=0;if(!per||mul_u64(per,p->total_shards,&all)||mul_u64(all,epochs,&all))return-1;
 /* provider escrow is 97.5% (development 0.5%, resilience 2%). Round up gross amount so maximum performance rewards remain funded. */
 uint64_t gross=0,rem=all%9750ULL,extra=0;if(mul_u64(all/9750ULL,10000ULL,&gross))return-1;if(rem){if(mul_u64(rem,10000ULL,&extra))return-1;extra=(extra+9749ULL)/9750ULL;if(add_u64(gross,extra,&gross))return-1;}if(!gross)gross=1;*out=gross;return 0;}
static int contract_matches(const QrxStorageContractState*c,const char*owner,const QrxDrivePreparedUpload*p,uint64_t rate){if(strcmp(c->owner,owner)||strcmp(c->profile,p->manifest.profile)||c->logical_bytes!=p->ciphertext_bytes||c->shard_count!=p->total_shards||c->shard_bytes!=p->shard_bytes||c->base_atoms_per_gib_epoch!=rate||CRYPTO_memcmp(c->manifest_root,p->manifest_hash,64))return-1;return 0;}
int qrx_drive_contract_next_step(QrxDB*db,const char*owner,const QrxDrivePreparedUpload*p,uint64_t h,uint64_t epochs,uint64_t rate,QrxDriveContractStep*out){
 if(!db||!owner||!*owner||!p||!out||!h)return-1;if(!epochs)epochs=QRX_DRIVE_CONTRACT_DEFAULT_EPOCHS;if(!rate)rate=QRX_DRIVE_CONTRACT_DEFAULT_RATE_ATOMS;if(qrx_drive_prepared_upload_verify_files(p))return-1;memset(out,0,sizeof(*out));out->assignments_required=p->total_shards;out->base_atoms_per_gib_epoch=rate;if(qrx_drive_contract_id(owner,p,out->contract_id))return-1;
 QrxStorageContractState c;if(qrx_storage_contract_get(db,out->contract_id,&c)!=0){uint64_t atoms=0,span=0;if(qrx_drive_contract_quote(p,epochs,rate,&atoms)||mul_u64(epochs,QRX_DRIVE_CONTRACT_EPOCH_BLOCKS,&span)||add_u64(h,span,&out->end_height))return-1;char mh[129];hex64(p->manifest_hash,mh);out->kind=QRX_DRIVE_CONTRACT_CREATE;snprintf(out->tx_type,sizeof(out->tx_type),"STORAGE_CONTRACT_CREATE");snprintf(out->to,sizeof(out->to),"%s",owner);out->amount_atoms=atoms;snprintf(out->payload,sizeof(out->payload),"contract_id=%s;profile=%s;logical_bytes=%llu;end_height=%llu;base_atoms_per_gib_epoch=%llu;manifest_root_hex=%s",out->contract_id,p->manifest.profile,(unsigned long long)p->ciphertext_bytes,(unsigned long long)out->end_height,(unsigned long long)rate,mh);return 0;}
 if(contract_matches(&c,owner,p,rate))return-1;out->end_height=c.end_height;for(unsigned i=0;i<p->total_shards;i++){QrxStorageAssignmentRecord a;if(qrx_storage_assignment_get(db,out->contract_id,i,&a)==0){if(strcmp(a.object_id,p->shard_object_ids[i])||CRYPTO_memcmp(a.merkle_root,p->shard_merkle_roots[i],64)||a.leaf_count!=p->shard_leaf_counts[i])return-1;out->assignments_present++;continue;}char provider[129],mr[129];if(qrx_storage_assignment_provider_for_height(db,out->contract_id,p->shard_bytes,h+1,provider))return-1;hex64(p->shard_merkle_roots[i],mr);out->kind=QRX_DRIVE_CONTRACT_ASSIGN;out->shard_index=i;snprintf(out->tx_type,sizeof(out->tx_type),"STORAGE_ASSIGN");snprintf(out->to,sizeof(out->to),"%s",provider);snprintf(out->payload,sizeof(out->payload),"contract_id=%s;shard_index=%u;object_id=%s;merkle_root_hex=%s;leaf_count=%llu",out->contract_id,i,p->shard_object_ids[i],mr,(unsigned long long)p->shard_leaf_counts[i]);return 0;}
 out->kind=QRX_DRIVE_CONTRACT_READY_UPLOAD;snprintf(out->tx_type,sizeof(out->tx_type),"-");snprintf(out->to,sizeof(out->to),"-");return 0;
}
