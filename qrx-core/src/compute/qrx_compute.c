#include "compute/qrx_compute.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <ctype.h>
#include <time.h>

#if !defined(_WIN32)
#include <dlfcn.h>
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <unistd.h>
#else
#include <unistd.h>
#if defined(__linux__)
#include <sys/sysinfo.h>
#endif
#endif

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
#include <cpuid.h>
#endif

static int nonempty(const char *s, size_t n) { return s && memchr(s, '\0', n) && s[0]; }

int qrx_compute_capabilities_validate(const QrxComputeCapabilities *c) {
    if (!c || !nonempty(c->provider_id,sizeof(c->provider_id))) return -1;
    if (c->os < QRX_COMPUTE_OS_LINUX || c->os > QRX_COMPUTE_OS_MACOS) return -1;
    if (c->arch != QRX_COMPUTE_ARCH_X86_64 && c->arch != QRX_COMPUTE_ARCH_ARM64) return -1;
    if (!c->logical_cpu_count || !c->memory_total_bytes || c->memory_available_bytes > c->memory_total_bytes) return -1;
    if (c->accelerator_count > QRX_COMPUTE_MAX_ACCELERATORS || !c->max_parallel_tasks) return -1;
    for (uint32_t i=0;i<c->accelerator_count;i++) if (!nonempty(c->accelerators[i].name,sizeof(c->accelerators[i].name))) return -1;
    return 0;
}

int qrx_compute_task_validate(const QrxComputeTaskDescriptor *t) {
    if (!t || !nonempty(t->task_id,sizeof(t->task_id)) || !nonempty(t->owner,sizeof(t->owner))) return -1;
    if (!nonempty(t->runtime_id,sizeof(t->runtime_id)) || !nonempty(t->workload_hash,sizeof(t->workload_hash)) || !nonempty(t->input_commitment,sizeof(t->input_commitment))) return -1;
    if (t->required_arch != QRX_COMPUTE_ARCH_UNKNOWN && t->required_arch != QRX_COMPUTE_ARCH_X86_64 && t->required_arch != QRX_COMPUTE_ARCH_ARM64) return -1;
    if (!t->max_runtime_ms || !t->max_output_bytes || !t->max_fee_atoms) return -1;
    /* Foundation invariant: arbitrary host access is never a valid Compute V1 task. */
    if (t->host_command_access || t->filesystem_write || t->network_access) return -1;
    return 0;
}

int qrx_compute_provider_eligible(const QrxComputeCapabilities *c, const QrxComputeTaskDescriptor *t) {
    if (qrx_compute_capabilities_validate(c) || qrx_compute_task_validate(t)) return 0;
    if (!c->sandbox_available) return 0;
    if (t->required_arch != QRX_COMPUTE_ARCH_UNKNOWN && t->required_arch != c->arch) return 0;
    if (c->memory_available_bytes < t->min_memory_bytes) return 0;
    return 1;
}

uint64_t qrx_compute_privacy_fingerprint(const QrxComputeCapabilities *c) {
    /* Coarse, stable scheduling class only: deliberately excludes serials, hostnames, IPs and exact free memory. */
    if (qrx_compute_capabilities_validate(c)) return 0;
    uint64_t gib = c->memory_total_bytes >> 30;
    uint64_t bucket = gib < 8 ? 1 : gib < 16 ? 2 : gib < 32 ? 3 : gib < 64 ? 4 : gib < 128 ? 5 : 6;
    uint64_t cpu_bucket = c->logical_cpu_count < 4 ? 1 : c->logical_cpu_count < 8 ? 2 : c->logical_cpu_count < 16 ? 3 : 4;
    return ((uint64_t)c->os<<56) | ((uint64_t)c->arch<<48) | (bucket<<40) | (cpu_bucket<<32) | ((uint64_t)c->accelerator_count<<24);
}

const char *qrx_compute_os_name(QrxComputeOs os){switch(os){case QRX_COMPUTE_OS_LINUX:return "LINUX";case QRX_COMPUTE_OS_WINDOWS:return "WINDOWS";case QRX_COMPUTE_OS_MACOS:return "MACOS";default:return "UNKNOWN";}}
const char *qrx_compute_arch_name(QrxComputeArch a){switch(a){case QRX_COMPUTE_ARCH_X86_64:return "X86_64";case QRX_COMPUTE_ARCH_ARM64:return "ARM64";default:return "UNKNOWN";}}

static QrxComputeOs detect_os(void) {
#if defined(_WIN32)
    return QRX_COMPUTE_OS_WINDOWS;
#elif defined(__APPLE__)
    return QRX_COMPUTE_OS_MACOS;
#elif defined(__linux__)
    return QRX_COMPUTE_OS_LINUX;
#else
    return QRX_COMPUTE_OS_UNKNOWN;
#endif
}

static QrxComputeArch detect_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return QRX_COMPUTE_ARCH_X86_64;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return QRX_COMPUTE_ARCH_ARM64;
#else
    return QRX_COMPUTE_ARCH_UNKNOWN;
#endif
}

static uint32_t detect_logical_cpus(void) {
#if defined(_WIN32)
    SYSTEM_INFO si; GetSystemInfo(&si); return si.dwNumberOfProcessors ? (uint32_t)si.dwNumberOfProcessors : 1U;
#elif defined(__APPLE__)
    uint32_t n=0; size_t len=sizeof(n); if (sysctlbyname("hw.logicalcpu", &n, &len, NULL, 0)==0 && n) return n; return 1U;
#else
    long n=sysconf(_SC_NPROCESSORS_ONLN); return n>0 ? (uint32_t)n : 1U;
#endif
}

static void detect_memory(uint64_t *total, uint64_t *avail) {
    *total=0; *avail=0;
#if defined(_WIN32)
    MEMORYSTATUSEX ms; memset(&ms,0,sizeof(ms)); ms.dwLength=sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) { *total=(uint64_t)ms.ullTotalPhys; *avail=(uint64_t)ms.ullAvailPhys; }
#elif defined(__APPLE__)
    /* macOS does not provide _SC_AVPHYS_PAGES.  Use the native Mach VM
     * counters instead.  Free + inactive pages is a conservative amount
     * of memory that can normally be reclaimed for compute workloads. */
    uint64_t mem=0; size_t len=sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &len, NULL, 0)==0) *total=mem;
    vm_statistics64_data_t vmstat;
    mach_msg_type_number_t count=HOST_VM_INFO64_COUNT;
    vm_size_t page_size=0;
    mach_port_t host=mach_host_self();
    if (host_page_size(host, &page_size)==KERN_SUCCESS &&
        host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vmstat, &count)==KERN_SUCCESS &&
        page_size>0) {
        uint64_t pages=(uint64_t)vmstat.free_count + (uint64_t)vmstat.inactive_count;
        *avail=pages*(uint64_t)page_size;
    }
    mach_port_deallocate(mach_task_self(), host);
    if (!*avail || (*total && *avail>*total)) *avail=*total;
#elif defined(__linux__)
    struct sysinfo si; if (sysinfo(&si)==0) { *total=(uint64_t)si.totalram*(uint64_t)si.mem_unit; *avail=(uint64_t)si.freeram*(uint64_t)si.mem_unit; }
#else
    long pages=sysconf(_SC_PHYS_PAGES), apages=sysconf(_SC_AVPHYS_PAGES), psz=sysconf(_SC_PAGESIZE);
    if (pages>0 && psz>0) *total=(uint64_t)pages*(uint64_t)psz;
    if (apages>0 && psz>0) *avail=(uint64_t)apages*(uint64_t)psz;
#endif
    if (!*total) *total=1;
    if (!*avail || *avail>*total) *avail=*total;
}

uint64_t qrx_compute_detect_cpu_features(void) {
    uint64_t f=0;
#if defined(__x86_64__) || defined(_M_X64)
    /* x86-64 baseline includes SSE2. Other features are runtime-detected where practical. */
    f |= QRX_CPU_FEATURE_SSE2;
  #if defined(__GNUC__) || defined(__clang__)
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx2")) f |= QRX_CPU_FEATURE_AVX2;
    if (__builtin_cpu_supports("fma")) f |= QRX_CPU_FEATURE_FMA;
    if (__builtin_cpu_supports("avx512f")) f |= QRX_CPU_FEATURE_AVX512F;
    #if defined(__GNUC__) && !defined(__clang__)
      #if (__GNUC__ >= 11)
        if (__builtin_cpu_supports("avx512bf16")) f |= QRX_CPU_FEATURE_BF16;
        if (__builtin_cpu_supports("amx-tile")) f |= QRX_CPU_FEATURE_AMX;
      #endif
    #endif
  #elif defined(_MSC_VER)
    int r[4]={0}; __cpuid(r,1); if (r[2] & (1<<12)) f |= QRX_CPU_FEATURE_FMA;
    __cpuidex(r,7,0); if (r[1] & (1<<5)) f |= QRX_CPU_FEATURE_AVX2; if (r[1] & (1<<16)) f |= QRX_CPU_FEATURE_AVX512F;
  #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    /* AArch64 has Advanced SIMD/NEON as an architectural baseline. */
    f |= QRX_CPU_FEATURE_NEON;
  #if defined(__ARM_FEATURE_SVE)
    f |= QRX_CPU_FEATURE_SVE;
  #endif
  #if defined(__ARM_FEATURE_SVE2)
    f |= QRX_CPU_FEATURE_SVE2;
  #endif
  #if defined(__ARM_FEATURE_BF16_VECTOR_ARITHMETIC) || defined(__ARM_FEATURE_BF16_SCALAR_ARITHMETIC)
    f |= QRX_CPU_FEATURE_BF16;
  #endif
#endif
    return f;
}

int qrx_compute_detect_local_capabilities(const char *provider_id, QrxComputeCapabilities *out) {
    if (!provider_id || !out || !provider_id[0] || strlen(provider_id)>=sizeof(out->provider_id)) return -1;
    memset(out,0,sizeof(*out));
    snprintf(out->provider_id,sizeof(out->provider_id),"%s",provider_id);
    out->os=detect_os(); out->arch=detect_arch();
    if (out->os==QRX_COMPUTE_OS_UNKNOWN || out->arch==QRX_COMPUTE_ARCH_UNKNOWN) return -2;
    out->logical_cpu_count=detect_logical_cpus();
    detect_memory(&out->memory_total_bytes,&out->memory_available_bytes);
    out->cpu_feature_flags=qrx_compute_detect_cpu_features();
    out->max_parallel_tasks=out->logical_cpu_count>1 ? out->logical_cpu_count/2 : 1;
    if (out->max_parallel_tasks>32) out->max_parallel_tasks=32;
    /* Sandboxing is a protocol/runtime property. The core supports the Compute V1 sandbox contract;
       backend-specific GPU/NPU probes are intentionally separate plugins and may append accelerators later. */
    out->sandbox_available=1;
    out->accelerator_count=0;
    return qrx_compute_capabilities_validate(out);
}

int qrx_compute_select_dispatch_profile(const QrxComputeCapabilities *c, QrxComputeDispatchProfile *out) {
    if (!out || qrx_compute_capabilities_validate(c)) return -1;
    memset(out,0,sizeof(*out)); out->cpu_feature_flags=c->cpu_feature_flags;
    if (c->arch==QRX_COMPUTE_ARCH_X86_64) {
        if (c->cpu_feature_flags & QRX_CPU_FEATURE_AVX512F) out->kernel_class=QRX_COMPUTE_KERNEL_AVX512;
        else if (c->cpu_feature_flags & QRX_CPU_FEATURE_AVX2) out->kernel_class=QRX_COMPUTE_KERNEL_AVX2;
        else if (c->cpu_feature_flags & QRX_CPU_FEATURE_SSE2) out->kernel_class=QRX_COMPUTE_KERNEL_SSE2;
        else out->kernel_class=QRX_COMPUTE_KERNEL_SCALAR;
    } else if (c->arch==QRX_COMPUTE_ARCH_ARM64) {
        if (c->cpu_feature_flags & QRX_CPU_FEATURE_SVE2) out->kernel_class=QRX_COMPUTE_KERNEL_SVE2;
        else if (c->cpu_feature_flags & QRX_CPU_FEATURE_SVE) out->kernel_class=QRX_COMPUTE_KERNEL_SVE;
        else if (c->cpu_feature_flags & QRX_CPU_FEATURE_NEON) out->kernel_class=QRX_COMPUTE_KERNEL_NEON;
        else out->kernel_class=QRX_COMPUTE_KERNEL_SCALAR;
    }
    out->recommended_parallel_tasks=c->max_parallel_tasks;
    /* Reserve 12.5% for the host; never advertise exact free memory to peers. */
    out->scheduling_memory_bytes=(c->memory_available_bytes/8ULL)*7ULL;
    return 0;
}

const char *qrx_compute_kernel_name(uint32_t k) {
    switch(k){case QRX_COMPUTE_KERNEL_SSE2:return "SSE2";case QRX_COMPUTE_KERNEL_AVX2:return "AVX2";case QRX_COMPUTE_KERNEL_AVX512:return "AVX512";case QRX_COMPUTE_KERNEL_NEON:return "NEON";case QRX_COMPUTE_KERNEL_SVE:return "SVE";case QRX_COMPUTE_KERNEL_SVE2:return "SVE2";default:return "SCALAR";}
}

static uint32_t benchmark_component_score(uint64_t measured, uint64_t reference) {
    if (!reference || !measured) return 0;
    /* Avoid floating-point consensus/scheduler drift. Saturating fixed-point ratio. */
    if (measured >= reference * 2ULL) return QRX_COMPUTE_COMPONENT_SCORE_CAP;
    return (uint32_t)((measured * (uint64_t)QRX_COMPUTE_NCU_BASE) / reference);
}

int qrx_compute_benchmark_validate(const QrxComputeBenchmarkMetrics *m) {
    if (!m || m->version != QRX_COMPUTE_BENCHMARK_VERSION) return -1;
    if ((m->source_mask & QRX_COMPUTE_BENCH_SRC_REQUIRED) != QRX_COMPUTE_BENCH_SRC_REQUIRED) return -1;
    if (!m->sample_count || !m->sample_window_ms) return -1;
    if (!m->bf16_mops || !m->mxfp4_mops || !m->memory_mib_s || !m->storage_mib_s || !m->network_mbit_s || !m->moe_milli_tokens_s || !m->inference_milli_tokens_s) return -1;
    /* Reject absurd values instead of allowing integer abuse or malformed telemetry. */
    if (m->sample_count > 1000000U || m->sample_window_ms > 86400000ULL) return -1;
    return 0;
}

int qrx_compute_benchmark_normalize(const QrxComputeBenchmarkMetrics *m, QrxComputeNormalizedScore *out) {
    if (!out || qrx_compute_benchmark_validate(m)) return -1;
    memset(out,0,sizeof(*out));
    out->version=QRX_COMPUTE_BENCHMARK_VERSION;

    const uint32_t bf16=benchmark_component_score(m->bf16_mops,QRX_COMPUTE_REF_BF16_MOPS);
    const uint32_t mx=benchmark_component_score(m->mxfp4_mops,QRX_COMPUTE_REF_MXFP4_MOPS);
    /* CPU blends both standardized arithmetic workloads equally. */
    out->cpu_score=(uint32_t)(((uint64_t)bf16+(uint64_t)mx)/2ULL);
    out->memory_score=benchmark_component_score(m->memory_mib_s,QRX_COMPUTE_REF_MEMORY_MIB_S);
    out->storage_score=benchmark_component_score(m->storage_mib_s,QRX_COMPUTE_REF_STORAGE_MIB_S);
    out->network_score=benchmark_component_score(m->network_mbit_s,QRX_COMPUTE_REF_NETWORK_MBIT_S);
    out->moe_score=benchmark_component_score(m->moe_milli_tokens_s,QRX_COMPUTE_REF_MOE_MILLI_TOKENS_S);
    out->inference_score=benchmark_component_score(m->inference_milli_tokens_s,QRX_COMPUTE_REF_INFERENCE_MILLI_TOKENS_S);

    /* V1 weights: arithmetic 30%, memory 15%, storage 10%, network 10%,
       MoE expert execution 20%, model inference 15%. Integer weights keep
       normalization deterministic across platforms. */
    const uint64_t weighted=(uint64_t)out->cpu_score*30ULL +
                            (uint64_t)out->memory_score*15ULL +
                            (uint64_t)out->storage_score*10ULL +
                            (uint64_t)out->network_score*10ULL +
                            (uint64_t)out->moe_score*20ULL +
                            (uint64_t)out->inference_score*15ULL;
    out->normalized_score=(uint32_t)(weighted/100ULL);

    if (out->normalized_score < 7500U) out->class_id=QRX_COMPUTE_SCORE_CLASS_ENTRY;
    else if (out->normalized_score < 12500U) out->class_id=QRX_COMPUTE_SCORE_CLASS_STANDARD;
    else if (out->normalized_score < 17500U) out->class_id=QRX_COMPUTE_SCORE_CLASS_HIGH;
    else out->class_id=QRX_COMPUTE_SCORE_CLASS_EXTREME;
    return 0;
}

const char *qrx_compute_score_class_name(uint32_t c) {
    switch(c) {
        case QRX_COMPUTE_SCORE_CLASS_ENTRY: return "ENTRY";
        case QRX_COMPUTE_SCORE_CLASS_STANDARD: return "STANDARD";
        case QRX_COMPUTE_SCORE_CLASS_HIGH: return "HIGH";
        case QRX_COMPUTE_SCORE_CLASS_EXTREME: return "EXTREME";
        default: return "UNKNOWN";
    }
}



