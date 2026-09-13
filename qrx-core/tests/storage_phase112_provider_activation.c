#include "storage/qrx_storage_activation.h"
#include "storage/qrx_storage_fs.h"
#include "storage/qrx_merkle.h"
#include "storage/qrx_drive_prepare.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void hx(const unsigned char*in,size_t n,char*out){static const char*x="0123456789abcdef";for(size_t i=0;i<n;i++){out[i*2]=x[in[i]>>4];out[i*2+1]=x[in[i]&15];}out[n*2]=0;}
int main(void){char d[]="/tmp/qrx-act-XXXXXX";assert(mkdtemp(d));char fsd[512],src[512];snprintf(fsd,sizeof(fsd),"%s/fs",d);snprintf(src,sizeof(src),"%s/shard.bin",d);size_t n=150000;unsigned char*b=malloc(n);for(size_t i=0;i<n;i++)b[i]=(unsigned char)(i*17u+3u);FILE*f=fopen(src,"wb");assert(f&&fwrite(b,1,n,f)==n);fclose(f);QrxStorageFs*fs=NULL;assert(!qrx_storage_fs_open(fsd,1<<24,0,&fs));char oid[65];assert(!qrx_storage_fs_put_file(fs,src,oid));unsigned char mr[64];size_t leaves=0;assert(!qrx_merkle_root_from_chunks(b,n,QRX_DRIVE_POSTOR_LEAF_BYTES,mr,&leaves));char mhex[129];hx(mr,64,mhex);QrxDB db;assert(!qrxdb_init(&db,d));char key[400],val[1200];snprintf(key,sizeof(key),"storage/assignment/c112/%010u",3u);snprintf(val,sizeof(val),"1|provider-A|1|%zu|%s|%s|%zu|0|0|0|0",n,oid,mhex,leaves);assert(!qrxdb_put(&db,key,val));assert(!qrx_storage_assignment_local_ready(&db,fs,"provider-A","c112",3));QrxStorageReadyAssignment r[4];size_t rn=0;assert(!qrx_storage_collect_ready_assignments(&db,fs,"provider-A",r,4,&rn)&&rn==1&&!strcmp(r[0].contract_id,"c112")&&r[0].shard_index==3);/* Merkle commitment mismatch must block activation even though CAS exists. */mhex[0]=mhex[0]=='0'?'1':'0';snprintf(val,sizeof(val),"1|provider-A|1|%zu|%s|%s|%zu|0|0|0|0",n,oid,mhex,leaves);assert(!qrxdb_put(&db,key,val));assert(qrx_storage_assignment_local_ready(&db,fs,"provider-A","c112",3)!=0);qrxdb_close(&db);qrx_storage_fs_close(fs);free(b);puts("PASS: provider activation requires exact local CAS, byte count and PoStor Merkle commitment before ACCEPT eligibility");return 0;}
