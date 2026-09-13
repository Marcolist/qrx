#include "net/qrx_net_dapp_bridge.h"
#include <openssl/crypto.h>
#include <stdio.h>
#include <string.h>

static void hx64(const uint8_t b[64],char o[129]){static const char h[]="0123456789abcdef";for(int i=0;i<64;i++){o[i*2]=h[b[i]>>4];o[i*2+1]=h[b[i]&15];}o[128]=0;}
static int uh64(const char*s,uint8_t b[64]){if(!s||strlen(s)!=128)return -1;for(int i=0;i<64;i++){int x,y;char a=s[i*2],c=s[i*2+1];x=a>='0'&&a<='9'?a-'0':a>='a'&&a<='f'?a-'a'+10:a>='A'&&a<='F'?a-'A'+10:-1;y=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(x<0||y<0)return -1;b[i]=(uint8_t)((x<<4)|y);}return 0;}
static int nz64(const uint8_t b[64]){uint8_t v=0;for(int i=0;i<64;i++)v|=b[i];return v!=0;}
int qrx_dapp_bridge_grant_create(const QrxDomainRecord *r,const uint8_t root[64],uint32_t scopes,uint64_t h,uint64_t exp,QrxDappGrantMode mode,uint64_t gid,uint64_t sid,QrxDappBridgeGrant *o){
 if(!r||!root||!o||!nz64(root)||!gid||mode<QRX_DAPP_GRANT_ONCE||mode>QRX_DAPP_GRANT_PERSISTENT)return -1;
 memset(o,0,sizeof(*o));if(qrx_dapp_permission_create(r,scopes,h,exp,&o->grant))return -1;memcpy(o->manifest_root,root,64);o->mode=mode;o->grant_id=gid;o->session_id=sid;o->window_start_height=h;return 0;
}
int qrx_dapp_bridge_authorize(QrxDappBridgeGrant *g,const QrxDomainRecord *r,const uint8_t root[64],uint64_t h,uint64_t sid,const QrxDappBridgeRequest *q){
 if(!g||!r||!root||!q||q->payload_len>QRX_DAPP_BRIDGE_MAX_PAYLOAD||!q->nonce)return 0;
 if(q->payload_len&& !q->payload)return 0;
 if(strcmp(q->domain,r->name)||CRYPTO_memcmp(q->publishing_key_commitment,r->publishing_key_commitment,64)||CRYPTO_memcmp(q->manifest_root,root,64))return 0;
 if(CRYPTO_memcmp(g->manifest_root,root,64)||!qrx_dapp_permission_valid(&g->grant,r,h,q->scope))return 0;
 if(g->mode==QRX_DAPP_GRANT_ONCE&&g->consumed)return 0;
 if(g->mode==QRX_DAPP_GRANT_SESSION&&(!g->session_id||g->session_id!=sid))return 0;
 if(q->nonce<=g->last_nonce)return 0;
 if(h>=g->window_start_height+QRX_DAPP_BRIDGE_WINDOW_BLOCKS){g->window_start_height=h;g->calls_in_window=0;}
 if(g->calls_in_window>=QRX_DAPP_BRIDGE_MAX_CALLS_PER_WINDOW)return 0;
 g->calls_in_window++;g->last_nonce=q->nonce;if(g->mode==QRX_DAPP_GRANT_ONCE)g->consumed=1;return 1;
}
int qrx_dapp_bridge_save(const char *path,const QrxDappBridgeGrant *g){
 if(!path||!g||g->mode!=QRX_DAPP_GRANT_PERSISTENT)return -1;char tmp[1024],pk[129],mr[129];if(snprintf(tmp,sizeof(tmp),"%s.tmp",path)>=(int)sizeof(tmp))return -1;hx64(g->grant.publishing_key_commitment,pk);hx64(g->manifest_root,mr);FILE*f=fopen(tmp,"wb");if(!f)return -1;int ok=fprintf(f,"2|%s|%s|%s|%s|%u|%llu|%llu|%u|%llu|%llu|%llu\n",g->grant.domain,g->grant.owner,pk,mr,g->grant.scopes,(unsigned long long)g->grant.granted_height,(unsigned long long)g->grant.expiry_height,(unsigned)g->mode,(unsigned long long)g->grant_id,(unsigned long long)g->last_nonce,(unsigned long long)g->window_start_height)>0&&fflush(f)==0&&fclose(f)==0;if(!ok){remove(tmp);return -1;}if(rename(tmp,path)){remove(tmp);return -1;}return 0;
}
int qrx_dapp_bridge_load(const char *path,QrxDappBridgeGrant *g){
 if(!path||!g)return -1;FILE*f=fopen(path,"rb");if(!f)return -1;char line[1800];if(!fgets(line,sizeof(line),f)){fclose(f);return -1;}fclose(f);char d[254],o[160],pk[129],mr[129];unsigned sc=0,mode=0;unsigned long long gh=0,eh=0,gid=0,nonce=0,ws=0;if(sscanf(line,"2|%253[^|]|%159[^|]|%128[^|]|%128[^|]|%u|%llu|%llu|%u|%llu|%llu|%llu",d,o,pk,mr,&sc,&gh,&eh,&mode,&gid,&nonce,&ws)!=11)return -1;memset(g,0,sizeof(*g));strcpy(g->grant.domain,d);strcpy(g->grant.owner,o);if(uh64(pk,g->grant.publishing_key_commitment)||uh64(mr,g->manifest_root))return -1;g->grant.scopes=sc;g->grant.granted_height=gh;g->grant.expiry_height=eh;g->mode=(QrxDappGrantMode)mode;g->grant_id=gid;g->last_nonce=nonce;g->window_start_height=ws;return g->mode==QRX_DAPP_GRANT_PERSISTENT&&gid&&sc&&!(sc&~QRX_DAPP_SCOPE_ALLOWED_MASK)?0:-1;
}
