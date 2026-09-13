#include "resource/qrx_resource.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define T_MKDIR(p) _mkdir(p)
#define T_PID _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define T_MKDIR(p) mkdir((p), 0700)
#define T_PID getpid()
#endif

int main(void) {
    QrxStorageContractSplit split;
    assert(QRX_DRIVE_V1_MIN_PROTOCOL_VERSION == 9LL);
    assert(QRX_DRIVE_V1_PLANNED_TARGET_TIME == 1796058000LL);
    assert(strcmp(QRX_DRIVE_V1_FEATURE_FLAG, "DRIVE_V1") == 0);
    assert(strcmp(QRX_NET_V1_FEATURE_FLAG, "QRX_NET_V1") == 0);
    assert(strcmp(QRX_ADVERTISING_V1_FEATURE_FLAG, "ADVERTISING_V1") == 0);
    assert(QRX_NET_V1_PLANNED_TARGET_TIME == 1796662800LL);
    assert(QRX_ADVERTISING_V1_PLANNED_TARGET_TIME == 1797354000LL);
    assert(QRX_COMPUTE_POUC_V1_PLANNED_TARGET_TIME == 1801414800LL);
    assert(QRX_STORAGE_DEV_SHARE_BPS == 50ULL);
    assert(QRX_STORAGE_RESILIENCE_RESERVE_BPS == 200ULL);
    assert(QRX_STORAGE_PROVIDER_BUDGET_BPS == 9750ULL);

    assert(qrx_storage_split_contract_value(1000000ULL,
                                            QRX_STORAGE_DEV_SHARE_BPS,
                                            QRX_STORAGE_RESILIENCE_RESERVE_BPS,
                                            &split) == 0);
    assert(split.development_atoms == 5000ULL);
    assert(split.resilience_atoms == 20000ULL);
    assert(split.provider_budget_atoms == 975000ULL);

    assert(qrx_storage_split_contract_value(UINT64_MAX,
                                            QRX_STORAGE_DEV_SHARE_BPS,
                                            QRX_STORAGE_RESILIENCE_RESERVE_BPS,
                                            &split) == 0);
    assert(split.development_atoms + split.resilience_atoms <= UINT64_MAX);
    assert(split.provider_budget_atoms == UINT64_MAX - split.development_atoms - split.resilience_atoms);

    assert(qrx_storage_split_contract_value(1, 9000, 2000, &split) != 0);

    const QrxStorageRedundancyProfile *standard = qrx_storage_profile_by_name("STANDARD");
    const QrxStorageRedundancyProfile *fast = qrx_storage_profile_by_name("FAST");
    const QrxStorageRedundancyProfile *archive = qrx_storage_profile_by_name("ARCHIVE");
    assert(standard && standard->data_shards == 10 && standard->parity_shards == 4);
    assert(fast && fast->full_replicas == 3);
    assert(archive && archive->data_shards == 16 && archive->parity_shards == 4);
    assert(qrx_storage_profile_by_name("UNKNOWN") == NULL);

    assert(qrx_resource_capacity_weight(0) == 0);
    assert(qrx_resource_capacity_weight(1) == 1);
    assert(qrx_resource_capacity_weight(1024ULL * 1024ULL) == 1);
    assert(qrx_resource_capacity_weight(4ULL * 1024ULL * 1024ULL) == 2);
    assert(qrx_resource_capacity_weight(100ULL * 1024ULL * 1024ULL) == 10);

    assert(strcmp(qrx_resource_type_name(QRX_RESOURCE_STORAGE), "STORAGE") == 0);
    assert(strcmp(qrx_resource_type_name(QRX_RESOURCE_COMPUTE), "COMPUTE") == 0);

    /* Mainnet is fail-closed until a post-Genesis protocol-9 DRIVE_V1 schedule exists. */
    {
        char root[512], gov[640], meta[640], upg[640];
#ifdef _WIN32
        const char *tmp = getenv("TEMP"); if (!tmp) tmp = ".";
#else
        const char *tmp = getenv("TMPDIR"); if (!tmp) tmp = "/tmp";
#endif
        snprintf(root, sizeof(root), "%s/qrx-resource-upgrade-%lld-%ld", tmp, (long long)time(NULL), (long)T_PID);
        snprintf(gov, sizeof(gov), "%s/governance", root);
        snprintf(meta, sizeof(meta), "%s/chain.meta", root);
        snprintf(upg, sizeof(upg), "%s/protocol_upgrades.db", gov);
        T_MKDIR(root); T_MKDIR(gov);
        FILE *f = fopen(meta, "wb"); assert(f); fputs("network_id=qrx-mainnet-community\n", f); fclose(f);
        assert(qrx_resource_drive_v1_scheduled(root) == 0);
        assert(qrx_resource_activation_height(root) == -1);
        assert(qrx_resource_protocol_enabled_at_height(root, 999999) == 0);
        assert(qrx_resource_qrx_net_v1_scheduled(root) == 0);
        assert(qrx_resource_advertising_v1_scheduled(root) == 0);
        assert(qrx_net_protocol_enabled_at_height(root, 999999) == 0);
        assert(qrx_advertising_protocol_enabled_at_height(root, 999999) == 0);
        /* A local legacy schedule must NOT become Mainnet consensus authority. */
        f = fopen(upg, "wb"); assert(f);
        fputs("700000|9|3|4|DRIVE_V1,QRX_NET_V1,SHIELDED_PROOF_V4|deadbeef\n", f);
        fputs("710000|9|3|4|ADVERTISING_V1|feedface\n", f); fclose(f);
        assert(qrx_resource_drive_v1_scheduled(root) == 0);
        assert(qrx_resource_activation_height(root) == -1);
        assert(qrx_resource_protocol_enabled_at_height(root, 700000) == 0);
        assert(qrx_resource_qrx_net_v1_scheduled(root) == 0);
        assert(qrx_net_activation_height(root) == -1);
        assert(qrx_net_protocol_enabled_at_height(root, 700000) == 0);
        assert(qrx_resource_advertising_v1_scheduled(root) == 0);
        assert(qrx_advertising_activation_height(root) == -1);
        assert(qrx_advertising_protocol_enabled_at_height(root, 710000) == 0);
    }

    puts("PASS: QRX 0.0.8.1 resource foundation uses post-Genesis DRIVE_V1 protocol-9 activation, sub-linear capacity weight, redundancy profiles and storage economics split");
    return 0;
}
