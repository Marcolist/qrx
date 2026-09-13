#include "compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void hex64(char x[65],char c){for(int i=0;i<64;i++)x[i]=c;x[64]=0;}
int main(void){
 QrxMoeNodeProfile n={0};n.version=QRX_MOE_PLACEMENT_VERSION;strcpy(n.node_id,"arm8-node");strcpy(n.pod_id,"eu-pod-1");n.arch=QRX_COMPUTE_ARCH_ARM64;n.memory_total_bytes=8ULL<<30;n.memory_available_bytes=4ULL<<30;n.nvme_available_bytes=100ULL<<30;n.accelerator_memory_available_bytes=0;n.latency_us_to_pod=2000;n.bandwidth_mbps_to_pod=2500;n.capability_mask=QRX_MOE_CAP_CPU|QRX_MOE_CAP_NEON;n.role_mask=QRX_MOE_ROLE_EXPERT|QRX_MOE_ROLE_CACHE|QRX_MOE_ROLE_VERIFY;strcpy(n.backend,"cpu-neon");n.node_class=QRX_MOE_NODE_MICRO;assert(qrx_moe_node_profile_validate(&n)==0);
 QrxMoeExpertPlacement p={0};p.version=QRX_MOE_PLACEMENT_VERSION;strcpy(p.node_id,n.node_id);strcpy(p.pod_id,n.pod_id);p.fragment.version=QRX_MOE_PLACEMENT_VERSION;strcpy(p.fragment.model_id,"kimi-k3");strcpy(p.fragment.model_version,"target-v1");p.fragment.layer_id=12;p.fragment.expert_id=31;p.fragment.fragment_id=0;p.fragment.fragment_count=1;hex64(p.fragment.content_root,'a');p.fragment.size_bytes=2ULL<<30;p.residency=QRX_MOE_RES_RAM;p.cache_state=QRX_MOE_CACHE_HOT;p.replica_ordinal=0;p.measured_latency_us=300;assert(qrx_moe_placement_validate(&n,&p)==0);char c1[65],c2[65];assert(qrx_moe_placement_commitment(&p,c1)==0);assert(qrx_moe_placement_commitment(&p,c2)==0&&!strcmp(c1,c2));assert(qrx_moe_placement_score(&n,&p)>0);
 p.fragment.size_bytes=5ULL<<30;assert(qrx_moe_placement_validate(&n,&p)==-2);
 assert(qrx_moe_activation_wire_bytes(1024,QRX_ACT_FP16)==2048);assert(qrx_moe_activation_wire_bytes(1024,QRX_ACT_FP8)==1024);assert(qrx_moe_activation_wire_bytes(1024,QRX_ACT_INT4)==512);
 QrxPoucOptimisticPolicy fast,std,cons;assert(qrx_pouc_optimistic_policy(QRX_POUC_OPT_FAST,9950,&fast)==0);assert(fast.redundant_workers==1&&fast.async_verification&&fast.allow_optimistic_forward);assert(qrx_pouc_optimistic_policy(QRX_POUC_OPT_STANDARD,9000,&std)==0);assert(std.challenge_bps>fast.challenge_bps);assert(qrx_pouc_optimistic_policy(QRX_POUC_OPT_CONSENSUS,10000,&cons)==0);assert(cons.redundant_workers==3&&!cons.async_verification&&!cons.allow_optimistic_forward);
 puts("compute_phase139_moe_traffic_optimistic: PASS");return 0;
}
