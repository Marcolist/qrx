#include "resource/qrx_resource_globe.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void){
    QrxResourceGlobeObservation o[5];memset(o,0,sizeof(o));
    for(int i=0;i<5;i++){
        o[i].version=QRX_RESOURCE_GLOBE_VERSION;snprintf(o[i].provider_id,sizeof(o[i].provider_id),"provider-%d",i);strcpy(o[i].region,i<4?"EU-WEST-COARSE":"SPARSE-HIDDEN");
        o[i].layer_mask=QRX_GLOBE_LAYER_STORAGE|QRX_GLOBE_LAYER_COMPUTE|QRX_GLOBE_LAYER_AI|QRX_GLOBE_LAYER_MODEL_CACHE|QRX_GLOBE_LAYER_NETWORK;
        o[i].storage_free_bytes=1000+i;o[i].compute_ncu_milli=10000;o[i].ai_milli_tokens_per_second=2500;o[i].model_cache_free_bytes=2000;o[i].network_egress_mbps=1000;
        o[i].latency_ms=20;o[i].utilization_bps=8500;o[i].reliability_bps=9900;o[i].demand_bps=9000;
        assert(qrx_resource_globe_observation_validate(&o[i])==0);
    }
    QrxResourceGlobeCell*c=NULL;size_t n=0;assert(qrx_resource_globe_build(o,5,3,&c,&n)==0);assert(n==2);
    int public_seen=0,hidden_seen=0;for(size_t i=0;i<n;i++){
        if(!strcmp(c[i].region,"EU-WEST-COARSE")){
            assert(c[i].provider_count==4);assert(c[i].publicly_visible==1);assert(c[i].compute_ncu_milli==40000);assert(c[i].ai_milli_tokens_per_second==10000);assert(c[i].opportunity_score>0);assert(c[i].demand_class==QRX_GLOBE_DEMAND_CRITICAL);assert(c[i].latency_class==QRX_GLOBE_LATENCY_LOW);char a[65],b[65];assert(qrx_resource_globe_cell_commitment(&c[i],a)==0);assert(qrx_resource_globe_cell_commitment(&c[i],b)==0);assert(!strcmp(a,b));public_seen=1;
        }else{assert(!strcmp(c[i].region,"SPARSE-HIDDEN"));assert(c[i].provider_count==1);assert(c[i].publicly_visible==0);hidden_seen=1;}
    }
    assert(public_seen&&hidden_seen);
    /* Duplicate provider observations in a region must not inflate privacy provider count. */
    QrxResourceGlobeObservation d[3];memset(d,0,sizeof(d));for(int i=0;i<3;i++){d[i]=o[0];strcpy(d[i].region,"DUP");strcpy(d[i].provider_id,i<2?"same":"other");}
    QrxResourceGlobeCell*dc=NULL;size_t dn=0;assert(qrx_resource_globe_build(d,3,3,&dc,&dn)==0&&dn==1);assert(dc[0].provider_count==2&&!dc[0].publicly_visible);qrx_resource_globe_free(dc);
    qrx_resource_globe_free(c);return 0;
}
