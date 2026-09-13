#include "net/qrx_net_dapp_bridge.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
int main(void){
 QrxDomainRecord r;memset(&r,0,sizeof(r));strcpy(r.name,"app.qrx");strcpy(r.owner,"alice");r.expiry_height=2000;r.publishing_key_commitment[0]=7;r.web_manifest_root[0]=9;
 QrxDappBridgeGrant g;assert(qrx_dapp_bridge_grant_create(&r,r.web_manifest_root,QRX_DAPP_SCOPE_PUBLIC_ADDRESS|QRX_DAPP_SCOPE_REQUEST_PAYMENT,100,1000,QRX_DAPP_GRANT_SESSION,11,44,&g)==0);
 QrxDappBridgeRequest q;memset(&q,0,sizeof(q));strcpy(q.domain,"app.qrx");memcpy(q.publishing_key_commitment,r.publishing_key_commitment,64);memcpy(q.manifest_root,r.web_manifest_root,64);q.scope=QRX_DAPP_SCOPE_PUBLIC_ADDRESS;q.nonce=1;
 assert(qrx_dapp_bridge_authorize(&g,&r,r.web_manifest_root,110,44,&q));
 assert(!qrx_dapp_bridge_authorize(&g,&r,r.web_manifest_root,110,44,&q)); /* replay */
 q.nonce=2;assert(!qrx_dapp_bridge_authorize(&g,&r,r.web_manifest_root,110,45,&q)); /* wrong session */
 q.nonce=3;uint8_t other[64]={1};assert(!qrx_dapp_bridge_authorize(&g,&r,other,110,44,&q)); /* stale root */
 q.nonce=4;q.payload_len=QRX_DAPP_BRIDGE_MAX_PAYLOAD+1;assert(!qrx_dapp_bridge_authorize(&g,&r,r.web_manifest_root,110,44,&q));
 q.payload_len=0;q.nonce=5;r.publishing_key_commitment[0]=8;assert(!qrx_dapp_bridge_authorize(&g,&r,r.web_manifest_root,110,44,&q)); /* key rotation invalidates */
 r.publishing_key_commitment[0]=7;QrxDappBridgeGrant once;assert(qrx_dapp_bridge_grant_create(&r,r.web_manifest_root,QRX_DAPP_SCOPE_REQUEST_PAYMENT,100,200,QRX_DAPP_GRANT_ONCE,12,0,&once)==0);q.scope=QRX_DAPP_SCOPE_REQUEST_PAYMENT;q.nonce=1;assert(qrx_dapp_bridge_authorize(&once,&r,r.web_manifest_root,101,0,&q));q.nonce=2;assert(!qrx_dapp_bridge_authorize(&once,&r,r.web_manifest_root,101,0,&q));
 QrxDappBridgeGrant persist;assert(qrx_dapp_bridge_grant_create(&r,r.web_manifest_root,QRX_DAPP_SCOPE_PUBLIC_ADDRESS,100,900,QRX_DAPP_GRANT_PERSISTENT,13,0,&persist)==0);char fn[]="/tmp/qrx-dapp-XXXXXX";int fd=mkstemp(fn);assert(fd>=0);close(fd);assert(qrx_dapp_bridge_save(fn,&persist)==0);QrxDappBridgeGrant loaded;assert(qrx_dapp_bridge_load(fn,&loaded)==0);assert(loaded.grant_id==13&&loaded.mode==QRX_DAPP_GRANT_PERSISTENT);remove(fn);
 puts("net_phase121_dapp_bridge: OK");return 0;
}
