#include "compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void hex64(char x[65],char c){for(int i=0;i<64;i++)x[i]=c;x[64]=0;}
static QrxMoeNodeProfile node(const char*id,uint32_t lat,uint32_t bw){QrxMoeNodeProfile n={0};n.version=QRX_MOE_PLACEMENT_VERSION;strcpy(n.node_id,id);strcpy(n.pod_id,"eu-pod-1");n.arch=QRX_COMPUTE_ARCH_ARM64;n.memory_total_bytes=16ULL<<30;n.memory_available_bytes=8ULL<<30;n.nvme_available_bytes=128ULL<<30;n.accelerator_memory_available_bytes=2ULL<<30;n.latency_us_to_pod=lat;n.bandwidth_mbps_to_pod=bw;n.capability_mask=QRX_MOE_CAP_CPU|QRX_MOE_CAP_NEON;n.role_mask=QRX_MOE_ROLE_EXPERT|QRX_MOE_ROLE_CACHE;strcpy(n.backend,"cpu-neon");n.node_class=QRX_MOE_NODE_LIGHT;return n;}
static QrxMoeExpertPlacement placement(const QrxMoeNodeProfile*n,uint32_t expert,QrxMoeResidency res,char hc){QrxMoeExpertPlacement p={0};p.version=QRX_MOE_PLACEMENT_VERSION;strcpy(p.node_id,n->node_id);strcpy(p.pod_id,n->pod_id);p.fragment.version=QRX_MOE_PLACEMENT_VERSION;strcpy(p.fragment.model_id,"kimi-k3");strcpy(p.fragment.model_version,"target-v1");p.fragment.layer_id=13;p.fragment.expert_id=expert;p.fragment.fragment_id=0;p.fragment.fragment_count=1;hex64(p.fragment.content_root,hc);p.fragment.size_bytes=512ULL<<20;p.residency=res;p.cache_state=res==QRX_MOE_RES_RAM?QRX_MOE_CACHE_HOT:QRX_MOE_CACHE_WARM;p.replica_ordinal=0;p.measured_latency_us=100;return p;}
int main(void){
 QrxMoeCoactivationStat s={0};s.version=QRX_MOE_COORDINATOR_VERSION;strcpy(s.model_id,"kimi-k3");strcpy(s.model_version,"target-v1");s.layer_id=12;s.expert_a=31;s.expert_b=87;s.coactivation_count=780;s.observation_count=1000;assert(qrx_moe_coactivation_validate(&s)==0);assert(qrx_moe_coactivation_bps(&s)==7800);s.expert_b=31;assert(qrx_moe_coactivation_validate(&s)!=0);s.expert_b=87;
 QrxMoeCoordinatorRequest r={0};r.version=QRX_MOE_COORDINATOR_VERSION;strcpy(r.pod_id,"eu-pod-1");r.current_layer_id=12;r.next_layer_id=13;r.selected_experts[0]=31;r.selected_experts[1]=87;r.selected_count=2;r.max_prefetch=4;r.max_prefetch_bytes=2ULL<<30;r.activation_encoding=QRX_ACT_FP8;assert(qrx_moe_coordinator_request_validate(&r)==0);
 QrxMoeNodeProfile a=node("node-a",1000,10000),b=node("node-b",5000,2500);assert(qrx_moe_node_profile_validate(&a)==0&&qrx_moe_node_profile_validate(&b)==0);
 QrxMoeExpertPlacement pa=placement(&a,41,QRX_MOE_RES_RAM,'a'),pb=placement(&b,83,QRX_MOE_RES_REMOTE,'b');assert(qrx_moe_placement_validate(&a,&pa)==0&&qrx_moe_placement_validate(&b,&pb)==0);
 assert(qrx_moe_locality_score(&a,&pa,9000)>qrx_moe_locality_score(&b,&pb,9000));
 QrxMoePrefetchPlan p1={0},p2={0};assert(qrx_moe_prefetch_plan_add(&r,&b,&pb,7000,&p1)==0);assert(qrx_moe_prefetch_plan_add(&r,&a,&pa,9000,&p1)==0);assert(p1.estimated_wire_bytes==pb.fragment.size_bytes);assert(qrx_moe_prefetch_plan_finalize(&r,&p1)==0);assert(!strcmp(p1.items[0].placement.node_id,"node-a"));
 /* Same set, reverse insertion order -> canonical sort and same commitment. */
 assert(qrx_moe_prefetch_plan_add(&r,&a,&pa,9000,&p2)==0);assert(qrx_moe_prefetch_plan_add(&r,&b,&pb,7000,&p2)==0);assert(qrx_moe_prefetch_plan_finalize(&r,&p2)==0);char c1[65],c2[65];assert(qrx_moe_prefetch_plan_commitment(&p1,c1)==0);assert(qrx_moe_prefetch_plan_commitment(&p2,c2)==0);assert(!strcmp(c1,c2));
 /* Duplicate placement is rejected and budget limit is hard. */
 assert(qrx_moe_prefetch_plan_add(&r,&a,&pa,9000,&p2)==-4);QrxMoeExpertPlacement big=placement(&a,99,QRX_MOE_RES_RAM,'c');big.fragment.size_bytes=3ULL<<30;assert(qrx_moe_prefetch_plan_add(&r,&a,&big,5000,&p2)!=0);
 puts("compute_phase140_moe_coordinator_prefetch: PASS");return 0;
}
