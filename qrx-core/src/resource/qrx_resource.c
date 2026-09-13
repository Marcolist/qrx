#include "resource/qrx_resource.h"
#include "chain_params.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static uint64_t mul_bps_floor(uint64_t amount, uint64_t bps) {
    /* floor(amount*bps/10000) without overflowing uint64_t. */
    return (amount / QRX_BPS_DENOMINATOR) * bps
         + ((amount % QRX_BPS_DENOMINATOR) * bps) / QRX_BPS_DENOMINATOR;
}

static int feature_flag_present(const char *flags, const char *needle) {
    if (!flags || !needle || !*needle) return 0;
    size_t nlen = strlen(needle);
    const char *p = flags;
    while (*p) {
        while (*p == ',' || *p == ' ' || *p == '\t') ++p;
        const char *e = p;
        while (*e && *e != ',' && *e != ' ' && *e != '\t' && *e != '\r' && *e != '\n') ++e;
        if ((size_t)(e - p) == nlen && memcmp(p, needle, nlen) == 0) return 1;
        p = e;
        while (*p && *p != ',') ++p;
        if (*p == ',') ++p;
    }
    return 0;
}

int qrx_resource_is_mainnet(const char *chain_dir) {
    char network_id[128];
    if (!chain_dir) return 0;
    if (qrx_chain_get_value(chain_dir, "network_id", network_id, sizeof(network_id)) != 0) return 0;
    return strstr(network_id, "mainnet") != NULL;
}

/* Backward-compatible protocol_upgrades.db format:
 * activation_height|protocol|min_tx|min_privacy|feature_flags|proposal_hash
 *
 * Keeping the on-disk shape unchanged is intentional: 0.0.7.7 nodes can read
 * a protocol-9 DRIVE_V1 schedule and warn operators that a mandatory update is
 * required before activation. */
static int find_feature_upgrade(const char *chain_dir, const char *feature, long long *activation_out) {
    if (activation_out) *activation_out = -1;
    if (!chain_dir || !feature || !*feature) return 0;
    char path[1024];
    snprintf(path, sizeof(path), "%s/governance/protocol_upgrades.db", chain_dir);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    char line[2048];
    long long best = -1;
    while (fgets(line, sizeof(line), f)) {
        long long h = -1, protocol = -1, min_tx = -1, min_priv = -1;
        char flags[512] = {0}, proposal_hash[256] = {0};
        if (sscanf(line, "%lld|%lld|%lld|%lld|%511[^|]|%255s",
                   &h, &protocol, &min_tx, &min_priv, flags, proposal_hash) != 6)
            continue;
        (void)min_tx; (void)min_priv; (void)proposal_hash;
        if (h < 0 || protocol < QRX_DRIVE_V1_MIN_PROTOCOL_VERSION) continue;
        if (!feature_flag_present(flags, feature)) continue;
        if (best < 0 || h < best) best = h;
    }
    fclose(f);
    if (activation_out) *activation_out = best;
    return best >= 0;
}

int qrx_resource_drive_v1_scheduled(const char *chain_dir) {
    if (!qrx_resource_is_mainnet(chain_dir)) return 1;
    return find_feature_upgrade(chain_dir, QRX_DRIVE_V1_FEATURE_FLAG, NULL);
}

int qrx_resource_qrx_net_v1_scheduled(const char *chain_dir) {
    if (!qrx_resource_is_mainnet(chain_dir)) return 1;
    return find_feature_upgrade(chain_dir, QRX_NET_V1_FEATURE_FLAG, NULL);
}

long long qrx_resource_activation_height(const char *chain_dir) {
    if (!qrx_resource_is_mainnet(chain_dir)) return 0LL;
    long long activation = -1;
    return find_feature_upgrade(chain_dir, QRX_DRIVE_V1_FEATURE_FLAG, &activation) ? activation : -1LL;
}

