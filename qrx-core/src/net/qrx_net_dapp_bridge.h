#pragma once
#include "net/qrx_net_permissions.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_DAPP_BRIDGE_MAX_PAYLOAD 16384u
#define QRX_DAPP_BRIDGE_WINDOW_BLOCKS 12u
#define QRX_DAPP_BRIDGE_MAX_CALLS_PER_WINDOW 32u

typedef enum { QRX_DAPP_GRANT_ONCE=1, QRX_DAPP_GRANT_SESSION=2, QRX_DAPP_GRANT_PERSISTENT=3 } QrxDappGrantMode;
typedef struct {
  QrxDappPermissionGrant grant;
  uint8_t manifest_root[64];
  QrxDappGrantMode mode;
  uint64_t grant_id;
  uint64_t session_id;
  uint64_t last_nonce;
  uint64_t window_start_height;
  uint32_t calls_in_window;
  int consumed;
} QrxDappBridgeGrant;

typedef struct {
  char domain[QRX_DOMAIN_MAX_NAME+1];
  uint8_t publishing_key_commitment[64];
  uint8_t manifest_root[64];
  uint32_t scope;
  uint64_t nonce;
  const uint8_t *payload;
  size_t payload_len;
} QrxDappBridgeRequest;

int qrx_dapp_bridge_grant_create(const QrxDomainRecord *record,const uint8_t manifest_root[64],uint32_t scopes,uint64_t height,uint64_t expiry,QrxDappGrantMode mode,uint64_t grant_id,uint64_t session_id,QrxDappBridgeGrant *out);
int qrx_dapp_bridge_authorize(QrxDappBridgeGrant *grant,const QrxDomainRecord *record,const uint8_t current_manifest_root[64],uint64_t height,uint64_t current_session_id,const QrxDappBridgeRequest *request);
int qrx_dapp_bridge_save(const char *path,const QrxDappBridgeGrant *grant);
int qrx_dapp_bridge_load(const char *path,QrxDappBridgeGrant *grant);
#ifdef __cplusplus
}
#endif
