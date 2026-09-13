#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_AURA_RELAY_VERSION 1u
#define QRX_AURA_RELAY_ROUTE_MAX 128u
#define QRX_AURA_RELAY_ENDPOINT_MAX 256u
#define QRX_AURA_RELAY_MAX_ROUTES 256u

typedef struct QrxAuraRelayServer QrxAuraRelayServer;
int qrx_aura_relay_endpoint_parse(const char *endpoint,char *host,size_t host_cap,uint16_t *port,char *route,size_t route_cap);
int qrx_aura_relay_server_start(const char *listen_host,uint16_t port,QrxAuraRelayServer **out);
int qrx_aura_relay_server_stop(QrxAuraRelayServer *server);
void qrx_aura_relay_server_free(QrxAuraRelayServer *server);
uint16_t qrx_aura_relay_server_port(const QrxAuraRelayServer *server);
int qrx_aura_relay_client_connect(const char *endpoint);
int qrx_aura_relay_provider_connect(const char *endpoint);
#ifdef __cplusplus
}
#endif
