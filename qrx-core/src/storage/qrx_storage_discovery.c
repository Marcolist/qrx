#include "storage/qrx_storage_discovery.h"
#include "resource/qrx_storage_consensus.h"
#include "resource/qrx_storage_market.h"
#include "resource/qrx_storage_repair.h"
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int safe_text(const char *s,size_t n){
    if(!s||!*s||strlen(s)>=n)return 0;
    for(const unsigned char*p=(const unsigned char*)s;*p;p++) if(*p<0x21||*p>0x7e||*p=='|') return 0;
    return 1;
}

int qrx_storage_announcement_validate(const QrxStorageProviderAnnouncement *a,uint64_t h){
    if(!a||a->version!=QRX_STORAGE_DISCOVERY_VERSION||!safe_text(a->provider_id,sizeof(a->provider_id))||
       !safe_text(a->endpoint,sizeof(a->endpoint))||!a->sequence||a->valid_until_height<a->valid_from_height||
       h<a->valid_from_height||h>a->valid_until_height||a->reliability_bps>10000)return -1;
    if(!(a->capabilities&QRX_STORAGE_DISCOVERY_CAP_RANGE_READ))return -1;
    /* Discovery endpoints are transport locators, never arbitrary URLs. Keep
     * v1 deliberately narrow to avoid browser/DNS confusion and injection. */
    if(strncmp(a->endpoint,"quic://",7)&&strncmp(a->endpoint,"qrxp2p://",9))return -1;
    return 0;
}

int qrx_storage_announcement_canonical(const QrxStorageProviderAnnouncement *a,uint8_t **out,size_t *out_len){
    if(!a||!out||!out_len)return -1;*out=NULL;*out_len=0;
    int n=snprintf(NULL,0,"qrx-storage-discovery-v1|%u|%s|%llu|%llu|%llu|%llu|%llu|%u|%llu|%u|%s",
        a->version,a->provider_id,(unsigned long long)a->sequence,(unsigned long long)a->valid_from_height,
        (unsigned long long)a->valid_until_height,(unsigned long long)a->capabilities,
        (unsigned long long)a->available_bytes,a->observed_latency_ms,(unsigned long long)a->observed_throughput_bps,
        a->reliability_bps,a->endpoint);
    if(n<=0)return -1;uint8_t*b=malloc((size_t)n+1);if(!b)return -1;
    int n2=snprintf((char*)b,(size_t)n+1,"qrx-storage-discovery-v1|%u|%s|%llu|%llu|%llu|%llu|%llu|%u|%llu|%u|%s",
        a->version,a->provider_id,(unsigned long long)a->sequence,(unsigned long long)a->valid_from_height,
        (unsigned long long)a->valid_until_height,(unsigned long long)a->capabilities,
        (unsigned long long)a->available_bytes,a->observed_latency_ms,(unsigned long long)a->observed_throughput_bps,
        a->reliability_bps,a->endpoint);
    if(n2!=n){free(b);return -1;}*out=b;*out_len=(size_t)n;return 0;
}

