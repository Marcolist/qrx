#include "storage/qrx_storage_postor_runtime.h"
#include "storage/qrx_storage_fs.h"
#include "storage/qrx_merkle.h"
#include "storage/qrx_drive_prepare.h"
#include "resource/qrx_storage_consensus.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void hx(const unsigned char*in,size_t n,char*out){static const char*x="0123456789abcdef";for(size_t i=0;i<n;i++){out[i*2]=x[in[i]>>4];out[i*2+1]=x[in[i]&15];}out[n*2]=0;}
int main(void){
 char d[]="/tmp/qrx-postor-runtime-XXXXXX";assert(mkdtemp(d));char fsd[512],src[512];snprintf(fsd,sizeof(fsd),"%s/fs",d);snprintf(src,sizeof(src),"%s/shard.bin",d);
 size_t n=QRX_DRIVE_POSTOR_LEAF_BYTES*3+917;unsigned char*b=malloc(n);assert(b);for(size_t i=0;i<n;i++)b[i]=(unsigned char)(i*29u+11u);FILE*f=fopen(src,"wb");assert(f&&fwrite(b,1,n,f)==n);fclose(f);
 QrxStorageFs*fs=NULL;assert(!qrx_storage_fs_open(fsd,1<<26,0,&fs));char oid[65];assert(!qrx_storage_fs_put_file(fs,src,oid));unsigned char root[64];size_t leaves=0;assert(!qrx_merkle_root_from_chunks(b,n,QRX_DRIVE_POSTOR_LEAF_BYTES,root,&leaves));char rh[129];hx(root,64,rh);
 QrxDB db;assert(!qrxdb_init(&db,d));uint64_t accepted=100,due=accepted+QRX_STORAGE_PROOF_WINDOW_BLOCKS;char bh[129];unsigned char hb[64];for(int i=0;i<64;i++)hb[i]=(unsigned char)(i+37);hx(hb,64,bh);assert(!qrxdb_chain_put_block(&db,due,bh,"finalized-postor-challenge"));char key[400],val[1200];snprintf(key,sizeof(key),"storage/assignment/c113/%010u",5u);snprintf(val,sizeof(val),"1|provider-P|2|%zu|%s|%s|%zu|%llu|0|0|0",n,oid,rh,leaves,(unsigned long long)accepted);assert(!qrxdb_put(&db,key,val));
 QrxPoStorDueAssignment a[4];size_t an=0;assert(!qrx_storage_postor_collect(&db,"provider-P",due-1,a,4,&an)&&an==1&&a[0].health==QRX_POSTOR_HEALTH_ACTIVE);assert(!qrx_storage_postor_collect(&db,"provider-P",due,a,4,&an)&&an==1&&a[0].health==QRX_POSTOR_HEALTH_PROOF_DUE&&a[0].epoch==1&&a[0].challenge_height==due);
 unsigned char*leaf=NULL;size_t leafn=0;QrxMerkleProof proof;assert(!qrx_storage_postor_build_local(&db,fs,"provider-P",&a[0],&leaf,&leafn,&proof));char*payload=NULL;assert(!qrx_storage_postor_payload(&a[0],leaf,leafn,&proof,&payload));QrxServiceEconomicEffect eff;assert(!qrx_storage_consensus_prepare(d,"STORAGE_POSTOR","provider-P","provider-P",0,payload,"tx113",due,&eff));
 /* Same proof cannot be replayed after epoch 1 has already become authoritative. */snprintf(val,sizeof(val),"1|provider-P|2|%zu|%s|%s|%zu|%llu|1|%llu|0",n,oid,rh,leaves,(unsigned long long)accepted,(unsigned long long)due);assert(!qrxdb_put(&db,key,val));assert(qrx_storage_consensus_prepare(d,"STORAGE_POSTOR","provider-P","provider-P",0,payload,"tx113-replay",due+1,&eff)!=0);
 uint64_t due2=due+QRX_STORAGE_PROOF_WINDOW_BLOCKS;for(int i=0;i<64;i++)hb[i]=(unsigned char)(i+91);hx(hb,64,bh);assert(!qrxdb_chain_put_block(&db,due2,bh,"finalized-postor-challenge-2"));
 assert(!qrx_storage_postor_collect(&db,"provider-P",due2+1,a,4,&an)&&an==1&&a[0].health==QRX_POSTOR_HEALTH_DEGRADED&&a[0].repair_ready&&a[0].epoch==2);
 free(payload);free(leaf);qrxdb_close(&db);qrx_storage_fs_close(fs);free(b);puts("PASS: automatic PoStor schedule, bounded-memory local Merkle proof, replay rejection and DEGRADED/repair readiness");return 0;
}
