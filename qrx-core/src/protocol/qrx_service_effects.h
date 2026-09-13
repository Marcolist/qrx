#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t debit_atoms;
    uint64_t self_credit_atoms;
    char recipient[160];
    uint64_t recipient_credit_atoms;
    uint64_t development_credit_atoms;
    uint64_t protocol_fee_atoms;
} QrxServiceEconomicEffect;

#ifdef __cplusplus
}
#endif
