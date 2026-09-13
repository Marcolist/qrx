#pragma once
#include <stddef.h>
#include <stdint.h>
#include "compute/qrx_aura_provider_host.h"
#include "compute/qrx_aura_job_journal.h"
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_AURA_SUPERVISOR_VERSION 1u
#define QRX_AURA_SUPERVISOR_MAX_RELAYS QRX_AURA_PROVIDER_RELAY_MAX

typedef struct {
    char endpoint[256];
    uint32_t successes;
    uint32_t failures;
    uint32_t consecutive_failures;
    uint32_t last_latency_ms;
    uint64_t cooldown_until_height;
    uint64_t last_success_height;
    uint8_t enabled;
} QrxAuraRelayHealth;

typedef struct {
    uint32_t version;
    QrxAuraRelayHealth relays[QRX_AURA_SUPERVISOR_MAX_RELAYS];
    uint32_t relay_count;
    int32_t active_relay;
    uint8_t relay_required;
    uint8_t runtime_healthy;
    uint32_t runtime_consecutive_failures;
    uint64_t last_runtime_probe_height;
    uint64_t restart_count;
    uint64_t revision;
    uint32_t keep_committed_jobs;
    uint32_t keep_prepared_jobs;
} QrxAuraProviderSupervisor;

typedef struct {
    uint32_t relay_count;
    int32_t active_relay;
    uint32_t available_relays;
    uint8_t runtime_healthy;
    uint32_t runtime_consecutive_failures;
    uint8_t restart_recommended;
    uint64_t revision;
} QrxAuraProviderSupervisorStatus;

int qrx_aura_provider_supervisor_init(QrxAuraProviderSupervisor *s,const QrxAuraProviderHostConfig *host);
int qrx_aura_provider_supervisor_record_relay(QrxAuraProviderSupervisor *s,uint32_t relay_index,int success,uint32_t latency_ms,uint64_t current_height);
int qrx_aura_provider_supervisor_select_relay(QrxAuraProviderSupervisor *s,uint64_t current_height);
int qrx_aura_provider_supervisor_record_runtime(QrxAuraProviderSupervisor *s,int healthy,uint64_t current_height);
int qrx_aura_provider_supervisor_restart_recommended(const QrxAuraProviderSupervisor *s,uint64_t current_height);
int qrx_aura_provider_supervisor_maintenance(QrxAuraProviderSupervisor *s,QrxAuraJobJournal *jobs,const char *job_journal_path,size_t *removed_out);
int qrx_aura_provider_supervisor_status(const QrxAuraProviderSupervisor *s,uint64_t current_height,QrxAuraProviderSupervisorStatus *out);
#ifdef __cplusplus
}
#endif
