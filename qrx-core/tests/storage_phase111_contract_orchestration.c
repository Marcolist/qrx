#include "storage/qrx_drive_prepare.h"
#include "storage/qrx_drive_contract.h"
#include "storage/qrx_drive_crypto.h"
#include "storage/qrx_drive_manifest.h"
#include "resource/qrx_storage_consensus.h"
#include "resource/qrx_storage_repair.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void provider(QrxDB*db,int i){char k[256],v[1200];snprintf(k,sizeof(k),"storage/provider/provider-%02d",i);snprintf(v,sizeof(v),"1|owner-%02d|2|100000000|1099511627776|0|0|10000|10000|10000|9000|operator-%02d|AS%05d|R%02d|AS%05d|R%02d|1",i,i,64500+i,i,64500+i,i);assert(qrxdb_put(db,k,v)==0);}
static void hashhex(int seed,char out[129]){static const char*x="0123456789abcdef";for(int i=0;i<64;i++){unsigned b=(unsigned)(seed+i*17);out[i*2]=x[(b>>4)&15];out[i*2+1]=x[b&15];}out[128]=0;}
int main(void){
 char d[]="/tmp/qrx-contract111-XXXXXX";assert(mkdtemp(d));char src[512],root[512];snprintf(src,sizeof(src),"%s/input.bin",d);snprintf(root,sizeof(root),"%s/prepared",d);FILE*f=fopen(src,"wb");assert(f);for(size_t i=0;i<350000;i++){unsigned char c=(unsigned char)(i*7u+3u);assert(fwrite(&c,1,1,f)==1);}fclose(f);
 EVP_PKEY*kem=NULL,*sig=NULL;assert(qrx_drive_generate_recipient_key(&kem)==0);assert(qrx_drive_manifest_generate_signing_key(&sig)==0);QrxDrivePreparedUpload p={0};assert(qrx_drive_prepare_private_upload(root,src,"STANDARD",10,4,kem,sig,&p)==0);for(unsigned i=0;i<p.total_shards;i++){assert(p.shard_leaf_counts[i]>0);unsigned char zero[64]={0};assert(memcmp(p.shard_merkle_roots[i],zero,64)!=0);}
 QrxDB db;assert(qrxdb_init(&db,d)==0);for(int i=0;i<20;i++)provider(&db,i);char bh[129];hashhex(1,bh);assert(qrxdb_chain_put_block(&db,100,bh,"block") == 0);
 QrxDriveContractStep st={0};assert(qrx_drive_contract_next_step(&db,"qrx1owner",&p,100,30,10000,&st)==0);assert(st.kind==QRX_DRIVE_CONTRACT_CREATE&&st.amount_atoms>0&&strstr(st.payload,"logical_bytes=")&&strstr(st.payload,"manifest_root_hex="));
 QrxServiceEconomicEffect e={0};assert(qrx_storage_consensus_prepare(d,st.tx_type,"qrx1owner",st.to,st.amount_atoms,st.payload,"contracttx",101,&e)==0);QrxDBBatch b;assert(qrxdb_batch_begin(&db,&b)==0);assert(qrx_storage_consensus_stage(&db,&b,d,st.tx_type,"qrx1owner",st.to,st.amount_atoms,st.payload,"contracttx",101)==0);assert(qrxdb_batch_commit(&b)==0);
 memset(&st,0,sizeof(st));assert(qrx_drive_contract_next_step(&db,"qrx1owner",&p,100,30,10000,&st)==0);assert(st.kind==QRX_DRIVE_CONTRACT_ASSIGN&&st.shard_index==0&&strncmp(st.to,"provider-",9)==0);assert(strstr(st.payload,p.shard_object_ids[0]));char leaves[64];snprintf(leaves,sizeof(leaves),"leaf_count=%llu",(unsigned long long)p.shard_leaf_counts[0]);assert(strstr(st.payload,leaves));
 assert(qrx_storage_consensus_prepare(d,st.tx_type,"qrx1owner",st.to,0,st.payload,"assigntx",101,&e)==0);assert(qrxdb_batch_begin(&db,&b)==0);assert(qrx_storage_consensus_stage(&db,&b,d,st.tx_type,"qrx1owner",st.to,0,st.payload,"assigntx",101)==0);assert(qrxdb_batch_commit(&b)==0);QrxStorageAssignmentRecord a;assert(qrx_storage_assignment_get(&db,st.contract_id,0,&a)==0);assert(a.state==QRX_ASSIGN_PENDING&&!strcmp(a.object_id,p.shard_object_ids[0])&&a.leaf_count==p.shard_leaf_counts[0]&&!memcmp(a.merkle_root,p.shard_merkle_roots[0],64));
 qrxdb_close(&db);qrx_drive_prepared_upload_free(&p);EVP_PKEY_free(kem);EVP_PKEY_free(sig);puts("PASS: prepared PRIVATE_PQ package deterministically creates contract and next-block assignment using CAS-bound PoStor Merkle commitments");return 0;
}
