#include "storage/qrx_drive_prepare.h"
#include "storage/qrx_drive_crypto.h"
#include "storage/qrx_drive_manifest.h"
#include "storage/qrx_drive_runtime.h"
#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void){
 char d[]="/tmp/qrx-prep110-XXXXXX";assert(mkdtemp(d));char src[512],root[512];snprintf(src,sizeof(src),"%s/input.bin",d);snprintf(root,sizeof(root),"%s/prepared",d);
 FILE*f=fopen(src,"wb");assert(f);for(size_t i=0;i<700000;i++){unsigned char c=(unsigned char)(i*7u+3u);assert(fwrite(&c,1,1,f)==1);}fclose(f);
 EVP_PKEY*kem=NULL,*sig=NULL;assert(qrx_drive_generate_recipient_key(&kem)==0);assert(qrx_drive_manifest_generate_signing_key(&sig)==0);
 QrxDrivePreparedUpload p={0};assert(qrx_drive_prepare_private_upload(root,src,"STANDARD",10,4,kem,sig,&p)==0);
 QrxDrivePreparedUpload loaded={0};assert(qrx_drive_prepared_upload_load(p.package_dir,sig,&loaded)==0);assert(!strcmp(loaded.prepare_id,p.prepare_id));assert(loaded.total_shards==14);assert(!memcmp(loaded.manifest_hash,p.manifest_hash,64));
 QrxShardProviderSource s[14]={0};char pid[14][32],ep[14][64];for(unsigned i=0;i<14;i++){snprintf(pid[i],sizeof(pid[i]),"provider-%u",i);snprintf(ep[i],sizeof(ep[i]),"qrxp2p://127.0.0.1:%u",30000+i);s[i].provider_id=pid[i];s[i].endpoint=ep[i];s[i].object_id_hex=loaded.shard_object_ids[i];s[i].source.shard_index=i;s[i].source.reliability_bps=10000;}
 assert(qrx_drive_prepared_upload_verify_sources(&loaded,s,14)==0);
 s[3].object_id_hex=loaded.shard_object_ids[4];assert(qrx_drive_prepared_upload_verify_sources(&loaded,s,14)!=0);s[3].object_id_hex=loaded.shard_object_ids[3];
 char journal[512];snprintf(journal,sizeof(journal),"%s/journal",d);QrxDriveRuntime*rt=NULL;assert(qrx_drive_runtime_open(d,journal,&rt)==0);assert(qrx_drive_runtime_set_contract_sources(rt,"c110",s,14)==0);
 /* A valid prepared package reaches the trusted runtime boundary. The sockets are deliberately absent, so the async job may fail later; start itself must accept exact IDs. */
 char tid[129];assert(qrx_drive_runtime_start_prepared_upload(rt,"c110",&loaded,tid)==0);assert(strlen(tid)>0);qrx_drive_runtime_close(rt);
 /* Tamper one persisted shard: load must fail before any runtime/provider use. */
 f=fopen(p.shard_paths[0],"ab");assert(f);fputc(0x42,f);fclose(f);QrxDrivePreparedUpload bad={0};assert(qrx_drive_prepared_upload_load(p.package_dir,sig,&bad)!=0);
 qrx_drive_prepared_upload_free(&loaded);qrx_drive_prepared_upload_free(&p);EVP_PKEY_free(kem);EVP_PKEY_free(sig);
 puts("PASS: prepared PRIVATE_PQ packages persist signed manifest metadata, revalidate shard CAS files, bind exact assignment object IDs, and enter runtime without re-erasure");return 0;
}
