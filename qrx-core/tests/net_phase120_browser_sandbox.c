#include "net/qrx_net_sandbox.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 QrxNetSandboxRequest a,b;
 assert(qrx_net_sandbox_parse_url("qrx://Example.QRX/assets/site.css",&a)==0);
 assert(!strcmp(a.domain,"example.qrx")&&!strcmp(a.path,"assets/site.css"));
 assert(!a.dns_allowed&&!a.scripts_allowed&&!a.wallet_ipc_allowed&&!a.filesystem_allowed&&!a.popup_allowed&&!a.downloads_allowed&&!a.external_network_allowed);
 assert(qrx_net_sandbox_parse_url("https://example.qrx/",&b)!=0);
 assert(qrx_net_sandbox_parse_url("qrx://example.qrx/../wallet.pem",&b)!=0);
 assert(qrx_net_sandbox_parse_url("qrx://example.qrx/%2e%2e/wallet.pem",&b)!=0);
 assert(qrx_net_sandbox_parse_url("qrx://example.qrx/a%5cb",&b)!=0);
 assert(qrx_net_sandbox_parse_url("qrx://example.qrx/a//b",&b)!=0);
 assert(qrx_net_sandbox_parse_url("qrx://other.qrx/index.html",&b)==0);
 assert(!qrx_net_sandbox_same_origin(&a,&b));
 assert(qrx_net_sandbox_parse_url("qrx://EXAMPLE.qrx/other.html",&b)==0);
 assert(qrx_net_sandbox_same_origin(&a,&b));
 assert(strstr(QRX_NET_SANDBOX_CSP,"script-src 'none'")&&strstr(QRX_NET_SANDBOX_CSP,"connect-src 'none'"));
 puts("net_phase120_browser_sandbox: OK");return 0;
}
