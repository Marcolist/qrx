#include "compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    QrxComputeCapabilities c;
    QrxComputeDispatchProfile p;
    int rc=qrx_compute_detect_local_capabilities("phase127-local",&c);
    assert(rc==0);
    assert(qrx_compute_capabilities_validate(&c)==0);
    assert(strcmp(c.provider_id,"phase127-local")==0);
    assert(c.logical_cpu_count>=1);
    assert(c.memory_total_bytes>=c.memory_available_bytes);
    assert(c.max_parallel_tasks>=1);
    assert(c.sandbox_available==1);
    assert(qrx_compute_select_dispatch_profile(&c,&p)==0);
    assert(p.recommended_parallel_tasks>=1);
    assert(p.scheduling_memory_bytes<=c.memory_available_bytes);
    assert(qrx_compute_privacy_fingerprint(&c)!=0);
    assert(qrx_compute_kernel_name(p.kernel_class)!=NULL);
#if defined(__x86_64__) || defined(_M_X64)
    assert(c.arch==QRX_COMPUTE_ARCH_X86_64);
    assert((c.cpu_feature_flags & QRX_CPU_FEATURE_SSE2)!=0);
#elif defined(__aarch64__) || defined(_M_ARM64)
    assert(c.arch==QRX_COMPUTE_ARCH_ARM64);
    assert((c.cpu_feature_flags & QRX_CPU_FEATURE_NEON)!=0);
#endif
    printf("compute_phase127_capability_discovery PASS os=%s arch=%s cpus=%u kernel=%s\n",
           qrx_compute_os_name(c.os),qrx_compute_arch_name(c.arch),c.logical_cpu_count,
           qrx_compute_kernel_name(p.kernel_class));
    return 0;
}
