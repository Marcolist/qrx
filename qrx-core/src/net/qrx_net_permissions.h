#pragma once
#include "net/qrx_net_name.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_DAPP_SCOPE_PUBLIC_ADDRESS   (1u<<0)
#define QRX_DAPP_SCOPE_REQUEST_SIGNATURE (1u<<1)
#define QRX_DAPP_SCOPE_REQUEST_PAYMENT   (1u<<2)
#define QRX_DAPP_SCOPE_PUBLIC_DRIVE_READ (1u<<3)
#define QRX_DAPP_SCOPE_NOTIFICATIONS     (1u<<4)
#define QRX_DAPP_SCOPE_ALLOWED_MASK (QRX_DAPP_SCOPE_PUBLIC_ADDRESS|QRX_DAPP_SCOPE_REQUEST_SIGNATURE|QRX_DAPP_SCOPE_REQUEST_PAYMENT|QRX_DAPP_SCOPE_PUBLIC_DRIVE_READ|QRX_DAPP_SCOPE_NOTIFICATIONS)
typedef struct {char domain[254];char owner[160];uint8_t publishing_key_commitment[64];uint32_t scopes;uint64_t granted_height;uint64_t expiry_height;} QrxDappPermissionGrant;
int qrx_dapp_permission_create(const QrxDomainRecord *record,uint32_t scopes,uint64_t height,uint64_t expiry,QrxDappPermissionGrant *out);
int qrx_dapp_permission_valid(const QrxDappPermissionGrant *grant,const QrxDomainRecord *record,uint64_t height,uint32_t requested_scope);
int qrx_dapp_permission_save(const char *path,const QrxDappPermissionGrant *grant);
int qrx_dapp_permission_load(const char *path,QrxDappPermissionGrant *grant);
#ifdef __cplusplus
}
#endif
