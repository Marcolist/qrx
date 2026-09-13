#include "storage/qrx_storage_discovery.h"
#include "storage/qrx_drive_manifest.h"
#include "resource/qrx_storage_market.h"
#include "resource/qrx_storage_repair.h"
#include "qrxdb.h"
#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct { char ids[14][32]; EVP_PKEY *keys[14]; } KeySet;
static int key_lookup(void *v,const char *id,EVP_PKEY **out){
    KeySet*k=v;*out=NULL;for(int i=0;i<14;i++)if(!strcmp(id,k->ids[i])){
        if(EVP_PKEY_up_ref(k->keys[i])!=1)return -1;*out=k->keys[i];return 0;
    }return -1;
}
static void put_provider(QrxDB*db,const char*id){
    char k[256],v[1024];snprintf(k,sizeof(k),"storage/provider/%s",id);
    snprintf(v,sizeof(v),"1|%s|%u|1000|1099511627776|0|0|9950|9950|10000|9900|op-%s|AS64500|EU|AS64500|EU|1",id,QRX_PROVIDER_ACTIVE,id);
    assert(qrxdb_put(db,k,v)==0);
}
static void put_assignment(QrxDB*db,const char*cid,unsigned shard,const char*pid){
    char k[256],v[1024],rh[129],oid[65];memset(rh,'0',128);rh[128]=0;memset(oid,'a'+(shard%6),64);oid[64]=0;
    snprintf(k,sizeof(k),"storage/assignment/%s/%010u",cid,shard);
    snprintf(v,sizeof(v),"1|%s|%u|4096|%s|%s|1|100|1|100|0",pid,QRX_ASSIGN_ACTIVE,oid,rh);assert(qrxdb_put(db,k,v)==0);
}
static QrxStorageProviderAnnouncement ann(const char*pid,unsigned i,uint64_t seq){
    QrxStorageProviderAnnouncement a;memset(&a,0,sizeof(a));a.version=QRX_STORAGE_DISCOVERY_VERSION;snprintf(a.provider_id,sizeof(a.provider_id),"%s",pid);
    a.sequence=seq;a.valid_from_height=90;a.valid_until_height=200;a.capabilities=QRX_STORAGE_DISCOVERY_CAP_RANGE_READ|QRX_STORAGE_DISCOVERY_CAP_RESUME|QRX_STORAGE_DISCOVERY_CAP_QUIC;
    a.available_bytes=1ULL<<40;a.observed_latency_ms=100-i*3;a.observed_throughput_bps=100000000ULL+i*1000000ULL;a.reliability_bps=9800+i*10;
    snprintf(a.endpoint,sizeof(a.endpoint),"quic://10.0.0.%u:4433",i+1);return a;
}
int main(void){
    char tmp[]="/tmp/qrx-discovery-XXXXXX";assert(mkdtemp(tmp));QrxDB db;assert(qrxdb_init(&db,tmp)==0);KeySet ks;memset(&ks,0,sizeof(ks));
    QrxStorageDiscoveryTable gossip,local;qrx_storage_discovery_init(&gossip);qrx_storage_discovery_init(&local);
    for(unsigned i=0;i<14;i++){
        snprintf(ks.ids[i],sizeof(ks.ids[i]),"provider-%02u",i);assert(qrx_drive_manifest_generate_signing_key(&ks.keys[i])==0);put_provider(&db,ks.ids[i]);put_assignment(&db,"contract-A",i,ks.ids[i]);
        QrxStorageProviderAnnouncement a=ann(ks.ids[i],i,1);uint8_t*sig=NULL;size_t sl=0;assert(qrx_storage_announcement_sign(ks.keys[i],&a,&sig,&sl)==0);
        assert(qrx_storage_discovery_ingest(&gossip,&db,&a,sig,sl,100,key_lookup,&ks)==0);free(sig);
    }
    assert(gossip.count==14);
    assert(qrx_storage_discovery_merge(&local,&db,&gossip,100,key_lookup,&ks)==14);assert(local.count==14);
    /* anti-replay */
    QrxStorageProviderAnnouncement stale=ann(ks.ids[0],0,1);uint8_t*ss=NULL;size_t sn=0;assert(qrx_storage_announcement_sign(ks.keys[0],&stale,&ss,&sn)==0);assert(qrx_storage_discovery_ingest(&local,&db,&stale,ss,sn,100,key_lookup,&ks)!=0);free(ss);
    /* forged endpoint under another provider key is rejected */
    QrxStorageProviderAnnouncement forged=ann(ks.ids[1],1,2);assert(qrx_storage_announcement_sign(ks.keys[0],&forged,&ss,&sn)==0);assert(qrx_storage_discovery_ingest(&local,&db,&forged,ss,sn,100,key_lookup,&ks)!=0);free(ss);
    /* newer signed gossip replaces prior endpoint/metrics */
    QrxStorageProviderAnnouncement newer=ann(ks.ids[0],0,2);newer.observed_latency_ms=4;snprintf(newer.endpoint,sizeof(newer.endpoint),"qrxp2p://peer-zero");assert(qrx_storage_announcement_sign(ks.keys[0],&newer,&ss,&sn)==0);assert(qrx_storage_discovery_ingest(&local,&db,&newer,ss,sn,100,key_lookup,&ks)==0);free(ss);assert(qrx_storage_discovery_find(&local,ks.ids[0])->sequence==2);
    QrxShardProviderSource src[14];size_t n=0;assert(qrx_storage_discovery_sources_for_contract(&local,&db,"contract-A",14,100,src,14,&n)==0&&n==14);
    int seen[14]={0};for(size_t i=0;i<n;i++){assert(src[i].source.shard_index<14);seen[src[i].source.shard_index]=1;}for(int i=0;i<14;i++)assert(seen[i]);
    /* expired announcements disappear and can no longer source downloads */
    assert(qrx_storage_discovery_prune(&local,201)==14);assert(local.count==0);assert(qrx_storage_discovery_sources_for_contract(&local,&db,"contract-A",14,201,src,14,&n)!=0);
    qrx_storage_discovery_free(&gossip);qrx_storage_discovery_free(&local);for(int i=0;i<14;i++)EVP_PKEY_free(ks.keys[i]);qrxdb_close(&db);
    puts("PASS: signed gossip discovery is chain-bound, anti-replay, expiring, mergeable and resolves authoritative 10-of-14 download sources");return 0;
}
