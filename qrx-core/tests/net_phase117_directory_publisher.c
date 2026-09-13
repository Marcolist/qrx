#include "net/qrx_net_publisher.h"
#include "storage/qrx_drive_manifest.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#endif
static void wr(const char*p,const char*s){FILE*f=fopen(p,"wb");assert(f);assert(fwrite(s,1,strlen(s),f)==strlen(s));assert(fclose(f)==0);}
int main(void){
 char tmp[]="/tmp/qrxsite117-XXXXXX"; assert(mkdtemp(tmp));
 char web[512],sub[512],fsd[512],cat[512],p[512];snprintf(web,sizeof(web),"%s/web",tmp);snprintf(sub,sizeof(sub),"%s/assets",web);snprintf(fsd,sizeof(fsd),"%s/fs",tmp);snprintf(cat,sizeof(cat),"%s/catalog",tmp);assert(mkdir(web,0700)==0);assert(mkdir(sub,0700)==0);
 snprintf(p,sizeof(p),"%s/index.html",web);wr(p,"<html><h1>QRX</h1></html>");snprintf(p,sizeof(p),"%s/app.js",sub);wr(p,"console.log('qrx-net');");
 QrxStorageFs*fs=NULL;assert(qrx_storage_fs_open(fsd,0,0,&fs)==0);EVP_PKEY*k=NULL;assert(qrx_drive_manifest_generate_signing_key(&k)==0);
 QrxNetPublishJob j;assert(qrx_net_publisher_publish_directory(cat,fs,web,"MySite.QRX",3,123,k,&j)==0);assert(!strcmp(j.domain,"mysite.qrx"));assert(j.status==QRX_NET_PUBLISH_STATUS_PREPARED);assert(j.required_active_shards==10);
 QrxNetPublishedSite s;assert(qrx_net_publisher_load_version(cat,"mysite.qrx",3,&s)==0);assert(s.manifest.file_count==2);qrx_net_published_site_free(&s);
 j.storage_step_kind=QRX_NET_HOST_STEP_ASSIGN;j.storage_step_shard=4;j.storage_step_height=77;snprintf(j.storage_step_txid,sizeof(j.storage_step_txid),"pending-storage-tx");assert(qrx_net_publisher_job_save(&j)==0);QrxNetPublishJob l;assert(qrx_net_publisher_job_load(j.journal_path,&l)==0);assert(!memcmp(l.manifest_root,j.manifest_root,64));assert(l.storage_step_kind==QRX_NET_HOST_STEP_ASSIGN&&l.storage_step_shard==4&&l.storage_step_height==77&&!strcmp(l.storage_step_txid,"pending-storage-tx"));assert(qrx_net_publisher_job_set_active_shards(&l,9)==0);assert(l.status==QRX_NET_PUBLISH_STATUS_WAITING_STORAGE);assert(qrx_net_publisher_job_set_active_shards(&l,10)==0);assert(l.status==QRX_NET_PUBLISH_STATUS_READY_TO_ACTIVATE);assert(qrx_net_publisher_job_load(j.journal_path,&l)==0);assert(l.active_shards==10);
#ifndef _WIN32
 /* A symlink anywhere in the selected webroot fails the entire publish. */
 char outside[512],linkp[512];snprintf(outside,sizeof(outside),"%s/secret.txt",tmp);wr(outside,"wallet-secret");snprintf(linkp,sizeof(linkp),"%s/leak.txt",web);assert(symlink(outside,linkp)==0);QrxNetPublishJob bad;assert(qrx_net_publisher_publish_directory(cat,fs,web,"other.qrx",1,124,k,&bad)!=0);unlink(linkp);
#endif
 EVP_PKEY_free(k);qrx_storage_fs_close(fs);return 0;
}
