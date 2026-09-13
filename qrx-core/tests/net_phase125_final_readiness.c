#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "net/qrx_net_ads.h"
#include "net/qrx_net_youth.h"
#include "net/qrx_net_sandbox.h"
#include "resource/qrx_storage_atlas.h"
int main(void){
  assert(QRX_AD_REWARD_DELIVERY_BPS+QRX_AD_REWARD_PUBLISHER_BPS+QRX_AD_REWARD_VIEWER_BPS+QRX_AD_REWARD_PROTOCOL_BPS+QRX_AD_REWARD_DEVELOPMENT_BPS==10000u);
  assert(QRX_AD_MIN_PROVIDER_RECEIPTS>=3u);
  QrxYouthPolicy c,t; qrx_youth_policy_default(QRX_YOUTH_PROFILE_CHILD,&c); qrx_youth_policy_default(QRX_YOUTH_PROFILE_TEEN,&t);
  assert(c.ads_blocked&&c.viewer_rewards_blocked&&c.unrated_blocked); assert(t.ads_blocked&&t.viewer_rewards_blocked&&t.unrated_blocked);
  QrxNetSandboxRequest p; assert(qrx_net_sandbox_parse_url("qrx://Example.QRX/index.html",&p)==0); assert(!p.dns_allowed&&!p.wallet_ipc_allowed&&!p.filesystem_allowed&&!p.external_network_allowed);
  assert(qrx_net_sandbox_parse_url("https://example.qrx/",&p)!=0); assert(qrx_net_sandbox_parse_url("qrx://example.qrx/../wallet",&p)!=0);
  QrxStorageAtlasCell a={0}; snprintf(a.region,sizeof(a.region),"EU-A"); a.publicly_visible=1; a.provider_count=1; a.free_bytes=0; a.opportunity_score=100; QrxStorageMission*m=NULL;size_t n=0;
  assert(qrx_storage_missions_from_atlas(&a,1,1024,10,&m,&n)==0&&n==1&&m[0].incentive_factor_bps<=12000); free(m);
  puts("net_phase125_final_readiness PASS"); return 0;
}