static int qrx_model_hexroot(const char *s) {
    if (!s || strlen(s)!=QRX_MODEL_ROOT_HEX) return 0;
    for (size_t i=0;i<QRX_MODEL_ROOT_HEX;i++) if (!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}
static int qrx_model_zero_root(const char *s) {
    if (!qrx_model_hexroot(s)) return 0;
    for (size_t i=0;i<QRX_MODEL_ROOT_HEX;i++) if (s[i]!='0') return 0;
    return 1;
}
int qrx_ai_model_validate(const QrxAiModelRecord *m) {
    if (!m || m->registry_version!=QRX_MODEL_REGISTRY_VERSION) return -1;
    if (!m->model_id[0] || !m->model_version[0] || !m->architecture[0] || !m->runtime_id[0] || !m->license_id[0]) return -1;
    if (strnlen(m->model_id,sizeof(m->model_id))>=sizeof(m->model_id) || strnlen(m->model_version,sizeof(m->model_version))>=sizeof(m->model_version) || strnlen(m->architecture,sizeof(m->architecture))>=sizeof(m->architecture) || strnlen(m->runtime_id,sizeof(m->runtime_id))>=sizeof(m->runtime_id) || strnlen(m->license_id,sizeof(m->license_id))>=sizeof(m->license_id)) return -1;
    if (!qrx_model_hexroot(m->manifest_root) || !qrx_model_hexroot(m->tokenizer_root) || !qrx_model_hexroot(m->expert_manifest_root)) return -1;
    if (!m->min_memory_bytes || !m->min_storage_bytes) return -1;
    if (m->verification_profile<QRX_MODEL_VERIFY_LOW || m->verification_profile>QRX_MODEL_VERIFY_HIGH) return -1;
    if (m->is_moe) { if (qrx_model_zero_root(m->expert_manifest_root)) return -1; }
    else { if (!qrx_model_zero_root(m->expert_manifest_root)) return -1; }
    return 0;
}

static int qrx_digest_field(EVP_MD_CTX *ctx,const void *p,size_t n) {
    unsigned char len[8]; uint64_t x=(uint64_t)n;
    for (int i=7;i>=0;i--){len[i]=(unsigned char)(x&255U);x>>=8;}
    return EVP_DigestUpdate(ctx,len,sizeof(len))==1 && (!n || EVP_DigestUpdate(ctx,p,n)==1) ? 0 : -1;
}
int qrx_ai_model_commitment(const QrxAiModelRecord *m, char out_hex[65]) {
    if (!out_hex || qrx_ai_model_validate(m)) return -1;
    EVP_MD_CTX *ctx=EVP_MD_CTX_new(); unsigned char d[32]; unsigned n=0; int rc=-1;
    static const char domain[]="QRX-AI-MODEL-REGISTRY-V1";
    if (!ctx || EVP_DigestInit_ex(ctx,EVP_sha3_256(),NULL)!=1 || EVP_DigestUpdate(ctx,domain,sizeof(domain)-1)!=1) goto done;
#define F(x) if(qrx_digest_field(ctx,(x),strlen(x))) goto done
    F(m->model_id); F(m->model_version); F(m->architecture); F(m->runtime_id); F(m->manifest_root); F(m->tokenizer_root); F(m->expert_manifest_root); F(m->license_id);
#undef F
    unsigned char nums[21]; memset(nums,0,sizeof(nums));
    uint64_t vals[2]={m->min_memory_bytes,m->min_storage_bytes}; size_t o=0;
    for(int v=0;v<2;v++) for(int i=7;i>=0;i--) nums[o++]=(unsigned char)((vals[v]>>(i*8))&255U);
    nums[o++]=(unsigned char)m->verification_profile; nums[o++]=m->is_moe; nums[o++]=(unsigned char)m->registry_version;
    if (qrx_digest_field(ctx,nums,o) || EVP_DigestFinal_ex(ctx,d,&n)!=1 || n!=32) goto done;
    static const char hex[]="0123456789abcdef"; for(int i=0;i<32;i++){out_hex[i*2]=hex[d[i]>>4];out_hex[i*2+1]=hex[d[i]&15];} out_hex[64]=0; rc=0;
done: EVP_MD_CTX_free(ctx); return rc;
}
int qrx_ai_model_registry_add(QrxAiModelRegistry *r,const QrxAiModelRecord *m) {
    if (!r || qrx_ai_model_validate(m) || r->count>=64) return -1;
    for(uint32_t i=0;i<r->count;i++) if(!strcmp(r->records[i].model_id,m->model_id) && !strcmp(r->records[i].model_version,m->model_version)) return -2;
    r->records[r->count++]=*m; return 0;
}
const QrxAiModelRecord *qrx_ai_model_registry_find(const QrxAiModelRegistry *r,const char *id,const char *ver) {
    if(!r||!id||!ver) return NULL; for(uint32_t i=0;i<r->count;i++) if(!strcmp(r->records[i].model_id,id)&&!strcmp(r->records[i].model_version,ver)) return &r->records[i]; return NULL;
}

/* ---- 0.0.9.4 Compute Job Protocol & Job Graphs ------------------------- */
static int qrx_job_ref_valid(const char *s,size_t n) {
    if (!s || !memchr(s,'\0',n) || !s[0]) return 0;
    /* References are opaque content-addressed/object IDs at this layer.
       Ban whitespace/control characters to keep canonical commitments stable. */
    for (size_t i=0;s[i];i++) if ((unsigned char)s[i]<=0x20U || (unsigned char)s[i]==0x7fU) return 0;
    return 1;
}
const char *qrx_compute_job_type_name(QrxComputeJobType t) {
    switch(t) {
        case QRX_JOB_AI_INFERENCE:return "AI_INFERENCE";
        case QRX_JOB_CODE_GENERATION:return "CODE_GENERATION";
        case QRX_JOB_CODE_EXECUTION:return "CODE_EXECUTION";
        case QRX_JOB_DOCUMENT_GENERATION:return "DOCUMENT_GENERATION";
        case QRX_JOB_DATA_ANALYSIS:return "DATA_ANALYSIS";
        case QRX_JOB_IMAGE_GENERATION:return "IMAGE_GENERATION";
        case QRX_JOB_COMPILE:return "COMPILE";
        case QRX_JOB_TEST:return "TEST";
        case QRX_JOB_CONVERSION:return "CONVERSION";
        case QRX_JOB_RESEARCH:return "RESEARCH";
        case QRX_JOB_CUSTOM_SANDBOXED:return "CUSTOM_SANDBOXED";
        default:return "UNKNOWN";
    }
}
int qrx_compute_job_node_validate(const QrxComputeJobNode *n) {
    if (!n || !n->node_id) return -1;
    if (n->job_type<QRX_JOB_AI_INFERENCE || n->job_type>QRX_JOB_CUSTOM_SANDBOXED) return -1;
    if (!nonempty(n->runtime_id,sizeof(n->runtime_id))) return -1;
    if (!qrx_job_ref_valid(n->input_ref,sizeof(n->input_ref)) || !qrx_job_ref_valid(n->output_ref,sizeof(n->output_ref))) return -1;
    if (!n->max_runtime_ms || !n->max_output_bytes || !n->max_fee_atoms) return -1;
    if (n->capability_mask & ~QRX_JOB_CAP_ALLOWED_V1) return -1;
    if (!(n->capability_mask & QRX_JOB_CAP_READ_INPUTS)) return -1;
    /* Execution/build/test jobs must explicitly request the sandbox capability. */
    if ((n->job_type==QRX_JOB_CODE_EXECUTION || n->job_type==QRX_JOB_COMPILE || n->job_type==QRX_JOB_TEST || n->job_type==QRX_JOB_CUSTOM_SANDBOXED) && !(n->capability_mask & QRX_JOB_CAP_SANDBOX_EXEC)) return -1;
    /* AI-specific work binds to a registered model identity. */
    if (n->job_type==QRX_JOB_AI_INFERENCE || n->job_type==QRX_JOB_CODE_GENERATION || n->job_type==QRX_JOB_IMAGE_GENERATION || n->job_type==QRX_JOB_RESEARCH) {
        if (!nonempty(n->model_id,sizeof(n->model_id)) || !nonempty(n->model_version,sizeof(n->model_version))) return -1;
        if (!(n->capability_mask & QRX_JOB_CAP_MODEL_INFERENCE)) return -1;
    }
    return 0;
}

int qrx_compute_job_graph_topological_order(const QrxComputeJobGraph *g,uint32_t out_ids[QRX_COMPUTE_JOB_MAX_NODES]) {
    if (!g || !out_ids || !g->node_count || g->node_count>QRX_COMPUTE_JOB_MAX_NODES || g->edge_count>QRX_COMPUTE_JOB_MAX_EDGES) return -1;
    uint32_t indeg[QRX_COMPUTE_JOB_MAX_NODES]={0}, used[QRX_COMPUTE_JOB_MAX_NODES]={0};
    /* Node IDs are external stable IDs; map edges by linear scan (small bounded graph). */
    for (uint32_t e=0;e<g->edge_count;e++) {
        int fi=-1,ti=-1;
        if (g->edges[e].from_node==g->edges[e].to_node) return -2;
        for(uint32_t i=0;i<g->node_count;i++) {
            if(g->nodes[i].node_id==g->edges[e].from_node) fi=(int)i;
            if(g->nodes[i].node_id==g->edges[e].to_node) ti=(int)i;
        }
        if(fi<0||ti<0) return -2;
        indeg[ti]++;
    }
    for(uint32_t pos=0;pos<g->node_count;pos++) {
        int best=-1;
        /* Deterministic tie-break: smallest node_id first. */
        for(uint32_t i=0;i<g->node_count;i++) if(!used[i] && indeg[i]==0 && (best<0 || g->nodes[i].node_id<g->nodes[best].node_id)) best=(int)i;
        if(best<0) return -3; /* cycle */
        used[best]=1; out_ids[pos]=g->nodes[best].node_id;
        for(uint32_t e=0;e<g->edge_count;e++) if(g->edges[e].from_node==g->nodes[best].node_id) {
            for(uint32_t j=0;j<g->node_count;j++) if(g->nodes[j].node_id==g->edges[e].to_node) { if(!indeg[j]) return -2; indeg[j]--; break; }
        }
    }
    return 0;
}

int qrx_compute_job_graph_validate(const QrxComputeJobGraph *g) {
    if(!g || g->protocol_version!=QRX_COMPUTE_JOB_PROTOCOL_VERSION) return -1;
    if(!nonempty(g->graph_id,sizeof(g->graph_id)) || !nonempty(g->owner,sizeof(g->owner))) return -1;
    if(!g->nonce || !g->max_total_fee_atoms || !g->expiry_height) return -1;
    if(!g->node_count || g->node_count>QRX_COMPUTE_JOB_MAX_NODES || g->edge_count>QRX_COMPUTE_JOB_MAX_EDGES) return -1;
    uint64_t fee_sum=0;
    for(uint32_t i=0;i<g->node_count;i++) {
        if(qrx_compute_job_node_validate(&g->nodes[i])) return -1;
        for(uint32_t j=0;j<i;j++) if(g->nodes[j].node_id==g->nodes[i].node_id) return -1;
        if(UINT64_MAX-fee_sum<g->nodes[i].max_fee_atoms) return -1;
        fee_sum+=g->nodes[i].max_fee_atoms;
    }
    if(fee_sum>g->max_total_fee_atoms) return -1;
    for(uint32_t e=0;e<g->edge_count;e++) {
        for(uint32_t x=0;x<e;x++) if(g->edges[x].from_node==g->edges[e].from_node && g->edges[x].to_node==g->edges[e].to_node) return -1;
    }
    uint32_t order[QRX_COMPUTE_JOB_MAX_NODES];
    return qrx_compute_job_graph_topological_order(g,order)==0 ? 0 : -1;
}

static int qrx_job_digest_u64(EVP_MD_CTX *ctx,uint64_t x) {
    unsigned char b[8]; for(int i=7;i>=0;i--){b[i]=(unsigned char)(x&255U);x>>=8;} return qrx_digest_field(ctx,b,sizeof(b));
}
static int qrx_job_digest_u32(EVP_MD_CTX *ctx,uint32_t x) {
    unsigned char b[4]; for(int i=3;i>=0;i--){b[i]=(unsigned char)(x&255U);x>>=8;} return qrx_digest_field(ctx,b,sizeof(b));
}
int qrx_compute_job_graph_commitment(const QrxComputeJobGraph *g,char out_hex[65]) {
    if(!out_hex || qrx_compute_job_graph_validate(g)) return -1;
    EVP_MD_CTX *ctx=EVP_MD_CTX_new(); unsigned char d[32]; unsigned dn=0; int rc=-1;
    static const char domain[]="QRX-COMPUTE-JOB-GRAPH-V1";
    if(!ctx || EVP_DigestInit_ex(ctx,EVP_sha3_256(),NULL)!=1 || EVP_DigestUpdate(ctx,domain,sizeof(domain)-1)!=1) goto done;
    if(qrx_digest_field(ctx,g->graph_id,strlen(g->graph_id)) || qrx_digest_field(ctx,g->owner,strlen(g->owner)) || qrx_job_digest_u64(ctx,g->nonce) || qrx_job_digest_u64(ctx,g->max_total_fee_atoms) || qrx_job_digest_u64(ctx,g->expiry_height) || qrx_job_digest_u32(ctx,g->node_count) || qrx_job_digest_u32(ctx,g->edge_count)) goto done;
    /* Canonical node serialization is by ascending node_id, independent of in-memory insertion order. */
    uint32_t idx[QRX_COMPUTE_JOB_MAX_NODES]; for(uint32_t i=0;i<g->node_count;i++) idx[i]=i;
    for(uint32_t i=0;i<g->node_count;i++) for(uint32_t j=i+1;j<g->node_count;j++) if(g->nodes[idx[j]].node_id<g->nodes[idx[i]].node_id){uint32_t t=idx[i];idx[i]=idx[j];idx[j]=t;}
    for(uint32_t k=0;k<g->node_count;k++) {
        const QrxComputeJobNode *n=&g->nodes[idx[k]];
        if(qrx_job_digest_u32(ctx,n->node_id)||qrx_job_digest_u32(ctx,(uint32_t)n->job_type)||qrx_digest_field(ctx,n->runtime_id,strlen(n->runtime_id))||qrx_digest_field(ctx,n->model_id,strlen(n->model_id))||qrx_digest_field(ctx,n->model_version,strlen(n->model_version))||qrx_digest_field(ctx,n->input_ref,strlen(n->input_ref))||qrx_digest_field(ctx,n->output_ref,strlen(n->output_ref))||qrx_job_digest_u64(ctx,n->min_memory_bytes)||qrx_job_digest_u64(ctx,n->max_runtime_ms)||qrx_job_digest_u64(ctx,n->max_output_bytes)||qrx_job_digest_u64(ctx,n->max_fee_atoms)||qrx_job_digest_u32(ctx,n->capability_mask)) goto done;
    }
    /* Edges are canonicalized lexicographically by (from,to). */
    uint32_t ei[QRX_COMPUTE_JOB_MAX_EDGES]; for(uint32_t i=0;i<g->edge_count;i++) ei[i]=i;
    for(uint32_t i=0;i<g->edge_count;i++) for(uint32_t j=i+1;j<g->edge_count;j++) {const QrxComputeJobEdge *a=&g->edges[ei[i]],*b=&g->edges[ei[j]]; if(b->from_node<a->from_node || (b->from_node==a->from_node && b->to_node<a->to_node)){uint32_t t=ei[i];ei[i]=ei[j];ei[j]=t;}}
    for(uint32_t k=0;k<g->edge_count;k++) if(qrx_job_digest_u32(ctx,g->edges[ei[k]].from_node)||qrx_job_digest_u32(ctx,g->edges[ei[k]].to_node)) goto done;
    if(EVP_DigestFinal_ex(ctx,d,&dn)!=1||dn!=32) goto done;
    {static const char hex[]="0123456789abcdef"; for(int i=0;i<32;i++){out_hex[i*2]=hex[d[i]>>4];out_hex[i*2+1]=hex[d[i]&15];}out_hex[64]=0;} rc=0;
done: EVP_MD_CTX_free(ctx); return rc;
}


/* ---- 0.0.9.5 Compute Market, Quotes & Escrow -------------------------- */
static const QrxComputeJobNode *qrx_job_find_node(const QrxComputeJobGraph *g,uint32_t node_id) {
    if(!g) return NULL;
    for(uint32_t i=0;i<g->node_count;i++) if(g->nodes[i].node_id==node_id) return &g->nodes[i];
    return NULL;
}
static int qrx_hex64(const char *s) {
    if(!s || strlen(s)!=64) return 0;
    for(size_t i=0;i<64;i++) if(!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}
int qrx_compute_quote_validate(const QrxComputeQuote *q,const QrxComputeJobGraph *g,uint64_t height) {
    if(!q || !g || qrx_compute_job_graph_validate(g)) return -1;
    if(q->market_version!=QRX_COMPUTE_MARKET_VERSION || !nonempty(q->quote_id,sizeof(q->quote_id)) || !nonempty(q->provider_id,sizeof(q->provider_id))) return -1;
    if(!qrx_hex64(q->graph_commitment) || !q->node_id || !q->price_atoms || !q->expiry_height || q->expiry_height<=height) return -1;
    if(!q->normalized_compute_score || !q->reliability_bps || q->reliability_bps>QRX_COMPUTE_BPS || !q->estimated_latency_ms) return -1;
    if(q->verification_level<QRX_COMPUTE_VERIFY_LOW || q->verification_level>QRX_COMPUTE_VERIFY_HIGH) return -1;
    if(q->model_local>1 || q->fasttrack_available>1) return -1;
    const QrxComputeJobNode *n=qrx_job_find_node(g,q->node_id); if(!n || q->price_atoms>n->max_fee_atoms) return -1;
    char commitment[65]; if(qrx_compute_job_graph_commitment(g,commitment) || strcmp(commitment,q->graph_commitment)) return -1;
    return 0;
}
int qrx_compute_quote_book_add(QrxComputeQuoteBook *b,const QrxComputeQuote *q,const QrxComputeJobGraph *g,uint64_t height) {
    if(!b || qrx_compute_quote_validate(q,g,height) || b->count>=QRX_COMPUTE_MAX_QUOTES) return -1;
    for(uint32_t i=0;i<b->count;i++) if(!strcmp(b->quotes[i].quote_id,q->quote_id)) return -2;
    b->quotes[b->count++]=*q; return 0;
}
int qrx_compute_quote_rank_score(const QrxComputeQuote *q,const QrxComputeJobNode *n,uint32_t *out) {
    if(!q||!n||!out||!n->max_fee_atoms||!q->price_atoms||q->price_atoms>n->max_fee_atoms||q->reliability_bps>QRX_COMPUTE_BPS) return -1;
    uint64_t price=(n->max_fee_atoms-q->price_atoms)*QRX_COMPUTE_BPS/n->max_fee_atoms;
    uint64_t ncu=q->normalized_compute_score>=20000U?QRX_COMPUTE_BPS:(uint64_t)q->normalized_compute_score*QRX_COMPUTE_BPS/20000U;
    uint64_t latency=q->estimated_latency_ms>=10000U?0ULL:(uint64_t)(10000U-q->estimated_latency_ms);
    uint64_t locality=q->model_local?QRX_COMPUTE_BPS:0U;
    uint64_t verify=(uint64_t)q->verification_level*QRX_COMPUTE_BPS/QRX_COMPUTE_VERIFY_HIGH;
    /* V1 deterministic weights: price 25, NCU 20, reliability 20,
       model locality 15, latency 10, verification capability 10. */
    uint64_t weighted=price*25ULL+ncu*20ULL+(uint64_t)q->reliability_bps*20ULL+locality*15ULL+latency*10ULL+verify*10ULL;
    *out=(uint32_t)(weighted/100ULL); return 0;
}
const QrxComputeQuote *qrx_compute_quote_select(const QrxComputeQuoteBook *b,const QrxComputeJobGraph *g,uint32_t node_id,uint32_t min_verify,uint8_t require_local,uint8_t require_fast,uint64_t height) {
    if(!b||!g||min_verify<QRX_COMPUTE_VERIFY_LOW||min_verify>QRX_COMPUTE_VERIFY_HIGH) return NULL;
    const QrxComputeJobNode *n=qrx_job_find_node(g,node_id); if(!n) return NULL;
    const QrxComputeQuote *best=NULL; uint32_t best_score=0;
    for(uint32_t i=0;i<b->count;i++) {
        const QrxComputeQuote *q=&b->quotes[i];
        if(q->node_id!=node_id || qrx_compute_quote_validate(q,g,height) || q->verification_level<min_verify || (require_local&&!q->model_local) || (require_fast&&!q->fasttrack_available)) continue;
        uint32_t score=0; if(qrx_compute_quote_rank_score(q,n,&score)) continue;
        if(!best || score>best_score || (score==best_score && (q->price_atoms<best->price_atoms || (q->price_atoms==best->price_atoms && strcmp(q->provider_id,best->provider_id)<0)))) {best=q;best_score=score;}
    }
    return best;
}
int qrx_compute_escrow_lock(QrxComputeEscrow *e,const QrxComputeJobGraph *g,uint64_t fasttrack,uint64_t height) {
    if(!e||!g||qrx_compute_job_graph_validate(g)||g->expiry_height<=height) return -1;
    if(g->max_total_fee_atoms && fasttrack > (g->max_total_fee_atoms / QRX_COMPUTE_BPS) * QRX_FASTTRACK_MAX_PREMIUM_BPS
       + ((g->max_total_fee_atoms % QRX_COMPUTE_BPS) * QRX_FASTTRACK_MAX_PREMIUM_BPS) / QRX_COMPUTE_BPS) return -2;
    if(UINT64_MAX-g->max_total_fee_atoms<fasttrack) return -1;
    memset(e,0,sizeof(*e)); e->market_version=QRX_COMPUTE_MARKET_VERSION;
    if(qrx_compute_job_graph_commitment(g,e->graph_commitment)) return -1;
    snprintf(e->owner,sizeof(e->owner),"%s",g->owner); e->max_compute_atoms=g->max_total_fee_atoms; e->fasttrack_fee_atoms=fasttrack; e->verification_reward_atoms=0; e->verification_reward_spent_atoms=0; e->locked_atoms=g->max_total_fee_atoms+fasttrack; e->expiry_height=g->expiry_height; e->state=QRX_COMPUTE_ESCROW_LOCKED; return 0;
}
int qrx_compute_escrow_mark_assigned(QrxComputeEscrow *e) {
    if(!e||e->state!=QRX_COMPUTE_ESCROW_LOCKED) return -1; e->state=QRX_COMPUTE_ESCROW_ASSIGNED; return 0;
}
int qrx_compute_escrow_settle(QrxComputeEscrow *e,uint64_t actual,uint64_t height) {
    if(!e || (e->state!=QRX_COMPUTE_ESCROW_LOCKED && e->state!=QRX_COMPUTE_ESCROW_ASSIGNED)) return -1;
    if(height>e->expiry_height || actual>e->max_compute_atoms) return -1;
    e->actual_compute_atoms=actual;
    e->fasttrack_dev_atoms=(e->fasttrack_fee_atoms*QRX_FASTTRACK_DEV_SHARE_BPS)/QRX_COMPUTE_BPS;
    e->fasttrack_net_atoms=(e->fasttrack_fee_atoms*QRX_FASTTRACK_NETWORK_SHARE_BPS)/QRX_COMPUTE_BPS;
    /* The remaining FastTrack surcharge (90%, plus deterministic integer rounding)
       belongs to the provider. It is derived at consensus settlement time from
       fee - development - network, so no additional escrow field is required. */
    if(e->verification_reward_spent_atoms>e->verification_reward_atoms) return -1;
    e->refund_atoms=(e->max_compute_atoms-actual)+(e->verification_reward_atoms-e->verification_reward_spent_atoms);
    e->state=QRX_COMPUTE_ESCROW_SETTLED; return 0;
}
int qrx_compute_escrow_refund(QrxComputeEscrow *e,uint64_t height) {
    if(!e || (e->state!=QRX_COMPUTE_ESCROW_LOCKED && e->state!=QRX_COMPUTE_ESCROW_ASSIGNED)) return -1;
    e->actual_compute_atoms=0; e->fasttrack_dev_atoms=0; e->fasttrack_net_atoms=0; if(e->verification_reward_spent_atoms>e->locked_atoms) return -1; e->refund_atoms=e->locked_atoms-e->verification_reward_spent_atoms;
    e->state=height>e->expiry_height?QRX_COMPUTE_ESCROW_EXPIRED:QRX_COMPUTE_ESCROW_REFUNDED; return 0;
}
const char *qrx_compute_escrow_state_name(QrxComputeEscrowState s) {
    switch(s){case QRX_COMPUTE_ESCROW_EMPTY:return "EMPTY";case QRX_COMPUTE_ESCROW_LOCKED:return "LOCKED";case QRX_COMPUTE_ESCROW_ASSIGNED:return "ASSIGNED";case QRX_COMPUTE_ESCROW_SETTLED:return "SETTLED";case QRX_COMPUTE_ESCROW_REFUNDED:return "REFUNDED";case QRX_COMPUTE_ESCROW_CANCELLED:return "CANCELLED";case QRX_COMPUTE_ESCROW_EXPIRED:return "EXPIRED";default:return "UNKNOWN";}
}

/* ---- 0.0.9.6 Secure Execution Sandbox + Budget Guard ------------------ */
int qrx_compute_sandbox_policy_default(const QrxComputeJobNode *n,QrxComputeSandboxPolicy *p) {
    if(!n||!p) return -1; memset(p,0,sizeof(*p)); p->version=QRX_SANDBOX_POLICY_VERSION;
    if(n->capability_mask & QRX_JOB_CAP_READ_INPUTS) p->permissions|=QRX_SANDBOX_ALLOW_INPUT_READ;
    if(n->capability_mask & QRX_JOB_CAP_WRITE_ARTIFACTS) p->permissions|=QRX_SANDBOX_ALLOW_ARTIFACT_WRITE;
    if(n->capability_mask & QRX_JOB_CAP_MODEL_INFERENCE) p->permissions|=QRX_SANDBOX_ALLOW_MODEL_READ;
    if(n->capability_mask & QRX_JOB_CAP_SANDBOX_EXEC) p->permissions|=QRX_SANDBOX_ALLOW_SUBPROCESS;
    p->memory_limit_bytes=n->min_memory_bytes ? n->min_memory_bytes*2ULL : 256ULL*1024ULL*1024ULL;
    if(p->memory_limit_bytes<n->min_memory_bytes) p->memory_limit_bytes=UINT64_MAX;
    p->cpu_time_limit_ms=n->max_runtime_ms; p->wall_time_limit_ms=n->max_runtime_ms; p->output_limit_bytes=n->max_output_bytes;
    p->max_subprocesses=(n->capability_mask&QRX_JOB_CAP_SANDBOX_EXEC)?16U:0U;
    return qrx_compute_sandbox_policy_validate(n,p);
}
int qrx_compute_sandbox_policy_validate(const QrxComputeJobNode *n,const QrxComputeSandboxPolicy *p) {
    if(!n||!p||p->version!=QRX_SANDBOX_POLICY_VERSION) return -1;
    if(p->permissions & ~QRX_SANDBOX_ALLOWED_V1) return -1;
    if(p->wallet_keys_visible||p->host_secrets_visible||p->host_fs_visible||p->unrestricted_network) return -2;
    if(!p->memory_limit_bytes||p->memory_limit_bytes<n->min_memory_bytes||!p->cpu_time_limit_ms||p->cpu_time_limit_ms>n->max_runtime_ms||!p->wall_time_limit_ms||p->wall_time_limit_ms>n->max_runtime_ms||!p->output_limit_bytes||p->output_limit_bytes>n->max_output_bytes) return -1;
    if((p->permissions&QRX_SANDBOX_ALLOW_SUBPROCESS) && !(n->capability_mask&QRX_JOB_CAP_SANDBOX_EXEC)) return -3;
    if((p->permissions&QRX_SANDBOX_ALLOW_INPUT_READ) && !(n->capability_mask&QRX_JOB_CAP_READ_INPUTS)) return -3;
    if((p->permissions&QRX_SANDBOX_ALLOW_ARTIFACT_WRITE) && !(n->capability_mask&QRX_JOB_CAP_WRITE_ARTIFACTS)) return -3;
    if((p->permissions&QRX_SANDBOX_ALLOW_MODEL_READ) && !(n->capability_mask&QRX_JOB_CAP_MODEL_INFERENCE)) return -3;
    /* V1 deliberately denies direct network. A later allowlisted proxy capability
       can expose specific destinations without granting raw host networking. */
    if(p->permissions&QRX_SANDBOX_ALLOW_NETWORK) return -4;
    return 0;
}
int qrx_compute_budget_guard_init(QrxComputeBudgetGuard *g,uint64_t authorized) {
    if(!g||!authorized) return -1; memset(g,0,sizeof(*g)); g->authorized_atoms=authorized; g->state=QRX_BUDGET_RUNNING; return 0;
}
int qrx_compute_budget_guard_charge(QrxComputeBudgetGuard *g,uint64_t total) {
    if(!g||!g->authorized_atoms||total<g->verified_spent_atoms||g->state==QRX_BUDGET_COMPLETED) return -1;
    if(total>g->authorized_atoms) return -2; /* never spend beyond prior authorization */
    g->verified_spent_atoms=total;
    uint64_t bps=(total>=g->authorized_atoms)?10000ULL:(total*10000ULL/g->authorized_atoms);
    if(total==g->authorized_atoms) g->state=QRX_BUDGET_PAUSED_EXHAUSTED;
    else if(bps>=QRX_BUDGET_CRITICAL_BPS) g->state=QRX_BUDGET_CHECKPOINTING;
    else if(bps>=QRX_BUDGET_WARNING_BPS) g->state=QRX_BUDGET_WARNING;
    else g->state=QRX_BUDGET_RUNNING;
    return 0;
}
int qrx_compute_budget_guard_checkpoint(QrxComputeBudgetGuard *g,const char *ref) {
    if(!g||!ref||!ref[0]||strlen(ref)>=sizeof(g->checkpoint_ref)) return -1;
    if(g->state!=QRX_BUDGET_CHECKPOINTING&&g->state!=QRX_BUDGET_WARNING&&g->state!=QRX_BUDGET_PAUSED_EXHAUSTED) return -2;
    snprintf(g->checkpoint_ref,sizeof(g->checkpoint_ref),"%s",ref); g->checkpoint_seq++;
    if(g->verified_spent_atoms>=g->authorized_atoms) g->state=QRX_BUDGET_PAUSED_EXHAUSTED; return 0;
}
int qrx_compute_budget_guard_extend(QrxComputeBudgetGuard *g,uint64_t add) {
    if(!g||!add||g->state!=QRX_BUDGET_PAUSED_EXHAUSTED||UINT64_MAX-g->authorized_atoms<add) return -1;
    g->authorized_atoms+=add; g->extension_atoms+=add; g->state=QRX_BUDGET_EXTENDED; return 0;
}
int qrx_compute_budget_guard_resume(QrxComputeBudgetGuard *g) {
    if(!g||g->state!=QRX_BUDGET_EXTENDED||!g->checkpoint_seq) return -1; g->state=QRX_BUDGET_RESUMING; return 0;
}
int qrx_compute_budget_guard_complete(QrxComputeBudgetGuard *g) {
    if(!g||g->verified_spent_atoms>g->authorized_atoms||g->state==QRX_BUDGET_PAUSED_EXHAUSTED) return -1; g->state=QRX_BUDGET_COMPLETED; return 0;
}
const char *qrx_compute_budget_state_name(QrxComputeBudgetState s) {
    switch(s){case QRX_BUDGET_RUNNING:return "RUNNING";case QRX_BUDGET_WARNING:return "BUDGET_WARNING";case QRX_BUDGET_CHECKPOINTING:return "CHECKPOINTING";case QRX_BUDGET_PAUSED_EXHAUSTED:return "PAUSED_BUDGET_EXHAUSTED";case QRX_BUDGET_EXTENDED:return "BUDGET_EXTENDED";case QRX_BUDGET_RESUMING:return "RESUMING";case QRX_BUDGET_COMPLETED:return "COMPLETED";default:return "UNKNOWN";}
}

/* ---- 0.0.9.7 Proof of Useful Compute verification --------------------- */
static int qrx_pouc_hex(const char *s){return qrx_hex64(s);}
int qrx_pouc_receipt_validate(const QrxPoucReceipt *r){
 if(!r||r->version!=QRX_POUC_VERSION||!r->node_id||!nonempty(r->provider_id,sizeof(r->provider_id))||!nonempty(r->runtime_id,sizeof(r->runtime_id)))return -1;
 if(!qrx_pouc_hex(r->graph_commitment)||!qrx_pouc_hex(r->input_commitment)||!qrx_pouc_hex(r->model_commitment)||!qrx_pouc_hex(r->execution_params_commitment)||!qrx_pouc_hex(r->result_commitment))return -1;
 if(!r->verified_compute_atoms||!r->started_height||r->completed_height<r->started_height)return -1;
 if(r->verification_mode<QRX_POUC_VERIFY_SPOT||r->verification_mode>QRX_POUC_VERIFY_REDUNDANT_2_OF_3)return -1; return 0;
}
int qrx_pouc_receipt_commitment(const QrxPoucReceipt *r,char out[65]){
 if(qrx_pouc_receipt_validate(r)||!out)return -1; EVP_MD_CTX *c=EVP_MD_CTX_new(); if(!c)return -1; unsigned char h[32];unsigned int n=0;int ok=1;
 #define POUC_UPD(x,l) do{if(ok&&EVP_DigestUpdate(c,(x),(l))!=1)ok=0;}while(0)
 if(EVP_DigestInit_ex(c,EVP_sha3_256(),NULL)!=1)ok=0; const char dom[]="QRX/POUC/RECEIPT/V1"; POUC_UPD(dom,sizeof(dom)-1); POUC_UPD(r->graph_commitment,64); POUC_UPD(&r->node_id,sizeof(r->node_id)); POUC_UPD(r->provider_id,strlen(r->provider_id)); POUC_UPD(r->input_commitment,64); POUC_UPD(r->model_commitment,64); POUC_UPD(r->runtime_id,strlen(r->runtime_id)); POUC_UPD(r->execution_params_commitment,64); POUC_UPD(r->result_commitment,64); POUC_UPD(&r->verified_compute_atoms,sizeof(r->verified_compute_atoms)); POUC_UPD(&r->started_height,sizeof(r->started_height)); POUC_UPD(&r->completed_height,sizeof(r->completed_height)); POUC_UPD(&r->verification_mode,sizeof(r->verification_mode)); if(ok&&EVP_DigestFinal_ex(c,h,&n)!=1)ok=0;EVP_MD_CTX_free(c); if(!ok||n!=32)return -1; static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;
 #undef POUC_UPD
}
int qrx_pouc_verify_result(const QrxPoucReceipt *r,uint32_t vc,uint32_t mc,QrxPoucVerification *o){
 if(qrx_pouc_receipt_validate(r)||!o||mc>vc)return -1; memset(o,0,sizeof(*o)); if(qrx_pouc_receipt_commitment(r,o->receipt_commitment))return -1;o->verifier_count=vc;o->matching_verifiers=mc;
 switch(r->verification_mode){case QRX_POUC_VERIFY_SPOT:o->challenge_required=1;o->accepted=(vc>=1&&mc>=1);break;case QRX_POUC_VERIFY_RANDOM:o->challenge_required=1;o->accepted=(vc>=2&&mc>=1);break;case QRX_POUC_VERIFY_REDUNDANT_2_OF_3:o->challenge_required=0;o->accepted=(vc>=3&&mc>=2);break;default:return -1;}return 0;
}

/* ---- 0.0.9.8 AURA Agent Runtime & Tool API --------------------------- */
static int qrx_aura_tool_requires_cap(QrxAuraToolType t, uint32_t caps) {
    switch(t) {
        case QRX_AURA_TOOL_MODEL_INFERENCE: return (caps & QRX_AURA_CAP_MODEL_INFERENCE) ? 0 : -1;
        case QRX_AURA_TOOL_READ_ARTIFACT: return (caps & QRX_AURA_CAP_READ_ARTIFACT) ? 0 : -1;
        case QRX_AURA_TOOL_WRITE_ARTIFACT: return (caps & QRX_AURA_CAP_WRITE_ARTIFACT) ? 0 : -1;
        case QRX_AURA_TOOL_SUBMIT_JOB: return (caps & QRX_AURA_CAP_SUBMIT_JOB) ? 0 : -1;
        case QRX_AURA_TOOL_JOB_STATUS: return (caps & QRX_AURA_CAP_SUBMIT_JOB) ? 0 : -1;
        case QRX_AURA_TOOL_STREAM_RESULT: return (caps & QRX_AURA_CAP_STREAM_RESULT) ? 0 : -1;
        case QRX_AURA_TOOL_RESEARCH: return (caps & QRX_AURA_CAP_RESEARCH) ? 0 : -1;
        case QRX_AURA_TOOL_CODE_GENERATION:
        case QRX_AURA_TOOL_CODE_EXECUTION:
        case QRX_AURA_TOOL_COMPILE:
        case QRX_AURA_TOOL_TEST: return (caps & QRX_AURA_CAP_CODE) ? 0 : -1;
        default: return -1;
    }
}
const char *qrx_aura_tool_type_name(QrxAuraToolType t) {
    switch(t) {
        case QRX_AURA_TOOL_MODEL_INFERENCE:return "MODEL_INFERENCE";
        case QRX_AURA_TOOL_READ_ARTIFACT:return "READ_ARTIFACT";
        case QRX_AURA_TOOL_WRITE_ARTIFACT:return "WRITE_ARTIFACT";
        case QRX_AURA_TOOL_SUBMIT_JOB:return "SUBMIT_JOB";
        case QRX_AURA_TOOL_JOB_STATUS:return "JOB_STATUS";
        case QRX_AURA_TOOL_STREAM_RESULT:return "STREAM_RESULT";
        case QRX_AURA_TOOL_RESEARCH:return "RESEARCH";
        case QRX_AURA_TOOL_CODE_GENERATION:return "CODE_GENERATION";
        case QRX_AURA_TOOL_CODE_EXECUTION:return "CODE_EXECUTION";
        case QRX_AURA_TOOL_COMPILE:return "COMPILE";
        case QRX_AURA_TOOL_TEST:return "TEST";
        default:return "UNKNOWN";
    }
}
const char *qrx_aura_session_state_name(QrxAuraSessionState s) {
    switch(s) {
        case QRX_AURA_SESSION_OPEN:return "OPEN";
        case QRX_AURA_SESSION_BUDGET_WARNING:return "BUDGET_WARNING";
        case QRX_AURA_SESSION_PAUSED:return "PAUSED";
        case QRX_AURA_SESSION_COMPLETED:return "COMPLETED";
        case QRX_AURA_SESSION_CANCELLED:return "CANCELLED";
        default:return "UNKNOWN";
    }
}
int qrx_aura_agent_manifest_validate(const QrxAuraAgentManifest *m) {
    if(!m || m->version!=QRX_AURA_RUNTIME_VERSION) return -1;
    if(!nonempty(m->agent_id,sizeof(m->agent_id)) || !nonempty(m->owner,sizeof(m->owner))) return -1;
    if(!nonempty(m->model_id,sizeof(m->model_id)) || !nonempty(m->model_version,sizeof(m->model_version))) return -1;
    if(!m->capability_mask || (m->capability_mask & ~QRX_AURA_CAP_ALLOWED_V1)) return -1;
    if(!(m->capability_mask & QRX_AURA_CAP_CHAT) || !(m->capability_mask & QRX_AURA_CAP_MODEL_INFERENCE)) return -1;
    if(!m->max_steps || m->max_steps>QRX_AURA_MAX_STEPS || !m->max_job_fee_atoms || !m->max_session_fee_atoms || m->max_job_fee_atoms>m->max_session_fee_atoms) return -1;
    if(m->require_user_approval_for_code_execution>1) return -1;
    return 0;
}
int qrx_aura_agent_manifest_commitment(const QrxAuraAgentManifest *m,char out[65]) {
    if(!out || qrx_aura_agent_manifest_validate(m)) return -1;
    EVP_MD_CTX *c=EVP_MD_CTX_new(); unsigned char h[32]; unsigned int hn=0; int ok=1;
    const char dom[]="QRX/AURA/AGENT-MANIFEST/V1";
    if(!c || EVP_DigestInit_ex(c,EVP_sha3_256(),NULL)!=1) ok=0;
#define AURA_F(x) do{if(ok && qrx_digest_field(c,(x),strlen(x)))ok=0;}while(0)
    if(ok && EVP_DigestUpdate(c,dom,sizeof(dom)-1)!=1) ok=0;
    AURA_F(m->agent_id); AURA_F(m->owner); AURA_F(m->model_id); AURA_F(m->model_version);
#undef AURA_F
    if(ok && (qrx_job_digest_u32(c,m->version)||qrx_job_digest_u32(c,m->capability_mask)||qrx_job_digest_u32(c,m->max_steps)||qrx_job_digest_u64(c,m->max_job_fee_atoms)||qrx_job_digest_u64(c,m->max_session_fee_atoms)||qrx_job_digest_u32(c,(uint32_t)m->require_user_approval_for_code_execution))) ok=0;
    if(ok && EVP_DigestFinal_ex(c,h,&hn)!=1) ok=0; EVP_MD_CTX_free(c); if(!ok||hn!=32)return -1;
    static const char hx[]="0123456789abcdef"; for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];} out[64]=0; return 0;
}
int qrx_aura_session_open(const QrxAuraAgentManifest *m,const char *session_id,uint64_t authorized,QrxAuraSession *o) {
    if(!o||!session_id||!session_id[0]||strlen(session_id)>=QRX_AURA_MAX_ID||qrx_aura_agent_manifest_validate(m)||!authorized||authorized>m->max_session_fee_atoms) return -1;
    memset(o,0,sizeof(*o)); o->version=QRX_AURA_RUNTIME_VERSION; snprintf(o->session_id,sizeof(o->session_id),"%s",session_id); snprintf(o->owner,sizeof(o->owner),"%s",m->owner);
    if(qrx_aura_agent_manifest_commitment(m,o->agent_commitment)) return -1; o->authorized_atoms=authorized; o->state=QRX_AURA_SESSION_OPEN; return 0;
}
int qrx_aura_session_charge(QrxAuraSession *s,uint64_t total) {
    if(!s||s->version!=QRX_AURA_RUNTIME_VERSION||s->state==QRX_AURA_SESSION_COMPLETED||s->state==QRX_AURA_SESSION_CANCELLED||total<s->spent_atoms) return -1;
    if(total>s->authorized_atoms) return -2;
    s->spent_atoms=total;
    if(total==s->authorized_atoms) s->state=QRX_AURA_SESSION_PAUSED;
    else if(total*10000ULL/s->authorized_atoms>=QRX_BUDGET_WARNING_BPS) s->state=QRX_AURA_SESSION_BUDGET_WARNING;
    else s->state=QRX_AURA_SESSION_OPEN;
    return 0;
}
int qrx_aura_session_record_step(const QrxAuraAgentManifest *m,QrxAuraSession *s) { if(qrx_aura_agent_manifest_validate(m)||!s||s->state==QRX_AURA_SESSION_COMPLETED||s->state==QRX_AURA_SESSION_CANCELLED||s->state==QRX_AURA_SESSION_PAUSED||strcmp(s->owner,m->owner)||s->step_count>=m->max_steps)return -1; s->step_count++; return 0; }
int qrx_aura_session_complete(QrxAuraSession *s) { if(!s||s->state==QRX_AURA_SESSION_PAUSED||s->state==QRX_AURA_SESSION_CANCELLED||s->state==QRX_AURA_SESSION_COMPLETED)return -1; s->state=QRX_AURA_SESSION_COMPLETED; return 0; }
int qrx_aura_session_cancel(QrxAuraSession *s) { if(!s||s->state==QRX_AURA_SESSION_COMPLETED||s->state==QRX_AURA_SESSION_CANCELLED)return -1; s->state=QRX_AURA_SESSION_CANCELLED; return 0; }

