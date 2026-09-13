#pragma once
#include <stddef.h>

#define QRX_GENESIS_GOVERNANCE_ROOT_COUNT 5
#define QRX_GENESIS_GOVERNANCE_THRESHOLD 3

typedef struct {
    const char *key_id;
    const char *public_key_hex;
} qrx_genesis_governance_root_t;

extern const qrx_genesis_governance_root_t QRX_GENESIS_GOVERNANCE_ROOTS[QRX_GENESIS_GOVERNANCE_ROOT_COUNT];
int qrx_genesis_governance_material_ready(void);