int qrx_storage_announcement_hash(const QrxStorageProviderAnnouncement *a,uint8_t out[64]){
    uint8_t*b=NULL;size_t n=0;if(!out||qrx_storage_announcement_canonical(a,&b,&n))return -1;
    EVP_MD_CTX*c=EVP_MD_CTX_new();unsigned int z=0;int rc=-1;
    if(c&&EVP_DigestInit_ex(c,EVP_sha3_512(),NULL)==1&&EVP_DigestUpdate(c,b,n)==1&&EVP_DigestFinal_ex(c,out,&z)==1&&z==64)rc=0;
    EVP_MD_CTX_free(c);free(b);return rc;
}
int qrx_storage_announcement_sign(EVP_PKEY*k,const QrxStorageProviderAnnouncement*a,uint8_t**sig,size_t*slen){
    if(!k||!a||!sig||!slen)return -1;*sig=NULL;*slen=0;uint8_t h[64];if(qrx_storage_announcement_hash(a,h))return -1;
    EVP_MD_CTX*c=EVP_MD_CTX_new();if(!c)return -1;size_t n=0;uint8_t*s=NULL;int rc=-1;
    if(EVP_DigestSignInit_ex(c,NULL,NULL,NULL,NULL,k,NULL)!=1||EVP_DigestSign(c,NULL,&n,h,sizeof(h))!=1||!n)goto done;
    s=malloc(n);if(!s)goto done;if(EVP_DigestSign(c,s,&n,h,sizeof(h))!=1)goto done;*sig=s;*slen=n;s=NULL;rc=0;
done:free(s);EVP_MD_CTX_free(c);return rc;
}
int qrx_storage_announcement_verify(EVP_PKEY*k,const QrxStorageProviderAnnouncement*a,const uint8_t*sig,size_t slen){
    if(!k||!a||!sig||!slen)return -1;uint8_t h[64];if(qrx_storage_announcement_hash(a,h))return -1;
    EVP_MD_CTX*c=EVP_MD_CTX_new();if(!c)return -1;int rc=-1;
    if(EVP_DigestVerifyInit_ex(c,NULL,NULL,NULL,NULL,k,NULL)==1&&EVP_DigestVerify(c,sig,slen,h,sizeof(h))==1)rc=0;
    EVP_MD_CTX_free(c);return rc;
}
void qrx_storage_discovery_init(QrxStorageDiscoveryTable*t){if(t)memset(t,0,sizeof(*t));}
static void entry_free(QrxStorageDiscoveryEntry*e){if(e){free(e->signature);memset(e,0,sizeof(*e));}}
void qrx_storage_discovery_free(QrxStorageDiscoveryTable*t){if(!t)return;for(size_t i=0;i<t->count;i++)entry_free(&t->entries[i]);free(t->entries);memset(t,0,sizeof(*t));}
static long find_index(const QrxStorageDiscoveryTable*t,const char*pid){if(!t||!pid)return -1;for(size_t i=0;i<t->count;i++)if(!strcmp(t->entries[i].announcement.provider_id,pid))return (long)i;return -1;}
const QrxStorageProviderAnnouncement*qrx_storage_discovery_find(const QrxStorageDiscoveryTable*t,const char*pid){long i=find_index(t,pid);return i<0?NULL:&t->entries[i].announcement;}
static int reserve(QrxStorageDiscoveryTable*t,size_t n){if(n<=t->capacity)return 0;if(n>QRX_STORAGE_DISCOVERY_MAX_ENTRIES)return -1;size_t c=t->capacity?t->capacity*2:32;if(c<n)c=n;if(c>QRX_STORAGE_DISCOVERY_MAX_ENTRIES)c=QRX_STORAGE_DISCOVERY_MAX_ENTRIES;void*p=realloc(t->entries,c*sizeof(*t->entries));if(!p)return -1;t->entries=p;t->capacity=c;return 0;}
int qrx_storage_discovery_ingest(QrxStorageDiscoveryTable*t,QrxDB*db,const QrxStorageProviderAnnouncement*a,const uint8_t*sig,size_t slen,uint64_t h,QrxStorageProviderKeyLookupFn lookup,void*kctx){
    if(!t||!db||!lookup||qrx_storage_announcement_validate(a,h))return -1;
    QrxStorageProviderState ps;if(qrx_storage_provider_get(db,a->provider_id,&ps)||ps.status!=QRX_PROVIDER_ACTIVE||!ps.proven_capacity_bytes)return -1;
    EVP_PKEY*pk=NULL;if(lookup(kctx,a->provider_id,&pk)||!pk)return -1;int vr=qrx_storage_announcement_verify(pk,a,sig,slen);EVP_PKEY_free(pk);if(vr)return -1;
    long x=find_index(t,a->provider_id);if(x>=0&&a->sequence<=t->entries[x].announcement.sequence)return -1;
    uint8_t*copy=malloc(slen);if(!copy)return -1;memcpy(copy,sig,slen);
    if(x<0){if(reserve(t,t->count+1)){free(copy);return -1;}x=(long)t->count++;memset(&t->entries[x],0,sizeof(t->entries[x]));}
    else free(t->entries[x].signature);
    t->entries[x].announcement=*a;t->entries[x].signature=copy;t->entries[x].signature_len=slen;t->entries[x].learned_height=h;return 0;
}
size_t qrx_storage_discovery_prune(QrxStorageDiscoveryTable*t,uint64_t h){if(!t)return 0;size_t w=0,removed=0;for(size_t i=0;i<t->count;i++){if(h>t->entries[i].announcement.valid_until_height){entry_free(&t->entries[i]);removed++;continue;}if(w!=i)t->entries[w]=t->entries[i];w++;}t->count=w;return removed;}
int qrx_storage_discovery_merge(QrxStorageDiscoveryTable*dst,QrxDB*db,const QrxStorageDiscoveryTable*src,uint64_t h,QrxStorageProviderKeyLookupFn lookup,void*kctx){
    if(!dst||!src)return -1;int accepted=0;for(size_t i=0;i<src->count;i++){const QrxStorageDiscoveryEntry*e=&src->entries[i];if(qrx_storage_discovery_ingest(dst,db,&e->announcement,e->signature,e->signature_len,h,lookup,kctx)==0)accepted++;}return accepted;
}
static int discovery_sources_for_contract_state(QrxStorageDiscoveryTable*t,QrxDB*db,const char*cid,uint32_t shard_count,uint64_t h,int upload,QrxShardProviderSource*out,size_t cap,size_t*nout){
    if(!t||!db||!cid||!*cid||!out||!nout||!cap)return -1;*nout=0;qrx_storage_discovery_prune(t,h);
    QrxShardProviderSource tmp[QRX_STORAGE_MAX_FETCH_SOURCES];size_t n=0;
    if(shard_count>QRX_STORAGE_MAX_FETCH_SOURCES)shard_count=QRX_STORAGE_MAX_FETCH_SOURCES;
    for(uint32_t s=0;s<shard_count&&n<QRX_STORAGE_MAX_FETCH_SOURCES;s++){
        QrxStorageAssignmentRecord ar;if(qrx_storage_assignment_get(db,cid,s,&ar))continue;if(upload){if(ar.state!=QRX_ASSIGN_PENDING&&ar.state!=QRX_ASSIGN_ACTIVE&&ar.state!=QRX_ASSIGN_REPAIRING)continue;}else if(ar.state!=QRX_ASSIGN_ACTIVE)continue;
        const QrxStorageProviderAnnouncement*a=qrx_storage_discovery_find(t,ar.provider_id);if(!a||qrx_storage_announcement_validate(a,h))continue;
        tmp[n].provider_id=a->provider_id;tmp[n].endpoint=a->endpoint;tmp[n].object_id_hex=ar.object_id;tmp[n].source.shard_index=s;tmp[n].source.expected_latency_ms=a->observed_latency_ms;
        tmp[n].source.throughput_bps=a->observed_throughput_bps;tmp[n].source.reliability_bps=a->reliability_bps;n++;
    }
    if(!n)return -1;QrxShardSource raw[QRX_STORAGE_MAX_FETCH_SOURCES];for(size_t i=0;i<n;i++)raw[i]=tmp[i].source;QrxShardFetchPlan plan;
    if(qrx_storage_fetch_plan(raw,n,n<QRX_STORAGE_STANDARD_DATA_SHARDS?n:QRX_STORAGE_STANDARD_DATA_SHARDS,0,&plan))return -1;
    size_t take=n<cap?n:cap;for(size_t i=0;i<take;i++){uint32_t shard=plan.ordered_shards[i];size_t j=0;for(;j<n;j++)if(tmp[j].source.shard_index==shard)break;if(j==n)return -1;out[i]=tmp[j];}
    *nout=take;return 0;
}
int qrx_storage_discovery_sources_for_contract(QrxStorageDiscoveryTable*t,QrxDB*db,const char*cid,uint32_t shard_count,uint64_t h,QrxShardProviderSource*out,size_t cap,size_t*nout){return discovery_sources_for_contract_state(t,db,cid,shard_count,h,0,out,cap,nout);}
int qrx_storage_discovery_sources_for_contract_upload(QrxStorageDiscoveryTable*t,QrxDB*db,const char*cid,uint32_t shard_count,uint64_t h,QrxShardProviderSource*out,size_t cap,size_t*nout){return discovery_sources_for_contract_state(t,db,cid,shard_count,h,1,out,cap,nout);}

