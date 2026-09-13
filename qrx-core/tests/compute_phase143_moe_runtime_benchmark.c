#include "compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static QrxMoeRuntimeDevice p40(void){
    QrxMoeRuntimeDevice d; memset(&d,0,sizeof(d));
    d.version=QRX_MOE_RUNTIME_VERSION; d.backend=QRX_MOE_BACKEND_CUDA;
    snprintf(d.device_name,sizeof(d.device_name),"NVIDIA Tesla P40");
    d.accelerator_features=QRX_MOE_ACCEL_FEAT_CUDA|QRX_MOE_ACCEL_FEAT_FP16|QRX_MOE_ACCEL_FEAT_FP32|QRX_MOE_ACCEL_FEAT_INT8;
    d.device_memory_bytes=24ULL*1024ULL*1024ULL*1024ULL; d.cuda_cc_major=6; d.cuda_cc_minor=1; d.max_batch_items=16; return d;
}
static QrxMoeTransportAdapter tcp(uint32_t latency,uint32_t mbps){
    QrxMoeTransportAdapter t; memset(&t,0,sizeof(t)); t.version=QRX_MOE_RUNTIME_VERSION;t.kind=QRX_MOE_TRANSPORT_TCP;snprintf(t.name,sizeof(t.name),"tcp");t.feature_mask=QRX_MOE_RUNTIME_FEAT_ASYNC|QRX_MOE_RUNTIME_FEAT_MULTIPLEX;t.latency_us=latency;t.bandwidth_mbps=mbps;t.max_inflight=32;t.mtu_bytes=1500;t.per_message_overhead_bytes=64;return t;
}
int main(void){
    QrxMoeRuntimeDevice d=p40(); assert(qrx_moe_runtime_device_validate(&d)==0); assert(qrx_moe_runtime_is_nvidia_p40(&d)==1); assert(qrx_moe_runtime_supports_tensor_core_kernel(&d)==0);
    d.accelerator_features|=QRX_MOE_ACCEL_FEAT_TENSOR_CORES; assert(qrx_moe_runtime_supports_tensor_core_kernel(&d)==1); d.accelerator_features&=~QRX_MOE_ACCEL_FEAT_TENSOR_CORES;
    QrxMoeTransportAdapter t=tcp(500,10000); assert(qrx_moe_transport_validate(&t)==0); t.feature_mask|=QRX_MOE_RUNTIME_FEAT_RDMA; assert(qrx_moe_transport_validate(&t)!=0); t.feature_mask&=~QRX_MOE_RUNTIME_FEAT_RDMA;

    QrxMoeBenchmarkScenario s; memset(&s,0,sizeof(s));s.version=QRX_MOE_BENCH_VERSION;snprintf(s.scenario_id,sizeof(s.scenario_id),"mixed-p40-cpu-v1");s.seed=42;s.duration_us=10000000ULL;s.concurrency=16;s.requested_batch_items=8;s.activation_bytes_per_token=262144;s.result_bytes_per_token=131072;s.synthetic_fault_bps=100;s.node_count=2;
    for(int i=0;i<2;i++){s.nodes[i].version=QRX_MOE_BENCH_VERSION;snprintf(s.nodes[i].node_id,sizeof(s.nodes[i].node_id),"node-%d",i);s.nodes[i].device=p40();s.nodes[i].transport=tcp(500+(uint32_t)i*200,10000);s.nodes[i].standalone_milli_tokens_per_second=8000;s.nodes[i].availability_bps=9900;}
    QrxMoeBenchmarkResult a,b; assert(qrx_moe_benchmark_scenario_validate(&s)==0);assert(qrx_moe_benchmark_run(&s,&a)==0);assert(qrx_moe_benchmark_run(&s,&b)==0);assert(memcmp(&a,&b,sizeof(a))==0);assert(a.raw_milli_tokens_per_second==15840);assert(a.estimated_milli_tokens_per_second>0);assert(a.estimated_milli_tokens_per_second<a.raw_milli_tokens_per_second);assert(a.estimated_tokens_completed>0);assert(a.estimated_wire_bytes>0);assert(a.effective_nodes_milli==1980);
    char ca[65],cb[65];assert(qrx_moe_benchmark_commitment(&s,&a,ca)==0);assert(qrx_moe_benchmark_commitment(&s,&b,cb)==0);assert(strcmp(ca,cb)==0);s.seed=43;assert(qrx_moe_benchmark_commitment(&s,&a,cb)==0);assert(strcmp(ca,cb)!=0);
    puts("compute_phase143_moe_runtime_benchmark: PASS");return 0;
}
