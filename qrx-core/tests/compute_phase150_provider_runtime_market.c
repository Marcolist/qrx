#include "resource/qrx_resource_provider_runtime.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static QrxResourceProviderPlan base_plan(void){
    QrxResourceProviderPlan p={0};p.version=QRX_RESOURCE_PROVIDER_VERSION;strcpy(p.region,"EU-WEST-COARSE");p.enabled_mask=QRX_PROVIDER_ENABLE_ALL;p.storage_bytes=2ULL<<40;p.model_cache_bytes=500ULL<<30;p.compute_threads=12;p.network_egress_mbps=500;p.accelerator_enabled=1;p.cuda_enabled=1;p.requires_wallet_approval=1;p.opportunity_score=91;p.expected_utilization_bps=8500;return p;
}
int main(void){
    QrxResourceProviderPlan p=base_plan();QrxResourceProviderRuntime r;assert(qrx_resource_provider_runtime_init(&r,"wallet:qrx:provider-1","alpha",&p)==0);assert(r.status==QRX_PROVIDER_RUNTIME_CONFIGURED);assert(qrx_resource_provider_runtime_mark_ready(&r)==0);
    QrxResourceProviderMarketRegistration reg={0};assert(qrx_resource_provider_market_register(&r,NULL,100,&reg)==-2);
    QrxResourceProviderWalletApproval a={0};a.version=QRX_PROVIDER_APPROVAL_VERSION;strcpy(a.approval_id,"approval-1");strcpy(a.provider_id,r.provider_id);strcpy(a.network,r.network);strcpy(a.plan_commitment,r.active_plan_commitment);a.approved_at_height=90;a.expires_at_height=200;a.approved=1;
    assert(qrx_resource_provider_market_register(&r,&a,100,&reg)==0);assert(r.status==QRX_PROVIDER_RUNTIME_REGISTERED&&reg.registration_revision==1&&reg.wallet_approved);char c1[65],c2[65];assert(qrx_resource_provider_market_registration_commitment(&reg,c1)==0&&qrx_resource_provider_market_registration_commitment(&reg,c2)==0&&!strcmp(c1,c2));
    assert(qrx_resource_provider_runtime_start_serving(&r)==0);assert(qrx_resource_provider_runtime_reserve(&r,1ULL<<40,200ULL<<30,8,200)==0);assert(r.active_jobs==1);
    QrxResourceProviderPlan smaller=p;smaller.storage_bytes=512ULL<<30;smaller.model_cache_bytes=100ULL<<30;smaller.compute_threads=4;smaller.network_egress_mbps=100;assert(qrx_resource_provider_runtime_request_plan(&r,&smaller)==1);assert(r.status==QRX_PROVIDER_RUNTIME_DRAINING&&r.has_pending_plan);assert(qrx_resource_provider_runtime_shutdown(&r)==-2);
    assert(qrx_resource_provider_runtime_release(&r,1ULL<<40,200ULL<<30,8,200)==0);assert(r.active_jobs==0&&!r.has_pending_plan&&r.status==QRX_PROVIDER_RUNTIME_READY);assert(r.active_plan.compute_threads==4);
    strcpy(a.plan_commitment,r.active_plan_commitment);strcpy(a.approval_id,"approval-2");a.approved_at_height=101;a.expires_at_height=0;assert(qrx_resource_provider_market_register(&r,&a,101,&reg)==0);assert(reg.registration_revision==2);assert(qrx_resource_provider_runtime_start_serving(&r)==0);assert(qrx_resource_provider_runtime_begin_drain(&r)==0);assert(qrx_resource_provider_runtime_shutdown(&r)==0&&r.status==QRX_PROVIDER_RUNTIME_OFFLINE);
    QrxResourceProviderWalletApproval bad=a;strcpy(bad.network,"mainnet");assert(qrx_resource_provider_wallet_approval_validate(&bad,r.provider_id,r.network,r.active_plan_commitment,102)==-2);
    puts("compute_phase150_provider_runtime_market: ok");return 0;
}
