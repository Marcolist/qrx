#include "net/qrx_net_registry.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void){char tmp[]="/tmp/qrxnet-reg-XXXXXX";assert(mkdtemp(tmp));QrxDB db;assert(qrxdb_init(&db,tmp)==0);uint8_t web[64]={1},pub[64]={2};QrxDomainRecord r;assert(qrx_net_registry_register(&db,"Example.QRX","owner-A","qrx1a",web,pub,100,1000,&r)==0&&r.sequence==1&&!strcmp(r.name,"example.qrx"));assert(qrx_net_registry_register(&db,"example.qrx","owner-B",NULL,NULL,NULL,101,2000,&r)!=0);QrxDomainUpdate u={QRX_RECORD_SET,"qrx1b",QRX_RECORD_KEEP,NULL,QRX_RECORD_KEEP,NULL};assert(qrx_net_registry_update(&db,"example.qrx","owner-A",1,200,&u,&r)==0&&r.sequence==2&&!strcmp(r.qub_address,"qrx1b")&&r.web_manifest_root[0]==1);assert(qrx_net_registry_renew(&db,"example.qrx","owner-A",2,300,2000,&r)==0&&r.sequence==3&&r.expiry_height==2000);assert(qrx_net_registry_transfer(&db,"example.qrx","owner-A",3,400,"owner-B",&r)==0&&r.sequence==4&&!strcmp(r.owner,"owner-B")&&r.qub_address[0]==0&&r.web_manifest_root[0]==0&&r.publishing_key_commitment[0]==0);assert(qrx_net_registry_update(&db,"example.qrx","owner-A",4,500,&u,&r)!=0);assert(qrx_net_registry_get(&db,"example.qrx",&r)==0&&!strcmp(r.owner,"owner-B"));assert(qrxdb_close(&db)==0);return 0;}
