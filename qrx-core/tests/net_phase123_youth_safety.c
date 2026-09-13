#include "net/qrx_net_youth.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 QrxYouthPolicy c,t,a,r;QrxYouthContentRating general={7,QRX_YOUTH_CAT_GENERAL,1},mature={18,QRX_YOUTH_CAT_ADULT,1},gamble={12,QRX_YOUTH_CAT_GAMBLING,1},unrated={0,0,0};
 qrx_youth_policy_default(QRX_YOUTH_PROFILE_CHILD,&c);assert(qrx_youth_policy_validate(&c)==0);assert(c.ads_blocked&&c.viewer_rewards_blocked);assert(qrx_youth_evaluate_navigation(&c,"kids.qrx",&general)==QRX_YOUTH_ALLOW);assert(qrx_youth_evaluate_navigation(&c,"x.qrx",&mature)==QRX_YOUTH_BLOCK_AGE);assert(qrx_youth_evaluate_navigation(&c,"x.qrx",&gamble)==QRX_YOUTH_BLOCK_CATEGORY);assert(qrx_youth_evaluate_navigation(&c,"x.qrx",&unrated)==QRX_YOUTH_BLOCK_UNRATED);assert(qrx_youth_evaluate_capability(&c,QRX_YOUTH_CAP_SPONSORED_ADS,0)==QRX_YOUTH_BLOCK_ADS);assert(qrx_youth_evaluate_capability(&c,QRX_YOUTH_CAP_VIEWER_REWARD,0)==QRX_YOUTH_BLOCK_VIEWER_REWARD);assert(qrx_youth_evaluate_capability(&c,QRX_YOUTH_CAP_PAYMENT,1)==QRX_YOUTH_BLOCK_CAPABILITY);
 assert(qrx_youth_policy_add_allow(&c,"Kids.QRX")==0);assert(qrx_youth_evaluate_navigation(&c,"kids.qrx",&general)==QRX_YOUTH_ALLOW);assert(qrx_youth_evaluate_navigation(&c,"other.qrx",&general)==QRX_YOUTH_BLOCK_DOMAIN);assert(qrx_youth_policy_add_block(&c,"kids.qrx")==0);assert(qrx_youth_evaluate_navigation(&c,"kids.qrx",&general)==QRX_YOUTH_BLOCK_DOMAIN);
 qrx_youth_policy_default(QRX_YOUTH_PROFILE_TEEN,&t);assert(qrx_youth_evaluate_capability(&t,QRX_YOUTH_CAP_DAPP,0)==QRX_YOUTH_ALLOW);assert(qrx_youth_evaluate_capability(&t,QRX_YOUTH_CAP_SPONSORED_ADS,0)==QRX_YOUTH_BLOCK_ADS);
 qrx_youth_policy_default(QRX_YOUTH_PROFILE_ADULT,&a);assert(qrx_youth_evaluate_navigation(&a,"x.qrx",&mature)==QRX_YOUTH_ALLOW);assert(qrx_youth_evaluate_capability(&a,QRX_YOUTH_CAP_PAYMENT,UINT64_MAX)==QRX_YOUTH_ALLOW);
 const char *f="phase123-youth-policy.bin";remove(f);assert(qrx_youth_policy_save_encrypted(f,&t,"4826")==0);memset(&r,0,sizeof(r));assert(qrx_youth_policy_load_encrypted(f,&r,"4826")==0);assert(memcmp(&r,&t,sizeof(t))==0);assert(qrx_youth_policy_load_encrypted(f,&r,"0000")!=0);
 FILE*fp=fopen(f,"r+b");assert(fp);fseek(fp,-1,SEEK_END);int ch=fgetc(fp);fseek(fp,-1,SEEK_END);fputc(ch^0x55,fp);fclose(fp);assert(qrx_youth_policy_load_encrypted(f,&r,"4826")!=0);remove(f);
 puts("net_phase123_youth_safety PASS");return 0;
}
