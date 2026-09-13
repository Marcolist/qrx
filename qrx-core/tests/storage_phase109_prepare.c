#include "storage/qrx_drive_prepare.h"
#include "storage/qrx_drive_crypto.h"
#include "storage/qrx_drive_manifest.h"
#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void){char d[]="/tmp/qrx-prep109-XXXXXX";assert(mkdtemp(d));char src[512],root[512];snprintf(src,sizeof(src),"%s/input.bin",d);snprintf(root,sizeof(root),"%s/prepared",d);FILE*f=fopen(src,"wb");assert(f);for(size_t i=0;i<1800000;i++){unsigned char c=(unsigned char)(i*13u+9u);assert(fwrite(&c,1,1,f)==1);}fclose(f);EVP_PKEY*kem=NULL,*sig=NULL;assert(qrx_drive_generate_recipient_key(&kem)==0);assert(qrx_drive_manifest_generate_signing_key(&sig)==0);QrxDrivePreparedUpload p={0};assert(qrx_drive_prepare_private_upload(root,src,"STANDARD",10,4,kem,sig,&p)==0);assert(p.total_shards==14&&p.data_shards==10&&p.parity_shards==4);assert(!strcmp(p.manifest.crypto_suite,"qrx-drive-pq-v1"));assert(!strcmp(p.manifest.profile,"STANDARD"));assert(p.manifest.plaintext_bytes==1800000&&p.manifest.ciphertext_bytes>p.manifest.plaintext_bytes);assert(p.shard_bytes>0&&p.manifest_signature_len>0);assert(qrx_drive_manifest_verify(sig,&p.manifest,p.manifest_signature,p.manifest_signature_len)==0);for(unsigned i=0;i<14;i++){assert(strlen(p.shard_object_ids[i])==64);FILE*s=fopen(p.shard_paths[i],"rb");assert(s);fclose(s);for(unsigned j=i+1;j<14;j++)assert(strcmp(p.shard_object_ids[i],p.shard_object_ids[j]));}qrx_drive_prepared_upload_free(&p);EVP_PKEY_free(kem);EVP_PKEY_free(sig);puts("PASS: PRIVATE_PQ preflight fixes ciphertext and 10+4 shard CAS IDs before contract creation and signs qrx-drive-pq-v1 manifest");return 0;}
