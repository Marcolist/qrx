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
static QrxMoeTransportAdapter tcp(void){
    QrxMoeTransportAdapter t;memset(&t,0,sizeof(t));t.version=QRX_MOE_RUNTIME_VERSION;t.kind=QRX_MOE_TRANSPORT_TCP;snprintf(t.name,sizeof(t.name),"tcp");t.feature_mask=QRX_MOE_RUNTIME_FEAT_ASYNC|QRX_MOE_RUNTIME_FEAT_MULTIPLEX;t.latency_us=250;t.bandwidth_mbps=10000;t.max_inflight=32;t.mtu_bytes=1500;t.per_message_overhead_bytes=64;return t;
}
static int echo_exec(void *ctx,const void *in,size_t in_n,void *out,size_t out_cap,size_t *out_n){(void)ctx;if(out_cap<in_n)return -9;memcpy(out,in,in_n);*out_n=in_n;return 0;}
int main(void){
    QrxMoeRuntimeInventory inv;assert(qrx_moe_runtime_discover_native(&inv)==0);assert(qrx_moe_runtime_inventory_validate(&inv)==0);assert(inv.device_count>=1);assert(inv.devices[0].source==QRX_MOE_DISCOVERY_CPU_NATIVE);assert(inv.devices[0].runtime_present==1);assert(inv.devices[0].execution_adapter_ready==1);

    QrxMoeRuntimeDevice d=p40();assert(qrx_moe_runtime_is_nvidia_p40(&d)==1);uint32_t km=qrx_moe_runtime_kernel_mask(&d);assert(km&QRX_MOE_KERNEL_FP32);assert(km&QRX_MOE_KERNEL_FP16);assert(km&QRX_MOE_KERNEL_INT8);assert(!(km&QRX_MOE_KERNEL_TENSORCORE));
    QrxMoeWorkerAdapter wa;memset(&wa,0,sizeof(wa));wa.version=QRX_MOE_DISCOVERY_VERSION;snprintf(wa.name,sizeof(wa.name),"cuda-test-adapter");wa.backend=QRX_MOE_BACKEND_CUDA;wa.kernel_mask=km;wa.ready=1;wa.execute=echo_exec;assert(qrx_moe_worker_adapter_validate(&wa)==0);
    QrxMoeWorkerRequirement r;memset(&r,0,sizeof(r));r.version=QRX_MOE_DISCOVERY_VERSION;r.backend=QRX_MOE_BACKEND_CUDA;r.required_kernel_features=QRX_MOE_KERNEL_FP16|QRX_MOE_KERNEL_INT8;r.required_device_memory_bytes=8ULL<<30;r.requested_batch_items=8;assert(qrx_moe_worker_eligible(&d,&r)==1);char ib[4]={'q','r','x','!'},ob[4]={0};size_t on=0;assert(qrx_moe_worker_adapter_execute(&wa,&d,&r,ib,sizeof(ib),ob,sizeof(ob),&on)==0);assert(on==4&&memcmp(ib,ob,4)==0);r.required_kernel_features|=QRX_MOE_KERNEL_TENSORCORE;assert(qrx_moe_worker_eligible(&d,&r)==0);assert(qrx_moe_worker_adapter_execute(&wa,&d,&r,ib,sizeof(ib),ob,sizeof(ob),&on)!=0);r.required_kernel_features=QRX_MOE_KERNEL_FP16;r.required_device_memory_bytes=25ULL<<30;assert(qrx_moe_worker_eligible(&d,&r)==0);

    QrxMoeCalibrationProfile p;memset(&p,0,sizeof(p));p.version=QRX_MOE_CALIBRATION_VERSION;snprintf(p.profile_id,sizeof(p.profile_id),"p40-k3-cal-v1");p.device=d;p.measured_at_unix_ms=123456789;p.memory_bandwidth_bytes_per_second=300000000000ULL;p.compute_ops_per_second=12000000000000ULL;assert(qrx_moe_calibration_record_model_sample(&p,8,1103448,8)==0);assert(p.model_milli_tokens_per_second>7200&&p.model_milli_tokens_per_second<7300);assert(qrx_moe_calibration_record_transport_probe(&p,1250000,2000)==0);assert(p.transport_latency_us==1000);assert(p.transport_bandwidth_mbps==10000);assert(qrx_moe_calibration_profile_validate(&p)==0);p.model_milli_tokens_per_second=7250;p.sample_count=5;p.transport_latency_us=300;p.transport_bandwidth_mbps=10000;
    char a[65],b[65];assert(qrx_moe_calibration_commitment(&p,a)==0);assert(qrx_moe_calibration_commitment(&p,b)==0);assert(strcmp(a,b)==0);p.model_milli_tokens_per_second++;assert(qrx_moe_calibration_commitment(&p,b)==0);assert(strcmp(a,b)!=0);p.model_milli_tokens_per_second--;

    QrxMoeBenchmarkNode n;memset(&n,0,sizeof(n));n.version=QRX_MOE_BENCH_VERSION;snprintf(n.node_id,sizeof(n.node_id),"p40-node");n.device=d;n.transport=tcp();n.standalone_milli_tokens_per_second=1;n.availability_bps=9900;assert(qrx_moe_calibration_apply_to_benchmark(&p,&n)==0);assert(n.standalone_milli_tokens_per_second==7250);assert(n.transport.latency_us==300);assert(n.transport.bandwidth_mbps==10000);
    puts("compute_phase144_native_runtime_calibration: PASS");return 0;
}
