#include "compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 QrxComputeCapabilities c={0}; strcpy(c.provider_id,"provider-test"); c.os=QRX_COMPUTE_OS_MACOS; c.arch=QRX_COMPUTE_ARCH_ARM64; c.logical_cpu_count=8; c.memory_total_bytes=16ULL<<30; c.memory_available_bytes=12ULL<<30; c.max_parallel_tasks=2; c.sandbox_available=1; c.accelerator_count=2; c.accelerators[0].type=QRX_ACCEL_GPU; strcpy(c.accelerators[0].name,"integrated-gpu"); c.accelerators[1].type=QRX_ACCEL_NPU; strcpy(c.accelerators[1].name,"neural-engine");
 assert(qrx_compute_capabilities_validate(&c)==0); assert(qrx_compute_privacy_fingerprint(&c)!=0);
 QrxComputeTaskDescriptor t={0}; strcpy(t.task_id,"task-1"); strcpy(t.owner,"qrx1owner"); strcpy(t.runtime_id,"runtime-v1"); strcpy(t.workload_hash,"sha3:work"); strcpy(t.input_commitment,"sha3:input"); t.required_arch=QRX_COMPUTE_ARCH_ARM64; t.min_memory_bytes=8ULL<<30; t.max_runtime_ms=60000; t.max_output_bytes=1<<20; t.max_fee_atoms=1000;
 assert(qrx_compute_task_validate(&t)==0); assert(qrx_compute_provider_eligible(&c,&t)==1);
 t.host_command_access=1; assert(qrx_compute_task_validate(&t)!=0); t.host_command_access=0;
 t.network_access=1; assert(qrx_compute_task_validate(&t)!=0); t.network_access=0;
 t.required_arch=QRX_COMPUTE_ARCH_X86_64; assert(qrx_compute_provider_eligible(&c,&t)==0);
 puts("compute_phase126_foundation PASS"); return 0;
}
