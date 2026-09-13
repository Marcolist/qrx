#include "net/qrx_net_permissions.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void){QrxDomainRecord r;memset(&r,0,sizeof(r));strcpy(r.name,"app.qrx");strcpy(r.owner,"owner-a");r.expiry_height=1000;r.publishing_key_commitment[0]=42;QrxDappPermissionGrant g;assert(qrx_dapp_permission_create(&r,QRX_DAPP_SCOPE_PUBLIC_ADDRESS|QRX_DAPP_SCOPE_REQUEST_PAYMENT,100,800,&g)==0);assert(qrx_dapp_permission_valid(&g,&r,200,QRX_DAPP_SCOPE_REQUEST_PAYMENT));assert(!qrx_dapp_permission_valid(&g,&r,200,QRX_DAPP_SCOPE_REQUEST_SIGNATURE));char tmp[]="/tmp/qrxperm-XXXXXX";int fd=mkstemp(tmp);assert(fd>=0);close(fd);assert(qrx_dapp_permission_save(tmp,&g)==0);QrxDappPermissionGrant h;assert(qrx_dapp_permission_load(tmp,&h)==0&&h.scopes==g.scopes);strcpy(r.owner,"owner-b");assert(!qrx_dapp_permission_valid(&h,&r,200,QRX_DAPP_SCOPE_REQUEST_PAYMENT));strcpy(r.owner,"owner-a");r.publishing_key_commitment[0]=43;assert(!qrx_dapp_permission_valid(&h,&r,200,QRX_DAPP_SCOPE_REQUEST_PAYMENT));remove(tmp);return 0;}
