#include "compute/qrx_aura_model_fabric.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define GIB (1024ULL*1024ULL*1024ULL)

static void hexfill(char out[65],char c){for(int i=0;i<64;i++)out[i]=c;out[64]=0;}

static QrxAuraPodCapacity pod(const char*provider,const char*id,const char*region,uint64_t free_gib,uint64_t mtps,uint32_t net,uint32_t lat,uint32_t rel,uint32_t hit,uint32_t locality,QrxMoeRuntimeBackend backend,uint32_t accel,uint32_t roles){
    QrxAuraPodCapacity p;memset(&p,0,sizeof(p));p.version=QRX_AURA_FABRIC_VERSION;snprintf(p.provider_id,sizeof(p.provider_id),"%s",provider);snprintf(p.pod_id,sizeof(p.pod_id),"%s",id);snprintf(p.region,sizeof(p.region),"%s",region);p.backend=backend;p.accelerator_features=accel;p.role_mask=roles;p.usable_memory_bytes=(free_gib+2)*GIB;p.free_memory_bytes=free_gib*GIB;p.model_cache_free_bytes=(free_gib/2)*GIB;p.measured_milli_tokens_per_second=mtps;p.network_egress_mbps=net;p.latency_ms=lat;p.utilization_bps=2500;p.reliability_bps=rel;p.cache_hit_bps=hit;p.expert_locality_bps=locality;p.max_context_tokens=32768;p.node_count=1;p.available=1;hexfill(p.calibration_commitment,'a');assert(qrx_aura_pod_capacity_validate(&p)==0);return p;
}

static QrxAuraModelCapabilityProfile model(const char*id,const char*family,QrxAuraModelTier tier,uint64_t min_gib,uint64_t rec_gib,uint64_t min_tps,uint32_t min_pods,uint32_t net,uint32_t lat,uint32_t rel,uint32_t hit,uint32_t locality,uint32_t q,uint32_t coding,uint32_t reasoning,uint32_t rag,uint32_t tool,int moe,int single){
    QrxAuraModelCapabilityProfile m;memset(&m,0,sizeof(m));m.version=QRX_AURA_FABRIC_VERSION;snprintf(m.model_id,sizeof(m.model_id),"%s",id);snprintf(m.model_version,sizeof(m.model_version),"1");snprintf(m.family,sizeof(m.family),"%s",family);snprintf(m.runtime_id,sizeof(m.runtime_id),"qrx-llm-runtime-v1");snprintf(m.quantization,sizeof(m.quantization),"Q4");m.tier=tier;m.min_memory_bytes=min_gib*GIB;m.recommended_memory_bytes=rec_gib*GIB;m.min_storage_bytes=2*GIB;m.min_aggregate_milli_tokens_per_second=min_tps;m.min_pods=min_pods;m.min_network_mbps=net;m.max_latency_ms=lat;m.min_reliability_bps=rel;m.min_cache_hit_bps=hit;m.min_expert_locality_bps=locality;m.max_context_tokens=8192;m.quality_bps=q;m.coding_bps=coding;m.reasoning_bps=reasoning;m.rag_bps=rag;m.tool_use_bps=tool;m.is_moe=(uint8_t)moe;m.supports_single_device=(uint8_t)single;assert(qrx_aura_model_profile_validate(&m)==0);return m;
}

