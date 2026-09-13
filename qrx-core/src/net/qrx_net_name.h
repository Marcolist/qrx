#pragma once
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_DOMAIN_MAX_NAME 253
#define QRX_DOMAIN_GRACE_BLOCKS 60480ULL

typedef enum {QRX_DOMAIN_PRICE_FIXED=1,QRX_DOMAIN_PRICE_AUCTION_REQUIRED=2} QrxDomainPriceMode;
typedef struct {QrxDomainPriceMode mode;uint64_t annual_atoms;uint64_t reservation_bond_atoms;} QrxDomainPrice;
typedef struct {uint64_t development_atoms;uint64_t registry_atoms;} QrxDomainFeeSplit;
typedef struct {
    char name[QRX_DOMAIN_MAX_NAME+1];
    uint8_t name_hash[64];
    char owner[160];
    char qub_address[160];
    uint8_t web_manifest_root[64];
    uint8_t publishing_key_commitment[64];
    uint64_t created_height;
    uint64_t expiry_height;
    uint64_t sequence;
} QrxDomainRecord;
int qrx_domain_normalize(const char *input,char out[QRX_DOMAIN_MAX_NAME+1]);
int qrx_domain_name_hash(const char *normalized_name,uint8_t out[64]);
int qrx_domain_price(const char *normalized_name,uint64_t base_annual_atoms,uint64_t owned_domain_count,QrxDomainPrice *out);
int qrx_domain_fee_split(uint64_t atoms,QrxDomainFeeSplit *out);
int qrx_domain_is_active(const QrxDomainRecord *r,uint64_t height);
int qrx_domain_in_grace(const QrxDomainRecord *r,uint64_t height);
int qrx_domain_pubkey_commitment(EVP_PKEY *public_key,uint8_t out[64]);
#ifdef __cplusplus
}
#endif
