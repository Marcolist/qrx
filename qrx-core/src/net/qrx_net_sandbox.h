#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_NET_SANDBOX_DOMAIN_MAX 253
#define QRX_NET_SANDBOX_PATH_MAX 1024
#define QRX_NET_SANDBOX_CSP "default-src 'none'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self'; media-src 'self'; object-src 'none'; frame-src 'none'; child-src 'none'; connect-src 'none'; script-src 'none'; worker-src 'none'; manifest-src 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'self'"
typedef struct {
 char domain[QRX_NET_SANDBOX_DOMAIN_MAX+1];
 char path[QRX_NET_SANDBOX_PATH_MAX+1];
 int dns_allowed;
 int scripts_allowed;
 int wallet_ipc_allowed;
 int filesystem_allowed;
 int popup_allowed;
 int downloads_allowed;
 int external_network_allowed;
} QrxNetSandboxRequest;
/* Strict parser for resources served by the verified QRX custom protocol.
 * Only qrx://<name.qrx>/<path> is accepted. It never maps to DNS/HTTP and
 * rejects traversal/control/backslash/query tricks before reaching storage. */
int qrx_net_sandbox_parse_url(const char *url,QrxNetSandboxRequest *out);
int qrx_net_sandbox_same_origin(const QrxNetSandboxRequest *a,const QrxNetSandboxRequest *b);
#ifdef __cplusplus
}
#endif
