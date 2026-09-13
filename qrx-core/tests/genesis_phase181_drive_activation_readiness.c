#include "chain_params.h"
#include "economics/qrx_economics.h"
#include "resource/qrx_activation_readiness.h"
#include "resource/qrx_resource.h"
#include "resource/qrx_storage_atlas.h"
#include "qrxdb.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GIB (1024ULL*1024ULL*1024ULL)

static void put_providers(const char *dir, uint64_t current_height, uint64_t age_blocks) {
    QrxDB db; QrxDBBatch b;
    assert(qrxdb_init(&db, dir) == 0);
    assert(qrxdb_batch_begin(&db, &b) == 0);
    for (int i = 0; i < 14; ++i) {
        char id[64], key[256], val[1400], hk[320], hv[64];
        const char *region = i < 5 ? "eu-west" : (i < 10 ? "eu-central" : "us-east");
        char asn[32];
        snprintf(id, sizeof(id), "qrx-provider-%02d", i + 1);
        snprintf(asn, sizeof(asn), "AS%d", 64500 + (i % 4));
        snprintf(key, sizeof(key), "storage/provider/%s", id);
        snprintf(val, sizeof(val),
                 "1|%s|2|1000000|%llu|0|0|8000|8000|8000|8000|operator-%02d|%s|%s|%s|%s|1",
                 id, (unsigned long long)GIB, i + 1, asn, region, asn, region);
        assert(qrxdb_batch_put(&b, key, val) == 0);
        snprintf(hk, sizeof(hk), "storage/provider-activation-height/%s", id);
        uint64_t activated = current_height >= age_blocks ? current_height - age_blocks : 0;
        snprintf(hv, sizeof(hv), "%llu", (unsigned long long)activated);
        assert(qrxdb_batch_put(&b, hk, hv) == 0);
    }
    assert(qrxdb_batch_commit(&b) == 0);
    qrxdb_close(&db);
}

static void set_provider_ages(const char *dir, uint64_t current_height, uint64_t age_blocks) {
    QrxDB db; QrxDBBatch b;
    assert(qrxdb_init(&db, dir) == 0);
    assert(qrxdb_batch_begin(&db, &b) == 0);
    for (int i = 0; i < 14; ++i) {
        char id[64], hk[320], hv[64];
        snprintf(id, sizeof(id), "qrx-provider-%02d", i + 1);
        snprintf(hk, sizeof(hk), "storage/provider-activation-height/%s", id);
        uint64_t activated = current_height >= age_blocks ? current_height - age_blocks : 0;
        snprintf(hv, sizeof(hv), "%llu", (unsigned long long)activated);
        assert(qrxdb_batch_put(&b, hk, hv) == 0);
    }
    assert(qrxdb_batch_commit(&b) == 0);
    qrxdb_close(&db);
}

int main(void) {
    char dir[] = "/tmp/qrx-phase181-XXXXXX";
    assert(mkdtemp(dir));
    assert(qrx_chain_write_genesis(dir,
                                   "qrx-mainnet-phase181", "9", "QRXP181", "Phase181",
                                   20, 5000,
                                   (long long)QRX_MAX_SUPPLY_ATOMS,
                                   (long long)QRX_INITIAL_BLOCK_REWARD_ATOMS,
                                   0, QRX_BLOCK_TIME_SECONDS,
                                   100, 524288, 8192, 30, 70, 0,
                                   "qrx1dev-phase181", 1789488000LL) == 0);

    /* Pre-DRIVE allow-list contains provider readiness operations only. */
    assert(qrx_storage_preflight_tx_type("STORAGE_CAPACITY_COMMIT"));
    assert(qrx_storage_preflight_tx_type("STORAGE_CAPACITY_PROVE"));
    assert(qrx_storage_preflight_tx_type("STORAGE_PROVIDER_BOND"));
    assert(qrx_storage_preflight_tx_type("STORAGE_PROVIDER_BIND_DISCOVERY_KEY"));
    assert(qrx_storage_preflight_tx_type("STORAGE_PROVIDER_ACTIVATE"));
    assert(qrx_storage_preflight_tx_type("STORAGE_ATTEST"));
    assert(!qrx_storage_preflight_tx_type("STORAGE_CONTRACT_CREATE"));
    assert(!qrx_storage_preflight_tx_type("STORAGE_SETTLE_EPOCH"));
    assert(!qrx_storage_preflight_tx_type("STORAGE_REPAIR_SETTLE"));

    const uint64_t h = 70000;
    QrxDriveActivationReadiness r;
    assert(qrx_drive_activation_readiness(dir, h, QRX_DRIVE_V1_PLANNED_TARGET_TIME + 1, &r) == 0);
    assert(r.status == QRX_DRIVE_READINESS_NOT_READY);
    assert(r.serving_providers == 0);

    /* Enough real capacity/diversity but providers are too young -> soak. */
    put_providers(dir, h, 100);
    assert(qrx_drive_activation_readiness(dir, h, QRX_DRIVE_V1_PLANNED_TARGET_TIME + 1, &r) == 0);
    assert(r.criteria_passed == r.criteria_required);
    assert(r.serving_providers == 14);
    assert(r.independent_operators == 14);
    assert(r.independent_asns == 4);
    assert(r.visible_regions == 3);
    assert(r.proven_bytes == 14ULL * GIB);
    assert(r.avg_availability_bps == 8000);
    assert(r.avg_proof_success_bps == 8000);
    assert(r.health_score >= 60 && r.health_score <= 100);
    assert(r.mature_attested_providers == 0);
    assert(r.status == QRX_DRIVE_READINESS_WAITING_SOAK);

    /* Once all 14 attested providers have survived the full soak, date policy remains. */
    set_provider_ages(dir, h, QRX_DRIVE_READINESS_SOAK_BLOCKS);
    assert(qrx_drive_activation_readiness(dir, h, QRX_DRIVE_V1_PLANNED_TARGET_TIME - 1, &r) == 0);
    assert(r.mature_attested_providers == 14);
    assert(r.status == QRX_DRIVE_READINESS_WAITING_TARGET_DATE);
    assert(qrx_drive_activation_readiness(dir, h, QRX_DRIVE_V1_PLANNED_TARGET_TIME + 1, &r) == 0);
    assert(r.status == QRX_DRIVE_READINESS_READY);

    /* A consensus schedule changes the status to SCHEDULED, then ACTIVE at H. */
    QrxDB db; QrxDBBatch b;
    assert(qrxdb_init(&db, dir) == 0);
    assert(qrxdb_batch_begin(&db, &b) == 0);
    assert(qrxdb_batch_put(&b,
        "governance:protocol:schedule:00000000000000070100:phase181",
        "9|3|4|DRIVE_V1|phase181|tx-phase181") == 0);
    assert(qrxdb_batch_commit(&b) == 0);
    qrxdb_close(&db);
    assert(qrx_drive_activation_readiness(dir, h, QRX_DRIVE_V1_PLANNED_TARGET_TIME + 1, &r) == 0);
    assert(r.status == QRX_DRIVE_READINESS_ALREADY_SCHEDULED);
    assert(r.activation_height == 70100);
    assert(qrx_drive_activation_readiness(dir, 70100, QRX_DRIVE_V1_PLANNED_TARGET_TIME + 1, &r) == 0);
    assert(r.status == QRX_DRIVE_READINESS_ACTIVE);

    puts("phase181: PASS - consensus-derived Drive readiness progresses NOT_READY -> SOAK -> DATE -> READY -> SCHEDULED -> ACTIVE");
    return 0;
}
