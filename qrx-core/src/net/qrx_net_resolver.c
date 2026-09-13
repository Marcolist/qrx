#include "net/qrx_net_resolver.h"
#include "net/qrx_net_name.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
static int copy_host_path(const char *s,char *host,size_t hs,char *path,size_t ps){const char *slash=strchr(s,'/');size_t n=slash?(size_t)(slash-s):strlen(s);if(!n||n>=hs)return -1;memcpy(host,s,n);host[n]=0;if(slash){if(strlen(slash)>=ps)return -1;strcpy(path,slash);}else strcpy(path,"/");return 0;}
int qrx_browser_resolve_input(const char *in,QrxBrowserResolution *o){if(!in||!o)return -1;while(isspace((unsigned char)*in))in++;size_t len=strlen(in);while(len&&isspace((unsigned char)in[len-1]))len--;if(!len||len>=QRX_BROWSER_URL_MAX)return -1;char s[QRX_BROWSER_URL_MAX];memcpy(s,in,len);s[len]=0;memset(o,0,sizeof(*o));if(strchr(s,' ')||strchr(s,'\t')){o->route=QRX_BROWSER_ROUTE_SEARCH;o->dns_allowed=1;snprintf(o->canonical_url,sizeof(o->canonical_url),"search:%s",s);return 0;}
    const char *rest=s;int explicit_qrx=0;if(!strncmp(rest,"qrx://",6)){explicit_qrx=1;rest+=6;}else if(!strncmp(rest,"https://",8))rest+=8;else if(!strncmp(rest,"http://",7))rest+=7;
    char host[254],path[768];if(copy_host_path(rest,host,sizeof(host),path,sizeof(path))!=0)return -1;char norm[254];if(qrx_domain_normalize(host,norm)==0){o->route=QRX_BROWSER_ROUTE_QRX;o->dns_allowed=0;strcpy(o->qrx_name,norm);strcpy(o->path,path);snprintf(o->canonical_url,sizeof(o->canonical_url),"qrx://%s%s",norm,path);return 0;}if(explicit_qrx)return -1;o->route=QRX_BROWSER_ROUTE_WWW;o->dns_allowed=1;if(!strncmp(s,"http://",7)||!strncmp(s,"https://",8))snprintf(o->canonical_url,sizeof(o->canonical_url),"%s",s);else snprintf(o->canonical_url,sizeof(o->canonical_url),"https://%s",s);return 0;}
