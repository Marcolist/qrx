#pragma once
#include <stddef.h>
#include <stdint.h>
#include <openssl/evp.h>
#include "compute/qrx_aura_model_distribution.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_AURA_MODEL_GOV_VERSION 1u
#define QRX_AURA_MODEL_GOV_MAX_ENTRIES 256u
#define QRX_AURA_MODEL_LICENSE_RULE_MAX 32u
#define QRX_AURA_MODEL_GOV_REASON_MAX 256u
#define QRX_AURA_MODEL_PROVENANCE_URI_MAX 512u

typedef enum {
    QRX_AURA_LICENSE_ALLOW=1,
    QRX_AURA_LICENSE_DENY=2
} QrxAuraModelLicenseAction;

typedef struct {
    char license_id[QRX_AURA_MODEL_LICENSE_MAX];
    QrxAuraModelLicenseAction action;
} QrxAuraModelLicenseRule;

typedef struct {
    uint32_t version;
    uint8_t default_allow;
    uint8_t require_provenance;
    uint8_t allow_external_origin;
    uint32_t rule_count;
    QrxAuraModelLicenseRule rules[QRX_AURA_MODEL_LICENSE_RULE_MAX];
} QrxAuraModelLicensePolicy;

typedef struct {
    uint32_t version;
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char manifest_root[65];
    char publisher_id[QRX_GLOBE_PROVIDER_MAX+1];
    char license_id[QRX_AURA_MODEL_LICENSE_MAX];
    char source_uri[QRX_AURA_MODEL_PROVENANCE_URI_MAX];
    char source_commit[65];
    char parent_manifest_root[65];
    uint64_t recorded_at_height;
} QrxAuraModelProvenance;

typedef enum {
    QRX_AURA_MODEL_GOV_APPROVE=1,
    QRX_AURA_MODEL_GOV_BLOCK=2,
    QRX_AURA_MODEL_GOV_ROLLBACK=3
} QrxAuraModelGovernanceAction;

typedef struct {
    uint32_t version;
    char governor_id[QRX_GLOBE_PROVIDER_MAX+1];
    uint64_t sequence;
    uint64_t effective_height;
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char manifest_root[65];
    char rollback_manifest_root[65];
    QrxAuraModelGovernanceAction action;
    char reason[QRX_AURA_MODEL_GOV_REASON_MAX];
} QrxAuraModelGovernanceDecision;

typedef struct {
    QrxAuraModelGovernanceDecision decision;
    uint8_t *signature;
    size_t signature_len;
} QrxAuraModelGovernanceEntry;

typedef struct {
    uint32_t version;
    QrxAuraModelGovernanceEntry entries[QRX_AURA_MODEL_GOV_MAX_ENTRIES];
    uint32_t count;
    uint64_t revision;
} QrxAuraModelGovernanceState;

typedef int (*QrxAuraModelGovernorAuthorizeFn)(void *ctx,const char *governor_id);

void qrx_aura_model_license_policy_init(QrxAuraModelLicensePolicy *policy);
int qrx_aura_model_license_policy_add(QrxAuraModelLicensePolicy *policy,const char *license_id,QrxAuraModelLicenseAction action);
int qrx_aura_model_license_policy_allows(const QrxAuraModelLicensePolicy *policy,const char *license_id);
int qrx_aura_model_provenance_validate(const QrxAuraModelProvenance *p,const QrxAuraModelCatalogAnnouncement *a);
int qrx_aura_model_governance_decision_hash(const QrxAuraModelGovernanceDecision *d,uint8_t out[32]);
int qrx_aura_model_governance_decision_sign(EVP_PKEY *key,const QrxAuraModelGovernanceDecision *d,uint8_t **sig,size_t *sig_len);
int qrx_aura_model_governance_decision_verify(EVP_PKEY *key,const QrxAuraModelGovernanceDecision *d,const uint8_t *sig,size_t sig_len);
void qrx_aura_model_governance_state_init(QrxAuraModelGovernanceState *state);
void qrx_aura_model_governance_state_free(QrxAuraModelGovernanceState *state);
int qrx_aura_model_governance_ingest(QrxAuraModelGovernanceState *state,const QrxAuraModelGovernanceDecision *d,
                                     const uint8_t *sig,size_t sig_len,uint64_t current_height,
                                     QrxAuraModelDistKeyLookupFn key_lookup,void *key_ctx,
                                     QrxAuraModelGovernorAuthorizeFn authorize,void *auth_ctx);
const QrxAuraModelGovernanceDecision *qrx_aura_model_governance_find(const QrxAuraModelGovernanceState *state,const char *model_id,const char *model_version);
int qrx_aura_model_governance_evaluate(const QrxAuraModelGovernanceState *state,const QrxAuraModelLicensePolicy *policy,
                                       const QrxAuraModelCatalogAnnouncement *a,const QrxAuraModelProvenance *provenance,
                                       uint64_t current_height,char reason[QRX_AURA_MODEL_GOV_REASON_MAX]);
int qrx_aura_model_catalog_ingest_governed(QrxAuraModelCatalog *catalog,const QrxAuraModelCatalogAnnouncement *a,
                                           const uint8_t *sig,size_t sig_len,uint64_t current_height,
                                           QrxAuraModelDistKeyLookupFn publisher_key_lookup,void *publisher_key_ctx,
                                           QrxAuraModelCatalogAuthorizeFn publisher_authorize,void *publisher_auth_ctx,
                                           const QrxAuraModelGovernanceState *state,const QrxAuraModelLicensePolicy *policy,
                                           const QrxAuraModelProvenance *provenance);
#ifdef __cplusplus
}
#endif
