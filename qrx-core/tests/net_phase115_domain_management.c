#include "net/qrx_net_consensus.h"
#include "net/qrx_net_name.h"
#include "net/qrx_net_registry.h"
#include "storage/qrx_drive_manifest.h"
#include "qrxdb.h"
#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct { int n; uint64_t seq[8]; } Scan;
static int cb(const QrxDomainRecord *r, void *vp){ Scan *s=vp; if(s->n<8)s->seq[s->n]=r->sequence; s->n++; return 0; }
int main(void){
 char tmp[]="/tmp/qrx-net-domain-mgmt-XXXXXX"; assert(mkdtemp(tmp));
 QrxDB db; assert(qrxdb_init(&db,tmp)==0);
 EVP_PKEY *mldsa=NULL; assert(qrx_drive_manifest_generate_signing_key(&mldsa)==0);
 uint8_t pc[64]; assert(qrx_domain_pubkey_commitment(mldsa,pc)==0);
 EVP_PKEY_CTX *ec=EVP_PKEY_CTX_new_from_name(NULL,"ED25519",NULL); assert(ec); EVP_PKEY *ed=NULL; assert(EVP_PKEY_keygen_init(ec)>0&&EVP_PKEY_generate(ec,&ed)>0); EVP_PKEY_CTX_free(ec);
 assert(qrx_domain_pubkey_commitment(ed,pc)!=0); EVP_PKEY_free(ed);
 const char *owner="qrx1owner", *new_owner="qrx1newowner", *name="portal.qrx";
 QrxDomainPrice pr; assert(qrx_domain_price(name,QRX_DOMAIN_DEFAULT_BASE_ANNUAL_ATOMS,0,&pr)==0);
 uint8_t web[64]; memset(web,0xAB,sizeof(web)); assert(qrx_domain_pubkey_commitment(mldsa,pc)==0);
 char wh[129],ph[129]; static const char hx[]="0123456789abcdef"; for(int i=0;i<64;i++){wh[i*2]=hx[web[i]>>4];wh[i*2+1]=hx[web[i]&15];ph[i*2]=hx[pc[i]>>4];ph[i*2+1]=hx[pc[i]&15];}wh[128]=ph[128]=0;
 char payload[1024]; snprintf(payload,sizeof(payload),"name=%s;years=1;qub_address=%s;web_manifest_root_hex=%s;publishing_commitment_hex=%s",name,owner,wh,ph);
 QrxDBBatch b; uint64_t amount=pr.annual_atoms+pr.reservation_bond_atoms; assert(qrxdb_batch_begin(&db,&b)==0); assert(qrx_net_consensus_stage(&db,&b,tmp,"DOMAIN_REGISTER",owner,owner,amount,payload,"reg",100)==0); assert(qrxdb_batch_commit(&b)==0);
 QrxDomainRecord r; assert(qrx_net_registry_get(&db,name,&r)==0 && r.sequence==1 && !strcmp(r.owner,owner));
 char upd[1024]; snprintf(upd,sizeof(upd),"name=%s;sequence=1;qub_mode=CLEAR;qub_address=-;web_mode=KEEP;web_manifest_root_hex=-;publishing_mode=KEEP;publishing_commitment_hex=-",name);
 assert(qrxdb_batch_begin(&db,&b)==0); assert(qrx_net_consensus_stage(&db,&b,tmp,"DOMAIN_UPDATE",owner,owner,0,upd,"upd",101)==0); assert(qrxdb_batch_commit(&b)==0);
 assert(qrx_net_consensus_prepare(tmp,"DOMAIN_UPDATE",owner,owner,0,upd,"replay",102,(QrxServiceEconomicEffect[1]){{0}})!=0);
 char tr[256]; snprintf(tr,sizeof(tr),"name=%s;sequence=2",name); assert(qrxdb_batch_begin(&db,&b)==0); assert(qrx_net_consensus_stage(&db,&b,tmp,"DOMAIN_TRANSFER",owner,new_owner,0,tr,"xfer",103)==0); assert(qrxdb_batch_commit(&b)==0);
 assert(qrx_net_registry_get(&db,name,&r)==0 && r.sequence==3 && !strcmp(r.owner,new_owner));
 for(int i=0;i<64;i++)assert(r.web_manifest_root[i]==0 && r.publishing_key_commitment[i]==0); assert(!r.qub_address[0]);
 Scan hist={0}; assert(qrx_net_registry_history(&db,name,cb,&hist)==3 && hist.n==3); assert(hist.seq[0]==1 && hist.seq[1]==2 && hist.seq[2]==3);
 Scan own={0}; assert(qrx_net_registry_list(&db,new_owner,cb,&own)==1 && own.n==1 && own.seq[0]==3);
 EVP_PKEY_free(mldsa); assert(qrxdb_close(&db)==0); puts("PASS: QRX-Net domain management enforces ML-DSA publishing identity, sequence anti-replay, history, and transfer record clearing"); return 0;
}
