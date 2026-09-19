#include "qrx_genesis_governance.h"
#include <ctype.h>
#include <string.h>

/* Phase 7.2 final Genesis input surface.
 * Replace ONLY the five public_key_hex placeholder strings below with the
 * Ed25519 public_key_hex values produced by `qrx governance-keygen`.
 * Private keys must never be copied into the source tree or Genesis. */
const qrx_genesis_governance_root_t QRX_GENESIS_GOVERNANCE_ROOTS[QRX_GENESIS_GOVERNANCE_ROOT_COUNT] = {
    {"DEV_GOV_1", "8f9ff6ec3693c92ffb073ac7f8e4cf76e676ebb9f82a12d9afd01f1edeaa0631"},
    {"DEV_GOV_2", "0953ce77866644556532d9539b7439aa9629d0264dad73d05c082a86b6aa8359"},
    {"DEV_GOV_3", "5496f12881d64ddacf38d836cd9e637591897fa6f9cffdd41a2d9c94aa08fb61"},
    {"DEV_GOV_4", "8098202bee6290fe76dccff6ab5f0994b0275a211371867fc639864ef5af18f3"},
    {"DEV_GOV_5", "4f71860006566ecfa528538780c54ccde039d86a7c49fb6570f91a8ab2519d8c"},
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