int qrx_aura_tool_call_validate(const QrxAuraAgentManifest *m,const QrxAuraSession *s,const QrxAuraToolCall *c) {
    if(qrx_aura_agent_manifest_validate(m)||!s||!c||s->version!=QRX_AURA_RUNTIME_VERSION||c->version!=QRX_AURA_RUNTIME_VERSION) return -1;
    if(s->state!=QRX_AURA_SESSION_OPEN && s->state!=QRX_AURA_SESSION_BUDGET_WARNING) return -1;
    if(s->step_count>=m->max_steps) return -5;
    if(strcmp(s->owner,m->owner)||strcmp(c->session_id,s->session_id)) return -1;
    char ac[65]; if(qrx_aura_agent_manifest_commitment(m,ac)||strcmp(ac,s->agent_commitment)) return -1;
    if(!nonempty(c->call_id,sizeof(c->call_id))||!qrx_job_ref_valid(c->input_ref,sizeof(c->input_ref))) return -1;
    if(!nonempty(c->runtime_id,sizeof(c->runtime_id))||!c->requested_fee_atoms||!c->max_runtime_ms||!c->max_output_bytes) return -1;
    if(c->requested_fee_atoms>m->max_job_fee_atoms||c->requested_fee_atoms>s->authorized_atoms-s->spent_atoms) return -2;
    if(qrx_aura_tool_requires_cap(c->tool_type,m->capability_mask)) return -3;
    if((c->tool_type==QRX_AURA_TOOL_CODE_EXECUTION||c->tool_type==QRX_AURA_TOOL_COMPILE||c->tool_type==QRX_AURA_TOOL_TEST) && m->require_user_approval_for_code_execution && !c->user_approved) return -4;
    if(c->user_approved>1) return -1;
    /* Tools that create artifacts must name an output reference. */
    if(c->tool_type==QRX_AURA_TOOL_MODEL_INFERENCE||c->tool_type==QRX_AURA_TOOL_WRITE_ARTIFACT||c->tool_type==QRX_AURA_TOOL_RESEARCH||c->tool_type==QRX_AURA_TOOL_CODE_GENERATION||c->tool_type==QRX_AURA_TOOL_CODE_EXECUTION||c->tool_type==QRX_AURA_TOOL_COMPILE||c->tool_type==QRX_AURA_TOOL_TEST) {
        if(!qrx_job_ref_valid(c->output_ref,sizeof(c->output_ref))) return -1;
    }
    return 0;
}
int qrx_aura_tool_call_commitment(const QrxAuraToolCall *c,char out[65]) {
    if(!c||!out||c->version!=QRX_AURA_RUNTIME_VERSION||!nonempty(c->call_id,sizeof(c->call_id))||!nonempty(c->session_id,sizeof(c->session_id))) return -1;
    EVP_MD_CTX *x=EVP_MD_CTX_new(); unsigned char h[32]; unsigned int hn=0; int ok=1; const char dom[]="QRX/AURA/TOOL-CALL/V1";
    if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0; if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;
#define TC_F(v) do{if(ok&&qrx_digest_field(x,(v),strlen(v)))ok=0;}while(0)
    TC_F(c->call_id);TC_F(c->session_id);TC_F(c->input_ref);TC_F(c->output_ref);TC_F(c->runtime_id);
#undef TC_F
    if(ok&&(qrx_job_digest_u32(x,c->version)||qrx_job_digest_u32(x,(uint32_t)c->tool_type)||qrx_job_digest_u64(x,c->requested_fee_atoms)||qrx_job_digest_u64(x,c->max_runtime_ms)||qrx_job_digest_u64(x,c->max_output_bytes)||qrx_job_digest_u32(x,(uint32_t)c->user_approved)))ok=0;
    if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;
}
int qrx_aura_tool_call_to_job_node(const QrxAuraAgentManifest *m,const QrxAuraSession *s,const QrxAuraToolCall *c,uint32_t node_id,QrxComputeJobNode *o) {
    if(!o||!node_id||qrx_aura_tool_call_validate(m,s,c)) return -1;
    memset(o,0,sizeof(*o)); o->node_id=node_id; snprintf(o->runtime_id,sizeof(o->runtime_id),"%s",c->runtime_id); snprintf(o->input_ref,sizeof(o->input_ref),"%s",c->input_ref); snprintf(o->output_ref,sizeof(o->output_ref),"%s",c->output_ref[0]?c->output_ref:c->input_ref);
    o->max_runtime_ms=c->max_runtime_ms; o->max_output_bytes=c->max_output_bytes; o->max_fee_atoms=c->requested_fee_atoms; o->capability_mask=QRX_JOB_CAP_READ_INPUTS;
    switch(c->tool_type) {
        case QRX_AURA_TOOL_MODEL_INFERENCE: o->job_type=QRX_JOB_AI_INFERENCE; o->capability_mask|=QRX_JOB_CAP_MODEL_INFERENCE|QRX_JOB_CAP_STREAM_OUTPUT|QRX_JOB_CAP_WRITE_ARTIFACTS; break;
        case QRX_AURA_TOOL_RESEARCH: o->job_type=QRX_JOB_RESEARCH; o->capability_mask|=QRX_JOB_CAP_MODEL_INFERENCE|QRX_JOB_CAP_STREAM_OUTPUT|QRX_JOB_CAP_WRITE_ARTIFACTS; break;
        case QRX_AURA_TOOL_CODE_GENERATION: o->job_type=QRX_JOB_CODE_GENERATION; o->capability_mask|=QRX_JOB_CAP_MODEL_INFERENCE|QRX_JOB_CAP_STREAM_OUTPUT|QRX_JOB_CAP_WRITE_ARTIFACTS; break;
        case QRX_AURA_TOOL_CODE_EXECUTION: o->job_type=QRX_JOB_CODE_EXECUTION; o->capability_mask|=QRX_JOB_CAP_SANDBOX_EXEC|QRX_JOB_CAP_WRITE_ARTIFACTS; break;
        case QRX_AURA_TOOL_COMPILE: o->job_type=QRX_JOB_COMPILE; o->capability_mask|=QRX_JOB_CAP_SANDBOX_EXEC|QRX_JOB_CAP_WRITE_ARTIFACTS; break;
        case QRX_AURA_TOOL_TEST: o->job_type=QRX_JOB_TEST; o->capability_mask|=QRX_JOB_CAP_SANDBOX_EXEC|QRX_JOB_CAP_WRITE_ARTIFACTS; break;
        default: return -2; /* non-compute metadata/tools are handled by the client/runtime, not provider execution */
    }
    if(o->job_type==QRX_JOB_AI_INFERENCE||o->job_type==QRX_JOB_RESEARCH||o->job_type==QRX_JOB_CODE_GENERATION){snprintf(o->model_id,sizeof(o->model_id),"%s",m->model_id);snprintf(o->model_version,sizeof(o->model_version),"%s",m->model_version);}
    return qrx_compute_job_node_validate(o);
}

