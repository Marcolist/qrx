#include "storage/qrx_drive_private_pq.h"
#include "storage/qrx_drive_crypto.h"
#include "storage/qrx_drive_manifest.h"
#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int same(const char*a,const char*b){FILE*x=fopen(a,"rb"),*y=fopen(b,"rb");if(!x||!y)return 0;int ok=1;for(;;){unsigned char p[8192],q[8192];size_t n=fread(p,1,sizeof(p),x),m=fread(q,1,sizeof(q),y);if(n!=m||memcmp(p,q,n)){ok=0;break;}if(!n)break;}fclose(x);fclose(y);return ok;}
int main(void){char d[]="/tmp/qrx-pq108-XXXXXX";assert(mkdtemp(d));char src[512],enc[512],dst[512],bad[512];snprintf(src,sizeof(src),"%s/src.bin",d);snprintf(enc,sizeof(enc),"%s/object.pq",d);snprintf(dst,sizeof(dst),"%s/dst.bin",d);snprintf(bad,sizeof(bad),"%s/bad.bin",d);FILE*f=fopen(src,"wb");assert(f);for(size_t i=0;i<3*1024*1024+333;i++){unsigned char c=(unsigned char)(i*37u+11u);assert(fwrite(&c,1,1,f)==1);}fclose(f);EVP_PKEY*kem=NULL,*sig=NULL,*wrong=NULL;assert(qrx_drive_generate_recipient_key(&kem)==0);assert(qrx_drive_manifest_generate_signing_key(&sig)==0);assert(qrx_drive_manifest_generate_signing_key(&wrong)==0);uint64_t pn=0,cn=0;assert(qrx_drive_private_pq_encrypt_file(src,enc,kem,sig,&pn,&cn)==0);assert(pn==3*1024*1024+333&&cn>pn);assert(qrx_drive_private_pq_decrypt_file(enc,dst,kem,sig,&pn)==0);assert(same(src,dst));assert(qrx_drive_private_pq_decrypt_file(enc,bad,kem,wrong,NULL)!=0);FILE*t=fopen(enc,"r+b");assert(t);assert(fseek(t,200,SEEK_SET)==0);int c=fgetc(t);assert(c!=EOF);assert(fseek(t,200,SEEK_SET)==0);fputc(c^0x40,t);fclose(t);assert(qrx_drive_private_pq_decrypt_file(enc,bad,kem,sig,NULL)!=0);EVP_PKEY_free(kem);EVP_PKEY_free(sig);EVP_PKEY_free(wrong);puts("PASS: PRIVATE_PQ encrypts chunkwise with AES-256-GCM, hybrid X25519MLKEM768 key wrapping, ML-DSA-65 signature verification, and tamper rejection");return 0;}
