#include "net/qrx_net_ads.h"
#include "net/qrx_net_consensus.h"
#include "net/qrx_net_name.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void put_provider(QrxDB*db,const char*id,const char*op,const char*asn,const char*region){
 char k[256],v[1400]; snprintf(k,sizeof(k),"storage/provider/%s",id);
 snprintf(v,sizeof(v),"1|%s|2|100000|1000000000|0|0|10000|10000|10000|10000|%s|%s|%s|%s|%s|1",id,op,asn,region,asn,region);
 assert(qrxdb_put(db,k,v)==0);
}
static void stage(QrxDB*db,const char*dir,const char*tx,const char*from,const char*to,uint64_t amount,const char*payload,const char*txid,uint64_t h){QrxDBBatch b;assert(qrxdb_batch_begin(db,&b)==0);assert(qrx_net_consensus_stage(db,&b,dir,tx,from,to,amount,payload,txid,h)==0);assert(qrxdb_batch_commit(&b)==0);}
int main(void){
 QrxAdRewardSplit sp; assert(qrx_ad_reward_split(1000,&sp)==0); assert(sp.delivery_atoms==550&&sp.publisher_atoms==250&&sp.viewer_atoms==150&&sp.protocol_atoms==45&&sp.development_atoms==5);
 assert(qrx_ad_creative_policy("image/webp",1024,0,0,0,0)==0); assert(qrx_ad_creative_policy("text/html",1024,0,0,0,0)!=0); assert(qrx_ad_creative_policy("image/png",1024,1,0,0,0)!=0); assert(qrx_ad_creative_policy("image/png",1024,0,1,0,0)!=0);
 char dir[]="/tmp/qrx-ad-XXXXXX"; assert(mkdtemp(dir)); QrxDB db; assert(qrxdb_init(&db,dir)==0);
 const char*publisher="qrx1publisher"; QrxDomainPrice pr; assert(qrx_domain_price("news.qrx",QRX_DOMAIN_DEFAULT_BASE_ANNUAL_ATOMS,0,&pr)==0); char dp[512]; snprintf(dp,sizeof(dp),"name=news.qrx;years=1;qub_address=%s;web_manifest_root_hex=-;publishing_commitment_hex=-",publisher); stage(&db,dir,"DOMAIN_REGISTER",publisher,publisher,pr.annual_atoms+pr.reservation_bond_atoms,dp,"dom",90);
 put_provider(&db,"qrx1p1","op1","AS1","DE"); put_provider(&db,"qrx1p2","op2","AS2","FR"); put_provider(&db,"qrx1p3","op3","AS3","NL");
 char root[129]; memset(root,'a',128); root[128]=0; char cp[1200]; snprintf(cp,sizeof(cp),"campaign_id=camp1;target_url=qrx://shop.qrx/;category=general;creative_root_hex=%s;start_height=101;end_height=500;cost_per_impression_atoms=1000",root); stage(&db,dir,"AD_CAMPAIGN_CREATE","qrx1adv","qrx1adv",10000,cp,"camp",100);
 const char*rp="campaign_id=camp1;impression_id=imp1;publisher_domain=news.qrx;viewer_reward_address=qrx1viewer;viewer_token_hash=vt1;creative_root_hex=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa;epoch=1";
 stage(&db,dir,"AD_DELIVERY_RECEIPT","qrx1p1","qrx1p1",0,rp,"r1",150); stage(&db,dir,"AD_DELIVERY_RECEIPT","qrx1p2","qrx1p2",0,rp,"r2",150); stage(&db,dir,"AD_DELIVERY_RECEIPT","qrx1p3","qrx1p3",0,rp,"r3",150);
 const char*settle="campaign_id=camp1;impression_id=imp1;publisher_domain=news.qrx;viewer_reward_address=qrx1viewer;viewer_token_hash=vt1;epoch=1"; QrxServiceEconomicEffect e={0}; assert(qrx_net_consensus_prepare(dir,"AD_IMPRESSION_SETTLE",publisher,publisher,0,settle,"s",150,&e)==0); assert(e.protocol_fee_atoms==45&&e.development_credit_atoms==5); stage(&db,dir,"AD_IMPRESSION_SETTLE",publisher,publisher,0,settle,"s",150);
 QrxAdCampaign c; assert(qrx_ad_campaign_get(&db,"camp1",&c)==0&&c.remaining_budget_atoms==9000&&c.settled_impressions==1);
 assert(qrx_net_consensus_prepare(dir,"AD_IMPRESSION_SETTLE",publisher,publisher,0,settle,"s2",150,&e)!=0); /* replay/frequency cap */
 char v[64]; assert(qrxdb_get(&db,"qrxnet/ad/reward/qrx1viewer",v,sizeof(v))==0&&strtoull(v,0,10)==150); assert(qrxdb_get(&db,"qrxnet/ad/reward/qrx1publisher",v,sizeof(v))==0&&strtoull(v,0,10)==250);
 assert(qrx_net_consensus_prepare(dir,"AD_REWARD_CLAIM","qrx1viewer","qrx1viewer",0,"-","claim",151,&e)==0&&e.self_credit_atoms==150); stage(&db,dir,"AD_REWARD_CLAIM","qrx1viewer","qrx1viewer",0,"-","claim",151); assert(qrxdb_get(&db,"qrxnet/ad/reward/qrx1viewer",v,sizeof(v))==0&&strtoull(v,0,10)==0);
 assert(qrxdb_close(&db)==0); puts("PASS: QRX Ad Shield uses escrowed campaigns, 3-provider receipts, replay/frequency caps, and 55/25/15/4.5/0.5 rewards"); return 0;
}