/* 0.0.9.10 AURA Chat Protocol Foundation */
int qrx_aura_conversation_validate(const QrxAuraConversation *c) {
    if(!c||c->version!=QRX_AURA_CHAT_VERSION) return -1;
    if(!nonempty(c->conversation_id,sizeof(c->conversation_id))||!nonempty(c->owner,sizeof(c->owner))||!nonempty(c->session_id,sizeof(c->session_id))) return -1;
    if(c->message_count>QRX_AURA_CHAT_MAX_MESSAGES||!c->max_conversation_fee_atoms||c->spent_atoms>c->max_conversation_fee_atoms||c->encrypted_context!=1) return -1;
    if(c->context_ref[0]&&!qrx_job_ref_valid(c->context_ref,sizeof(c->context_ref))) return -1;
    return 0;
}
int qrx_aura_conversation_open(const QrxAuraAgentManifest *m,const QrxAuraSession *s,const char *id,const char *title,const char *ctx,uint64_t max,QrxAuraConversation *o) {
    if(!o||qrx_aura_agent_manifest_validate(m)||!s||s->version!=QRX_AURA_RUNTIME_VERSION||strcmp(s->owner,m->owner)||!id||!id[0]||strlen(id)>=QRX_AURA_MAX_ID||!max||max>s->authorized_atoms-s->spent_atoms) return -1;
    if(ctx&&ctx[0]&&!qrx_job_ref_valid(ctx,QRX_AURA_MAX_REF)) return -1;
    memset(o,0,sizeof(*o));o->version=QRX_AURA_CHAT_VERSION;snprintf(o->conversation_id,sizeof(o->conversation_id),"%s",id);snprintf(o->owner,sizeof(o->owner),"%s",m->owner);snprintf(o->session_id,sizeof(o->session_id),"%s",s->session_id);if(title)snprintf(o->title,sizeof(o->title),"%s",title);if(ctx)snprintf(o->context_ref,sizeof(o->context_ref),"%s",ctx);o->max_conversation_fee_atoms=max;o->encrypted_context=1;return qrx_aura_conversation_validate(o);
}
int qrx_aura_chat_message_validate(const QrxAuraConversation *c,const QrxAuraChatMessage *m) {
    if(qrx_aura_conversation_validate(c)||!m||m->version!=QRX_AURA_CHAT_VERSION||strcmp(m->conversation_id,c->conversation_id)||!nonempty(m->message_id,sizeof(m->message_id))) return -1;
    if(m->role<QRX_AURA_CHAT_ROLE_SYSTEM||m->role>QRX_AURA_CHAT_ROLE_TOOL||m->state<QRX_AURA_CHAT_MSG_PENDING||m->state>QRX_AURA_CHAT_MSG_FAILED) return -1;
    if(!qrx_job_ref_valid(m->content_ref,sizeof(m->content_ref))||m->artifact_count>QRX_AURA_CHAT_MAX_ARTIFACTS||m->verified_fee_atoms>m->authorized_fee_atoms) return -1;
    for(uint32_t i=0;i<m->artifact_count;i++)if(!qrx_job_ref_valid(m->artifact_refs[i],QRX_AURA_MAX_REF))return -1;
    return 0;
}
int qrx_aura_chat_message_commitment(const QrxAuraChatMessage *m,char out[65]) {
    if(!m||!out||m->version!=QRX_AURA_CHAT_VERSION||!nonempty(m->message_id,sizeof(m->message_id)))return -1;EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/AURA/CHAT-MESSAGE/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;
#define CHAT_F(v) do{if(ok&&qrx_digest_field(x,(v),strlen(v)))ok=0;}while(0)
    CHAT_F(m->message_id);CHAT_F(m->conversation_id);CHAT_F(m->parent_message_id);CHAT_F(m->content_ref);CHAT_F(m->job_ref);for(uint32_t i=0;i<m->artifact_count;i++)CHAT_F(m->artifact_refs[i]);
#undef CHAT_F
    if(ok&&(qrx_job_digest_u32(x,(uint32_t)m->role)||qrx_job_digest_u32(x,(uint32_t)m->state)||qrx_job_digest_u32(x,m->artifact_count)||qrx_job_digest_u64(x,m->authorized_fee_atoms)||qrx_job_digest_u64(x,m->verified_fee_atoms)||qrx_job_digest_u64(x,m->stream_sequence)))ok=0;if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;
}
int qrx_aura_chat_begin_assistant(const QrxAuraConversation*c,const char*mid,const char*parent,const char*in,const char*out,uint64_t fee,QrxAuraToolCall*call,QrxAuraChatMessage*m){if(!call||!m||qrx_aura_conversation_validate(c)||!mid||!mid[0]||!in||!out||!fee||fee>c->max_conversation_fee_atoms-c->spent_atoms)return -1;memset(call,0,sizeof(*call));call->version=QRX_AURA_RUNTIME_VERSION;snprintf(call->call_id,sizeof(call->call_id),"chat:%s",mid);snprintf(call->session_id,sizeof(call->session_id),"%s",c->session_id);call->tool_type=QRX_AURA_TOOL_MODEL_INFERENCE;snprintf(call->input_ref,sizeof(call->input_ref),"%s",in);snprintf(call->output_ref,sizeof(call->output_ref),"%s",out);snprintf(call->runtime_id,sizeof(call->runtime_id),"qrx-aura-chat-v1");call->requested_fee_atoms=fee;call->max_runtime_ms=300000;call->max_output_bytes=16*1024*1024ULL;memset(m,0,sizeof(*m));m->version=QRX_AURA_CHAT_VERSION;snprintf(m->message_id,sizeof(m->message_id),"%s",mid);snprintf(m->conversation_id,sizeof(m->conversation_id),"%s",c->conversation_id);if(parent)snprintf(m->parent_message_id,sizeof(m->parent_message_id),"%s",parent);m->role=QRX_AURA_CHAT_ROLE_ASSISTANT;m->state=QRX_AURA_CHAT_MSG_PENDING;snprintf(m->content_ref,sizeof(m->content_ref),"%s",out);m->authorized_fee_atoms=fee;return qrx_aura_chat_message_validate(c,m);}
int qrx_aura_chat_stream_advance(QrxAuraChatMessage*m,uint64_t seq){if(!m||(m->state!=QRX_AURA_CHAT_MSG_PENDING&&m->state!=QRX_AURA_CHAT_MSG_STREAMING)||seq<=m->stream_sequence)return -1;m->stream_sequence=seq;m->state=QRX_AURA_CHAT_MSG_STREAMING;return 0;}
int qrx_aura_chat_complete(QrxAuraConversation*c,QrxAuraChatMessage*m,uint64_t fee,const char*job){if(qrx_aura_conversation_validate(c)||!m||(m->state!=QRX_AURA_CHAT_MSG_PENDING&&m->state!=QRX_AURA_CHAT_MSG_STREAMING)||fee>m->authorized_fee_atoms||c->spent_atoms+fee>c->max_conversation_fee_atoms||!job||!job[0]||strlen(job)>=QRX_AURA_CHAT_MAX_JOB_REF)return -1;m->verified_fee_atoms=fee;snprintf(m->job_ref,sizeof(m->job_ref),"%s",job);m->state=QRX_AURA_CHAT_MSG_COMPLETE;c->spent_atoms+=fee;c->message_count++;return 0;}
int qrx_aura_chat_cancel(QrxAuraChatMessage*m){if(!m||(m->state!=QRX_AURA_CHAT_MSG_PENDING&&m->state!=QRX_AURA_CHAT_MSG_STREAMING))return -1;m->state=QRX_AURA_CHAT_MSG_CANCELLED;return 0;}
int qrx_aura_chat_add_artifact(QrxAuraChatMessage*m,const char*r){if(!m||!r||!qrx_job_ref_valid(r,QRX_AURA_MAX_REF)||m->artifact_count>=QRX_AURA_CHAT_MAX_ARTIFACTS)return -1;snprintf(m->artifact_refs[m->artifact_count++],QRX_AURA_MAX_REF,"%s",r);return 0;}
int qrx_aura_chat_branch(const QrxAuraConversation*s,const QrxAuraChatMessage*from,const char*id,QrxAuraConversation*out){if(qrx_aura_conversation_validate(s)||!from||!id||!id[0]||strlen(id)>=QRX_AURA_MAX_ID||!out||strcmp(from->conversation_id,s->conversation_id))return -1;*out=*s;snprintf(out->conversation_id,sizeof(out->conversation_id),"%s",id);out->message_count=0;out->spent_atoms=0;return 0;}


/* 0.0.9.11 AURA Files / Projects / Artifact Creation */
static int qrx_aura_hex64(const char *s) {
    if(!s || strlen(s)!=64) return 0;
    for(size_t i=0;i<64;i++) if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f'))) return 0;
    return 1;
}
static int qrx_aura_relative_path_safe(const char *p) {
    if(!p || !p[0] || strlen(p)>=QRX_AURA_PROJECT_MAX_PATH || p[0]=='/' || p[0]=='\\') return 0;
    if(strstr(p,"..") || strchr(p,'\\') || strchr(p,':')) return 0;
    for(const unsigned char *s=(const unsigned char*)p;*s;s++) if(*s<32) return 0;
    return 1;
}
int qrx_aura_artifact_validate(const QrxAuraArtifact *a) {
    if(!a||a->version!=QRX_AURA_ARTIFACT_VERSION) return -1;
    if(!nonempty(a->artifact_id,sizeof(a->artifact_id))||!nonempty(a->owner,sizeof(a->owner))||!nonempty(a->name,sizeof(a->name))) return -1;
    if(a->type<QRX_AURA_ARTIFACT_TEXT||a->type>QRX_AURA_ARTIFACT_OTHER) return -1;
    if(!qrx_job_ref_valid(a->drive_ref,sizeof(a->drive_ref))||!a->size_bytes||a->private_pq>1||a->immutable!=1) return -1;
    if(a->source_job_ref[0]&&!qrx_job_ref_valid(a->source_job_ref,sizeof(a->source_job_ref))) return -1;
    if(a->content_commitment[0]&&!qrx_aura_hex64(a->content_commitment)) return -1;
    if(a->extension[0]) { if(a->extension[0]!='.'||strlen(a->extension)>=QRX_AURA_ARTIFACT_MAX_EXT) return -1; }
    return 0;
}
int qrx_aura_artifact_commitment(const QrxAuraArtifact *a,char out[65]) {
    if(qrx_aura_artifact_validate(a)||!out) return -1;
    EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/AURA/ARTIFACT/V1";
    if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;
#define AR_F(v) do{if(ok&&qrx_digest_field(x,(v),strlen(v)))ok=0;}while(0)
    AR_F(a->artifact_id);AR_F(a->owner);AR_F(a->name);AR_F(a->mime_type);AR_F(a->extension);AR_F(a->drive_ref);AR_F(a->source_job_ref);AR_F(a->content_commitment);
#undef AR_F
    if(ok&&(qrx_job_digest_u32(x,a->version)||qrx_job_digest_u32(x,(uint32_t)a->type)||qrx_job_digest_u64(x,a->size_bytes)||qrx_job_digest_u32(x,a->private_pq)||qrx_job_digest_u32(x,a->immutable)))ok=0;
    if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;
}
int qrx_aura_project_validate(const QrxAuraProject *p) {
    if(!p||p->version!=QRX_AURA_PROJECT_VERSION||!nonempty(p->project_id,sizeof(p->project_id))||!nonempty(p->owner,sizeof(p->owner))||!nonempty(p->name,sizeof(p->name)))return -1;
    if(p->root_ref[0]&&!qrx_job_ref_valid(p->root_ref,sizeof(p->root_ref)))return -1;if(p->private_pq>1||p->file_count>QRX_AURA_PROJECT_MAX_FILES)return -1;
    for(uint32_t i=0;i<p->file_count;i++){if(!qrx_aura_relative_path_safe(p->files[i].path)||!qrx_aura_hex64(p->files[i].artifact_commitment))return -1;for(uint32_t j=0;j<i;j++)if(!strcmp(p->files[i].path,p->files[j].path))return -1;}
    return 0;
}
int qrx_aura_project_add_file(QrxAuraProject*p,const char*path,const QrxAuraArtifact*a){if(!p||!a||qrx_aura_artifact_validate(a)||!qrx_aura_relative_path_safe(path)||p->file_count>=QRX_AURA_PROJECT_MAX_FILES||strcmp(p->owner,a->owner))return -1;for(uint32_t i=0;i<p->file_count;i++)if(!strcmp(p->files[i].path,path))return -2;char h[65];if(qrx_aura_artifact_commitment(a,h))return -1;snprintf(p->files[p->file_count].path,sizeof(p->files[p->file_count].path),"%s",path);snprintf(p->files[p->file_count].artifact_commitment,65,"%s",h);p->file_count++;return 0;}
int qrx_aura_project_commitment(const QrxAuraProject*p,char out[65]){if(qrx_aura_project_validate(p)||!out)return -1;EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/AURA/PROJECT/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;
#define PR_F(v) do{if(ok&&qrx_digest_field(x,(v),strlen(v)))ok=0;}while(0)
PR_F(p->project_id);PR_F(p->owner);PR_F(p->name);PR_F(p->root_ref);for(uint32_t i=0;i<p->file_count;i++){PR_F(p->files[i].path);PR_F(p->files[i].artifact_commitment);} 
#undef PR_F
if(ok&&(qrx_job_digest_u32(x,p->version)||qrx_job_digest_u32(x,p->file_count)||qrx_job_digest_u32(x,p->private_pq)))ok=0;if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;}
int qrx_aura_artifact_prepare_write_call(const QrxAuraConversation*c,const QrxAuraArtifact*a,const char*call_id,const char*input,uint64_t fee,QrxAuraToolCall*out){if(!out||qrx_aura_conversation_validate(c)||qrx_aura_artifact_validate(a)||strcmp(c->owner,a->owner)||!call_id||!call_id[0]||strlen(call_id)>=QRX_AURA_MAX_ID||!qrx_job_ref_valid(input,QRX_AURA_MAX_REF)||!fee||fee>c->max_conversation_fee_atoms-c->spent_atoms)return -1;memset(out,0,sizeof(*out));out->version=QRX_AURA_RUNTIME_VERSION;snprintf(out->call_id,sizeof(out->call_id),"%s",call_id);snprintf(out->session_id,sizeof(out->session_id),"%s",c->session_id);out->tool_type=QRX_AURA_TOOL_WRITE_ARTIFACT;snprintf(out->input_ref,sizeof(out->input_ref),"%s",input);snprintf(out->output_ref,sizeof(out->output_ref),"%s",a->drive_ref);snprintf(out->runtime_id,sizeof(out->runtime_id),"qrx-aura-artifact-v1");out->requested_fee_atoms=fee;out->max_runtime_ms=300000;out->max_output_bytes=a->size_bytes;return 0;}
int qrx_aura_artifact_attach_to_chat(QrxAuraChatMessage*m,const QrxAuraArtifact*a){if(!m||qrx_aura_artifact_validate(a))return -1;return qrx_aura_chat_add_artifact(m,a->drive_ref);}

