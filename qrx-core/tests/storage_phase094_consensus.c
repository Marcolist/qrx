#include "resource/qrx_storage_consensus.h"
#include "resource/qrx_resource.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned long long getu64(QrxDB *db,const char *k){char b[128]={0};if(qrxdb_get(db,k,b,sizeof(b))!=0)return 0;return strtoull(b,NULL,10);}
int main(void){
    char tmp[]="/tmp/qrx-storage-consensus-XXXXXX"; assert(mkdtemp(tmp));
    QrxDB db; assert(qrxdb_init(&db,tmp)==0);
    const char *owner="qrx1storageowner";
    const char *cid="contract-alpha";
    char manifest[129]; memset(manifest,'0',128); manifest[128]=0;
    char payload[1024]; snprintf(payload,sizeof(payload),"contract_id=%s;profile=STANDARD;logical_bytes=1073741824;end_height=20000;base_atoms_per_gib_epoch=10000;manifest_root_hex=%s",cid,manifest);
    QrxServiceEconomicEffect e={0};
    assert(qrx_storage_consensus_prepare(tmp,"STORAGE_CONTRACT_CREATE",owner,owner,1000000,payload,"tx-storage-create",100,&e)==0);
    assert(e.debit_atoms==1000000);
    assert(e.development_credit_atoms==5000);
    assert(e.self_credit_atoms==0 && e.recipient_credit_atoms==0 && e.protocol_fee_atoms==0);
    QrxDBBatch b; assert(qrxdb_batch_begin(&db,&b)==0);
    assert(qrx_storage_consensus_stage(&db,&b,tmp,"STORAGE_CONTRACT_CREATE",owner,owner,1000000,payload,"tx-storage-create",100)==0);
    assert(qrxdb_batch_commit(&b)==0);
    assert(getu64(&db,"consensus:storage:provider_escrow")==975000);
    assert(getu64(&db,"consensus:storage:resilience")==20000);
    QrxStorageContractState c; assert(qrx_storage_contract_get(&db,cid,&c)==0);
    assert(c.original_atoms==1000000 && c.development_atoms==5000 && c.provider_escrow_atoms==975000 && c.resilience_atoms==20000);
    assert(c.shard_count==14 && c.logical_bytes==1073741824ULL);
    assert(qrxdb_close(&db)==0);
    puts("PASS: storage contract economic effect and locked pools stage atomically");
    return 0;
}
