#pragma once
#include <stdint.h>

#define QRX_BLOCK_TIME_SECONDS 10LL
#define QRX_BLOCKS_PER_DAY 8640LL
#define QRX_BLOCKS_PER_YEAR 3153600LL

/* Genesis/Mainnet monetary policy. 1 QUB = 100,000,000 atoms. */
#define QRX_MAX_SUPPLY_ATOMS 2100000000000000ULL
#define QRX_INITIAL_BLOCK_REWARD_ATOMS 25000000ULL /* 0.25 QUB every 10 seconds */
#define QRX_INITIAL_BLOCK_REWARD_ATOMS_STR "25000000"
#define QRX_HALVING_INTERVAL_BLOCKS 12614400LL /* four years at a 10-second target */

/* Native-asset anti-spam burns are denominated in current block-reward units.
 * At Genesis (0.25 QUB subsidy) these preserve the historical atom quotes,
 * then automatically scale down with halvings instead of becoming a permanent
 * scarcity bottleneck. No burn ever mints QUB. */
#define QRX_ASSET_BURN_MAIN_REWARD_UNITS 400ULL
#define QRX_ASSET_BURN_SUB_REWARD_UNITS 100ULL
#define QRX_ASSET_BURN_UNIQUE_REWARD_UNITS 10ULL
#define QRX_ASSET_BURN_CHANNEL_REWARD_UNITS 100ULL
#define QRX_ASSET_BURN_QUALIFIER_REWARD_UNITS 1000ULL
#define QRX_ASSET_BURN_SUBQUALIFIER_REWARD_UNITS 100ULL
#define QRX_ASSET_BURN_RESTRICTED_REWARD_UNITS 2000ULL
#define QRX_ASSET_BURN_REISSUE_REWARD_UNITS 40ULL
#define QRX_ASSET_BURN_TAG_REWARD_UNITS 4ULL

#define QRX_BOOTSTRAP_LOCK_DAYS 180LL
#define QRX_BOOTSTRAP_LOCK_BLOCKS (QRX_BOOTSTRAP_LOCK_DAYS * QRX_BLOCKS_PER_DAY)

/*
 * Reward policy:
 * Year 1: 20% dev share
 * Year 2: 10% dev share
 * Year 3: 5% dev share
 * Year 4+: 2% dev share
 */
int qrx_dev_fund_percent(int64_t block_height);
uint64_t qrx_dev_reward_share(uint64_t total_reward_atoms, int64_t block_height);
uint64_t qrx_validator_reward_share(uint64_t total_reward_atoms, int64_t block_height);
uint64_t qrx_asset_burn_from_reward_units(uint64_t block_reward_atoms, uint64_t reward_units);
