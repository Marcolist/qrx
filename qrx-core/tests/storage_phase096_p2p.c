#include "storage/qrx_storage_p2p.h"
#include "resource/qrx_storage_repair.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void){char tmp[]="/tmp/qrx-p2p-XXXXXX",root[]="/tmp/qrx-p2p-fs-XXXXXX";assert(mkdtemp(tmp)&&mkdtemp(root));QrxDB db;assert(qrxdb_init(&db,tmp)==0);QrxStorageFs*fs=NULL;assert(qrx_storage_fs_open(root,1024*1024,0,&fs)==0);unsigned char data[4096];for(size_t i=0;i<sizeof(data);i++)data[i]=(unsigned char)(i&255);char oid[65];assert(qrx_storage_fs_put(fs,data,sizeof(data),oid)==0);char k[256],v[1024],rh[129];memset(rh,'0',128);rh[128]=0;snprintf(k,sizeof(k),"storage/assignment/c1/%010u",0);snprintf(v,sizeof(v),"1|provider-A|%u|4096|%s|%s|1|100|1|100|0",QRX_ASSIGN_ACTIVE,oid,rh);assert(qrxdb_put(&db,k,v)==0);unsigned char*out=NULL;size_t n=0;assert(qrx_storage_p2p_read_authorized(&db,fs,"provider-A","c1",0,oid,100,256,&out,&n)==0&&n==256);assert(!memcmp(out,data+100,256));free(out);assert(qrx_storage_p2p_read_authorized(&db,fs,"provider-B","c1",0,oid,0,10,&out,&n)!=0);assert(qrx_storage_p2p_read_authorized(&db,fs,"provider-A","c1",0,oid,5000,10,&out,&n)!=0);assert(qrx_storage_p2p_read_authorized(&db,fs,"provider-A","c1",0,oid,0,QRX_STORAGE_P2P_MAX_RANGE+1,&out,&n)!=0);assert(qrx_storage_p2p_read_authorized(&db,fs,"provider-A","c1",0,oid,0,0,&out,&n)==0&&n==0);free(out);qrx_storage_fs_close(fs);qrxdb_close(&db);puts("PASS: P2P range serving is assignment-authorized, bounded, resumable and provider-bound");return 0;}
