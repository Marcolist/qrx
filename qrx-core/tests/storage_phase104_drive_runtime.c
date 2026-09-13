#include "storage/qrx_drive_runtime.h"
#include "storage/qrx_storage_network.h"
#include "storage/qrx_storage_fs.h"
#include "storage/qrx_erasure.h"
#include "qrxdb.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
typedef struct{int fd;QrxDB*db;QrxStorageFs*fs;int count;}Srv;
static void*server(void*v){Srv*s=v;for(int i=0;i<s->count;i++)assert(qrx_storage_qrxp2p_serve_once(s->fd,s->db,s->fs,"provider-A")==0);return NULL;}
static QrxDriveJobSnapshot waitjob(QrxDriveRuntime*r,const char*id){QrxDriveJobSnapshot s;for(int i=0;i<1000;i++){assert(qrx_drive_runtime_get(r,id,&s)==0);if(s.state>=QRX_DRIVE_JOB_COMPLETED)return s;usleep(10000);}assert(!"timeout");return s;}
int main(void){char d[]="/tmp/qrx-runtime-XXXXXX";assert(mkdtemp(d));char fsd[512],jr[512],src[512],dst[512],tmpd[512];snprintf(fsd,sizeof(fsd),"%s/provider",d);snprintf(jr,sizeof(jr),"%s/journals",d);snprintf(src,sizeof(src),"%s/input.bin",d);snprintf(dst,sizeof(dst),"%s/output.bin",d);snprintf(tmpd,sizeof(tmpd),"%s/hashfs",d);
 size_t sz=700000;unsigned char*data=malloc(sz);assert(data);for(size_t i=0;i<sz;i++)data[i]=(unsigned char)(i*29u+17u);FILE*f=fopen(src,"wb");assert(f&&fwrite(data,1,sz,f)==sz);fclose(f);
 QrxErasureSet es;assert(qrx_erasure_encode(data,sz,10,4,&es)==0);QrxStorageFs*hashfs=NULL;assert(qrx_storage_fs_open(tmpd,20*1024*1024,0,&hashfs)==0);char oid[14][65];for(unsigned i=0;i<14;i++)assert(qrx_storage_fs_put(hashfs,es.shards[i],es.shard_size,oid[i])==0);qrx_storage_fs_close(hashfs);
 QrxDB db;assert(qrxdb_init(&db,d)==0);char rh[129];memset(rh,'0',128);rh[128]=0;for(unsigned i=0;i<14;i++){char k[256],vbuf[1024];snprintf(k,sizeof(k),"storage/assignment/contract-R/%010u",i);snprintf(vbuf,sizeof(vbuf),"1|provider-A|2|%llu|%s|%s|1|100|1|100|0",(unsigned long long)es.shard_size,oid[i],rh);assert(qrxdb_put(&db,k,vbuf)==0);}qrx_erasure_free(&es);
 QrxStorageFs*pfs=NULL;assert(qrx_storage_fs_open(fsd,20*1024*1024,0,&pfs)==0);int l=socket(AF_INET,SOCK_STREAM,0);assert(l>=0);struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=0;assert(bind(l,(struct sockaddr*)&a,sizeof(a))==0&&listen(l,32)==0);socklen_t al=sizeof(a);assert(getsockname(l,(struct sockaddr*)&a,&al)==0);char ep[128];snprintf(ep,sizeof(ep),"qrxp2p://127.0.0.1:%u",(unsigned)ntohs(a.sin_port));Srv sv={l,&db,pfs,24};pthread_t st;assert(pthread_create(&st,NULL,server,&sv)==0);
 QrxShardProviderSource sources[14];for(unsigned i=0;i<14;i++){sources[i].provider_id="provider-A";sources[i].endpoint=ep;sources[i].object_id_hex=oid[i];sources[i].source=(QrxShardSource){i,2,100000000,9999};}
 QrxDriveRuntime*r=NULL;assert(qrx_drive_runtime_open(d,jr,&r)==0);assert(qrx_drive_runtime_set_contract_sources(r,"contract-R",sources,14)==0);char up[129];assert(qrx_drive_runtime_start_upload(r,"contract-R",src,10,4,up)==0);QrxDriveJobSnapshot us=waitjob(r,up);assert(us.state==QRX_DRIVE_JOB_COMPLETED&&us.completed_shards==14);for(unsigned i=0;i<14;i++)assert(qrx_storage_fs_has(pfs,oid[i]));
 char down[129];assert(qrx_drive_runtime_start_download(r,"contract-R",dst,sz,(sz+9)/10,10,4,down)==0);QrxDriveJobSnapshot ds=waitjob(r,down);assert(ds.state==QRX_DRIVE_JOB_COMPLETED&&ds.completed_shards>=10&&ds.sources_started>=10);FILE*g=fopen(dst,"rb");assert(g);unsigned char*got=malloc(sz);assert(got&&fread(got,1,sz,g)==sz);fclose(g);assert(memcmp(got,data,sz)==0);free(got);free(data);
 QrxDriveJobSnapshot list[4];assert(qrx_drive_runtime_list(r,list,4)==2);qrx_drive_runtime_close(r);pthread_join(st,NULL);close(l);qrx_storage_fs_close(pfs);qrxdb_close(&db);puts("PASS: daemon-style transfer runtime uploads 14 shards and downloads/reconstructs 10-of-14 over real qrxp2p sockets");return 0;}
