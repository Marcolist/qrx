#include "net/qrx_net_sandbox.h"
#include "net/qrx_net_registry.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
static int hx(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return 10+c-'a';if(c>='A'&&c<='F')return 10+c-'A';return -1;}
static int pct_decode_path(const char*in,char*out,size_t cap){size_t n=0;if(!in||!out||cap<2)return-1;while(*in&&*in!='?'&&*in!='#'){unsigned char c=(unsigned char)*in++;if(c=='%'){int a=hx(*in++),b=hx(*in++);if(a<0||b<0)return-1;c=(unsigned char)((a<<4)|b);}if(c<0x20||c==0x7f||c=='\\'||c==0)return-1;if(n+1>=cap)return-1;out[n++]=(char)c;}out[n]=0;return 0;}
int qrx_net_sandbox_parse_url(const char*url,QrxNetSandboxRequest*out){if(!url||!out)return-1;memset(out,0,sizeof(*out));const char*p=url;if(strncmp(p,"qrx://",6))return-1;p+=6;const char*slash=strchr(p,'/');const char*q=strpbrk(p,"?#");const char*end=slash?slash:(q?q:p+strlen(p));if(q&&q<end)end=q;size_t dn=(size_t)(end-p);if(!dn||dn>QRX_NET_SANDBOX_DOMAIN_MAX)return-1;char d[254];memcpy(d,p,dn);d[dn]=0;if(qrx_domain_normalize(d,out->domain))return-1;if(slash&&slash<end)slash=NULL;const char*rp=slash?slash+1:"";char dec[QRX_NET_SANDBOX_PATH_MAX+1];if(pct_decode_path(rp,dec,sizeof(dec)))return-1;if(!*dec)strcpy(dec,"index.html");if(dec[0]=='/'||strstr(dec,"../")||!strcmp(dec,"..")||strstr(dec,"/..")||strstr(dec,"./")||strstr(dec,"//"))return-1;snprintf(out->path,sizeof(out->path),"%s",dec);out->dns_allowed=0;out->scripts_allowed=0;out->wallet_ipc_allowed=0;out->filesystem_allowed=0;out->popup_allowed=0;out->downloads_allowed=0;out->external_network_allowed=0;return 0;}
int qrx_net_sandbox_same_origin(const QrxNetSandboxRequest*a,const QrxNetSandboxRequest*b){return a&&b&&!strcmp(a->domain,b->domain);}