/* 0.0.9.12 AURA Code Generation, Build & Test Workspace */
static int qrx_aura_code_one_stage_bit(uint32_t m){return m && !(m&(m-1U)) && !(m&~QRX_AURA_CODE_STAGE_ALLOWED_V1);}
static uint64_t qrx_aura_code_total_stage_fee(const QrxAuraCodeWorkspace*w){uint64_t t=0;for(uint32_t i=0;i<w->stage_count;i++){if(UINT64_MAX-t<w->stages[i].max_fee_atoms)return UINT64_MAX;t+=w->stages[i].max_fee_atoms;}return t;}
int qrx_aura_code_workspace_init(const QrxAuraProject*p,const char*id,const char*src,uint64_t max,QrxAuraCodeWorkspace*out){
    if(!out||qrx_aura_project_validate(p)||!id||!id[0]||strlen(id)>=QRX_AURA_MAX_ID||!src||!qrx_job_ref_valid(src,QRX_AURA_MAX_REF)||!max)return -1;
    memset(out,0,sizeof(*out));out->version=QRX_AURA_CODE_WORKSPACE_VERSION;snprintf(out->workspace_id,sizeof(out->workspace_id),"%s",id);snprintf(out->owner,sizeof(out->owner),"%s",p->owner);snprintf(out->source_project_ref,sizeof(out->source_project_ref),"%s",src);out->max_total_fee_atoms=max;out->state=QRX_AURA_CODE_PLAN_DRAFT;if(qrx_aura_project_commitment(p,out->project_commitment))return -1;return 0;
}
int qrx_aura_code_workspace_validate(const QrxAuraCodeWorkspace*w){
    if(!w||w->version!=QRX_AURA_CODE_WORKSPACE_VERSION||!nonempty(w->workspace_id,sizeof(w->workspace_id))||!nonempty(w->owner,sizeof(w->owner))||!qrx_aura_hex64(w->project_commitment)||!qrx_job_ref_valid(w->source_project_ref,sizeof(w->source_project_ref))||!w->max_total_fee_atoms||w->stage_count>QRX_AURA_CODE_MAX_STAGES||w->user_approved_execution>1)return -1;
    if(w->state<QRX_AURA_CODE_PLAN_DRAFT||w->state>QRX_AURA_CODE_PLAN_CANCELLED)return -1;
    if(w->patch_artifact_ref[0]&&!qrx_job_ref_valid(w->patch_artifact_ref,sizeof(w->patch_artifact_ref)))return -1;if(w->build_log_ref[0]&&!qrx_job_ref_valid(w->build_log_ref,sizeof(w->build_log_ref)))return -1;if(w->test_log_ref[0]&&!qrx_job_ref_valid(w->test_log_ref,sizeof(w->test_log_ref)))return -1;
    for(uint32_t i=0;i<w->stage_count;i++){const QrxAuraCodeStage*s=&w->stages[i];if(s->stage_id!=i+1||!qrx_aura_code_one_stage_bit(s->stage_mask)||!nonempty(s->label,sizeof(s->label))||!qrx_job_ref_valid(s->input_ref,sizeof(s->input_ref))||!qrx_job_ref_valid(s->output_ref,sizeof(s->output_ref))||!nonempty(s->runtime_id,sizeof(s->runtime_id))||!s->max_fee_atoms||!s->max_runtime_ms||!s->max_output_bytes||s->requires_user_approval>1)return -1; if((s->stage_mask&(QRX_AURA_CODE_STAGE_EXECUTE|QRX_AURA_CODE_STAGE_COMPILE|QRX_AURA_CODE_STAGE_TEST))&&!s->requires_user_approval)return -1;}
    if(qrx_aura_code_total_stage_fee(w)>w->max_total_fee_atoms)return -1;return 0;
}
int qrx_aura_code_workspace_add_stage(QrxAuraCodeWorkspace*w,uint32_t mask,const char*label,const char*in,const char*out,const char*rt,uint64_t fee,uint64_t ms,uint64_t bytes){
    if(!w||w->state!=QRX_AURA_CODE_PLAN_DRAFT||w->stage_count>=QRX_AURA_CODE_MAX_STAGES||!qrx_aura_code_one_stage_bit(mask)||!label||!label[0]||strlen(label)>=QRX_AURA_CODE_MAX_LABEL||!qrx_job_ref_valid(in,QRX_AURA_MAX_REF)||!qrx_job_ref_valid(out,QRX_AURA_MAX_REF)||!rt||!rt[0]||strlen(rt)>=QRX_AURA_CODE_MAX_RUNTIME||!fee||!ms||!bytes)return -1;uint64_t used=qrx_aura_code_total_stage_fee(w);if(used==UINT64_MAX||fee>w->max_total_fee_atoms||used>w->max_total_fee_atoms-fee)return -2;
    QrxAuraCodeStage*s=&w->stages[w->stage_count];memset(s,0,sizeof(*s));s->stage_id=w->stage_count+1;s->stage_mask=mask;snprintf(s->label,sizeof(s->label),"%s",label);snprintf(s->input_ref,sizeof(s->input_ref),"%s",in);snprintf(s->output_ref,sizeof(s->output_ref),"%s",out);snprintf(s->runtime_id,sizeof(s->runtime_id),"%s",rt);s->max_fee_atoms=fee;s->max_runtime_ms=ms;s->max_output_bytes=bytes;s->requires_user_approval=(mask&(QRX_AURA_CODE_STAGE_EXECUTE|QRX_AURA_CODE_STAGE_COMPILE|QRX_AURA_CODE_STAGE_TEST))?1:0;w->stage_count++;return qrx_aura_code_workspace_validate(w);
}
int qrx_aura_code_workspace_approve(QrxAuraCodeWorkspace*w,uint8_t approve){if(qrx_aura_code_workspace_validate(w)||w->state!=QRX_AURA_CODE_PLAN_DRAFT||approve>1||!w->stage_count)return -1;for(uint32_t i=0;i<w->stage_count;i++)if(w->stages[i].requires_user_approval&&!approve)return -2;w->user_approved_execution=approve;w->state=QRX_AURA_CODE_PLAN_APPROVED;return 0;}
int qrx_aura_code_workspace_commitment(const QrxAuraCodeWorkspace*w,char out[65]){if(qrx_aura_code_workspace_validate(w)||!out)return -1;EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/AURA/CODE-WORKSPACE/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;
#define CW_F(v) do{if(ok&&qrx_digest_field(x,(v),strlen(v)))ok=0;}while(0)
CW_F(w->workspace_id);CW_F(w->owner);CW_F(w->project_commitment);CW_F(w->source_project_ref);CW_F(w->patch_artifact_ref);CW_F(w->build_log_ref);CW_F(w->test_log_ref);for(uint32_t i=0;i<w->stage_count;i++){const QrxAuraCodeStage*s=&w->stages[i];CW_F(s->label);CW_F(s->input_ref);CW_F(s->output_ref);CW_F(s->runtime_id);if(ok&&(qrx_job_digest_u32(x,s->stage_id)||qrx_job_digest_u32(x,s->stage_mask)||qrx_job_digest_u64(x,s->max_fee_atoms)||qrx_job_digest_u64(x,s->max_runtime_ms)||qrx_job_digest_u64(x,s->max_output_bytes)||qrx_job_digest_u32(x,s->requires_user_approval)))ok=0;}
#undef CW_F
if(ok&&(qrx_job_digest_u32(x,w->version)||qrx_job_digest_u64(x,w->max_total_fee_atoms)||qrx_job_digest_u32(x,w->stage_count)||qrx_job_digest_u32(x,(uint32_t)w->state)||qrx_job_digest_u32(x,w->user_approved_execution)))ok=0;if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;}
int qrx_aura_code_stage_prepare_tool_call(const QrxAuraCodeWorkspace*w,const QrxAuraConversation*c,uint32_t idx,const char*call_id,QrxAuraToolCall*out){if(!out||qrx_aura_code_workspace_validate(w)||qrx_aura_conversation_validate(c)||strcmp(w->owner,c->owner)||w->state!=QRX_AURA_CODE_PLAN_APPROVED||idx>=w->stage_count||!call_id||!call_id[0]||strlen(call_id)>=QRX_AURA_MAX_ID)return -1;const QrxAuraCodeStage*s=&w->stages[idx];memset(out,0,sizeof(*out));out->version=QRX_AURA_RUNTIME_VERSION;snprintf(out->call_id,sizeof(out->call_id),"%s",call_id);snprintf(out->session_id,sizeof(out->session_id),"%s",c->session_id);snprintf(out->input_ref,sizeof(out->input_ref),"%s",s->input_ref);snprintf(out->output_ref,sizeof(out->output_ref),"%s",s->output_ref);snprintf(out->runtime_id,sizeof(out->runtime_id),"%s",s->runtime_id);out->requested_fee_atoms=s->max_fee_atoms;out->max_runtime_ms=s->max_runtime_ms;out->max_output_bytes=s->max_output_bytes;out->user_approved=s->requires_user_approval?w->user_approved_execution:0;switch(s->stage_mask){case QRX_AURA_CODE_STAGE_GENERATE:out->tool_type=QRX_AURA_TOOL_CODE_GENERATION;break;case QRX_AURA_CODE_STAGE_EXECUTE:out->tool_type=QRX_AURA_TOOL_CODE_EXECUTION;break;case QRX_AURA_CODE_STAGE_COMPILE:out->tool_type=QRX_AURA_TOOL_COMPILE;break;case QRX_AURA_CODE_STAGE_TEST:out->tool_type=QRX_AURA_TOOL_TEST;break;default:return -1;}return 0;}
const char*qrx_aura_code_plan_state_name(QrxAuraCodePlanState s){switch(s){case QRX_AURA_CODE_PLAN_DRAFT:return "DRAFT";case QRX_AURA_CODE_PLAN_APPROVED:return "APPROVED";case QRX_AURA_CODE_PLAN_RUNNING:return "RUNNING";case QRX_AURA_CODE_PLAN_COMPLETED:return "COMPLETED";case QRX_AURA_CODE_PLAN_FAILED:return "FAILED";case QRX_AURA_CODE_PLAN_CANCELLED:return "CANCELLED";default:return "UNKNOWN";}}


/* 0.0.9.13 Workspace Execution Lifecycle / Patch-Diff / Build-Test Results */
static int qrx_aura_exec_ref_optional(const char *s){return !s||!s[0]||qrx_job_ref_valid(s,QRX_AURA_MAX_REF);}
static int qrx_aura_exec_result_commitment_optional(const char *s){return !s||!s[0]||qrx_aura_hex64(s);}
const char *qrx_aura_code_stage_status_name(QrxAuraCodeStageStatus s){switch(s){case QRX_AURA_CODE_STAGE_PENDING:return "PENDING";case QRX_AURA_CODE_STAGE_RUNNING:return "RUNNING";case QRX_AURA_CODE_STAGE_PASSED:return "PASSED";case QRX_AURA_CODE_STAGE_FAILED:return "FAILED";case QRX_AURA_CODE_STAGE_CANCELLED:return "CANCELLED";default:return "UNKNOWN";}}
int qrx_aura_code_execution_init(const QrxAuraCodeWorkspace*w,const char*id,QrxAuraCodeExecution*out){
    if(!out||qrx_aura_code_workspace_validate(w)||w->state!=QRX_AURA_CODE_PLAN_APPROVED||!id||!id[0]||strlen(id)>=QRX_AURA_MAX_ID)return -1;
    memset(out,0,sizeof(*out));out->version=QRX_AURA_CODE_EXECUTION_VERSION;snprintf(out->execution_id,sizeof(out->execution_id),"%s",id);snprintf(out->owner,sizeof(out->owner),"%s",w->owner);out->stage_count=w->stage_count;out->state=QRX_AURA_CODE_PLAN_RUNNING;
    if(qrx_aura_code_workspace_commitment(w,out->workspace_commitment))return -1;
    for(uint32_t i=0;i<out->stage_count;i++){out->results[i].stage_id=i+1;out->results[i].status=QRX_AURA_CODE_STAGE_PENDING;}
    return 0;
}
int qrx_aura_code_execution_validate(const QrxAuraCodeWorkspace*w,const QrxAuraCodeExecution*e){
    if(!w||!e||qrx_aura_code_workspace_validate(w)||e->version!=QRX_AURA_CODE_EXECUTION_VERSION||!nonempty(e->execution_id,sizeof(e->execution_id))||strcmp(e->owner,w->owner)||!qrx_aura_hex64(e->workspace_commitment)||e->stage_count!=w->stage_count||e->stage_count>QRX_AURA_CODE_MAX_STAGES||e->current_stage_index>e->stage_count||e->final_user_approved>1)return -1;
    char wh[65];if(qrx_aura_code_workspace_commitment(w,wh)||strcmp(wh,e->workspace_commitment))return -1;
    if(e->state!=QRX_AURA_CODE_PLAN_RUNNING&&e->state!=QRX_AURA_CODE_PLAN_COMPLETED&&e->state!=QRX_AURA_CODE_PLAN_FAILED&&e->state!=QRX_AURA_CODE_PLAN_CANCELLED)return -1;
    if(!qrx_aura_exec_ref_optional(e->patch_artifact_ref)||!qrx_aura_exec_ref_optional(e->diff_artifact_ref)||!qrx_aura_exec_ref_optional(e->build_report_ref)||!qrx_aura_exec_ref_optional(e->test_report_ref))return -1;
    uint64_t sum=0;uint32_t running=0;
    for(uint32_t i=0;i<e->stage_count;i++){const QrxAuraCodeStageResult*r=&e->results[i];if(r->stage_id!=i+1||r->status<QRX_AURA_CODE_STAGE_PENDING||r->status>QRX_AURA_CODE_STAGE_CANCELLED||!qrx_aura_exec_ref_optional(r->output_ref)||!qrx_aura_exec_ref_optional(r->log_ref)||!qrx_aura_exec_result_commitment_optional(r->result_commitment)||!nonempty(r->summary,sizeof(r->summary))&&r->status>=QRX_AURA_CODE_STAGE_PASSED)return -1;if(r->status==QRX_AURA_CODE_STAGE_RUNNING)running++;if(r->charged_fee_atoms>w->stages[i].max_fee_atoms)return -1;if(UINT64_MAX-sum<r->charged_fee_atoms)return -1;sum+=r->charged_fee_atoms;if(r->finished_ms&&r->started_ms&&r->finished_ms<r->started_ms)return -1;}
    if(running>1||sum!=e->total_charged_atoms||sum>w->max_total_fee_atoms)return -1;return 0;
}
int qrx_aura_code_execution_start_stage(const QrxAuraCodeWorkspace*w,QrxAuraCodeExecution*e,uint32_t idx,uint64_t started){
    if(!e||qrx_aura_code_execution_validate(w,e)||e->state!=QRX_AURA_CODE_PLAN_RUNNING||idx>=e->stage_count||!started)return -1;if(idx>0&&e->results[idx-1].status!=QRX_AURA_CODE_STAGE_PASSED)return -2;QrxAuraCodeStageResult*r=&e->results[idx];if(r->status!=QRX_AURA_CODE_STAGE_PENDING)return -3;r->status=QRX_AURA_CODE_STAGE_RUNNING;r->started_ms=started;e->current_stage_index=idx;return 0;
}
int qrx_aura_code_execution_record_result(const QrxAuraCodeWorkspace*w,QrxAuraCodeExecution*e,uint32_t idx,QrxAuraCodeStageStatus st,const char*outref,const char*logref,const char*commit,const char*summary,int32_t exit_code,uint64_t fee,uint64_t finished){
    if(!e||qrx_aura_code_execution_validate(w,e)||idx>=e->stage_count||(st!=QRX_AURA_CODE_STAGE_PASSED&&st!=QRX_AURA_CODE_STAGE_FAILED&&st!=QRX_AURA_CODE_STAGE_CANCELLED)||!finished||!summary||!summary[0]||strlen(summary)>=QRX_AURA_CODE_RESULT_MAX_SUMMARY||!qrx_aura_exec_ref_optional(outref)||!qrx_aura_exec_ref_optional(logref)||!qrx_aura_exec_result_commitment_optional(commit)||fee>w->stages[idx].max_fee_atoms)return -1;QrxAuraCodeStageResult*r=&e->results[idx];if(r->status!=QRX_AURA_CODE_STAGE_RUNNING||finished<r->started_ms)return -2;if(fee>w->max_total_fee_atoms-e->total_charged_atoms)return -3;r->status=st;r->exit_code=exit_code;r->charged_fee_atoms=fee;r->finished_ms=finished;snprintf(r->output_ref,sizeof(r->output_ref),"%s",outref?outref:"");snprintf(r->log_ref,sizeof(r->log_ref),"%s",logref?logref:"");snprintf(r->result_commitment,sizeof(r->result_commitment),"%s",commit?commit:"");snprintf(r->summary,sizeof(r->summary),"%s",summary);e->total_charged_atoms+=fee;if(st==QRX_AURA_CODE_STAGE_FAILED)e->state=QRX_AURA_CODE_PLAN_FAILED;else if(st==QRX_AURA_CODE_STAGE_CANCELLED)e->state=QRX_AURA_CODE_PLAN_CANCELLED;else if(idx+1==e->stage_count)e->current_stage_index=e->stage_count;return 0;
}
int qrx_aura_code_execution_set_reports(QrxAuraCodeExecution*e,const char*p,const char*d,const char*b,const char*t){if(!e||!qrx_aura_exec_ref_optional(p)||!qrx_aura_exec_ref_optional(d)||!qrx_aura_exec_ref_optional(b)||!qrx_aura_exec_ref_optional(t))return -1;snprintf(e->patch_artifact_ref,sizeof(e->patch_artifact_ref),"%s",p?p:"");snprintf(e->diff_artifact_ref,sizeof(e->diff_artifact_ref),"%s",d?d:"");snprintf(e->build_report_ref,sizeof(e->build_report_ref),"%s",b?b:"");snprintf(e->test_report_ref,sizeof(e->test_report_ref),"%s",t?t:"");return 0;}
int qrx_aura_code_execution_finalize(const QrxAuraCodeWorkspace*w,QrxAuraCodeExecution*e){if(!e||qrx_aura_code_execution_validate(w,e)||e->state!=QRX_AURA_CODE_PLAN_RUNNING)return -1;for(uint32_t i=0;i<e->stage_count;i++)if(e->results[i].status!=QRX_AURA_CODE_STAGE_PASSED)return -2;if(!e->patch_artifact_ref[0]||!e->diff_artifact_ref[0]||!e->build_report_ref[0]||!e->test_report_ref[0])return -3;e->state=QRX_AURA_CODE_PLAN_COMPLETED;e->current_stage_index=e->stage_count;return 0;}
int qrx_aura_code_execution_user_approve(QrxAuraCodeExecution*e,uint8_t approve){if(!e||approve>1||e->state!=QRX_AURA_CODE_PLAN_COMPLETED)return -1;e->final_user_approved=approve;return 0;}
int qrx_aura_code_execution_commitment(const QrxAuraCodeWorkspace*w,const QrxAuraCodeExecution*e,char out[65]){if(!out||qrx_aura_code_execution_validate(w,e))return -1;EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/AURA/CODE-EXECUTION/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;
#define CE_F(v) do{if(ok&&qrx_digest_field(x,(v),strlen(v)))ok=0;}while(0)
CE_F(e->execution_id);CE_F(e->workspace_commitment);CE_F(e->owner);CE_F(e->patch_artifact_ref);CE_F(e->diff_artifact_ref);CE_F(e->build_report_ref);CE_F(e->test_report_ref);for(uint32_t i=0;i<e->stage_count;i++){const QrxAuraCodeStageResult*r=&e->results[i];CE_F(r->output_ref);CE_F(r->log_ref);CE_F(r->result_commitment);CE_F(r->summary);if(ok&&(qrx_job_digest_u32(x,r->stage_id)||qrx_job_digest_u32(x,(uint32_t)r->status)||qrx_job_digest_u32(x,(uint32_t)r->exit_code)||qrx_job_digest_u64(x,r->charged_fee_atoms)||qrx_job_digest_u64(x,r->started_ms)||qrx_job_digest_u64(x,r->finished_ms)))ok=0;}
#undef CE_F
if(ok&&(qrx_job_digest_u32(x,e->version)||qrx_job_digest_u32(x,e->current_stage_index)||qrx_job_digest_u32(x,e->stage_count)||qrx_job_digest_u64(x,e->total_charged_atoms)||qrx_job_digest_u32(x,(uint32_t)e->state)||qrx_job_digest_u32(x,e->final_user_approved)))ok=0;if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;}

