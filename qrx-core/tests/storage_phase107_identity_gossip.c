#include "resource/qrx_storage_consensus.h"
#include "storage/qrx_storage_discovery.h"
#include "storage/qrx_drive_manifest.h"
#include <openssl/pem.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *pub_b64(EVP_PKEY *k){BIO*b=BIO_new(BIO_s_mem());assert(b&&PEM_write_bio_PUBKEY(b,k)==1);BUF_MEM*m=NULL;BIO_get_mem_ptr(b,&m);assert(m&&m->length);size_t cap=4*((m->length+2)/3)+1;char*out=malloc(cap);assert(out);int n=EVP_EncodeBlock((unsigned char*)out,(const unsigned char*)m->data,(int)m->length);assert(n>0);out[n]=0;BIO_free(b);return out;}
int main(void){
 char d[]="/tmp/qrx-idgossip-XXXXXX";assert(mkdtemp(d));QrxDB db;assert(qrxdb_init(&db,d)==0);
 /* Active/proven provider record in canonical v1 consensus encoding. */
 assert(qrxdb_put(&db,"storage/provider/provider-A","1|provider-A|2|1000000|1073741824|0|0|10000|10000|10000|9000|operator-A|AS1|EU-WEST|AS1|EU-WEST|1")==0);
 EVP_PKEY*k=NULL;assert(qrx_drive_manifest_generate_signing_key(&k)==0);char*b64=pub_b64(k);size_t pn=strlen(b64)+64;char*p=malloc(pn);snprintf(p,pn,"public_key_pem_b64=%s",b64);
 QrxServiceEconomicEffect e;assert(qrx_storage_consensus_prepare(d,"STORAGE_PROVIDER_BIND_DISCOVERY_KEY","provider-A","provider-A",0,p,"tx-bind",100,&e)==0);
 QrxDBBatch batch;assert(qrxdb_batch_begin(&db,&batch)==0);assert(qrx_storage_consensus_stage(&db,&batch,d,"STORAGE_PROVIDER_BIND_DISCOVERY_KEY","provider-A","provider-A",0,p,"tx-bind",100)==0);assert(qrxdb_batch_commit(&batch)==0);
 EVP_PKEY*loaded=NULL;assert(qrx_storage_provider_discovery_key_lookup(&db,"provider-A",&loaded)==0);assert(loaded);EVP_PKEY_free(loaded);
 QrxStorageProviderAnnouncement a={0};a.version=1;strcpy(a.provider_id,"provider-A");a.sequence=1;a.valid_from_height=90;a.valid_until_height=200;a.capabilities=QRX_STORAGE_DISCOVERY_CAP_RANGE_READ|QRX_STORAGE_DISCOVERY_CAP_RESUME;a.available_bytes=1000000;a.reliability_bps=9900;strcpy(a.endpoint,"qrxp2p://127.0.0.1:19999");uint8_t*sig=NULL;size_t sl=0;assert(qrx_storage_announcement_sign(k,&a,&sig,&sl)==0);
 QrxStorageDiscoveryTable t;qrx_storage_discovery_init(&t);assert(qrx_storage_discovery_ingest(&t,&db,&a,sig,sl,100,qrx_storage_provider_discovery_key_lookup,&db)==0);assert(t.count==1);
 EVP_PKEY*wrong=NULL;assert(qrx_drive_manifest_generate_signing_key(&wrong)==0);uint8_t*bad=NULL;size_t bl=0;a.sequence=2;assert(qrx_storage_announcement_sign(wrong,&a,&bad,&bl)==0);assert(qrx_storage_discovery_ingest(&t,&db,&a,bad,bl,100,qrx_storage_provider_discovery_key_lookup,&db)!=0);assert(t.count==1);
 free(bad);EVP_PKEY_free(wrong);free(sig);qrx_storage_discovery_free(&t);free(p);free(b64);EVP_PKEY_free(k);qrxdb_close(&db);puts("PASS: consensus-bound ML-DSA provider identity gates discovery gossip and rejects a wrong signing key");return 0;
}
