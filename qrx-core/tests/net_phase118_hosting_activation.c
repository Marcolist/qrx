#include "net/qrx_net_publisher.h"
#include "storage/qrx_drive_manifest.h"
#include "resource/qrx_storage_consensus.h"
#include "resource/qrx_storage_repair.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
static void wr(const char*p,const char*s){FILE*f=fopen(p,"wb");assert(f);assert(fwrite(s,1,strlen(s),f)==strlen(s));assert(fclose(f)==0);}
static void provider(QrxDB*db,int i){char k[256],v[1200];snprintf(k,sizeof(k),"storage/provider/provider-%02d",i);snprintf(v,sizeof(v),"1|owner-%02d|2|100000000|1099511627776|0|0|10000|10000|10000|9000|operator-%02d|AS%05d|R%02d|AS%05d|R%02d|1",i,i,64500+i,i,64500+i,i);assert(qrxdb_put(db,k,v)==0);}
static void hashhex(int seed,char out[129]){static const char*x="0123456789abcdef";for(int i=0;i<64;i++){unsigned b=(unsigned)(seed+i*17);out[i*2]=x[(b>>4)&15];out[i*2+1]=x[b&15];}out[128]=0;}
int main(void){
 char tmp[]="/tmp/qrxhost118-XXXXXX";assert(mkdtemp(tmp));char web[512],fsd[512],cat[512],p[512];snprintf(web,sizeof(web),"%s/web",tmp);snprintf(fsd,sizeof(fsd),"%s/fs",tmp);snprintf(cat,sizeof(cat),"%s/catalog",tmp);assert(mkdir(web,0700)==0);snprintf(p,sizeof(p),"%s/index.html",web);wr(p,"<html>distributed qrx-net</html>");
 QrxStorageFs*fs=NULL;assert(qrx_storage_fs_open(fsd,0,0,&fs)==0);EVP_PKEY*k=NULL;assert(qrx_drive_manifest_generate_signing_key(&k)==0);QrxNetPublishJob j;assert(qrx_net_publisher_publish_directory(cat,fs,web,"host.qrx",4,100,k,&j)==0);QrxDrivePreparedUpload prep;assert(qrx_net_publisher_load_prepared(&j,k,&prep)==0);assert(!strcmp(prep.manifest.crypto_suite,QRX_DRIVE_SUITE_PUBLIC_SIGNED));assert(prep.total_shards==14);
 QrxDB db;assert(qrxdb_init(&db,tmp)==0);for(int i=0;i<24;i++)provider(&db,i);char bh[129];hashhex(7,bh);assert(qrxdb_chain_put_block(&db,100,bh,"block")==0);
 QrxNetHostStep st;assert(qrx_net_publisher_host_next_step(&db,"qrx1owner",&j,&prep,100,30,10000,&st)==0);assert(st.kind==QRX_NET_HOST_STEP_CREATE);char mr[129];static const char*x="0123456789abcdef";for(int i=0;i<64;i++){mr[i*2]=x[j.manifest_root[i]>>4];mr[i*2+1]=x[j.manifest_root[i]&15];}mr[128]=0;assert(strstr(st.payload,mr));assert(!strstr(st.payload,"qrx-drive-pq-v1"));snprintf(j.storage_contract_id,sizeof(j.storage_contract_id),"%s",st.contract_id);assert(qrx_net_publisher_job_save(&j)==0);
 QrxServiceEconomicEffect e={0};assert(qrx_storage_consensus_prepare(tmp,st.tx_type,"qrx1owner",st.to,st.amount_atoms,st.payload,"webcontract",101,&e)==0);QrxDBBatch b;assert(qrxdb_batch_begin(&db,&b)==0);assert(qrx_storage_consensus_stage(&db,&b,tmp,st.tx_type,"qrx1owner",st.to,st.amount_atoms,st.payload,"webcontract",101)==0);assert(qrxdb_batch_commit(&b)==0);
 char providers[14][129];memset(providers,0,sizeof(providers));for(unsigned i=0;i<14;i++){assert(qrx_net_publisher_host_next_step(&db,"qrx1owner",&j,&prep,100,30,10000,&st)==0);assert(st.kind==QRX_NET_HOST_STEP_ASSIGN&&st.shard_index==i);snprintf(providers[i],sizeof(providers[i]),"%s",st.to);char txid[64];snprintf(txid,sizeof(txid),"assign-%u",i);assert(qrx_storage_consensus_prepare(tmp,st.tx_type,"qrx1owner",st.to,0,st.payload,txid,101,&e)==0);assert(qrxdb_batch_begin(&db,&b)==0);assert(qrx_storage_consensus_stage(&db,&b,tmp,st.tx_type,"qrx1owner",st.to,0,st.payload,txid,101)==0);assert(qrxdb_batch_commit(&b)==0);}
 assert(qrx_net_publisher_host_next_step(&db,"qrx1owner",&j,&prep,100,30,10000,&st)==0);assert(st.kind==QRX_NET_HOST_STEP_READY_UPLOAD);snprintf(j.transfer_id,sizeof(j.transfer_id),"upload-started");assert(qrx_net_publisher_job_save(&j)==0);assert(qrx_net_publisher_host_next_step(&db,"qrx1owner",&j,&prep,100,30,10000,&st)==0);assert(st.kind==QRX_NET_HOST_STEP_WAIT_ACTIVE);
 for(unsigned i=0;i<10;i++){char payload[256],txid[64];snprintf(payload,sizeof(payload),"contract_id=%s;shard_index=%u",j.storage_contract_id,i);snprintf(txid,sizeof(txid),"accept-%u",i);assert(qrx_storage_consensus_prepare(tmp,"STORAGE_ASSIGN_ACCEPT",providers[i],providers[i],0,payload,txid,102,&e)==0);assert(qrxdb_batch_begin(&db,&b)==0);assert(qrx_storage_consensus_stage(&db,&b,tmp,"STORAGE_ASSIGN_ACCEPT",providers[i],providers[i],0,payload,txid,102)==0);assert(qrxdb_batch_commit(&b)==0);}
 assert(qrx_net_publisher_refresh_storage(&db,&j,&prep)==0);assert(j.active_shards==10&&j.status==QRX_NET_PUBLISH_STATUS_READY_TO_ACTIVATE);assert(qrx_net_publisher_host_next_step(&db,"qrx1owner",&j,&prep,102,30,10000,&st)==0);assert(st.kind==QRX_NET_HOST_STEP_READY_ACTIVATE);
 /* Exact assignment binding is mandatory: a locally altered prepared object id invalidates readiness. */
 char keep=prep.shard_object_ids[0][0];prep.shard_object_ids[0][0]=keep=='a'?'b':'a';assert(qrx_net_publisher_refresh_storage(&db,&j,&prep)!=0);prep.shard_object_ids[0][0]=keep;
 qrxdb_close(&db);qrx_drive_prepared_upload_free(&prep);EVP_PKEY_free(k);qrx_storage_fs_close(fs);puts("PASS: PUBLIC_SIGNED site root is bound to deterministic 10+4 storage assignments and activates only after 10 exact ACTIVE shards");return 0;
}