/* 0.0.9.14 Heterogeneous MoE Pods / Traffic Optimization / Optimistic PoUC */
QrxMoeNodeClass qrx_moe_classify_memory(uint64_t b){uint64_t g=b>>30;if(g<16)return QRX_MOE_NODE_MICRO;if(g<64)return QRX_MOE_NODE_LIGHT;if(g<256)return QRX_MOE_NODE_STANDARD;return QRX_MOE_NODE_HOT;}
int qrx_moe_node_profile_validate(const QrxMoeNodeProfile*p){if(!p||p->version!=QRX_MOE_PLACEMENT_VERSION||!nonempty(p->node_id,sizeof(p->node_id))||!nonempty(p->pod_id,sizeof(p->pod_id))||!nonempty(p->backend,sizeof(p->backend)))return -1;if(p->arch!=QRX_COMPUTE_ARCH_X86_64&&p->arch!=QRX_COMPUTE_ARCH_ARM64)return -1;if(!p->memory_total_bytes||p->memory_available_bytes>p->memory_total_bytes||!p->role_mask||!p->capability_mask)return -1;if(p->node_class!=qrx_moe_classify_memory(p->memory_total_bytes))return -1;if(!p->bandwidth_mbps_to_pod)return -1;return 0;}
int qrx_moe_fragment_validate(const QrxMoeExpertFragment*f){if(!f||f->version!=QRX_MOE_PLACEMENT_VERSION||!nonempty(f->model_id,sizeof(f->model_id))||!nonempty(f->model_version,sizeof(f->model_version))||!qrx_aura_hex64(f->content_root)||!f->size_bytes||!f->fragment_count)return -1;if(f->fragment_id>=f->fragment_count)return -1;return 0;}
int qrx_moe_placement_validate(const QrxMoeNodeProfile*n,const QrxMoeExpertPlacement*p){if(qrx_moe_node_profile_validate(n)||!p||p->version!=QRX_MOE_PLACEMENT_VERSION||strcmp(n->node_id,p->node_id)||strcmp(n->pod_id,p->pod_id)||qrx_moe_fragment_validate(&p->fragment))return -1;if(!(n->role_mask&(QRX_MOE_ROLE_EXPERT|QRX_MOE_ROLE_CACHE)))return -1;if(p->residency<QRX_MOE_RES_REMOTE||p->residency>QRX_MOE_RES_VRAM||p->cache_state<QRX_MOE_CACHE_COLD||p->cache_state>QRX_MOE_CACHE_HOT||p->replica_ordinal>=QRX_MOE_MAX_REPLICAS)return -1;if(p->residency==QRX_MOE_RES_RAM&&p->fragment.size_bytes>n->memory_available_bytes)return -2;if(p->residency==QRX_MOE_RES_VRAM&&p->fragment.size_bytes>n->accelerator_memory_available_bytes)return -2;if(p->residency==QRX_MOE_RES_NVME&&p->fragment.size_bytes>n->nvme_available_bytes)return -2;return 0;}
int qrx_moe_placement_commitment(const QrxMoeExpertPlacement*p,char out[65]){if(!p||!out||p->version!=QRX_MOE_PLACEMENT_VERSION||qrx_moe_fragment_validate(&p->fragment)||!nonempty(p->node_id,sizeof(p->node_id))||!nonempty(p->pod_id,sizeof(p->pod_id)))return -1;EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/MOE/PLACEMENT/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;
#define MP_F(v) do{if(ok&&qrx_digest_field(x,(v),strlen(v)))ok=0;}while(0)
MP_F(p->node_id);MP_F(p->pod_id);MP_F(p->fragment.model_id);MP_F(p->fragment.model_version);MP_F(p->fragment.content_root);
#undef MP_F
if(ok&&(qrx_job_digest_u32(x,p->version)||qrx_job_digest_u32(x,p->fragment.layer_id)||qrx_job_digest_u32(x,p->fragment.expert_id)||qrx_job_digest_u32(x,p->fragment.fragment_id)||qrx_job_digest_u32(x,p->fragment.fragment_count)||qrx_job_digest_u64(x,p->fragment.size_bytes)||qrx_job_digest_u32(x,(uint32_t)p->residency)||qrx_job_digest_u32(x,(uint32_t)p->cache_state)||qrx_job_digest_u32(x,p->replica_ordinal)||qrx_job_digest_u32(x,p->measured_latency_us)))ok=0;if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;}
uint64_t qrx_moe_activation_wire_bytes(uint64_t n,QrxActivationEncoding e){uint64_t bits;switch(e){case QRX_ACT_FP16:case QRX_ACT_BF16:bits=16;break;case QRX_ACT_FP8:case QRX_ACT_INT8:bits=8;break;case QRX_ACT_INT4:bits=4;break;default:return 0;}if(n>UINT64_MAX/bits)return UINT64_MAX;uint64_t total=n*bits;return (total+7)/8;}
uint64_t qrx_moe_placement_score(const QrxMoeNodeProfile*n,const QrxMoeExpertPlacement*p){if(qrx_moe_placement_validate(n,p))return 0;uint64_t s=1000000ULL;uint64_t lat=(uint64_t)n->latency_us_to_pod+p->measured_latency_us;s-=lat>300000?300000:lat;uint64_t bw=n->bandwidth_mbps_to_pod>100000?100000:n->bandwidth_mbps_to_pod;s+=bw*2;if(p->residency==QRX_MOE_RES_VRAM)s+=250000;else if(p->residency==QRX_MOE_RES_RAM)s+=180000;else if(p->residency==QRX_MOE_RES_NVME)s+=70000;if(p->cache_state==QRX_MOE_CACHE_HOT)s+=100000;else if(p->cache_state==QRX_MOE_CACHE_WARM)s+=50000;return s;}
int qrx_pouc_optimistic_policy(QrxPoucOptimisticTier t,uint32_t rel,QrxPoucOptimisticPolicy*out){if(!out||rel>10000)return -1;memset(out,0,sizeof(*out));out->tier=t;switch(t){case QRX_POUC_OPT_FAST:out->challenge_bps=rel>=9900?100:300;out->redundant_workers=1;out->async_verification=1;out->allow_optimistic_forward=1;break;case QRX_POUC_OPT_STANDARD:out->challenge_bps=rel>=9900?200:rel>=9500?500:1000;out->redundant_workers=1;out->async_verification=1;out->allow_optimistic_forward=1;break;case QRX_POUC_OPT_HIGH:out->challenge_bps=rel>=9900?1000:2000;out->redundant_workers=2;out->async_verification=1;out->allow_optimistic_forward=1;break;case QRX_POUC_OPT_CONSENSUS:out->challenge_bps=10000;out->redundant_workers=3;out->async_verification=0;out->allow_optimistic_forward=0;break;default:return -1;}if(out->challenge_bps<QRX_POUC_OPT_MIN_CHALLENGE_BPS)out->challenge_bps=QRX_POUC_OPT_MIN_CHALLENGE_BPS;return 0;}

/* 0.0.9.15 MoE Pod Coordinator / Co-Activation Locality / Predictive Prefetch */
int qrx_moe_coactivation_validate(const QrxMoeCoactivationStat*s){
    if(!s||s->version!=QRX_MOE_COORDINATOR_VERSION||!nonempty(s->model_id,sizeof(s->model_id))||!nonempty(s->model_version,sizeof(s->model_version)))return -1;
    if(s->expert_a==s->expert_b||!s->observation_count||s->coactivation_count>s->observation_count)return -1;
    return 0;
}
uint32_t qrx_moe_coactivation_bps(const QrxMoeCoactivationStat*s){
    if(qrx_moe_coactivation_validate(s))return 0;
    if(s->coactivation_count>UINT64_MAX/10000ULL)return 10000U;
    return (uint32_t)((s->coactivation_count*10000ULL)/s->observation_count);
}
uint64_t qrx_moe_locality_score(const QrxMoeNodeProfile*n,const QrxMoeExpertPlacement*p,uint32_t co_bps){
    if(co_bps>10000||qrx_moe_placement_validate(n,p))return 0;
    uint64_t base=qrx_moe_placement_score(n,p);
    /* Co-activation can add at most 250k points; locality never changes model routing. */
    return base + ((uint64_t)co_bps*25ULL);
}
int qrx_moe_coordinator_request_validate(const QrxMoeCoordinatorRequest*r){
    if(!r||r->version!=QRX_MOE_COORDINATOR_VERSION||!nonempty(r->pod_id,sizeof(r->pod_id)))return -1;
    if(r->next_layer_id<=r->current_layer_id||!r->selected_count||r->selected_count>QRX_MOE_MAX_SELECTED_EXPERTS)return -1;
    if(!r->max_prefetch||r->max_prefetch>QRX_MOE_MAX_PREFETCH||!r->max_prefetch_bytes)return -1;
    if(!qrx_moe_activation_wire_bytes(1,r->activation_encoding))return -1;
    for(uint32_t i=0;i<r->selected_count;i++)for(uint32_t j=i+1;j<r->selected_count;j++)if(r->selected_experts[i]==r->selected_experts[j])return -1;
    return 0;
}
static QrxMoePrefetchAction qrx_moe_prefetch_action(QrxMoeResidency r){
    switch(r){case QRX_MOE_RES_NVME:return QRX_MOE_PREFETCH_NVME_TO_RAM;case QRX_MOE_RES_RAM:return QRX_MOE_PREFETCH_RAM_TO_VRAM;case QRX_MOE_RES_REMOTE:return QRX_MOE_PREFETCH_REMOTE_TO_RAM;case QRX_MOE_RES_VRAM:return QRX_MOE_PREFETCH_NONE;default:return QRX_MOE_PREFETCH_NONE;}
}
int qrx_moe_prefetch_plan_add(const QrxMoeCoordinatorRequest*r,const QrxMoeNodeProfile*n,const QrxMoeExpertPlacement*p,uint32_t predicted_bps,QrxMoePrefetchPlan*plan){
    if(qrx_moe_coordinator_request_validate(r)||!plan||predicted_bps>10000||qrx_moe_placement_validate(n,p))return -1;
    if(strcmp(r->pod_id,p->pod_id)||p->fragment.layer_id!=r->next_layer_id)return -1;
    if(plan->version==0){memset(plan,0,sizeof(*plan));plan->version=QRX_MOE_COORDINATOR_VERSION;snprintf(plan->pod_id,sizeof(plan->pod_id),"%s",r->pod_id);plan->next_layer_id=r->next_layer_id;}
    if(plan->version!=QRX_MOE_COORDINATOR_VERSION||strcmp(plan->pod_id,r->pod_id)||plan->next_layer_id!=r->next_layer_id)return -1;
    if(plan->item_count>=r->max_prefetch||plan->item_count>=QRX_MOE_MAX_PREFETCH)return -2;
    if(plan->total_prefetch_bytes>r->max_prefetch_bytes||p->fragment.size_bytes>r->max_prefetch_bytes-plan->total_prefetch_bytes)return -3;
    for(uint32_t i=0;i<plan->item_count;i++)if(!strcmp(plan->items[i].placement.node_id,p->node_id)&&plan->items[i].placement.fragment.layer_id==p->fragment.layer_id&&plan->items[i].placement.fragment.expert_id==p->fragment.expert_id&&plan->items[i].placement.fragment.fragment_id==p->fragment.fragment_id)return -4;
    QrxMoePrefetchItem*it=&plan->items[plan->item_count++];memset(it,0,sizeof(*it));it->version=QRX_MOE_COORDINATOR_VERSION;it->placement=*p;it->action=qrx_moe_prefetch_action(p->residency);it->predicted_bps=predicted_bps;it->locality_score=qrx_moe_locality_score(n,p,predicted_bps);
    /* Wire estimate covers fetching the fragment only when remote. Resident/NVMe moves are local. */
    it->estimated_wire_bytes=(p->residency==QRX_MOE_RES_REMOTE)?p->fragment.size_bytes:0;
    plan->total_prefetch_bytes+=p->fragment.size_bytes;plan->estimated_wire_bytes+=it->estimated_wire_bytes;return 0;
}
static int qrx_moe_prefetch_cmp(const void*a,const void*b){const QrxMoePrefetchItem*x=(const QrxMoePrefetchItem*)a,*y=(const QrxMoePrefetchItem*)b;if(x->locality_score>y->locality_score)return -1;if(x->locality_score<y->locality_score)return 1;int c=strcmp(x->placement.node_id,y->placement.node_id);if(c)return c;if(x->placement.fragment.expert_id<y->placement.fragment.expert_id)return -1;if(x->placement.fragment.expert_id>y->placement.fragment.expert_id)return 1;if(x->placement.fragment.fragment_id<y->placement.fragment.fragment_id)return -1;if(x->placement.fragment.fragment_id>y->placement.fragment.fragment_id)return 1;return 0;}
int qrx_moe_prefetch_plan_finalize(const QrxMoeCoordinatorRequest*r,QrxMoePrefetchPlan*p){if(qrx_moe_coordinator_request_validate(r)||!p||p->version!=QRX_MOE_COORDINATOR_VERSION||strcmp(p->pod_id,r->pod_id)||p->next_layer_id!=r->next_layer_id||p->item_count>r->max_prefetch||p->total_prefetch_bytes>r->max_prefetch_bytes)return -1;qsort(p->items,p->item_count,sizeof(p->items[0]),qrx_moe_prefetch_cmp);return 0;}
int qrx_moe_prefetch_plan_commitment(const QrxMoePrefetchPlan*p,char out[65]){if(!p||!out||p->version!=QRX_MOE_COORDINATOR_VERSION||!nonempty(p->pod_id,sizeof(p->pod_id))||p->item_count>QRX_MOE_MAX_PREFETCH)return -1;EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/MOE/PREFETCH-PLAN/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;
#define PF_F(v) do{if(ok&&qrx_digest_field(x,(v),strlen(v)))ok=0;}while(0)
PF_F(p->pod_id);for(uint32_t i=0;i<p->item_count;i++){const QrxMoePrefetchItem*it=&p->items[i];PF_F(it->placement.node_id);PF_F(it->placement.fragment.model_id);PF_F(it->placement.fragment.model_version);PF_F(it->placement.fragment.content_root);if(ok&&(qrx_job_digest_u32(x,it->placement.fragment.layer_id)||qrx_job_digest_u32(x,it->placement.fragment.expert_id)||qrx_job_digest_u32(x,it->placement.fragment.fragment_id)||qrx_job_digest_u32(x,(uint32_t)it->action)||qrx_job_digest_u64(x,it->locality_score)||qrx_job_digest_u64(x,it->predicted_bps)||qrx_job_digest_u64(x,it->estimated_wire_bytes)))ok=0;}
#undef PF_F
if(ok&&(qrx_job_digest_u32(x,p->version)||qrx_job_digest_u32(x,p->next_layer_id)||qrx_job_digest_u32(x,p->item_count)||qrx_job_digest_u64(x,p->total_prefetch_bytes)||qrx_job_digest_u64(x,p->estimated_wire_bytes)))ok=0;if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;}

