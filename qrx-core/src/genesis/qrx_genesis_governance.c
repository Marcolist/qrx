#include "qrx_genesis_governance.h"
#include <ctype.h>
#include <string.h>

/* Phase 7.2 final Genesis input surface.
 * Replace ONLY the five public_key_hex placeholder strings below with the
 * Ed25519 public_key_hex values produced by `qrx governance-keygen`.
 * Private keys must never be copied into the source tree or Genesis. */
const qrx_genesis_governance_root_t QRX_GENESIS_GOVERNANCE_ROOTS[QRX_GENESIS_GOVERNANCE_ROOT_COUNT] = {
    {"DEV_GOV_1", "REPLACE_WITH_DEV_GOV_1_PUBLIC_KEY_HEX"},
    {"DEV_GOV_2", "REPLACE_WITH_DEV_GOV_2_PUBLIC_KEY_HEX"},
    {"DEV_GOV_3", "REPLACE_WITH_DEV_GOV_3_PUBLIC_KEY_HEX"},
    {"DEV_GOV_4", "REPLACE_WITH_DEV_GOV_4_PUBLIC_KEY_HEX"},
    {"DEV_GOV_5", "REPLACE_WITH_DEV_GOV_5_PUBLIC_KEY_HEX"},
};

static int hex64(const char *s){
    if(!s || strlen(s)!=64) return 0;
    for(size_t i=0;i<64;i++) if(!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}
int qrx_genesis_governance_material_ready(void){
    if(QRX_GENESIS_GOVERNANCE_THRESHOLD < 1 || QRX_GENESIS_GOVERNANCE_THRESHOLD > QRX_GENESIS_GOVERNANCE_ROOT_COUNT) return 0;
    for(size_t i=0;i<QRX_GENESIS_GOVERNANCE_ROOT_COUNT;i++){
        if(!QRX_GENESIS_GOVERNANCE_ROOTS[i].key_id || !*QRX_GENESIS_GOVERNANCE_ROOTS[i].key_id || !hex64(QRX_GENESIS_GOVERNANCE_ROOTS[i].public_key_hex)) return 0;
        for(size_t j=0;j<i;j++) if(!strcmp(QRX_GENESIS_GOVERNANCE_ROOTS[i].public_key_hex,QRX_GENESIS_GOVERNANCE_ROOTS[j].public_key_hex)) return 0;
    }
    return 1;
}
