#pragma once
#include "qrxdb.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_AD_REWARD_DELIVERY_BPS 5500u
#define QRX_AD_REWARD_PUBLISHER_BPS 2500u
#define QRX_AD_REWARD_VIEWER_BPS 1500u
#define QRX_AD_REWARD_PROTOCOL_BPS 450u
#define QRX_AD_REWARD_DEVELOPMENT_BPS 50u
#define QRX_AD_MIN_PROVIDER_RECEIPTS 3u
#define QRX_AD_FREQUENCY_EPOCH_BLOCKS 144u
#define QRX_AD_MIN_IMPRESSION_ATOMS 100u
#define QRX_AD_MAX_CREATIVE_BYTES (4u*1024u*1024u)

typedef struct {
 char advertiser[160], campaign_id[129], target_url[512], category[48];
 uint8_t creative_root[64];
 uint64_t start_height,end_height,cost_per_impression_atoms,total_budget_atoms,remaining_budget_atoms,settled_impressions;
 uint32_t status;
} QrxAdCampaign;
typedef struct {uint64_t delivery_atoms,publisher_atoms,viewer_atoms,protocol_atoms,development_atoms;} QrxAdRewardSplit;
int qrx_ad_reward_split(uint64_t atoms,QrxAdRewardSplit*out);
int qrx_ad_creative_policy(const char *mime,size_t bytes,int has_script,int has_tracking,int has_popup,int has_fingerprinting);
int qrx_ad_campaign_get(QrxDB *db,const char *campaign_id,QrxAdCampaign*out);
#ifdef __cplusplus
}
#endif
