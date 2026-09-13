#include "resource/qrx_resource_dashboard.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 QrxStorageAtlasProvider p[14]; memset(p,0,sizeof(p));
 for(int i=0;i<14;i++){snprintf(p[i].provider_id,sizeof(p[i].provider_id),"provider-%d",i);snprintf(p[i].asn,sizeof(p[i].asn),"AS%d",i%8);strcpy(p[i].region,i<7?"europe-west":"southern-africa");p[i].network_attested=1;strcpy(p[i].capacity.provider_id,p[i].provider_id);p[i].capacity.configured_bytes=2000;p[i].capacity.physical_eligible_bytes=1800;p[i].capacity.proven_bytes=1600;p[i].capacity.allocated_physical_bytes=800;p[i].capacity.availability_bps=9900;p[i].capacity.proof_success_bps=9950;}
 QrxResourceWorkloadStats w={.active_contracts=4,.logical_user_bytes=8000,.purchased_logical_bytes=12000,.healthy_shards=52,.degraded_shards=3,.repairing_shards=1,.active_domains=9,.hosted_sites=7,.website_logical_bytes=2048,.website_requests_24h=1000,.website_cache_hits_24h=750};
 QrxResourceDashboardSnapshot d; QrxStorageAtlasCell *cells=NULL; size_t n=0;
 assert(qrx_resource_dashboard_build(p,14,&w,qrx_storage_profile_by_name("STANDARD"),5,&d,&cells,&n)==0);
 assert(d.serving_providers==14); assert(d.independent_asns==8); assert(d.visible_regions==2&&d.hidden_regions==0); assert(d.active_contracts==4); assert(d.hosted_sites==7); assert(d.website_cache_hit_bps==7500); assert(d.health_score<=100); assert(d.storage.healthy_shards==52); assert(n==2); qrx_storage_atlas_free(cells); return 0;
}
