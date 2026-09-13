#include "net/qrx_net_consensus.h"
#include "net/qrx_net_name.h"
#include "net/qrx_net_registry.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned long long getu64(QrxDB *db,const char *k){char b[128]={0};if(qrxdb_get(db,k,b,sizeof(b))!=0)return 0;return strtoull(b,NULL,10);}
int main(void){
    char tmp[]="/tmp/qrx-net-consensus-XXXXXX"; assert(mkdtemp(tmp));
    QrxDB db; assert(qrxdb_init(&db,tmp)==0);
    const char *owner="qrx1domainowner"; const char *name="alpha.qrx";
    QrxDomainPrice pr; assert(qrx_domain_price(name,QRX_DOMAIN_DEFAULT_BASE_ANNUAL_ATOMS,0,&pr)==0); assert(pr.mode==QRX_DOMAIN_PRICE_FIXED);
    uint64_t amount=pr.annual_atoms+pr.reservation_bond_atoms;
    char payload[512]; snprintf(payload,sizeof(payload),"name=%s;years=1;qub_address=-;web_manifest_root_hex=-;publishing_commitment_hex=-",name);
    QrxServiceEconomicEffect e={0}; assert(qrx_net_consensus_prepare(tmp,"DOMAIN_REGISTER",owner,owner,amount,payload,"tx-domain-register",100,&e)==0);
    QrxDomainFeeSplit fs; assert(qrx_domain_fee_split(pr.annual_atoms,&fs)==0);
    assert(e.debit_atoms==amount && e.development_credit_atoms==fs.development_atoms && e.protocol_fee_atoms==fs.registry_atoms);
    assert(e.development_credit_atoms+e.protocol_fee_atoms+pr.reservation_bond_atoms==amount);
    QrxDBBatch b; assert(qrxdb_batch_begin(&db,&b)==0); assert(qrx_net_consensus_stage(&db,&b,tmp,"DOMAIN_REGISTER",owner,owner,amount,payload,"tx-domain-register",100)==0); assert(qrxdb_batch_commit(&b)==0);
    assert(getu64(&db,"consensus:qrxnet:domain_bonds")==pr.reservation_bond_atoms);
    QrxDomainRecord r; assert(qrx_net_registry_get(&db,name,&r)==0); assert(!strcmp(r.owner,owner)); assert(r.sequence==1);
    char renew[256]; snprintf(renew,sizeof(renew),"name=%s;sequence=1;years=1",name); memset(&e,0,sizeof(e)); assert(qrx_net_consensus_prepare(tmp,"DOMAIN_RENEW",owner,owner,pr.annual_atoms,renew,"tx-domain-renew",200,&e)==0); assert(e.development_credit_atoms+e.protocol_fee_atoms==pr.annual_atoms);
    assert(qrxdb_close(&db)==0);
    puts("PASS: QRX-Net domain rent, development share, fee-pool share and reservation bond balance exactly");
    return 0;
}
