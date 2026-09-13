#include "compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static QrxComputeBenchmarkMetrics reference_profile(void) {
    QrxComputeBenchmarkMetrics m; memset(&m,0,sizeof(m));
    m.version=QRX_COMPUTE_BENCHMARK_VERSION;
    m.source_mask=QRX_COMPUTE_BENCH_SRC_REQUIRED;
    m.sample_count=5; m.sample_window_ms=5000;
    m.bf16_mops=QRX_COMPUTE_REF_BF16_MOPS;
    m.mxfp4_mops=QRX_COMPUTE_REF_MXFP4_MOPS;
    m.memory_mib_s=QRX_COMPUTE_REF_MEMORY_MIB_S;
    m.storage_mib_s=QRX_COMPUTE_REF_STORAGE_MIB_S;
    m.network_mbit_s=QRX_COMPUTE_REF_NETWORK_MBIT_S;
    m.moe_milli_tokens_s=QRX_COMPUTE_REF_MOE_MILLI_TOKENS_S;
    m.inference_milli_tokens_s=QRX_COMPUTE_REF_INFERENCE_MILLI_TOKENS_S;
    return m;
}

int main(void) {
    QrxComputeBenchmarkMetrics m=reference_profile();
    QrxComputeNormalizedScore s;
    assert(qrx_compute_benchmark_validate(&m)==0);
    assert(qrx_compute_benchmark_normalize(&m,&s)==0);
    assert(s.normalized_score==QRX_COMPUTE_NCU_BASE);
    assert(s.cpu_score==QRX_COMPUTE_NCU_BASE);
    assert(s.memory_score==QRX_COMPUTE_NCU_BASE);
    assert(s.storage_score==QRX_COMPUTE_NCU_BASE);
    assert(s.network_score==QRX_COMPUTE_NCU_BASE);
    assert(s.moe_score==QRX_COMPUTE_NCU_BASE);
    assert(s.inference_score==QRX_COMPUTE_NCU_BASE);
    assert(s.class_id==QRX_COMPUTE_SCORE_CLASS_STANDARD);

    m.bf16_mops*=2; m.mxfp4_mops*=2; m.memory_mib_s*=2; m.storage_mib_s*=2; m.network_mbit_s*=2; m.moe_milli_tokens_s*=2; m.inference_milli_tokens_s*=2;
    assert(qrx_compute_benchmark_normalize(&m,&s)==0);
    assert(s.normalized_score==QRX_COMPUTE_COMPONENT_SCORE_CAP);
    assert(s.class_id==QRX_COMPUTE_SCORE_CLASS_EXTREME);

    m=reference_profile(); m.source_mask &= ~QRX_COMPUTE_BENCH_SRC_NETWORK;
    assert(qrx_compute_benchmark_validate(&m)!=0);
    m=reference_profile(); m.network_mbit_s=0;
    assert(qrx_compute_benchmark_validate(&m)!=0);
    m=reference_profile(); m.version++;
    assert(qrx_compute_benchmark_validate(&m)!=0);

    printf("compute_phase128_benchmark_normalization PASS reference=%u class=%s\
",
           QRX_COMPUTE_NCU_BASE,qrx_compute_score_class_name(QRX_COMPUTE_SCORE_CLASS_STANDARD));
    return 0;
}
