#include "chain_params.h"
#include "economics/qrx_economics.h"
#include "resource/qrx_resource.h"
#include "net/qrx_net_consensus.h"
#include "qrxdb.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void mkdir_or_die(const char *p) { assert(mkdir(p, 0700) == 0); }

int main(void) {
    char dir[] = "/tmp/qrx-genesis179-XXXXXX";
    assert(mkdtemp(dir));

    assert(qrx_chain_write_genesis(dir,
                                   "qrx-mainnet",
                                   "62",
                                   "QRXM62",
                                   "QRX Mainnet",
                                   20,
                                   5000,
                                   (long long)QRX_MAX_SUPPLY_ATOMS,
                                   (long long)QRX_INITIAL_BLOCK_REWARD_ATOMS,
                                   0,
                                   QRX_BLOCK_TIME_SECONDS,
                                   100,
                                   524288,
                                   8192,
                                   30,
                                   70,
                                   0,
                                   "qrx1dev-placeholder",
                                   1789488000LL) == 0);

    char memo[1024];
    assert(qrx_chain_get_value(dir, "genesis_memo", memo, sizeof(memo)) == 0);
    assert(strcmp(memo,
        "At the threshold of the AI age, amid global change, QRX was launched to keep value, verification, and digital sovereignty in the hands of people -- and to help ensure that the opportunities of artificial intelligence are open to all, not reserved for a few.") == 0);

    char policy[128];
    assert(qrx_chain_get_value_at_height(dir, 0, "asset_burn_policy", policy, sizeof(policy)) == 0);
    assert(strcmp(policy, "block_reward_units_v1") == 0);
    assert(qrx_chain_get_ll_at_height_or_default(dir, 0, "asset_issue_main_burn_reward_units", -1) == 400);
    assert(qrx_chain_get_ll_at_height_or_default(dir, 0, "asset_issue_restricted_burn_reward_units", -1) == 2000);
    assert(qrx_chain_get_ll_at_height_or_default(dir, 0, "asset_tag_burn_reward_units", -1) == 4);

    long long r0 = qrx_chain_get_block_reward_at_height(dir, 0, QRX_INITIAL_BLOCK_REWARD_ATOMS, QRX_HALVING_INTERVAL_BLOCKS);
    long long r1 = qrx_chain_get_block_reward_at_height(dir, QRX_HALVING_INTERVAL_BLOCKS, QRX_INITIAL_BLOCK_REWARD_ATOMS, QRX_HALVING_INTERVAL_BLOCKS);
    assert(r0 == 25000000LL);
    assert(r1 == 12500000LL);
    assert(qrx_asset_burn_from_reward_units((uint64_t)r0, QRX_ASSET_BURN_MAIN_REWARD_UNITS) == 10000000000ULL);
    assert(qrx_asset_burn_from_reward_units((uint64_t)r1, QRX_ASSET_BURN_MAIN_REWARD_UNITS) == 5000000000ULL);
    assert(qrx_asset_burn_from_reward_units((uint64_t)r0, QRX_ASSET_BURN_RESTRICTED_REWARD_UNITS) == 50000000000ULL);
    assert(qrx_asset_burn_from_reward_units(0, QRX_ASSET_BURN_TAG_REWARD_UNITS) == QRX_ASSET_BURN_TAG_REWARD_UNITS);

    char gov[1024], upg[1024];
    snprintf(gov, sizeof(gov), "%s/governance", dir);
    snprintf(upg, sizeof(upg), "%s/protocol_upgrades.db", gov);
    mkdir_or_die(gov);

    assert(strcmp(QRX_ADVERTISING_V1_FEATURE_FLAG, "ADVERTISING_V1") == 0);
    assert(qrx_net_protocol_enabled_at_height(dir, 1000) == 0);
    assert(qrx_advertising_protocol_enabled_at_height(dir, 1000) == 0);

    /* Legacy local files are ignored by Mainnet after the on-chain governance migration. */
    FILE *f = fopen(upg, "wb"); assert(f);
    fputs("100|9|3|4|QRX_NET_V1|legacy-net-proposal\n", f);
    fputs("200|9|3|4|ADVERTISING_V1|legacy-ad-proposal\n", f);
    fclose(f);
    assert(qrx_net_protocol_enabled_at_height(dir, 1000) == 0);
    assert(qrx_advertising_protocol_enabled_at_height(dir, 1000) == 0);

    /* The authoritative schedule is the consensus QRXDB state. Phase 181 tests
       the full 3-of-5 signed transaction path that creates these records. */
    QrxDB gdb; QrxDBBatch gb; assert(qrxdb_init(&gdb, dir) == 0);
    assert(qrxdb_batch_begin(&gdb, &gb) == 0);
    assert(qrxdb_batch_put(&gb, "governance:protocol:schedule:00000000000000000100:net-proposal",
                           "9|3|4|QRX_NET_V1|net-proposal|tx-net") == 0);
    assert(qrxdb_batch_put(&gb, "governance:protocol:schedule:00000000000000000200:ad-proposal",
                           "9|3|4|ADVERTISING_V1|ad-proposal|tx-ad") == 0);
    assert(qrxdb_batch_commit(&gb) == 0); qrxdb_close(&gdb);
    assert(qrx_net_protocol_enabled_at_height(dir, 99) == 0);
    assert(qrx_net_protocol_enabled_at_height(dir, 100) == 1);
    assert(qrx_advertising_protocol_enabled_at_height(dir, 199) == 0);

    {
        char roothex[129]; memset(roothex, 'a', 128); roothex[128] = 0;
        char payload[1024];
        snprintf(payload, sizeof(payload),
                 "campaign_id=gate-test;target_url=https://example.invalid;category=test;creative_root_hex=%s;start_height=150;end_height=300;cost_per_impression_atoms=100", roothex);
        QrxServiceEconomicEffect eff;
        /* Even a direct consensus call cannot bypass the missing ADVERTISING_V1 gate. */
        assert(qrx_net_consensus_prepare(dir, "AD_CAMPAIGN_CREATE", "qrx1advertiser", "qrx1advertiser", 100, payload, "tx-pre-ad", 150, &eff) != 0);
    }

    assert(qrx_resource_advertising_v1_scheduled(dir) == 1);
    assert(qrx_advertising_activation_height(dir) == 200);
    assert(qrx_advertising_protocol_enabled_at_height(dir, 199) == 0);
    assert(qrx_advertising_protocol_enabled_at_height(dir, 200) == 1);
    {
        char roothex[129]; memset(roothex, 'b', 128); roothex[128] = 0;
        char payload[1024];
        snprintf(payload, sizeof(payload),
                 "campaign_id=gate-test-live;target_url=https://example.invalid;category=test;creative_root_hex=%s;start_height=200;end_height=300;cost_per_impression_atoms=100", roothex);
        QrxServiceEconomicEffect eff;
        assert(qrx_net_consensus_prepare(dir, "AD_CAMPAIGN_CREATE", "qrx1advertiser", "qrx1advertiser", 100, payload, "tx-post-ad", 200, &eff) == 0);
        assert(eff.debit_atoms == 100);
    }

    assert(qrx_resource_target_time(dir) == 1796058000LL);
    assert(qrx_net_target_time(dir) == 1796662800LL);
    assert(qrx_advertising_target_time(dir) == 1797354000LL);
    assert(qrx_compute_pouc_target_time(dir) == 1801414800LL);

    printf("PASS: Genesis memo is canonical, asset burns scale with subsidy, and ADVERTISING_V1 is independently activation-gated\n");
    return 0;
}
