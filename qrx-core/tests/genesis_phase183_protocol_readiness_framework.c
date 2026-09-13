#include "chain_params.h"
#include "economics/qrx_economics.h"
#include "resource/qrx_activation_readiness.h"
#include "resource/qrx_resource.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GIB (1024ULL*1024ULL*1024ULL)

static void seed_storage(QrxDBBatch *b,uint64_t h){
    for(int i=0;i<14;i++){
        char id[64],k[320],v[1400],asn[32],region[32],hv[64];
        snprintf(id,sizeof(id),"p%02d",i);snprintf(asn,sizeof(asn),"AS%d",64500+(i%4));snprintf(region,sizeof(region),"r%d",i%3);
        snprintf(k,sizeof(k),"storage/provider/%s",id);
        snprintf(v,sizeof(v),"1|%s|2|1000000|%llu|%llu|0|9900|9900|9900|9900|operator-%02d|%s|%s|%s|%s|1",id,(unsigned long long)GIB,(unsigned long long)(GIB*3/10),i,asn,region,asn,region);
        assert(qrxdb_batch_put(b,k,v)==0);
        snprintf(k,sizeof(k),"storage/provider-activation-height/%s",id);snprintf(hv,sizeof(hv),"%llu",(unsigned long long)(h-QRX_DRIVE_READINESS_SOAK_BLOCKS));assert(qrxdb_batch_put(b,k,hv)==0);
    }
    for(int i=0;i<3;i++){char k[128];snprintf(k,sizeof(k),"storage/contract/c%d",i);char cv[256];snprintf(cv,sizeof(cv),"1|owner|100|%llu|0|0|0|0|0|0|0|0|0|0|0|0|0|1",(unsigned long long)GIB);assert(qrxdb_batch_put(b,k,cv)==0);}
    for(int i=0;i<3;i++){char k[128];snprintf(k,sizeof(k),"qrxnet/domain/site%d.qrx",i);assert(qrxdb_batch_put(b,k,"x")==0);}
}
static void seed_compute(QrxDBBatch *b){
    for(int i=0;i<8;i++){char k[160];snprintf(k,sizeof(k),"compute/provider-identity/qrx-provider-%d",i);assert(qrxdb_batch_put(b,k,"preflight")==0);}
    static const char*n[]={"verification_stable","rewards_simulated","fasttrack_fair","standard_tasks_protected","fasttrack_dev_share_transparent","model_integrity_verified","provider_fraud_controlled","resource_globe_privacy_checked","server_independence_verified"};
    for(size_t i=0;i<sizeof(n)/sizeof(n[0]);i++){char k[200];snprintf(k,sizeof(k),"consensus:compute:readiness:%s",n[i]);assert(qrxdb_batch_put(b,k,"1")==0);}
}
static void schedule(QrxDBBatch *b,uint64_t h,const char *feature,const char *id){char k[320],v[512];snprintf(k,sizeof(k),"governance:protocol:schedule:%020llu:%s",(unsigned long long)h,id);snprintf(v,sizeof(v),"9|3|4|%s|%s|tx-%s",feature,id,id);assert(qrxdb_batch_put(b,k,v)==0);}