/* 0.0.8.64: bounded binary envelope. Persisting the signature is essential:
 * cache restore re-runs the same ingest verification as live gossip. */
static void put_u32_be(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static uint32_t get_u32_be(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
int qrx_storage_discovery_wire_encode(const QrxStorageProviderAnnouncement*a,const uint8_t*sig,size_t sl,uint8_t**out,size_t*outn){
    if(!a||!sig||!sl||sl>65535||!out||!outn)return -1;uint8_t*c=NULL;size_t cn=0;if(qrx_storage_announcement_canonical(a,&c,&cn)||cn>65535)return -1;
    size_t n=8+4+cn+4+sl;uint8_t*b=malloc(n);if(!b){free(c);return -1;}memcpy(b,"QRXDISC1",8);put_u32_be(b+8,(uint32_t)cn);memcpy(b+12,c,cn);put_u32_be(b+12+cn,(uint32_t)sl);memcpy(b+16+cn,sig,sl);free(c);*out=b;*outn=n;return 0;
}
int qrx_storage_discovery_wire_decode(const uint8_t*in,size_t n,QrxStorageProviderAnnouncement*a,uint8_t**sig,size_t*sl){
    if(!in||n<16||!a||!sig||!sl||memcmp(in,"QRXDISC1",8))return -1;uint32_t cn=get_u32_be(in+8);if(cn>65535||12ULL+cn+4>n)return -1;uint32_t sn=get_u32_be(in+12+cn);if(!sn||sn>65535||16ULL+cn+sn!=n)return -1;
    char*s=malloc((size_t)cn+1);if(!s)return -1;memcpy(s,in+12,cn);s[cn]=0;memset(a,0,sizeof(*a));unsigned ver=0,lat=0,rel=0;unsigned long long seq=0,vf=0,vu=0,cap=0,av=0,tp=0;
    char pid[129]={0},ep[QRX_STORAGE_DISCOVERY_MAX_ENDPOINT]={0};int m=sscanf(s,"qrx-storage-discovery-v1|%u|%128[^|]|%llu|%llu|%llu|%llu|%llu|%u|%llu|%u|%255s",&ver,pid,&seq,&vf,&vu,&cap,&av,&lat,&tp,&rel,ep);free(s);if(m!=11)return -1;
    a->version=ver;snprintf(a->provider_id,sizeof(a->provider_id),"%s",pid);a->sequence=seq;a->valid_from_height=vf;a->valid_until_height=vu;a->capabilities=cap;a->available_bytes=av;a->observed_latency_ms=lat;a->observed_throughput_bps=tp;a->reliability_bps=rel;snprintf(a->endpoint,sizeof(a->endpoint),"%s",ep);
    uint8_t*x=malloc(sn);if(!x)return -1;memcpy(x,in+16+cn,sn);*sig=x;*sl=sn;return 0;
}
int qrx_storage_discovery_cache_save(const QrxStorageDiscoveryTable*t,const char*path){if(!t||!path)return -1;char tmp[1200];snprintf(tmp,sizeof(tmp),"%s.tmp",path);FILE*f=fopen(tmp,"wb");if(!f)return -1;if(fwrite("QRXDC01\n",1,8,f)!=8){fclose(f);remove(tmp);return -1;}for(size_t i=0;i<t->count;i++){uint8_t*b=NULL;size_t n=0;if(qrx_storage_discovery_wire_encode(&t->entries[i].announcement,t->entries[i].signature,t->entries[i].signature_len,&b,&n)){fclose(f);remove(tmp);return -1;}uint8_t h[4];put_u32_be(h,(uint32_t)n);if(fwrite(h,1,4,f)!=4||fwrite(b,1,n,f)!=n){free(b);fclose(f);remove(tmp);return -1;}free(b);}if(fclose(f)){remove(tmp);return -1;}remove(path);return rename(tmp,path)==0?0:-1;}
int qrx_storage_discovery_cache_load(QrxStorageDiscoveryTable*t,QrxDB*db,const char*path,uint64_t h,QrxStorageProviderKeyLookupFn lookup,void*kctx){if(!t||!db||!path||!lookup)return -1;FILE*f=fopen(path,"rb");if(!f)return -1;char magic[8];if(fread(magic,1,8,f)!=8||memcmp(magic,"QRXDC01\n",8)){fclose(f);return -1;}int accepted=0;for(;;){uint8_t z[4];size_t r=fread(z,1,4,f);if(!r)break;if(r!=4){accepted=-1;break;}uint32_t n=get_u32_be(z);if(!n||n>131072){accepted=-1;break;}uint8_t*b=malloc(n);if(!b||fread(b,1,n,f)!=n){free(b);accepted=-1;break;}QrxStorageProviderAnnouncement a;uint8_t*s=NULL;size_t sn=0;if(!qrx_storage_discovery_wire_decode(b,n,&a,&s,&sn)&&!qrx_storage_discovery_ingest(t,db,&a,s,sn,h,lookup,kctx))accepted++;free(s);free(b);}fclose(f);return accepted;}
