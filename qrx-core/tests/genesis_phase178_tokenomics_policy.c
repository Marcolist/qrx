#include "economics/qrx_economics.h"
#include "resource/qrx_resource.h"
#include "compute/qrx_compute.h"
#include "net/qrx_net_ads.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static QrxComputeJobGraph make_graph(void) {
    QrxComputeJobGraph g;
    memset(&g, 0, sizeof(g));
    g.protocol_version = QRX_COMPUTE_JOB_PROTOCOL_VERSION;
    snprintf(g.graph_id, sizeof(g.graph_id), "tokenomics-fasttrack");
    snprintf(g.owner, sizeof(g.owner), "qrx-owner");
    g.nonce = 178;
    g.max_total_fee_atoms = 400000000ULL;
    g.expiry_height = 1000;
    g.node_count = 1;
    QrxComputeJobNode *n = &g.nodes[0];
    n->node_id = 1;
    n->job_type = QRX_JOB_AI_INFERENCE;
    snprintf(n->runtime_id, sizeof(n->runtime_id), "qrx-ai-v1");
    snprintf(n->model_id, sizeof(n->model_id), "qrx/model");
    snprintf(n->model_version, sizeof(n->model_version), "1");
    snprintf(n->input_ref, sizeof(n->input_ref), "drive:in");
    snprintf(n->output_ref, sizeof(n->output_ref), "drive:out");
    n->min_memory_bytes = 1024;
    n->max_runtime_ms = 60000;
    n->max_output_bytes = 1024;
    n->max_fee_atoms = g.max_total_fee_atoms;
    n->capability_mask = QRX_JOB_CAP_READ_INPUTS | QRX_JOB_CAP_MODEL_INFERENCE | QRX_JOB_CAP_STREAM_OUTPUT;
    return g;
}

int main(void) {
    /* Protocol emission: exactly 0.25 QUB every 10 seconds at genesis. */
    assert(QRX_BLOCK_TIME_SECONDS == 10LL);
    assert(QRX_INITIAL_BLOCK_REWARD_ATOMS == 25000000ULL);
    assert(QRX_HALVING_INTERVAL_BLOCKS == 12614400LL);
    assert(QRX_MAX_SUPPLY_ATOMS == 2100000000000000ULL);
    assert((uint64_t)QRX_BLOCKS_PER_YEAR * QRX_INITIAL_BLOCK_REWARD_ATOMS == 78840000000000ULL); /* 788,400 QUB */

    /* Development reward comes out of the block subsidy; it never increases it. */
    assert(qrx_dev_reward_share(QRX_INITIAL_BLOCK_REWARD_ATOMS, 0) == 5000000ULL);
    assert(qrx_validator_reward_share(QRX_INITIAL_BLOCK_REWARD_ATOMS, 0) == 20000000ULL);
    assert(qrx_dev_reward_share(QRX_INITIAL_BLOCK_REWARD_ATOMS, 0) +
           qrx_validator_reward_share(QRX_INITIAL_BLOCK_REWARD_ATOMS, 0) == QRX_INITIAL_BLOCK_REWARD_ATOMS);

    /* Storage is client funded: 97.5% providers, 2% repair/resilience, 0.5% development. */
    assert(QRX_STORAGE_PROVIDER_BUDGET_BPS == 9750ULL);
    assert(QRX_STORAGE_RESILIENCE_RESERVE_BPS == 200ULL);
    assert(QRX_STORAGE_DEV_SHARE_BPS == 50ULL);
    QrxStorageContractSplit st;
    assert(qrx_storage_split_contract_value(1000000ULL, QRX_STORAGE_DEV_SHARE_BPS,
                                            QRX_STORAGE_RESILIENCE_RESERVE_BPS, &st) == 0);
    assert(st.provider_budget_atoms == 975000ULL);
    assert(st.resilience_atoms == 20000ULL);
    assert(st.development_atoms == 5000ULL);
    assert(st.provider_budget_atoms + st.resilience_atoms + st.development_atoms == 1000000ULL);

    /* Compute is client funded. A FastTrack premium is capped at 25% and rewards the provider. */
    assert(QRX_FASTTRACK_PROVIDER_SHARE_BPS == 9000U);
    assert(QRX_FASTTRACK_NETWORK_SHARE_BPS == 950U);
    assert(QRX_FASTTRACK_DEV_SHARE_BPS == 50U);
    assert(QRX_FASTTRACK_PROVIDER_SHARE_BPS + QRX_FASTTRACK_NETWORK_SHARE_BPS + QRX_FASTTRACK_DEV_SHARE_BPS == 10000U);
    assert(QRX_FASTTRACK_MAX_PREMIUM_BPS == 2500U);
    QrxComputeJobGraph g = make_graph();
    QrxComputeEscrow e;
    assert(qrx_compute_escrow_lock(&e, &g, 100000000ULL, 10) == 0); /* exactly 25% */
    assert(e.locked_atoms == 500000000ULL);
    QrxComputeEscrow too_high;
    assert(qrx_compute_escrow_lock(&too_high, &g, 100000001ULL, 10) != 0);
    assert(qrx_compute_escrow_settle(&e, 270000000ULL, 100) == 0);
    uint64_t fasttrack_provider = e.fasttrack_fee_atoms - e.fasttrack_dev_atoms - e.fasttrack_net_atoms;
    assert(fasttrack_provider == 90000000ULL);
    assert(e.fasttrack_dev_atoms == 500000ULL);
    assert(e.fasttrack_net_atoms == 9500000ULL);
    assert(e.refund_atoms == 130000000ULL);
    assert(270000000ULL + fasttrack_provider + e.fasttrack_dev_atoms + e.fasttrack_net_atoms + e.refund_atoms == e.locked_atoms);

    /* Advertising is advertiser funded and conserved exactly. */
    assert(QRX_AD_REWARD_DELIVERY_BPS == 5500U);
    assert(QRX_AD_REWARD_PUBLISHER_BPS == 2500U);
    assert(QRX_AD_REWARD_VIEWER_BPS == 1500U);
    assert(QRX_AD_REWARD_PROTOCOL_BPS == 450U);
    assert(QRX_AD_REWARD_DEVELOPMENT_BPS == 50U);
    assert(QRX_AD_REWARD_DELIVERY_BPS + QRX_AD_REWARD_PUBLISHER_BPS + QRX_AD_REWARD_VIEWER_BPS +
           QRX_AD_REWARD_PROTOCOL_BPS + QRX_AD_REWARD_DEVELOPMENT_BPS == 10000U);
    QrxAdRewardSplit ad;
    assert(qrx_ad_reward_split(1000ULL, &ad) == 0);
    assert(ad.delivery_atoms == 550ULL && ad.publisher_atoms == 250ULL && ad.viewer_atoms == 150ULL);
    assert(ad.protocol_atoms == 45ULL && ad.development_atoms == 5ULL);
    assert(ad.delivery_atoms + ad.publisher_atoms + ad.viewer_atoms + ad.protocol_atoms + ad.development_atoms == 1000ULL);

    puts("genesis phase 178 tokenomics policy: PASS");
    return 0;
}