/* 0.0.9.16 MoE Micro-Batching / Persistent Pod Streams / Hierarchical Aggregation */
int qrx_moe_batch_item_validate(const QrxMoeBatchItem*i){
    if(!i||i->version!=QRX_MOE_BATCH_VERSION||!nonempty(i->request_id,sizeof(i->request_id))||!nonempty(i->session_id,sizeof(i->session_id))||!nonempty(i->model_id,sizeof(i->model_id))||!nonempty(i->model_version,sizeof(i->model_version)))return -1;
    if(!i->token_count||!i->activation_elements||!i->deadline_us||!qrx_moe_activation_wire_bytes(1,i->activation_encoding))return -1;
    return 0;
}
static int qrx_moe_batch_compatible(const QrxMoeMicroBatch*b,const QrxMoeBatchItem*i){
    return b->layer_id==i->layer_id&&b->expert_id==i->expert_id&&b->fragment_id==i->fragment_id&&b->activation_encoding==i->activation_encoding;
}
int qrx_moe_microbatch_add(QrxMoeMicroBatch*b,const char*pod,const QrxMoeBatchItem*i,uint32_t max_items,uint32_t max_tokens,uint64_t max_wire){
    if(!b||!pod||!nonempty(pod,QRX_MOE_MAX_POD_ID)||qrx_moe_batch_item_validate(i)||!max_items||max_items>QRX_MOE_MAX_BATCH_ITEMS||!max_tokens||!max_wire)return -1;
    uint64_t w=qrx_moe_activation_wire_bytes(i->activation_elements,i->activation_encoding);if(!w)return -1;
    if(b->version==0){memset(b,0,sizeof(*b));b->version=QRX_MOE_BATCH_VERSION;snprintf(b->pod_id,sizeof(b->pod_id),"%s",pod);b->layer_id=i->layer_id;b->expert_id=i->expert_id;b->fragment_id=i->fragment_id;b->activation_encoding=i->activation_encoding;b->earliest_deadline_us=i->deadline_us;}
    if(b->version!=QRX_MOE_BATCH_VERSION||strcmp(b->pod_id,pod)||!qrx_moe_batch_compatible(b,i))return -2;
    if(b->item_count>=max_items||b->item_count>=QRX_MOE_MAX_BATCH_ITEMS)return -3;
    if(i->token_count>max_tokens||b->total_tokens>max_tokens-i->token_count)return -4;
    if(b->estimated_wire_bytes>max_wire||w>max_wire-b->estimated_wire_bytes)return -5;
    for(uint32_t k=0;k<b->item_count;k++)if(!strcmp(b->items[k].request_id,i->request_id))return -6;
    b->items[b->item_count++]=*i;b->total_tokens+=i->token_count;b->total_activation_elements+=i->activation_elements;b->estimated_wire_bytes+=w;if(i->deadline_us<b->earliest_deadline_us)b->earliest_deadline_us=i->deadline_us;return 0;
}
static int qrx_moe_batch_item_cmp(const void*a,const void*b){const QrxMoeBatchItem*x=a,*y=b;if(x->deadline_us<y->deadline_us)return -1;if(x->deadline_us>y->deadline_us)return 1;return strcmp(x->request_id,y->request_id);}
int qrx_moe_microbatch_finalize(QrxMoeMicroBatch*b){if(!b||b->version!=QRX_MOE_BATCH_VERSION||!b->item_count||!nonempty(b->pod_id,sizeof(b->pod_id)))return -1;qsort(b->items,b->item_count,sizeof(b->items[0]),qrx_moe_batch_item_cmp);return 0;}
int qrx_moe_microbatch_commitment(const QrxMoeMicroBatch*b,char out[65]){if(!b||!out||b->version!=QRX_MOE_BATCH_VERSION||!b->item_count)return -1;EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/MOE/MICROBATCH/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;
#define MBF(v) do{if(ok&&qrx_digest_field(x,(v),strlen(v)))ok=0;}while(0)
MBF(b->pod_id);if(ok&&(qrx_job_digest_u32(x,b->layer_id)||qrx_job_digest_u32(x,b->expert_id)||qrx_job_digest_u32(x,b->fragment_id)||qrx_job_digest_u32(x,(uint32_t)b->activation_encoding)))ok=0;for(uint32_t k=0;k<b->item_count;k++){const QrxMoeBatchItem*i=&b->items[k];MBF(i->request_id);MBF(i->session_id);MBF(i->model_id);MBF(i->model_version);if(ok&&(qrx_job_digest_u32(x,i->token_count)||qrx_job_digest_u64(x,i->activation_elements)||qrx_job_digest_u64(x,i->deadline_us)))ok=0;}
#undef MBF
if(ok&&(qrx_job_digest_u32(x,b->item_count)||qrx_job_digest_u32(x,b->total_tokens)||qrx_job_digest_u64(x,b->total_activation_elements)||qrx_job_digest_u64(x,b->estimated_wire_bytes)||qrx_job_digest_u64(x,b->earliest_deadline_us)))ok=0;if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int j=0;j<32;j++){out[j*2]=hx[h[j]>>4];out[j*2+1]=hx[h[j]&15];}out[64]=0;return 0;}
int qrx_moe_stream_open(QrxMoePersistentStream*s,const char*sid,const char*pod,const char*local,const char*remote,QrxActivationEncoding enc,uint32_t feat,uint32_t maxinf,uint64_t now){if(!s||!sid||!pod||!local||!remote||!nonempty(sid,QRX_MOE_MAX_STREAM_ID)||!nonempty(pod,QRX_MOE_MAX_POD_ID)||!nonempty(local,129)||!nonempty(remote,129)||!strcmp(local,remote)||!qrx_moe_activation_wire_bytes(1,enc)||!maxinf)return -1;memset(s,0,sizeof(*s));s->version=QRX_MOE_BATCH_VERSION;snprintf(s->stream_id,sizeof(s->stream_id),"%s",sid);snprintf(s->pod_id,sizeof(s->pod_id),"%s",pod);snprintf(s->local_node_id,sizeof(s->local_node_id),"%s",local);snprintf(s->remote_node_id,sizeof(s->remote_node_id),"%s",remote);s->state=QRX_MOE_STREAM_OPEN;s->feature_mask=feat;s->activation_encoding=enc;s->max_inflight_batches=maxinf;s->opened_us=s->last_activity_us=now;return 0;}
int qrx_moe_stream_begin_batch(QrxMoePersistentStream*s,uint64_t bytes,uint64_t now){if(!s||s->version!=QRX_MOE_BATCH_VERSION||s->state!=QRX_MOE_STREAM_OPEN||!bytes||s->inflight_batches>=s->max_inflight_batches||now<s->last_activity_us)return -1;if(UINT64_MAX-s->bytes_sent<bytes)return -1;s->inflight_batches++;s->bytes_sent+=bytes;s->last_activity_us=now;return 0;}
int qrx_moe_stream_complete_batch(QrxMoePersistentStream*s,uint64_t bytes,uint64_t now){if(!s||s->version!=QRX_MOE_BATCH_VERSION||!s->inflight_batches||now<s->last_activity_us||UINT64_MAX-s->bytes_received<bytes)return -1;s->inflight_batches--;s->bytes_received+=bytes;s->last_activity_us=now;return 0;}
int qrx_moe_stream_set_state(QrxMoePersistentStream*s,QrxMoeStreamState st,uint64_t now){if(!s||s->version!=QRX_MOE_BATCH_VERSION||st<QRX_MOE_STREAM_OPEN||st>QRX_MOE_STREAM_FAILED||now<s->last_activity_us)return -1;if((st==QRX_MOE_STREAM_CLOSED||st==QRX_MOE_STREAM_FAILED)&&s->inflight_batches)return -2;s->state=st;s->last_activity_us=now;return 0;}
int qrx_moe_aggregation_add(QrxMoeAggregationPlan*p,const char*pod,const char*agg,uint32_t layer,uint32_t expert,const char*node,uint64_t bytes,uint64_t seq){if(!p||!pod||!agg||!node||!nonempty(pod,QRX_MOE_MAX_POD_ID)||!nonempty(agg,129)||!nonempty(node,129)||!bytes)return -1;if(p->version==0){memset(p,0,sizeof(*p));p->version=QRX_MOE_BATCH_VERSION;snprintf(p->pod_id,sizeof(p->pod_id),"%s",pod);snprintf(p->aggregator_node_id,sizeof(p->aggregator_node_id),"%s",agg);p->layer_id=layer;p->expert_id=expert;}if(p->version!=QRX_MOE_BATCH_VERSION||strcmp(p->pod_id,pod)||strcmp(p->aggregator_node_id,agg)||p->layer_id!=layer||p->expert_id!=expert||p->input_count>=QRX_MOE_MAX_AGG_ITEMS)return -2;for(uint32_t k=0;k<p->input_count;k++)if(!strcmp(p->inputs[k].node_id,node)||p->inputs[k].sequence==seq)return -3;if(UINT64_MAX-p->local_input_bytes<bytes)return -4;QrxMoeAggregateInput*i=&p->inputs[p->input_count++];snprintf(i->node_id,sizeof(i->node_id),"%s",node);i->partial_result_bytes=bytes;i->sequence=seq;p->local_input_bytes+=bytes;return 0;}
static int qrx_moe_agg_cmp(const void*a,const void*b){const QrxMoeAggregateInput*x=a,*y=b;if(x->sequence<y->sequence)return -1;if(x->sequence>y->sequence)return 1;return strcmp(x->node_id,y->node_id);}
int qrx_moe_aggregation_finalize(QrxMoeAggregationPlan*p,uint64_t uplink){if(!p||p->version!=QRX_MOE_BATCH_VERSION||!p->input_count||!uplink)return -1;qsort(p->inputs,p->input_count,sizeof(p->inputs[0]),qrx_moe_agg_cmp);p->uplink_result_bytes=uplink;return 0;}
uint64_t qrx_moe_aggregation_wire_savings(const QrxMoeAggregationPlan*p){if(!p||p->version!=QRX_MOE_BATCH_VERSION||p->local_input_bytes<=p->uplink_result_bytes)return 0;return p->local_input_bytes-p->uplink_result_bytes;}
int qrx_moe_aggregation_commitment(const QrxMoeAggregationPlan*p,char out[65]){if(!p||!out||p->version!=QRX_MOE_BATCH_VERSION||!p->input_count||!p->uplink_result_bytes)return -1;EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/MOE/AGGREGATION/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;
#define AGF(v) do{if(ok&&qrx_digest_field(x,(v),strlen(v)))ok=0;}while(0)
AGF(p->pod_id);AGF(p->aggregator_node_id);if(ok&&(qrx_job_digest_u32(x,p->layer_id)||qrx_job_digest_u32(x,p->expert_id)))ok=0;for(uint32_t k=0;k<p->input_count;k++){AGF(p->inputs[k].node_id);if(ok&&(qrx_job_digest_u64(x,p->inputs[k].partial_result_bytes)||qrx_job_digest_u64(x,p->inputs[k].sequence)))ok=0;}
#undef AGF
if(ok&&(qrx_job_digest_u32(x,p->input_count)||qrx_job_digest_u64(x,p->local_input_bytes)||qrx_job_digest_u64(x,p->uplink_result_bytes)))ok=0;if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int j=0;j<32;j++){out[j*2]=hx[h[j]>>4];out[j*2+1]=hx[h[j]&15];}out[64]=0;return 0;}

/* ---- 0.0.9.17 adaptive batching / backpressure / telemetry ---- */
static uint32_t qrx_ratio_bps_u32(uint32_t n,uint32_t d){if(!d)return 10000U;if(n>=d)return 10000U;return (uint32_t)(((uint64_t)n*10000ULL)/d);}
int qrx_moe_adaptive_policy_validate(const QrxMoeAdaptiveBatchPolicy*p){
    if(!p||p->version!=QRX_MOE_SCHED_VERSION)return -1;
    if(!p->min_batch_items||p->min_batch_items>p->target_batch_items||p->target_batch_items>p->max_batch_items||p->max_batch_items>QRX_MOE_MAX_BATCH_ITEMS)return -2;
    if(!p->max_wait_us||p->deadline_guard_us>p->max_wait_us)return -3;
    if(!p->elevated_queue_bps||p->elevated_queue_bps>=p->high_queue_bps||p->high_queue_bps>=p->critical_queue_bps||p->critical_queue_bps>10000U)return -4;
    return 0;
}
int qrx_moe_schedule_advice(const QrxMoeAdaptiveBatchPolicy*p,const QrxMoeSchedulerSnapshot*s,QrxMoeScheduleAdvice*out){
    if(qrx_moe_adaptive_policy_validate(p)||!s||!out||s->version!=QRX_MOE_SCHED_VERSION||!s->queue_capacity||!s->inflight_capacity)return -1;
    memset(out,0,sizeof(*out));out->queue_pressure_bps=qrx_ratio_bps_u32(s->queued_items,s->queue_capacity);out->inflight_pressure_bps=qrx_ratio_bps_u32(s->inflight_batches,s->inflight_capacity);
    uint32_t pressure=out->queue_pressure_bps>out->inflight_pressure_bps?out->queue_pressure_bps:out->inflight_pressure_bps;
    out->pressure=pressure>=p->critical_queue_bps?QRX_MOE_PRESSURE_CRITICAL:pressure>=p->high_queue_bps?QRX_MOE_PRESSURE_HIGH:pressure>=p->elevated_queue_bps?QRX_MOE_PRESSURE_ELEVATED:QRX_MOE_PRESSURE_NORMAL;
    uint32_t compatible=s->compatible_items; if(compatible>p->max_batch_items)compatible=p->max_batch_items; out->selected_batch_items=compatible;
    if(out->pressure==QRX_MOE_PRESSURE_CRITICAL){out->decision=s->alternate_replica_available?QRX_MOE_DISPATCH_REROUTE:QRX_MOE_DISPATCH_REJECT;return 0;}
    if(s->earliest_deadline_slack_us<=p->deadline_guard_us||s->oldest_item_age_us>=p->max_wait_us||compatible>=p->target_batch_items||out->pressure>=QRX_MOE_PRESSURE_HIGH){out->decision=QRX_MOE_DISPATCH_NOW;return 0;}
    if(compatible>=p->min_batch_items){out->decision=QRX_MOE_DISPATCH_WAIT;uint64_t left=p->max_wait_us-s->oldest_item_age_us;uint64_t dl=s->earliest_deadline_slack_us>p->deadline_guard_us?s->earliest_deadline_slack_us-p->deadline_guard_us:0;out->suggested_wait_us=left<dl?left:dl;return 0;}
    out->decision=s->alternate_replica_available&&out->pressure>=QRX_MOE_PRESSURE_ELEVATED?QRX_MOE_DISPATCH_REROUTE:QRX_MOE_DISPATCH_WAIT;out->suggested_wait_us=p->max_wait_us-s->oldest_item_age_us;return 0;
}
uint64_t qrx_moe_replica_candidate_score(const QrxMoeReplicaCandidate*c){
    if(!c||c->version!=QRX_MOE_SCHED_VERSION||!nonempty(c->node_id,129)||!c->queue_capacity||!c->inflight_capacity||!c->bandwidth_mbps||c->reliability_bps>10000U||c->residency<QRX_MOE_RES_REMOTE||c->residency>QRX_MOE_RES_VRAM||c->cache_state<QRX_MOE_CACHE_COLD||c->cache_state>QRX_MOE_CACHE_HOT)return 0;
    uint64_t score=0;uint32_t qp=qrx_ratio_bps_u32(c->queue_depth,c->queue_capacity),ip=qrx_ratio_bps_u32(c->inflight_batches,c->inflight_capacity),pressure=qp>ip?qp:ip;
    score+=(uint64_t)(10000U-pressure)*40ULL;score+=(uint64_t)c->reliability_bps*30ULL;score+=(uint64_t)c->residency*50000ULL;score+=(uint64_t)c->cache_state*40000ULL;
    score+=(uint64_t)(c->bandwidth_mbps>100000U?100000U:c->bandwidth_mbps)*2ULL;score+=c->latency_us>=1000000U?0ULL:(1000000ULL-c->latency_us)/5ULL;return score;
}
int qrx_moe_choose_replica(const QrxMoeReplicaCandidate*c,uint32_t count,uint32_t*idx){if(!c||!idx||!count||count>QRX_MOE_MAX_REPLICA_CANDIDATES)return -1;uint64_t best=0;uint32_t bi=0;for(uint32_t i=0;i<count;i++){uint64_t sc=qrx_moe_replica_candidate_score(&c[i]);if(!sc)return -2;if(i==0||sc>best||(sc==best&&strcmp(c[i].node_id,c[bi].node_id)<0)){best=sc;bi=i;}}*idx=bi;return 0;}
int qrx_moe_telemetry_validate(const QrxMoeThroughputTelemetry*t){if(!t||t->version!=QRX_MOE_TELEMETRY_VERSION||!nonempty(t->pod_id,QRX_MOE_MAX_POD_ID)||t->window_end_us<t->window_start_us)return -1;return 0;}
static int qrx_add_u64(uint64_t*a,uint64_t b){if(UINT64_MAX-*a<b)return -1;*a+=b;return 0;}
int qrx_moe_telemetry_record_batch(QrxMoeThroughputTelemetry*t,uint64_t req,uint64_t tok,uint64_t sent,uint64_t recv,uint64_t local,uint64_t wan,uint64_t compute,uint64_t queue,uint8_t rerouted,uint8_t failed){if(qrx_moe_telemetry_validate(t)||!req||!tok)return -1;if(qrx_add_u64(&t->requests_completed,req)||qrx_add_u64(&t->tokens_completed,tok)||qrx_add_u64(&t->activation_bytes_sent,sent)||qrx_add_u64(&t->result_bytes_received,recv)||qrx_add_u64(&t->local_aggregation_bytes,local)||qrx_add_u64(&t->wan_bytes,wan)||qrx_add_u64(&t->compute_us,compute)||qrx_add_u64(&t->queue_wait_us,queue)||qrx_add_u64(&t->batch_count,1)||qrx_add_u64(&t->reroute_count,rerouted?1:0)||qrx_add_u64(&t->failed_count,failed?1:0))return -2;return 0;}
static uint64_t qrx_rate_milli(uint64_t units,uint64_t start,uint64_t end){if(end<=start)return 0;uint64_t dur=end-start;if(units>UINT64_MAX/1000000000ULL)return UINT64_MAX;return (units*1000000000ULL)/dur;}
uint64_t qrx_moe_telemetry_tokens_per_second_milli(const QrxMoeThroughputTelemetry*t){if(qrx_moe_telemetry_validate(t))return 0;return qrx_rate_milli(t->tokens_completed,t->window_start_us,t->window_end_us);}
uint64_t qrx_moe_telemetry_requests_per_second_milli(const QrxMoeThroughputTelemetry*t){if(qrx_moe_telemetry_validate(t))return 0;return qrx_rate_milli(t->requests_completed,t->window_start_us,t->window_end_us);}
uint32_t qrx_moe_telemetry_wan_share_bps(const QrxMoeThroughputTelemetry*t){if(qrx_moe_telemetry_validate(t))return 0;uint64_t total=t->activation_bytes_sent+t->result_bytes_received;if(total<t->activation_bytes_sent)return 0;if(!total)return 0;if(t->wan_bytes>=total)return 10000U;return (uint32_t)((t->wan_bytes*10000ULL)/total);}
int qrx_moe_telemetry_commitment(const QrxMoeThroughputTelemetry*t,char out[65]){if(qrx_moe_telemetry_validate(t)||!out)return -1;EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/MOE/THROUGHPUT/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;if(ok&&qrx_digest_field(x,t->pod_id,strlen(t->pod_id)))ok=0;
#define T64(v) do{if(ok&&qrx_job_digest_u64(x,(v)))ok=0;}while(0)
T64(t->window_start_us);T64(t->window_end_us);T64(t->requests_completed);T64(t->tokens_completed);T64(t->activation_bytes_sent);T64(t->result_bytes_received);T64(t->local_aggregation_bytes);T64(t->wan_bytes);T64(t->compute_us);T64(t->queue_wait_us);T64(t->batch_count);T64(t->reroute_count);T64(t->rejected_count);T64(t->failed_count);
#undef T64
if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int j=0;j<32;j++){out[j*2]=hx[h[j]>>4];out[j*2+1]=hx[h[j]&15];}out[64]=0;return 0;}

/* ---- 0.0.9.18 runtime transports / reproducible benchmark harness ---- */
int qrx_moe_transport_validate(const QrxMoeTransportAdapter*a){
    if(!a||a->version!=QRX_MOE_RUNTIME_VERSION||a->kind<QRX_MOE_TRANSPORT_LOCAL||a->kind>QRX_MOE_TRANSPORT_JACCL_RDMA||!nonempty(a->name,QRX_MOE_MAX_TRANSPORT_NAME)||!a->max_inflight)return -1;
    if(a->kind!=QRX_MOE_TRANSPORT_LOCAL&&a->kind!=QRX_MOE_TRANSPORT_SHM&&!a->bandwidth_mbps)return -2;
    if((a->feature_mask&QRX_MOE_RUNTIME_FEAT_RDMA)&&a->kind!=QRX_MOE_TRANSPORT_JACCL_RDMA&&a->kind!=QRX_MOE_TRANSPORT_MPI)return -3;
    return 0;
}
int qrx_moe_runtime_device_validate(const QrxMoeRuntimeDevice*d){
    if(!d||d->version!=QRX_MOE_RUNTIME_VERSION||d->backend<QRX_MOE_BACKEND_CPU||d->backend>QRX_MOE_BACKEND_OTHER||!nonempty(d->device_name,QRX_MOE_MAX_DEVICE_NAME)||!d->max_batch_items)return -1;
    if(d->backend==QRX_MOE_BACKEND_CUDA){if(!(d->accelerator_features&QRX_MOE_ACCEL_FEAT_CUDA)||!d->cuda_cc_major||!d->device_memory_bytes)return -2;}
    if(d->backend==QRX_MOE_BACKEND_MLX_METAL&&((d->accelerator_features&(QRX_MOE_ACCEL_FEAT_MLX|QRX_MOE_ACCEL_FEAT_METAL))!=(QRX_MOE_ACCEL_FEAT_MLX|QRX_MOE_ACCEL_FEAT_METAL)))return -3;
    return 0;
}
int qrx_moe_runtime_is_nvidia_p40(const QrxMoeRuntimeDevice*d){
    if(qrx_moe_runtime_device_validate(d))return 0;
    return d->backend==QRX_MOE_BACKEND_CUDA&&d->cuda_cc_major==6U&&d->cuda_cc_minor==1U&&strstr(d->device_name,"P40")!=NULL;
}
int qrx_moe_runtime_supports_tensor_core_kernel(const QrxMoeRuntimeDevice*d){if(qrx_moe_runtime_device_validate(d))return 0;return (d->accelerator_features&QRX_MOE_ACCEL_FEAT_TENSOR_CORES)?1:0;}
int qrx_moe_benchmark_scenario_validate(const QrxMoeBenchmarkScenario*s){
    if(!s||s->version!=QRX_MOE_BENCH_VERSION||!nonempty(s->scenario_id,129)||!s->duration_us||!s->concurrency||!s->requested_batch_items||s->requested_batch_items>QRX_MOE_MAX_BATCH_ITEMS||!s->node_count||s->node_count>QRX_MOE_MAX_BENCH_NODES||s->synthetic_fault_bps>10000U)return -1;
    for(uint32_t i=0;i<s->node_count;i++){const QrxMoeBenchmarkNode*n=&s->nodes[i];if(n->version!=QRX_MOE_BENCH_VERSION||!nonempty(n->node_id,129)||qrx_moe_runtime_device_validate(&n->device)||qrx_moe_transport_validate(&n->transport)||!n->standalone_milli_tokens_per_second||n->availability_bps>10000U)return -2;for(uint32_t j=0;j<i;j++)if(!strcmp(n->node_id,s->nodes[j].node_id))return -3;}
    return 0;
}
static uint64_t qrx_u64_sat_add(uint64_t a,uint64_t b){return UINT64_MAX-a<b?UINT64_MAX:a+b;}
int qrx_moe_benchmark_run(const QrxMoeBenchmarkScenario*s,QrxMoeBenchmarkResult*out){
    if(qrx_moe_benchmark_scenario_validate(s)||!out)return -1;memset(out,0,sizeof(*out));out->version=QRX_MOE_BENCH_VERSION;uint64_t raw=0,wire_per_token=qrx_u64_sat_add(s->activation_bytes_per_token,s->result_bytes_per_token);uint64_t worst_transport_us=0;uint32_t wi=0;
    for(uint32_t i=0;i<s->node_count;i++){const QrxMoeBenchmarkNode*n=&s->nodes[i];uint64_t eff=(uint64_t)n->availability_bps/10ULL;out->effective_nodes_milli=qrx_u64_sat_add(out->effective_nodes_milli,eff);uint64_t node=(n->standalone_milli_tokens_per_second*(uint64_t)n->availability_bps)/10000ULL;raw=qrx_u64_sat_add(raw,node);uint64_t tu=n->transport.latency_us;if(n->transport.bandwidth_mbps&&wire_per_token){uint64_t payload_us=(wire_per_token>UINT64_MAX/8ULL)?UINT64_MAX:(wire_per_token*8ULL)/n->transport.bandwidth_mbps;tu=qrx_u64_sat_add(tu,payload_us);}if(tu>worst_transport_us){worst_transport_us=tu;wi=i;}}
    out->raw_milli_tokens_per_second=raw;out->bottleneck_transport_index=wi;uint32_t batch=s->requested_batch_items;if(batch>s->concurrency)batch=s->concurrency;uint64_t amortized_transport_us=batch?worst_transport_us/batch:worst_transport_us;uint64_t raw_token_us=raw?1000000000ULL/raw:UINT64_MAX;uint64_t total_token_us=qrx_u64_sat_add(raw_token_us,amortized_transport_us);uint64_t est=total_token_us?1000000000ULL/total_token_us:0;uint32_t loss=s->synthetic_fault_bps;if(loss>10000U)loss=10000U;est=(est*(10000U-loss))/10000U;out->fault_loss_bps=loss;out->estimated_milli_tokens_per_second=est;out->estimated_tokens_completed=(est*s->duration_us)/1000000000ULL;out->estimated_compute_us=out->estimated_tokens_completed*raw_token_us;out->estimated_transport_us=out->estimated_tokens_completed*amortized_transport_us;if(wire_per_token&&out->estimated_tokens_completed<=UINT64_MAX/wire_per_token)out->estimated_wire_bytes=out->estimated_tokens_completed*wire_per_token;else out->estimated_wire_bytes=UINT64_MAX;return 0;
}
int qrx_moe_benchmark_commitment(const QrxMoeBenchmarkScenario*s,const QrxMoeBenchmarkResult*r,char out[65]){
    if(qrx_moe_benchmark_scenario_validate(s)||!r||r->version!=QRX_MOE_BENCH_VERSION||!out)return -1;EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/MOE/BENCHMARK/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;if(ok&&qrx_digest_field(x,s->scenario_id,strlen(s->scenario_id)))ok=0;
#define B64(v) do{if(ok&&qrx_job_digest_u64(x,(v)))ok=0;}while(0)
#define B32(v) do{if(ok&&qrx_job_digest_u32(x,(v)))ok=0;}while(0)
    B64(s->seed);B64(s->duration_us);B32(s->concurrency);B32(s->requested_batch_items);B64(s->activation_bytes_per_token);B64(s->result_bytes_per_token);B32(s->synthetic_fault_bps);B32(s->node_count);
    for(uint32_t i=0;i<s->node_count;i++){const QrxMoeBenchmarkNode*n=&s->nodes[i];if(ok&&qrx_digest_field(x,n->node_id,strlen(n->node_id)))ok=0;B32(n->device.backend);B32(n->device.accelerator_features);B64(n->device.device_memory_bytes);B32(n->device.cuda_cc_major);B32(n->device.cuda_cc_minor);B32(n->transport.kind);B32(n->transport.feature_mask);B32(n->transport.latency_us);B32(n->transport.bandwidth_mbps);B64(n->standalone_milli_tokens_per_second);B32(n->availability_bps);}
    B64(r->raw_milli_tokens_per_second);B64(r->estimated_milli_tokens_per_second);B64(r->estimated_tokens_completed);B64(r->estimated_wire_bytes);B32(r->fault_loss_bps);
#undef B64
#undef B32
    if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int j=0;j<32;j++){out[j*2]=hx[h[j]>>4];out[j*2+1]=hx[h[j]&15];}out[64]=0;return 0;
}