int main(void){
    char dir[]="/tmp/qrx-phase183-XXXXXX";assert(mkdtemp(dir));
    assert(qrx_chain_write_genesis(dir,"qrx-mainnet-phase183","9","QRXP183","Phase183",20,5000,(long long)QRX_MAX_SUPPLY_ATOMS,(long long)QRX_INITIAL_BLOCK_REWARD_ATOMS,0,QRX_BLOCK_TIME_SECONDS,100,524288,8192,30,70,0,"qrx1dev-phase183",1789488000LL)==0);
    const uint64_t base=200000;QrxDB db;QrxDBBatch b;assert(qrxdb_init(&db,dir)==0);assert(qrxdb_batch_begin(&db,&b)==0);seed_storage(&b,base);seed_compute(&b);assert(qrxdb_batch_commit(&b)==0);qrxdb_close(&db);

    QrxProtocolActivationReadiness r;
    assert(qrx_protocol_activation_readiness(dir,QRX_NET_V1_FEATURE_FLAG,base,QRX_NET_V1_PLANNED_TARGET_TIME+1,&r)==0);assert(r.status==QRX_PROTOCOL_READINESS_LOCKED);
    assert(qrx_protocol_activation_readiness(dir,QRX_ADVERTISING_V1_FEATURE_FLAG,base,QRX_ADVERTISING_V1_PLANNED_TARGET_TIME+1,&r)==0);assert(r.status==QRX_PROTOCOL_READINESS_LOCKED);
    assert(qrx_protocol_activation_readiness(dir,QRX_COMPUTE_POUC_V1_FEATURE_FLAG,base,QRX_COMPUTE_POUC_V1_PLANNED_TARGET_TIME+1,&r)==0);assert(r.status==QRX_PROTOCOL_READINESS_LOCKED);

    assert(qrxdb_init(&db,dir)==0);assert(qrxdb_batch_begin(&db,&b)==0);schedule(&b,base+100,QRX_DRIVE_V1_FEATURE_FLAG,"drive");assert(qrxdb_batch_commit(&b)==0);qrxdb_close(&db);
    uint64_t hdrive=base+100;
    assert(qrx_protocol_activation_readiness(dir,QRX_NET_V1_FEATURE_FLAG,hdrive+100,QRX_NET_V1_PLANNED_TARGET_TIME+1,&r)==0);assert(r.status==QRX_PROTOCOL_READINESS_SOAKING);
    assert(qrx_protocol_activation_readiness(dir,QRX_NET_V1_FEATURE_FLAG,hdrive+QRX_NET_READINESS_SOAK_BLOCKS,QRX_NET_V1_PLANNED_TARGET_TIME-1,&r)==0);assert(r.status==QRX_PROTOCOL_READINESS_WAITING_TARGET_DATE);
    assert(qrx_protocol_activation_readiness(dir,QRX_NET_V1_FEATURE_FLAG,hdrive+QRX_NET_READINESS_SOAK_BLOCKS,QRX_NET_V1_PLANNED_TARGET_TIME+1,&r)==0);assert(r.status==QRX_PROTOCOL_READINESS_READY_FOR_GOVERNANCE);assert(r.criteria_passed==r.criteria_required);

    uint64_t netact=hdrive+QRX_NET_READINESS_SOAK_BLOCKS+100;assert(qrxdb_init(&db,dir)==0);assert(qrxdb_batch_begin(&db,&b)==0);schedule(&b,netact,QRX_NET_V1_FEATURE_FLAG,"net");assert(qrxdb_batch_commit(&b)==0);qrxdb_close(&db);
    assert(qrx_protocol_activation_readiness(dir,QRX_NET_V1_FEATURE_FLAG,netact-1,QRX_NET_V1_PLANNED_TARGET_TIME+1,&r)==0);assert(r.status==QRX_PROTOCOL_READINESS_SCHEDULED);
    assert(qrx_protocol_activation_readiness(dir,QRX_NET_V1_FEATURE_FLAG,netact,QRX_NET_V1_PLANNED_TARGET_TIME+1,&r)==0);assert(r.status==QRX_PROTOCOL_READINESS_ACTIVE);

    uint64_t had=netact+QRX_AD_READINESS_SOAK_BLOCKS;
    assert(qrx_protocol_activation_readiness(dir,QRX_ADVERTISING_V1_FEATURE_FLAG,had,QRX_ADVERTISING_V1_PLANNED_TARGET_TIME+1,&r)==0);assert(r.status==QRX_PROTOCOL_READINESS_READY_FOR_GOVERNANCE);assert(r.active_domains>=3);

    uint64_t hc=netact+QRX_COMPUTE_READINESS_SOAK_BLOCKS;
    assert(qrx_protocol_activation_readiness(dir,QRX_COMPUTE_POUC_V1_FEATURE_FLAG,hc,QRX_COMPUTE_POUC_V1_PLANNED_TARGET_TIME+1,&r)==0);assert(r.status==QRX_PROTOCOL_READINESS_READY_FOR_GOVERNANCE);assert(r.compute_providers==8);assert(r.compute_safety_flags==9);assert(r.dependencies_active==2);

    assert(qrx_protocol_activation_readiness(dir,"UNKNOWN_V1",hc,QRX_COMPUTE_POUC_V1_PLANNED_TARGET_TIME+1,&r)==-2);
    puts("phase183: PASS - common protocol readiness framework gates DRIVE -> QRX_NET -> ADVERTISING/COMPUTE with feature-specific soak and criteria");return 0;
}