int main(void){
    QrxAuraPodCapacityRegistry reg;assert(qrx_aura_pod_registry_init(&reg)==0);

    /* Two deliberately modest ARM64-like CPU profiles prove that model fabric
       membership is based on measured capability, not a product allowlist. */
    QrxAuraPodCapacity p[6];
    p[0]=pod("prov-pi","pod-pi5","EU-WEST",7,3500,1000,18,9900,7000,6500,QRX_MOE_BACKEND_CPU,QRX_MOE_ACCEL_FEAT_FP32,QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_ROUTER|QRX_AURA_POD_ROLE_PREPOST|QRX_AURA_POD_ROLE_VERIFY);
    p[1]=pod("prov-odroid","pod-n2p","EU-CENTRAL",3,500,300,35,9500,3000,2000,QRX_MOE_BACKEND_CPU,QRX_MOE_ACCEL_FEAT_FP32,QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_ROUTER|QRX_AURA_POD_ROLE_PREPOST);
    p[2]=pod("prov-orin","pod-orin","EU-WEST",12,10000,1000,12,9950,8500,8200,QRX_MOE_BACKEND_CUDA,QRX_MOE_ACCEL_FEAT_FP16|QRX_MOE_ACCEL_FEAT_FP32|QRX_MOE_ACCEL_FEAT_INT8|QRX_MOE_ACCEL_FEAT_CUDA,QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_MODEL_CACHE);
    p[3]=pod("prov-mac","pod-mac","EU-WEST",24,18000,1500,9,9980,9000,9000,QRX_MOE_BACKEND_MLX_METAL,QRX_MOE_ACCEL_FEAT_FP16|QRX_MOE_ACCEL_FEAT_FP32|QRX_MOE_ACCEL_FEAT_UNIFIED_MEM|QRX_MOE_ACCEL_FEAT_METAL|QRX_MOE_ACCEL_FEAT_MLX,QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_MODEL_CACHE);
    p[4]=pod("prov-ryzen","pod-ryzen","EU-CENTRAL",24,14000,1000,16,9920,8000,7800,QRX_MOE_BACKEND_OTHER,QRX_MOE_ACCEL_FEAT_FP16|QRX_MOE_ACCEL_FEAT_FP32|QRX_MOE_ACCEL_FEAT_INT8,QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_MODEL_CACHE);
    p[5]=pod("prov-p40","pod-p40","EU-WEST",20,25000,1000,14,9900,8500,8500,QRX_MOE_BACKEND_CUDA,QRX_MOE_ACCEL_FEAT_FP16|QRX_MOE_ACCEL_FEAT_FP32|QRX_MOE_ACCEL_FEAT_INT8|QRX_MOE_ACCEL_FEAT_CUDA,QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_MODEL_CACHE);

    assert(qrx_aura_pod_tier_classify(&p[0])==QRX_AURA_TIER_EDGE);
    assert(qrx_aura_pod_tier_classify(&p[1])==QRX_AURA_TIER_NANO);
    assert(qrx_aura_pod_tier_classify(&p[3])==QRX_AURA_TIER_LOCAL);
    for(size_t i=0;i<6;i++)assert(qrx_aura_pod_registry_upsert(&reg,&p[i])==0);
    assert(reg.count==6&&reg.revision==6);
    assert(qrx_aura_pod_registry_find(&reg,"pod-pi5")!=NULL);

    QrxAuraModelCapabilityProfile m[5];
    m[0]=model("qrx/aura-nano-qwen-class","Qwen-class",QRX_AURA_TIER_NANO,2,3,300,1,0,500,8000,0,0,4500,4000,3500,4500,4000,0,1);
    m[1]=model("qrx/aura-edge-qwen-class","Qwen-class",QRX_AURA_TIER_EDGE,6,8,2000,1,0,300,8500,0,0,6500,7000,6000,7000,6500,0,1);
    m[2]=model("qrx/aura-local","Qwen/Gemma/Llama-class",QRX_AURA_TIER_LOCAL,16,20,8000,1,0,250,9000,0,0,8200,8400,8500,8400,8200,0,1);
    m[3]=model("qrx/aura-k2-class","Kimi-K2-class",QRX_AURA_TIER_K2_CLASS,48,64,30000,3,100,120,9000,5000,6500,9200,9300,9300,9100,9000,1,0);
    m[4]=model("qrx/aura-k3-class","Kimi-K3-class",QRX_AURA_TIER_K3_CLASS,96,128,60000,6,250,80,9500,7000,8000,9800,9800,9850,9600,9500,1,0);

    uint32_t ep=0;uint64_t tps=0;
    assert(qrx_aura_model_readiness_bps(&m[3],p,6,&ep,&tps)==10000);
    assert(ep==6&&tps==71000);
    uint32_t k3=qrx_aura_model_readiness_bps(&m[4],p,6,NULL,NULL);assert(k3>8000&&k3<10000);

    QrxAuraFabricSnapshot s;QrxAuraGlobeCell*cells=NULL;size_t cn=0;
    assert(qrx_aura_fabric_build(p,6,m,5,3,&s,&cells,&cn)==0);
    assert(s.total_pods==6&&s.provider_count==6&&s.inference_pods==6);
    assert(s.tier_pods[QRX_AURA_TIER_NANO]==1);
    assert(s.tier_pods[QRX_AURA_TIER_EDGE]>=2);
    assert(s.tier_pods[QRX_AURA_TIER_LOCAL]>=2);
    assert(s.k2_readiness_bps==10000&&s.k3_readiness_bps==k3);
    assert(s.max_ready_tier==QRX_AURA_TIER_K2_CLASS);
    assert(cn==2&&s.visible_regions==1&&s.hidden_regions==1);
    int west=0,central=0;for(size_t i=0;i<cn;i++){if(!strcmp(cells[i].region,"EU-WEST")){west=1;assert(cells[i].provider_count==4&&cells[i].pod_count==4&&cells[i].publicly_visible);}if(!strcmp(cells[i].region,"EU-CENTRAL")){central=1;assert(cells[i].provider_count==2&&cells[i].pod_count==2&&!cells[i].publicly_visible);}}assert(west&&central);qrx_aura_globe_free(cells);

    QrxAuraRouteRequest r;memset(&r,0,sizeof(r));r.version=QRX_AURA_FABRIC_VERSION;r.mode=QRX_AURA_ROUTE_AUTO;r.task_class=QRX_AURA_TASK_CHAT;r.complexity_bps=1000;r.allow_degradation=1;
    QrxAuraRouteDecision d;
    /* A single calibrated Pi-5-class or ODROID-N2+-class CPU node can already
       make AUTO useful with a small enough registered model. These are
       synthetic capability profiles, not product-name performance promises. */
    assert(qrx_aura_route_model(&r,m,5,&p[0],1,&d)==0);assert(d.tier==QRX_AURA_TIER_NANO&&d.selected_pods==1);
    assert(qrx_aura_route_model(&r,m,5,&p[1],1,&d)==0);assert(d.tier==QRX_AURA_TIER_NANO&&d.selected_pods==1);
    assert(qrx_aura_route_model(&r,m,5,p,6,&d)==0);assert(d.tier==QRX_AURA_TIER_NANO&&!d.degraded);assert(d.selected_pods==1);assert(!strcmp(d.model_id,m[0].model_id));

    r.task_class=QRX_AURA_TASK_CODING;r.complexity_bps=6000;assert(qrx_aura_route_model(&r,m,5,p,6,&d)==0);assert(d.tier==QRX_AURA_TIER_EDGE&&!strcmp(d.model_id,m[1].model_id));

    r.task_class=QRX_AURA_TASK_REASONING;r.complexity_bps=9000;assert(qrx_aura_route_model(&r,m,5,p,6,&d)==0);assert(d.tier==QRX_AURA_TIER_K2_CLASS&&!d.degraded);char committed[65];assert(qrx_aura_route_decision_commitment(&d,committed)==0);assert(!strcmp(committed,d.decision_commitment));assert(!strcmp(committed,"6d2e7817575a1d3dbd488756cca15619cd1abe4ae4a942588606f41e24f19664"));

    QrxComputeJobNode j;memset(&j,0,sizeof(j));j.node_id=1;j.job_type=QRX_JOB_AI_INFERENCE;snprintf(j.input_ref,sizeof(j.input_ref),"drive:prompt");snprintf(j.output_ref,sizeof(j.output_ref),"drive:answer");j.max_runtime_ms=60000;j.max_output_bytes=4096;j.max_fee_atoms=1000000;j.capability_mask=QRX_JOB_CAP_READ_INPUTS|QRX_JOB_CAP_STREAM_OUTPUT;assert(qrx_aura_route_apply_to_job_node(&d,m,5,&j)==0);assert(!strcmp(j.model_id,m[3].model_id)&&!strcmp(j.runtime_id,m[3].runtime_id));assert(j.min_memory_bytes==m[3].min_memory_bytes);

    /* K3 PINNED refuses to silently downgrade when the fabric is not ready. */
    memset(&r,0,sizeof(r));r.version=QRX_AURA_FABRIC_VERSION;r.mode=QRX_AURA_ROUTE_PINNED;r.task_class=QRX_AURA_TASK_REASONING;r.complexity_bps=9500;snprintf(r.pinned_model_id,sizeof(r.pinned_model_id),"%s",m[4].model_id);snprintf(r.pinned_model_version,sizeof(r.pinned_model_version),"1");assert(qrx_aura_route_model(&r,m,5,p,6,&d)==-6);

    /* Simulate two substantial pods disappearing. AUTO remains useful and
       explicitly degrades to the best still feasible local profile. */
    QrxAuraPodCapacity reduced[4]={p[0],p[1],p[2],p[4]};memset(&r,0,sizeof(r));r.version=QRX_AURA_FABRIC_VERSION;r.mode=QRX_AURA_ROUTE_AUTO;r.task_class=QRX_AURA_TASK_REASONING;r.complexity_bps=9000;r.allow_degradation=1;assert(qrx_aura_route_model(&r,m,5,reduced,4,&d)==0);assert(d.tier==QRX_AURA_TIER_LOCAL&&d.degraded);assert(!strcmp(d.model_id,m[2].model_id));

    /* Utility-only low-memory nodes remain registerable and useful even when
       they cannot advertise LLM token throughput. */
    QrxAuraPodCapacity util=p[1];snprintf(util.pod_id,sizeof(util.pod_id),"pod-n2p-utility");util.role_mask=QRX_AURA_POD_ROLE_ROUTER|QRX_AURA_POD_ROLE_PREPOST|QRX_AURA_POD_ROLE_VERIFY;util.measured_milli_tokens_per_second=0;util.max_context_tokens=0;util.calibration_commitment[0]=0;assert(qrx_aura_pod_capacity_validate(&util)==0);assert(qrx_aura_pod_tier_classify(&util)==QRX_AURA_TIER_NANO);

    /* Native calibration bridge: generic CPU/ARM nodes do not need a product
       name in the protocol. */
    QrxMoeCalibrationProfile cal;memset(&cal,0,sizeof(cal));cal.version=QRX_MOE_CALIBRATION_VERSION;snprintf(cal.profile_id,sizeof(cal.profile_id),"arm64-edge-cal");cal.device.version=QRX_MOE_RUNTIME_VERSION;cal.device.backend=QRX_MOE_BACKEND_CPU;snprintf(cal.device.device_name,sizeof(cal.device.device_name),"Native ARM64 CPU");cal.device.accelerator_features=QRX_MOE_ACCEL_FEAT_FP32;cal.device.device_memory_bytes=8*GIB;cal.device.max_batch_items=4;cal.model_milli_tokens_per_second=2500;cal.model_batch_items=1;cal.sample_count=3;QrxAuraPodCapacity from_cal;assert(qrx_aura_pod_from_calibration("prov-arm","pod-arm","EU-NORTH",&cal,QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_ROUTER,6*GIB,1*GIB,500,20,2000,9800,5000,4000,8192,1,&from_cal)==0);assert(qrx_aura_pod_tier_classify(&from_cal)==QRX_AURA_TIER_EDGE);

    return 0;
}