/* ---- 0.0.9.19 native runtime discovery / worker eligibility / calibration ---- */
static uint64_t qrx_moe_native_total_memory(void){
#if defined(_WIN32)
    MEMORYSTATUSEX ms; memset(&ms,0,sizeof(ms)); ms.dwLength=sizeof(ms); if(GlobalMemoryStatusEx(&ms)) return (uint64_t)ms.ullTotalPhys; return 0;
#elif defined(__APPLE__)
    uint64_t v=0; size_t n=sizeof(v); if(sysctlbyname("hw.memsize",&v,&n,NULL,0)==0) return v; return 0;
#elif defined(__linux__)
    struct sysinfo si; if(sysinfo(&si)==0) return (uint64_t)si.totalram*(uint64_t)si.mem_unit; return 0;
#else
    return 0;
#endif
}
static uint32_t qrx_moe_native_cpu_count(void){
#if defined(_WIN32)
    SYSTEM_INFO si; GetSystemInfo(&si); return si.dwNumberOfProcessors?si.dwNumberOfProcessors:1U;
#else
    long n=sysconf(_SC_NPROCESSORS_ONLN); return n>0?(uint32_t)n:1U;
#endif
}
static int qrx_moe_inventory_push(QrxMoeRuntimeInventory*out,QrxMoeDiscoverySource source,const QrxMoeRuntimeDevice*d,uint8_t present,uint8_t ready){
    if(!out||!d||out->device_count>=QRX_MOE_MAX_DISCOVERED_DEVICES)return -1;
    QrxMoeDiscoveredDevice*x=&out->devices[out->device_count++]; memset(x,0,sizeof(*x));x->version=QRX_MOE_DISCOVERY_VERSION;x->source=source;x->device=*d;x->runtime_present=present;x->execution_adapter_ready=ready;return 0;
}

#if defined(_WIN32)
typedef HMODULE QrxDynLib;
static QrxDynLib qrx_dlopen_cuda(void){return LoadLibraryA("nvcuda.dll");}
static void*qrx_dlsym_cuda(QrxDynLib h,const char*n){return h?(void*)GetProcAddress(h,n):NULL;}
static void qrx_dlclose_cuda(QrxDynLib h){if(h)FreeLibrary(h);}
#else
typedef void* QrxDynLib;
static QrxDynLib qrx_dlopen_cuda(void){QrxDynLib h=dlopen("libcuda.so.1",RTLD_LAZY|RTLD_LOCAL);if(!h)h=dlopen("libcuda.so",RTLD_LAZY|RTLD_LOCAL);return h;}
static void*qrx_dlsym_cuda(QrxDynLib h,const char*n){return h?dlsym(h,n):NULL;}
static void qrx_dlclose_cuda(QrxDynLib h){if(h)dlclose(h);}
#endif

typedef int (*QrxCuInitFn)(unsigned int);
typedef int (*QrxCuDeviceGetCountFn)(int*);
typedef int (*QrxCuDeviceGetFn)(int*,int);
typedef int (*QrxCuDeviceGetNameFn)(char*,int,int);
typedef int (*QrxCuDeviceTotalMemFn)(size_t*,int);
typedef int (*QrxCuDeviceComputeCapabilityFn)(int*,int*,int);

static uint32_t qrx_cuda_feature_mask(uint32_t maj,uint32_t min){
    uint32_t f=QRX_MOE_ACCEL_FEAT_CUDA|QRX_MOE_ACCEL_FEAT_FP32;
    if(maj>=5U)f|=QRX_MOE_ACCEL_FEAT_FP16;
    if(maj>6U||(maj==6U&&min>=1U))f|=QRX_MOE_ACCEL_FEAT_INT8;
    /* Tensor cores first appeared in Volta SM 7.0; never infer them on Pascal/P40. */
    if(maj>=7U)f|=QRX_MOE_ACCEL_FEAT_TENSOR_CORES;
    return f;
}
static int qrx_moe_discover_cuda(QrxMoeRuntimeInventory*out){
    QrxDynLib h=qrx_dlopen_cuda(); if(!h)return 0;
    QrxCuInitFn cuInit=(QrxCuInitFn)qrx_dlsym_cuda(h,"cuInit");
    QrxCuDeviceGetCountFn getCount=(QrxCuDeviceGetCountFn)qrx_dlsym_cuda(h,"cuDeviceGetCount");
    QrxCuDeviceGetFn getDev=(QrxCuDeviceGetFn)qrx_dlsym_cuda(h,"cuDeviceGet");
    QrxCuDeviceGetNameFn getName=(QrxCuDeviceGetNameFn)qrx_dlsym_cuda(h,"cuDeviceGetName");
    QrxCuDeviceComputeCapabilityFn getCC=(QrxCuDeviceComputeCapabilityFn)qrx_dlsym_cuda(h,"cuDeviceComputeCapability");
    QrxCuDeviceTotalMemFn getMem=(QrxCuDeviceTotalMemFn)qrx_dlsym_cuda(h,"cuDeviceTotalMem_v2");if(!getMem)getMem=(QrxCuDeviceTotalMemFn)qrx_dlsym_cuda(h,"cuDeviceTotalMem");
    if(!cuInit||!getCount||!getDev||!getName||!getCC||!getMem||cuInit(0)!=0){qrx_dlclose_cuda(h);return 0;}
    out->cuda_driver_present=1;int count=0;if(getCount(&count)!=0||count<0){qrx_dlclose_cuda(h);return 0;}
    for(int i=0;i<count&&out->device_count<QRX_MOE_MAX_DISCOVERED_DEVICES;i++){
        int dev=0,maj=0,min=0;size_t mem=0;char name[QRX_MOE_MAX_DEVICE_NAME];memset(name,0,sizeof(name));
        if(getDev(&dev,i)!=0||getName(name,(int)sizeof(name),dev)!=0||getCC(&maj,&min,dev)!=0||getMem(&mem,dev)!=0)continue;
        QrxMoeRuntimeDevice d;memset(&d,0,sizeof(d));d.version=QRX_MOE_RUNTIME_VERSION;d.backend=QRX_MOE_BACKEND_CUDA;snprintf(d.device_name,sizeof(d.device_name),"%s",name);d.device_memory_bytes=(uint64_t)mem;d.cuda_cc_major=(uint32_t)maj;d.cuda_cc_minor=(uint32_t)min;d.accelerator_features=qrx_cuda_feature_mask((uint32_t)maj,(uint32_t)min);d.max_batch_items=64U;
        qrx_moe_inventory_push(out,QRX_MOE_DISCOVERY_CUDA_DRIVER,&d,1,1);
    }
    qrx_dlclose_cuda(h);return 0;
}

static int qrx_moe_discover_apple(QrxMoeRuntimeInventory*out){
#if defined(__APPLE__) && defined(__aarch64__)
    out->apple_silicon=1;out->metal_capable=1;
    QrxMoeRuntimeDevice d;memset(&d,0,sizeof(d));d.version=QRX_MOE_RUNTIME_VERSION;d.backend=QRX_MOE_BACKEND_OTHER;snprintf(d.device_name,sizeof(d.device_name),"Apple Silicon Metal");d.device_memory_bytes=qrx_moe_native_total_memory();d.accelerator_features=QRX_MOE_ACCEL_FEAT_FP16|QRX_MOE_ACCEL_FEAT_FP32|QRX_MOE_ACCEL_FEAT_UNIFIED_MEM|QRX_MOE_ACCEL_FEAT_METAL;d.max_batch_items=64U;
    /* Metal capability is native; MLX is deliberately not claimed without an adapter. */
    qrx_moe_inventory_push(out,QRX_MOE_DISCOVERY_APPLE_NATIVE,&d,1,0);
#else
    (void)out;
#endif
    return 0;
}
int qrx_moe_runtime_discover_native(QrxMoeRuntimeInventory*out){
    if(!out)return -1;memset(out,0,sizeof(*out));out->version=QRX_MOE_DISCOVERY_VERSION;
    QrxMoeRuntimeDevice cpu;memset(&cpu,0,sizeof(cpu));cpu.version=QRX_MOE_RUNTIME_VERSION;cpu.backend=QRX_MOE_BACKEND_CPU;snprintf(cpu.device_name,sizeof(cpu.device_name),"Native CPU");cpu.accelerator_features=QRX_MOE_ACCEL_FEAT_FP32;cpu.device_memory_bytes=qrx_moe_native_total_memory();cpu.max_batch_items=qrx_moe_native_cpu_count();if(!cpu.max_batch_items)cpu.max_batch_items=1;
    if(qrx_moe_inventory_push(out,QRX_MOE_DISCOVERY_CPU_NATIVE,&cpu,1,1))return -2;
    qrx_moe_discover_apple(out);qrx_moe_discover_cuda(out);return qrx_moe_runtime_inventory_validate(out);
}
int qrx_moe_runtime_inventory_validate(const QrxMoeRuntimeInventory*i){
    if(!i||i->version!=QRX_MOE_DISCOVERY_VERSION||!i->device_count||i->device_count>QRX_MOE_MAX_DISCOVERED_DEVICES)return -1;
    for(uint32_t n=0;n<i->device_count;n++){const QrxMoeDiscoveredDevice*d=&i->devices[n];if(d->version!=QRX_MOE_DISCOVERY_VERSION||d->source<QRX_MOE_DISCOVERY_CPU_NATIVE||d->source>QRX_MOE_DISCOVERY_CUDA_DRIVER||qrx_moe_runtime_device_validate(&d->device))return -2;if(d->execution_adapter_ready&&!d->runtime_present)return -3;}
    return 0;
}
uint32_t qrx_moe_runtime_kernel_mask(const QrxMoeRuntimeDevice*d){
    if(qrx_moe_runtime_device_validate(d))return 0;uint32_t m=0;if(d->accelerator_features&QRX_MOE_ACCEL_FEAT_FP32)m|=QRX_MOE_KERNEL_FP32;if(d->accelerator_features&QRX_MOE_ACCEL_FEAT_FP16)m|=QRX_MOE_KERNEL_FP16;if(d->accelerator_features&QRX_MOE_ACCEL_FEAT_INT8)m|=QRX_MOE_KERNEL_INT8;if(d->accelerator_features&QRX_MOE_ACCEL_FEAT_TENSOR_CORES)m|=QRX_MOE_KERNEL_TENSORCORE;if(d->accelerator_features&QRX_MOE_ACCEL_FEAT_MLX)m|=QRX_MOE_KERNEL_MLX;if(d->accelerator_features&QRX_MOE_ACCEL_FEAT_METAL)m|=QRX_MOE_KERNEL_METAL;return m;
}
int qrx_moe_worker_eligible(const QrxMoeRuntimeDevice*d,const QrxMoeWorkerRequirement*r){
    if(qrx_moe_runtime_device_validate(d)||!r||r->version!=QRX_MOE_DISCOVERY_VERSION||r->backend<QRX_MOE_BACKEND_CPU||r->backend>QRX_MOE_BACKEND_OTHER||!r->requested_batch_items)return 0;if(d->backend!=r->backend)return 0;if(d->device_memory_bytes<r->required_device_memory_bytes||d->max_batch_items<r->requested_batch_items)return 0;uint32_t have=qrx_moe_runtime_kernel_mask(d);return (have&r->required_kernel_features)==r->required_kernel_features;
}
int qrx_moe_calibration_profile_validate(const QrxMoeCalibrationProfile*p){
    if(!p||p->version!=QRX_MOE_CALIBRATION_VERSION||!nonempty(p->profile_id,129)||qrx_moe_runtime_device_validate(&p->device)||!p->sample_count)return -1;if(p->model_milli_tokens_per_second&&!p->model_batch_items)return -2;return 0;
}
int qrx_moe_calibration_apply_to_benchmark(const QrxMoeCalibrationProfile*p,QrxMoeBenchmarkNode*n){
    if(qrx_moe_calibration_profile_validate(p)||!n||n->version!=QRX_MOE_BENCH_VERSION||!p->model_milli_tokens_per_second)return -1;n->device=p->device;n->standalone_milli_tokens_per_second=p->model_milli_tokens_per_second;if(p->transport_latency_us)n->transport.latency_us=p->transport_latency_us;if(p->transport_bandwidth_mbps)n->transport.bandwidth_mbps=p->transport_bandwidth_mbps;return 0;
}
int qrx_moe_calibration_commitment(const QrxMoeCalibrationProfile*p,char out[65]){
    if(qrx_moe_calibration_profile_validate(p)||!out)return -1;EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned char h[32];unsigned int hn=0;int ok=1;const char dom[]="QRX/MOE/CALIBRATION/V1";if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1)ok=0;if(ok&&EVP_DigestUpdate(x,dom,sizeof(dom)-1)!=1)ok=0;if(ok&&qrx_digest_field(x,p->profile_id,strlen(p->profile_id)))ok=0;if(ok&&qrx_digest_field(x,p->device.device_name,strlen(p->device.device_name)))ok=0;
#define C64(v) do{if(ok&&qrx_job_digest_u64(x,(v)))ok=0;}while(0)
#define C32(v) do{if(ok&&qrx_job_digest_u32(x,(v)))ok=0;}while(0)
    C32(p->device.backend);C32(p->device.accelerator_features);C64(p->device.device_memory_bytes);C32(p->device.cuda_cc_major);C32(p->device.cuda_cc_minor);C64(p->measured_at_unix_ms);C64(p->memory_bandwidth_bytes_per_second);C64(p->compute_ops_per_second);C64(p->model_milli_tokens_per_second);C32(p->model_batch_items);C32(p->sample_count);C32(p->transport_latency_us);C32(p->transport_bandwidth_mbps);
#undef C64
#undef C32
    if(ok&&EVP_DigestFinal_ex(x,h,&hn)!=1)ok=0;EVP_MD_CTX_free(x);if(!ok||hn!=32)return -1;static const char hx[]="0123456789abcdef";for(int j=0;j<32;j++){out[j*2]=hx[h[j]>>4];out[j*2+1]=hx[h[j]&15];}out[64]=0;return 0;
}

int qrx_moe_worker_adapter_validate(const QrxMoeWorkerAdapter*a){
    if(!a||a->version!=QRX_MOE_DISCOVERY_VERSION||!nonempty(a->name,QRX_MOE_MAX_ADAPTER_NAME)||a->backend<QRX_MOE_BACKEND_CPU||a->backend>QRX_MOE_BACKEND_OTHER)return -1;
    if(a->ready&&(!a->execute||!a->kernel_mask))return -2;return 0;
}
int qrx_moe_worker_adapter_execute(const QrxMoeWorkerAdapter*a,const QrxMoeRuntimeDevice*d,const QrxMoeWorkerRequirement*r,const void*in,size_t in_n,void*out,size_t out_cap,size_t*out_n){
    if(qrx_moe_worker_adapter_validate(a)||!a->ready||!a->execute||!d||!r||!out_n)return -1;
    if(a->backend!=d->backend||!qrx_moe_worker_eligible(d,r))return -2;
    if((a->kernel_mask&r->required_kernel_features)!=r->required_kernel_features)return -3;
    if(in_n&&!in)return -4;if(out_cap&&!out)return -5;*out_n=0;return a->execute(a->ctx,in,in_n,out,out_cap,out_n);
}
int qrx_moe_calibration_record_model_sample(QrxMoeCalibrationProfile*p,uint64_t tokens,uint64_t elapsed_us,uint32_t batch){
    if(!p||p->version!=QRX_MOE_CALIBRATION_VERSION||!tokens||!elapsed_us||!batch)return -1;
    uint64_t milli=(tokens>UINT64_MAX/1000000000ULL)?UINT64_MAX:(tokens*1000000000ULL)/elapsed_us;
    uint32_t old=p->sample_count;if(old==UINT32_MAX)return -2;
    if(!old)p->model_milli_tokens_per_second=milli;else{uint64_t weighted=p->model_milli_tokens_per_second;if(weighted<=UINT64_MAX/old)weighted*=old;else weighted=UINT64_MAX;if(weighted!=UINT64_MAX&&UINT64_MAX-weighted>=milli)p->model_milli_tokens_per_second=(weighted+milli)/(old+1U);}
    p->sample_count=old+1U;p->model_batch_items=batch;return 0;
}
int qrx_moe_calibration_record_transport_probe(QrxMoeCalibrationProfile*p,uint64_t payload,uint64_t rtt_us){
    if(!p||p->version!=QRX_MOE_CALIBRATION_VERSION||!payload||!rtt_us)return -1;
    p->transport_latency_us=(uint32_t)(rtt_us/2ULL>UINT32_MAX?UINT32_MAX:rtt_us/2ULL);
    /* bidirectional payload over one RTT, expressed as decimal Mbit/s. */
    if(payload>UINT64_MAX/16ULL)p->transport_bandwidth_mbps=UINT32_MAX;else{uint64_t mbps=(payload*16ULL)/rtt_us;if(mbps>UINT32_MAX)mbps=UINT32_MAX;p->transport_bandwidth_mbps=(uint32_t)mbps;}
    return 0;
}
