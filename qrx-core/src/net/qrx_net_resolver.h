#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_BROWSER_URL_MAX 1024
typedef enum {QRX_BROWSER_ROUTE_QRX=1,QRX_BROWSER_ROUTE_WWW=2,QRX_BROWSER_ROUTE_SEARCH=3} QrxBrowserRoute;
typedef struct {QrxBrowserRoute route;int dns_allowed;char canonical_url[QRX_BROWSER_URL_MAX];char qrx_name[254];char path[768];} QrxBrowserResolution;
int qrx_browser_resolve_input(const char *input,QrxBrowserResolution *out);
#ifdef __cplusplus
}
#endif
