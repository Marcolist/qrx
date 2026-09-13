#pragma once
#include <stddef.h>
#include <stdint.h>
#include "compute/qrx_aura_provider_service.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_AURA_MOE_AGG_VERSION 1u
#define QRX_AURA_MOE_CONTRIB_MAGIC "QRXMC40\0"
#define QRX_AURA_MOE_RESULT_MAGIC  "QRXMR40\0"
#define QRX_AURA_MOE_MAX_VECTOR_ELEMENTS 1048576u

typedef struct {
    uint32_t version;
    char plan_commitment[65];
    uint32_t fragment_index;
    uint32_t fragment_count;
    uint32_t token_index;
    uint32_t vector_length;
    uint32_t q_fraction_bits;
    int32_t *values; /* weighted expert contribution in fixed-point */
} QrxAuraMoeContribution;

typedef struct {
    uint32_t version;
    char plan_commitment[65];
    uint32_t fragment_count;
    uint32_t token_index;
    uint32_t vector_length;
    uint32_t q_fraction_bits;
    int64_t *values; /* exact sum of all fragment contributions */
    char result_commitment[65];
} QrxAuraMoeAggregateResult;

int qrx_aura_moe_contribution_encode(const QrxAuraMoeContribution *contribution,uint8_t **out,size_t *out_len);
int qrx_aura_moe_contribution_decode(const uint8_t *in,size_t in_len,QrxAuraMoeContribution *out);
void qrx_aura_moe_contribution_free(QrxAuraMoeContribution *contribution);
int qrx_aura_moe_contribution_commitment(const QrxAuraMoeContribution *contribution,char out_hex[65]);

int qrx_aura_moe_aggregate_result_encode(const QrxAuraMoeAggregateResult *result,uint8_t **out,size_t *out_len);
int qrx_aura_moe_aggregate_result_decode(const uint8_t *in,size_t in_len,QrxAuraMoeAggregateResult *out);
void qrx_aura_moe_aggregate_result_free(QrxAuraMoeAggregateResult *result);
int qrx_aura_moe_aggregate_result_commitment(const QrxAuraMoeAggregateResult *result,char out_hex[65]);

int qrx_aura_moe_fixedpoint_aggregate(const uint8_t *const *fragment_outputs,const size_t *fragment_sizes,
                                      uint32_t fragment_count,QrxAuraMoeAggregateResult *out);
/* Direct QrxAuraRemoteAggregateFn adapter. It returns the encoded aggregate
 * result envelope so the caller can feed the deterministic vector to the next
 * model stage or a runtime-specific decoder. */
int qrx_aura_moe_fixedpoint_aggregate_adapter(void *ctx,const uint8_t *const *fragment_outputs,const size_t *fragment_sizes,
                                             uint32_t fragment_count,void *output,size_t output_capacity,size_t *output_bytes);

#ifdef __cplusplus
}
#endif