long long qrx_net_activation_height(const char *chain_dir) {
    if (!qrx_resource_is_mainnet(chain_dir)) return 0LL;
    long long activation = -1;
    return find_feature_upgrade(chain_dir, QRX_NET_V1_FEATURE_FLAG, &activation) ? activation : -1LL;
}

long long qrx_resource_target_time(const char *chain_dir) {
    (void)chain_dir;
    /* Informational planning target only. Consensus activation is by height. */
    return QRX_DRIVE_V1_PLANNED_TARGET_TIME;
}

int qrx_resource_protocol_enabled_at_height(const char *chain_dir, long long height) {
    if (!qrx_resource_is_mainnet(chain_dir)) return height >= 0;
    long long activation = qrx_resource_activation_height(chain_dir);
    return activation >= 0 && height >= activation;
}

int qrx_storage_protocol_enabled_at_height(const char *chain_dir, long long height) {
    return qrx_resource_protocol_enabled_at_height(chain_dir, height);
}

int qrx_net_protocol_enabled_at_height(const char *chain_dir, long long height) {
    if (!qrx_resource_is_mainnet(chain_dir)) return height >= 0;
    long long activation = qrx_net_activation_height(chain_dir);
    return activation >= 0 && height >= activation;
}

int qrx_storage_split_contract_value(uint64_t contract_atoms,
                                     uint64_t development_bps,
                                     uint64_t resilience_bps,
                                     QrxStorageContractSplit *out) {
    if (!out) return -1;
    if (development_bps > QRX_BPS_DENOMINATOR || resilience_bps > QRX_BPS_DENOMINATOR) return -1;
    if (development_bps + resilience_bps > QRX_BPS_DENOMINATOR) return -1;

    uint64_t dev = mul_bps_floor(contract_atoms, development_bps);
    uint64_t resilience = mul_bps_floor(contract_atoms, resilience_bps);
    if (dev > contract_atoms || resilience > contract_atoms - dev) return -1;

    out->development_atoms = dev;
    out->resilience_atoms = resilience;
    out->provider_budget_atoms = contract_atoms - dev - resilience;
    return 0;
}

static const QrxStorageRedundancyProfile PROFILES[] = {
    {"FAST", 0, 0, QRX_STORAGE_FAST_FULL_REPLICAS},
    {"STANDARD", QRX_STORAGE_STANDARD_DATA_SHARDS, QRX_STORAGE_STANDARD_PARITY_SHARDS, 0},
    {"ARCHIVE", QRX_STORAGE_ARCHIVE_DATA_SHARDS, QRX_STORAGE_ARCHIVE_PARITY_SHARDS, 0}
};

const QrxStorageRedundancyProfile *qrx_storage_profile_by_name(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < sizeof(PROFILES)/sizeof(PROFILES[0]); ++i)
        if (!strcmp(name, PROFILES[i].name)) return &PROFILES[i];
    return NULL;
}

uint64_t qrx_resource_capacity_weight(uint64_t proven_free_bytes) {
    /* Consensus-friendly integer sqrt of free MiB.  This deliberately makes
     * capacity opportunity sub-linear: more storage helps, but does not grant
     * linear control over placement. */
    uint64_t x = proven_free_bytes >> 20;
    if (x == 0) return proven_free_bytes ? 1 : 0;
    uint64_t r = 0;
    uint64_t bit = 1ULL << 62;
    while (bit > x) bit >>= 2;
    while (bit) {
        if (x >= r + bit) {
            x -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

const char *qrx_resource_type_name(QrxResourceType type) {
    switch (type) {
        case QRX_RESOURCE_STORAGE: return "STORAGE";
        case QRX_RESOURCE_COMPUTE: return "COMPUTE";
        case QRX_RESOURCE_GPU: return "GPU";
        case QRX_RESOURCE_SPECIALIZED: return "SPECIALIZED";
        default: return "UNKNOWN";
    }
}
