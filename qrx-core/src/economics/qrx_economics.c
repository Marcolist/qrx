#include "qrx_economics.h"

int qrx_dev_fund_percent(int64_t block_height) {
    if(block_height < QRX_BLOCKS_PER_YEAR) return 20;
    if(block_height < QRX_BLOCKS_PER_YEAR * 2LL) return 10;
    if(block_height < QRX_BLOCKS_PER_YEAR * 3LL) return 5;
    return 2;
}

uint64_t qrx_dev_reward_share(uint64_t total_reward_atoms, int64_t block_height) {
    int pct = qrx_dev_fund_percent(block_height);
    /* Overflow-safe floor(total * pct / 100). */
    return (total_reward_atoms / 100ULL) * (uint64_t)pct
         + ((total_reward_atoms % 100ULL) * (uint64_t)pct) / 100ULL;
}

uint64_t qrx_validator_reward_share(uint64_t total_reward_atoms, int64_t block_height) {
    uint64_t dev = qrx_dev_reward_share(total_reward_atoms, block_height);
    if(dev > total_reward_atoms) return 0;
    return total_reward_atoms - dev;
}

uint64_t qrx_asset_burn_from_reward_units(uint64_t block_reward_atoms, uint64_t reward_units) {
    if (!reward_units) return 0;
    if (!block_reward_atoms) return reward_units; /* 1 atom per reward unit floor after subsidy exhaustion. */
    if (block_reward_atoms > UINT64_MAX / reward_units) return UINT64_MAX;
    return block_reward_atoms * reward_units;
}
