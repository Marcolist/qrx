#include "storage/qrx_storage_network.h"
#include "storage/qrx_storage_fs.h"
#include "storage/qrx_transfer_journal.h"
#include "qrxdb.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
typedef struct{int fd;QrxDB*db;QrxStorageFs*fs;} Srv;
static void*server(void*v){Srv*s=v;assert(qrx_storage_qrxp2p_serve_once(s->fd,s->db,s->fs,"provider-A")==0);assert(qrx_storage_qrxp2p_serve_once(s->fd,s->db,s->fs,"provider-A")==0);return NULL;}
int main(void){char d[]="/tmp/qrx-netx-XXXXXX";assert(mkdtemp(d));char fsd[512],src[512],jr[512];snprintf(fsd,sizeof(fsd),"%s/fs",d);snprintf(src,sizeof(src),"%s/shard.bin",d);snprintf(jr,sizeof(jr),"%s/journals",d);unsigned char data[4096];for(size_t i=0;i<sizeof(data);i++)data[i]=(unsigned char)(i*13+9);FILE*f=fopen(src,"wb");assert(f&&fwrite(data,1,sizeof(data),f)==sizeof(data));fclose(f);
 QrxStorageFs*fs=NULL;assert(qrx_storage_fs_open(fsd,1<<20,0,&fs)==0);char oid[65];QrxStorageFs*tmpfs=NULL;char tmpd[512];snprintf(tmpd,sizeof(tmpd),"%s/tmpfs",d);assert(qrx_storage_fs_open(tmpd,1<<20,0,&tmpfs)==0);assert(qrx_storage_fs_put_file(tmpfs,src,oid)==0);qrx_storage_fs_close(tmpfs);
 QrxDB db;assert(qrxdb_init(&db,d)==0);char k[256],vbuf[1024],rh[129];memset(rh,'0',128);rh[128]=0;snprintf(k,sizeof(k),"storage/assignment/contract-X/%010u",0u);snprintf(vbuf,sizeof(vbuf),"1|provider-A|2|4096|%s|%s|1|100|1|100|0",oid,rh);assert(qrxdb_put(&db,k,vbuf)==0);
 int l=socket(AF_INET,SOCK_STREAM,0);assert(l>=0);struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=0;assert(bind(l,(struct sockaddr*)&a,sizeof(a))==0&&listen(l,8)==0);socklen_t al=sizeof(a);assert(getsockname(l,(struct sockaddr*)&a,&al)==0);char ep[128];snprintf(ep,sizeof(ep),"qrxp2p://127.0.0.1:%u",(unsigned)ntohs(a.sin_port));Srv sv={l,&db,fs};pthread_t th;assert(pthread_create(&th,NULL,server,&sv)==0);
 QrxShardProviderSource ps={"provider-A",ep,oid,{0,1,100000000,9999}};assert(qrx_storage_network_upload_file(&ps,"contract-X",src)==0);assert(qrx_storage_fs_has(fs,oid));QrxStorageNetworkFetchCtx fc={"contract-X",1000,1000};uint8_t*out=NULL;size_t on=0;uint8_t ign[64]={0};assert(qrx_storage_network_fetch_range(&fc,&ps,ign,777,333,&out,&on)==0&&on==333&&memcmp(out,data+777,333)==0);free(out);pthread_join(th,NULL);close(l);
 QrxTransferJournal j={0},r={0};snprintf(j.transfer_id,sizeof(j.transfer_id),"transfer-1");snprintf(j.contract_id,sizeof(j.contract_id),"contract-X");snprintf(j.direction,sizeof(j.direction),"download");snprintf(j.path,sizeof(j.path),"%s",src);j.total_bytes=4096;j.completed_bytes=2048;j.completed_shards=7;j.required_shards=10;snprintf(j.state,sizeof(j.state),"RUNNING");assert(qrx_transfer_journal_save(jr,&j)==0);assert(qrx_transfer_journal_load(jr,"transfer-1",&r)==0);assert(r.completed_bytes==2048&&r.completed_shards==7&&!strcmp(r.state,"RUNNING"));
 qrx_storage_fs_close(fs);qrxdb_close(&db);puts("PASS: real qrxp2p PUT/authorized range GET plus persistent transfer journal");return 0;}
