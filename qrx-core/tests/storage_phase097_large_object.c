#include "storage/qrx_drive_object.h"
#include "storage/qrx_drive_crypto.h"
#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void fill_file(const char *p,size_t n){FILE*f=fopen(p,"wb");assert(f);unsigned char b[4096];for(size_t i=0;i<sizeof(b);i++)b[i]=(unsigned char)((i*131u+17u)&255u);size_t w=0;while(w<n){size_t c=n-w;if(c>sizeof(b))c=sizeof(b);assert(fwrite(b,1,c,f)==c);w+=c;}assert(fclose(f)==0);}
static int same(const char*a,const char*b){FILE*x=fopen(a,"rb"),*y=fopen(b,"rb");if(!x||!y)return 0;unsigned char p[8192],q[8192];int ok=1;for(;;){size_t n=fread(p,1,sizeof(p),x),m=fread(q,1,sizeof(q),y);if(n!=m||memcmp(p,q,n)){ok=0;break;}if(n<sizeof(p))break;}fclose(x);fclose(y);return ok;}
int main(void){char d[]="/tmp/qrx-large-XXXXXX";assert(mkdtemp(d));char src[512],dst[512];snprintf(src,sizeof(src),"%s/source.bin",d);snprintf(dst,sizeof(dst),"%s/restored.bin",d);/* >64 chunks so the test forces a multi-level index tree. */size_t bytes=(size_t)70*64*1024+123;fill_file(src,bytes);QrxStorageFs*fs=NULL;assert(qrx_storage_fs_open(d,0,0,&fs)==0);EVP_PKEY*k=NULL;assert(qrx_drive_generate_recipient_key(&k)==0);QrxDriveObjectInfo a={0},b={0};assert(qrx_drive_object_put_file(fs,src,k,64*1024,&a)==0);assert(a.plaintext_bytes==bytes);assert(a.chunk_count==71);assert(a.tree_level>=1);assert(qrx_drive_object_get_file(fs,a.object_id,k,dst,&b)==0);assert(b.plaintext_bytes==bytes&&b.chunk_count==a.chunk_count);assert(same(src,dst));EVP_PKEY_free(k);qrx_storage_fs_close(fs);puts("PASS: large logical objects stream through hierarchical CAS indexes with bounded memory and no product-level file-size cap");return 0;}
