#include "storage/qrx_storage_discovery.h"
#include "storage/qrx_drive_manifest.h"
#include "storage/qrx_erasure.h"
#include "storage/qrx_storage_transport.h"
#include "resource/qrx_storage_market.h"
#include "qrxdb.h"
#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {char id[32]; EVP_PKEY *key;} Keys;
static int lookup(void*v,const char*id,EVP_PKEY**out){Keys*k=v;*out=NULL;if(strcmp(id,k->id))return -1;assert(EVP_PKEY_up_ref(k->key)==1);*out=k->key;return 0;}
static void put_provider(QrxDB*db,const char*id){char k[256],v[1024];snprintf(k,sizeof(k),"storage/provider/%s",id);snprintf(v,sizeof(v),"1|%s|%u|1000|1099511627776|0|0|9950|9950|10000|9900|op-%s|AS64500|EU|AS64500|EU|1",id,QRX_PROVIDER_ACTIVE,id);assert(qrxdb_put(db,k,v)==0);}
typedef struct {QrxErasureSet *e;} Fx;
static int fetch(void*v,const QrxShardProviderSource*s,const uint8_t oid[64],uint64_t off,size_t want,uint8_t**out,size_t*outn){(void)oid;Fx*f=v;unsigned sh=s->source.shard_index;if(sh==3||off+want>f->e->shard_size)return -1;*out=malloc(want);assert(*out);memcpy(*out,f->e->shards[sh]+off,want);*outn=want;return 0;}
int main(void){
 char dir[]="/tmp/qrx106-XXXXXX";assert(mkdtemp(dir));char cache[512],outfile[512];snprintf(cache,sizeof(cache),"%s/discovery.cache",dir);snprintf(outfile,sizeof(outfile),"%s/restored.bin",dir);
 QrxDB db;assert(qrxdb_init(&db,dir)==0);Keys ks={0};snprintf(ks.id,sizeof(ks.id),"provider-A");assert(qrx_drive_manifest_generate_signing_key(&ks.key)==0);put_provider(&db,ks.id);
 QrxStorageProviderAnnouncement a={0};a.version=1;snprintf(a.provider_id,sizeof(a.provider_id),"%s",ks.id);a.sequence=7;a.valid_from_height=100;a.valid_until_height=200;a.capabilities=QRX_STORAGE_DISCOVERY_CAP_RANGE_READ|QRX_STORAGE_DISCOVERY_CAP_RESUME;a.available_bytes=123456789;a.observed_latency_ms=12;a.observed_throughput_bps=90000000;a.reliability_bps=9975;snprintf(a.endpoint,sizeof(a.endpoint),"qrxp2p://127.0.0.1:1234");
 uint8_t*sig=NULL;size_t sl=0;assert(qrx_storage_announcement_sign(ks.key,&a,&sig,&sl)==0);uint8_t*wire=NULL;size_t wn=0;assert(qrx_storage_discovery_wire_encode(&a,sig,sl,&wire,&wn)==0);QrxStorageProviderAnnouncement b;uint8_t*sig2=NULL;size_t sl2=0;assert(qrx_storage_discovery_wire_decode(wire,wn,&b,&sig2,&sl2)==0);assert(!strcmp(b.provider_id,a.provider_id)&&b.sequence==a.sequence&&!strcmp(b.endpoint,a.endpoint));
 QrxStorageDiscoveryTable t,u;qrx_storage_discovery_init(&t);qrx_storage_discovery_init(&u);assert(qrx_storage_discovery_ingest(&t,&db,&b,sig2,sl2,120,lookup,&ks)==0);assert(qrx_storage_discovery_cache_save(&t,cache)==0);assert(qrx_storage_discovery_cache_load(&u,&db,cache,120,lookup,&ks)==1);assert(u.count==1&&qrx_storage_discovery_find(&u,ks.id)->sequence==7);
 /* tamper with cached signature: restore must not trust it */ FILE*cf=fopen(cache,"r+b");assert(cf);assert(fseek(cf,-1,SEEK_END)==0);int c=fgetc(cf);assert(fseek(cf,-1,SEEK_END)==0);fputc(c^0x01,cf);fclose(cf);QrxStorageDiscoveryTable bad;qrx_storage_discovery_init(&bad);assert(qrx_storage_discovery_cache_load(&bad,&db,cache,120,lookup,&ks)==0);assert(bad.count==0);
 free(sig);free(sig2);free(wire);qrx_storage_discovery_free(&t);qrx_storage_discovery_free(&u);qrx_storage_discovery_free(&bad);EVP_PKEY_free(ks.key);
 /* streaming reconstruction: 8 MiB logical object, 64 KiB stripes, one dead provider */ size_t nbytes=8*1024*1024+123;uint8_t*original=malloc(nbytes);assert(original);for(size_t i=0;i<nbytes;i++)original[i]=(uint8_t)(i*17u+11u);QrxErasureSet e;assert(qrx_erasure_encode(original,nbytes,10,4,&e)==0);QrxShardProviderSource src[14];char ids[14][16],oids[14][65];for(unsigned i=0;i<14;i++){snprintf(ids[i],sizeof(ids[i]),"p%u",i);memset(&src[i],0,sizeof(src[i]));src[i].provider_id=ids[i];src[i].source.shard_index=i;EVP_MD_CTX*hm=EVP_MD_CTX_new();unsigned char hh[32];unsigned int hhn=0;assert(hm&&EVP_DigestInit_ex(hm,EVP_sha3_256(),NULL)==1&&EVP_DigestUpdate(hm,e.shards[i],e.shard_size)==1&&EVP_DigestFinal_ex(hm,hh,&hhn)==1&&hhn==32);EVP_MD_CTX_free(hm);static const char*hx="0123456789abcdef";for(int q=0;q<32;q++){oids[i][q*2]=hx[hh[q]>>4];oids[i][q*2+1]=hx[hh[q]&15];}oids[i][64]=0;src[i].object_id_hex=oids[i];}
 Fx fx={&e};QrxMultiFetchStats st={0};assert(qrx_storage_stream_reconstruct_to_file(src,14,e.shard_size,10,4,nbytes,64*1024,fetch,&fx,outfile,&st)==0);FILE*f=fopen(outfile,"rb");assert(f);uint8_t*got=malloc(nbytes);assert(got&&fread(got,1,nbytes,f)==nbytes);fclose(f);assert(memcmp(got,original,nbytes)==0);assert(st.bytes_received < (uint64_t)nbytes*2);free(got);free(original);qrx_erasure_free(&e);qrxdb_close(&db);puts("PASS: signed gossip wire/cache revalidates authentication and large restore is stripe-streamed with bounded memory");return 0;
}
