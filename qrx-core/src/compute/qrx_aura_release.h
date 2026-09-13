#pragma once
#include <stddef.h>
#include <stdint.h>
#include "compute/qrx_aura_runtime_packages.h"
#include "compute/qrx_aura_model_governance.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_AURA_RELEASE_VERSION 1u
#define QRX_AURA_RELEASE_MAX_EVIDENCE 256u

typedef struct {
    QrxAuraModelProvenance provenance;
} QrxAuraReleaseModelEvidence;

typedef struct {
    uint32_t version;
    uint64_t height;
    uint32_t failures;
    uint32_t warnings;
    uint32_t active_models;
    uint32_t recommended_models;
    uint32_t governed_models;
    uint32_t distributable_models;
    uint32_t runtime_packages;
    uint32_t discovered_devices;
    uint8_t host_secure;
    uint8_t runtime_ready;
    uint8_t model_governance_ready;
    uint8_t distribution_ready;
    uint8_t ready;
    char selected_runtime_package[QRX_AURA_RUNTIME_PACKAGE_ID_MAX+1];
    char readiness_commitment[65];
} QrxAuraReleaseReadiness;

typedef struct {
    uint32_t version;
    uint8_t changed;
    uint8_t security_hardened;
    uint8_t runtime_auto_initialized;
    uint8_t job_journal_derived;
    uint8_t lease_journal_derived;
    uint8_t cache_budget_defaulted;
} QrxAuraReleaseMigrationReport;

int qrx_aura_release_migrate_host_config(QrxAuraProviderHostConfig *cfg,const char *state_dir,QrxAuraReleaseMigrationReport *report);
int qrx_aura_release_readiness_check(const QrxAuraProviderHostConfig *host,
                                     const QrxAuraRuntimePackageCatalog *runtime_catalog,
                                     const QrxAuraModelCatalog *model_catalog,
                                     const QrxAuraModelGovernanceState *governance,
                                     const QrxAuraModelLicensePolicy *license_policy,
                                     const QrxAuraReleaseModelEvidence *evidence,size_t evidence_count,
                                     const QrxAuraModelProviderIndex *providers,
                                     uint64_t height,QrxAuraReleaseReadiness *out);
int qrx_aura_release_readiness_commitment(const QrxAuraReleaseReadiness *readiness,char out_hex[65]);
#ifdef __cplusplus
}
#endif
