#include "resource/qrx_storage_capacity.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    QrxStorageProviderCapacity p[3];
    memset(p, 0, sizeof(p));

    strcpy(p[0].provider_id, "provider-a");
    p[0].configured_bytes = 1000;
    p[0].physical_eligible_bytes = 900;
    p[0].proven_bytes = 800;
    p[0].allocated_physical_bytes = 500;
    p[0].reserve_committed_bytes = 50;
    p[0].availability_bps = 9990;
    p[0].proof_success_bps = 10000;

    strcpy(p[1].provider_id, "provider-b");
    p[1].configured_bytes = 2000;
    p[1].physical_eligible_bytes = 2000;
    p[1].proven_bytes = 1500;
    p[1].allocated_physical_bytes = 1000;
    p[1].reserve_committed_bytes = 100;
    p[1].availability_bps = 9980;
    p[1].proof_success_bps = 9990;

    strcpy(p[2].provider_id, "provider-c");
    p[2].configured_bytes = 1000;
    p[2].physical_eligible_bytes = 900;
    p[2].proven_bytes = 700;
    p[2].allocated_physical_bytes = 300;
    p[2].reserve_committed_bytes = 0;
    p[2].availability_bps = 9900;
    p[2].proof_success_bps = 9950;

    const QrxStorageRedundancyProfile *standard = qrx_storage_profile_by_name("STANDARD");
    assert(standard);
    QrxStorageNetworkCapacity n;
    assert(qrx_storage_capacity_aggregate(p, 3, 1280, 100, 2, 1, standard, &n) == 0);
    assert(n.configured_bytes == 4000);
    assert(n.raw_eligible_bytes == 3800);
    assert(n.proven_bytes == 3000);
    assert(n.allocated_physical_bytes == 1800);
    assert(n.free_proven_physical_bytes == 1200);
    assert(n.available_usable_bytes == 857); /* floor(1200 * 10 / 14) */
    assert(n.usable_capacity_bytes == 2137);
    assert(n.reserve_committed_bytes == 150);
    assert(n.providers_total == 3 && n.providers_proven == 3);
    assert(n.utilization_bps == 6000);
    assert(n.resilience_headroom_bps == 4000);
    assert(n.redundancy_x10000 == 14062); /* 1800/1280 ~= 1.4062x */
    assert(n.healthy_shards == 100 && n.degraded_shards == 2 && n.repairing_shards == 1);

    assert(qrx_storage_logical_capacity_for_profile(1400, standard) == 1000);
    assert(qrx_storage_logical_capacity_for_profile(3000, qrx_storage_profile_by_name("FAST")) == 1000);

    /* Invalid monotonic capacity claims fail closed. */
    p[0].proven_bytes = 901; /* raw eligible is 900 */
    assert(qrx_storage_provider_capacity_validate(&p[0]) != 0);

    puts("PASS: QRX 0.0.8.2 capacity accounting and redundancy-aware usable capacity");
    return 0;
}
