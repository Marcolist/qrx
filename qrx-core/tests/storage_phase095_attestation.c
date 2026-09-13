#include "resource/qrx_storage_consensus.h"
#include "resource/qrx_storage_placement.h"
#include "resource/qrx_storage_market.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void put_provider(QrxDB *db,const char *id){char k[256],v[1024];snprintf(k,sizeof(k),"storage/provider/%s",id);snprintf(v,sizeof(v),"1|%s|%u|1000|1000000|0|0|8000|8000|10000|8000|op-%s|AS0|UNKNOWN|-|-|0",id,QRX_PROVIDER_ACTIVE,id);assert(qrxdb_put(db,k,v)==0);}
int main(void){char tmp[]="/tmp/qrx-attest-XXXXXX";assert(mkdtemp(tmp));QrxDB db;assert(qrxdb_init(&db,tmp)==0);put_provider(&db,"target");put_provider(&db,"att1");put_provider(&db,"att2");put_provider(&db,"att3");uint64_t h=QRX_STORAGE_ATTEST_EPOCH_BLOCKS*2+10,ep=h/QRX_STORAGE_ATTEST_EPOCH_BLOCKS;char p[256];snprintf(p,sizeof(p),"target_provider=target;asn=AS64500;region=ZA-GP;epoch=%llu",(unsigned long long)ep);const char*a[]={"att1","att2","att3"};for(int i=0;i<3;i++){QrxServiceEconomicEffect e={0};assert(qrx_storage_consensus_prepare(tmp,"STORAGE_ATTEST",a[i],a[i],0,p,"tx",h+i,&e)==0);QrxDBBatch b;assert(qrxdb_batch_begin(&db,&b)==0);assert(qrx_storage_consensus_stage(&db,&b,tmp,"STORAGE_ATTEST",a[i],a[i],0,p,"tx",h+i)==0);assert(qrxdb_batch_commit(&b)==0);}QrxStorageProviderState t;assert(qrx_storage_provider_get(&db,"target",&t)==0);assert(!strcmp(t.attested_asn,"AS64500")&&!strcmp(t.attested_region,"ZA-GP")&&t.attestation_epoch==ep);qrxdb_close(&db);puts("PASS: 3 independent bonded/active peer attestations establish failure-domain metadata without a central authority");return 0;}
