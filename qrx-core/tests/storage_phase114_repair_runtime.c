#include "storage/qrx_storage_repair_runtime.h"
#include "storage/qrx_storage_transport.h"
#include "storage/qrx_erasure.h"
#include "storage/qrx_storage_fs.h"
#include <openssl/evp.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

typedef struct { QrxErasureSet *es; } Ctx;
static void hx32(const unsigned char *d,size_t n,char out[65]){EVP_MD_CTX*c=EVP_MD_CTX_new();unsigned char h[32];unsigned z=0;assert(c&&EVP_DigestInit_ex(c,EVP_sha3_256(),0)==1&&EVP_DigestUpdate(c,d,n)==1&&EVP_DigestFinal_ex(c,h,&z)==1&&z==32);EVP_MD_CTX_free(c);static const char*x="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=x[h[i]>>4];out[i*2+1]=x[h[i]&15];}out[64]=0;}
static int fetch(void *v,const QrxShardProviderSource*s,const uint8_t oid[64],uint64_t off,size_t len,uint8_t **out,size_t*outn){(void)oid;Ctx*c=v;uint32_t i=s->source.shard_index;if(i>=c->es->data_shards+c->es->parity_shards||off+len>c->es->shard_size)return -1;*out=malloc(len);assert(*out);memcpy(*out,c->es->shards[i]+off,len);*outn=len;return 0;}
int main(void){
 unsigned char data[700003];for(size_t i=0;i<sizeof(data);i++)data[i]=(unsigned char)(i*37u+11u);QrxErasureSet es={0};assert(!qrx_erasure_encode(data,sizeof(data),10,4,&es));
 QrxShardProviderSource src[13];char ids[14][65];for(unsigned i=0;i<14;i++)hx32(es.shards[i],es.shard_size,ids[i]);size_t z=0;for(unsigned i=0;i<14;i++)if(i!=6){memset(&src[z],0,sizeof(src[z]));src[z].provider_id="p";src[z].endpoint="mock";src[z].object_id_hex=ids[i];src[z].source.shard_index=i;z++;}
 char d[]="/tmp/qrx-repair-XXXXXX";assert(mkdtemp(d));char out[512];snprintf(out,sizeof(out),"%s/rebuilt.bin",d);Ctx c={&es};QrxMultiFetchStats st={0};assert(!qrx_storage_stream_reconstruct_shard_to_file(src,13,es.shard_size,10,4,6,32768,fetch,&c,ids[6],out,&st));FILE*f=fopen(out,"rb");assert(f);unsigned char*b=malloc(es.shard_size);assert(b&&fread(b,1,es.shard_size,f)==es.shard_size);fclose(f);assert(!memcmp(b,es.shards[6],es.shard_size));free(b);
 QrxDB db;assert(!qrxdb_init(&db,d));char key[300],val[900],root[129];memset(root,'0',128);root[128]=0;snprintf(key,sizeof(key),"storage/assignment/c114/%010u",6u);snprintf(val,sizeof(val),"1|old|2|%zu|%s|%s|1|100|0|0|0",es.shard_size,ids[6],root);assert(!qrxdb_put(&db,key,val));QrxStorageRepairStartCandidate sc[2];size_t n=0;assert(!qrx_storage_repair_collect_starts(&db,100+QRX_STORAGE_PROOF_WINDOW_BLOCKS+1,sc,2,&n)&&n==1&&sc[0].shard_index==6);char*p=NULL;assert(!qrx_storage_repair_start_payload(&sc[0],&p)&&strstr(p,"contract_id=c114"));free(p);qrxdb_close(&db);qrx_erasure_free(&es);remove(out);rmdir(d);return 0;
}
