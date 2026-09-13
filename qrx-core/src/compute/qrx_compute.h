#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_COMPUTE_PROTOCOL_VERSION 1
#define QRX_COMPUTE_V1_MIN_PROTOCOL_VERSION 10LL
#define QRX_COMPUTE_V1_FEATURE_FLAG "COMPUTE_V1"
#define QRX_COMPUTE_MAX_ACCELERATORS 16

/* CPU feature flags are deliberately architecture-neutral bit positions. */
#define QRX_CPU_FEATURE_SSE2      (1ULL<<0)
#define QRX_CPU_FEATURE_AVX2      (1ULL<<1)
#define QRX_CPU_FEATURE_FMA       (1ULL<<2)
#define QRX_CPU_FEATURE_AVX512F   (1ULL<<3)
#define QRX_CPU_FEATURE_BF16      (1ULL<<4)
#define QRX_CPU_FEATURE_AMX       (1ULL<<5)
#define QRX_CPU_FEATURE_NEON      (1ULL<<16)
#define QRX_CPU_FEATURE_SVE       (1ULL<<17)
#define QRX_CPU_FEATURE_SVE2      (1ULL<<18)

#define QRX_COMPUTE_KERNEL_SCALAR 0
#define QRX_COMPUTE_KERNEL_SSE2   1
#define QRX_COMPUTE_KERNEL_AVX2   2
#define QRX_COMPUTE_KERNEL_AVX512 3
#define QRX_COMPUTE_KERNEL_NEON   4
#define QRX_COMPUTE_KERNEL_SVE    5
#define QRX_COMPUTE_KERNEL_SVE2   6

typedef enum {
    QRX_COMPUTE_OS_UNKNOWN=0, QRX_COMPUTE_OS_LINUX=1, QRX_COMPUTE_OS_WINDOWS=2, QRX_COMPUTE_OS_MACOS=3
} QrxComputeOs;
typedef enum {
    QRX_COMPUTE_ARCH_UNKNOWN=0, QRX_COMPUTE_ARCH_X86_64=1, QRX_COMPUTE_ARCH_ARM64=2
} QrxComputeArch;
typedef enum {
    QRX_ACCEL_CPU=1, QRX_ACCEL_GPU=2, QRX_ACCEL_NPU=3, QRX_ACCEL_AI=4
} QrxComputeAcceleratorType;

typedef struct {
    QrxComputeAcceleratorType type;
    char name[96];
    uint64_t memory_bytes;
    uint64_t capability_flags;
} QrxComputeAccelerator;

typedef struct {
    char provider_id[129];
    QrxComputeOs os;
    QrxComputeArch arch;
    uint32_t logical_cpu_count;
    uint64_t memory_total_bytes;
    uint64_t memory_available_bytes;
    uint64_t cpu_feature_flags;
    uint32_t accelerator_count;
    QrxComputeAccelerator accelerators[QRX_COMPUTE_MAX_ACCELERATORS];
    uint32_t max_parallel_tasks;
    uint8_t sandbox_available;
} QrxComputeCapabilities;

typedef struct {
    char task_id[129];
    char owner[160];
    char runtime_id[129];
    char workload_hash[129];
    char input_commitment[129];
    QrxComputeArch required_arch;
    uint64_t min_memory_bytes;
    uint64_t max_runtime_ms;
    uint64_t max_output_bytes;
    uint64_t max_fee_atoms;
    uint64_t nonce;
    uint8_t network_access;
    uint8_t filesystem_write;
    uint8_t host_command_access;
} QrxComputeTaskDescriptor;


/* 0.0.9.2 standardized benchmark protocol.
   10,000 score points == 1.0 QRX Normalized Compute Unit (NCU) at the
   version-1 reference profile. Component scores are capped at 2.0 NCU so a
   single synthetic metric cannot dominate scheduling. These values are
   scheduler calibration constants, not hardware performance claims or reward
   entitlements. */
#define QRX_COMPUTE_BENCHMARK_VERSION 1U
#define QRX_COMPUTE_NCU_BASE 10000U
#define QRX_COMPUTE_COMPONENT_SCORE_CAP 20000U
#define QRX_COMPUTE_REF_BF16_MOPS 1000ULL
#define QRX_COMPUTE_REF_MXFP4_MOPS 1500ULL
#define QRX_COMPUTE_REF_MEMORY_MIB_S 10000ULL
#define QRX_COMPUTE_REF_STORAGE_MIB_S 1000ULL
#define QRX_COMPUTE_REF_NETWORK_MBIT_S 1000ULL
#define QRX_COMPUTE_REF_MOE_MILLI_TOKENS_S 10000ULL
#define QRX_COMPUTE_REF_INFERENCE_MILLI_TOKENS_S 10000ULL

#define QRX_COMPUTE_BENCH_SRC_CPU       (1U<<0)
#define QRX_COMPUTE_BENCH_SRC_MEMORY    (1U<<1)
#define QRX_COMPUTE_BENCH_SRC_STORAGE   (1U<<2)
#define QRX_COMPUTE_BENCH_SRC_NETWORK   (1U<<3)
#define QRX_COMPUTE_BENCH_SRC_MOE       (1U<<4)
#define QRX_COMPUTE_BENCH_SRC_REQUIRED  (QRX_COMPUTE_BENCH_SRC_CPU|QRX_COMPUTE_BENCH_SRC_MEMORY|QRX_COMPUTE_BENCH_SRC_STORAGE|QRX_COMPUTE_BENCH_SRC_NETWORK|QRX_COMPUTE_BENCH_SRC_MOE)

typedef struct {
    uint32_t version;
    uint32_t source_mask;
    uint32_t sample_count;
    uint64_t sample_window_ms;
    uint64_t bf16_mops;
    uint64_t mxfp4_mops;
    uint64_t memory_mib_s;
    uint64_t storage_mib_s;
    uint64_t network_mbit_s;
    uint64_t moe_milli_tokens_s;
    uint64_t inference_milli_tokens_s;
} QrxComputeBenchmarkMetrics;

typedef struct {
    uint32_t version;
    uint32_t normalized_score;
    uint32_t cpu_score;
    uint32_t memory_score;
    uint32_t storage_score;
    uint32_t network_score;
    uint32_t moe_score;
    uint32_t inference_score;
    uint32_t class_id;
} QrxComputeNormalizedScore;

#define QRX_COMPUTE_SCORE_CLASS_ENTRY 1U
#define QRX_COMPUTE_SCORE_CLASS_STANDARD 2U
#define QRX_COMPUTE_SCORE_CLASS_HIGH 3U
#define QRX_COMPUTE_SCORE_CLASS_EXTREME 4U



/* 0.0.9.3 AI Model Registry. Large model payloads remain in QRX Drive; the
   registry carries deterministic identities, content roots and execution
   requirements only. Registry records must never embed model weights. */
#define QRX_MODEL_REGISTRY_VERSION 1U
#define QRX_MODEL_MAX_ID 96
#define QRX_MODEL_MAX_VERSION 48
#define QRX_MODEL_MAX_ARCH 96
#define QRX_MODEL_MAX_RUNTIME 96
#define QRX_MODEL_MAX_LICENSE 96
#define QRX_MODEL_ROOT_HEX 64

#define QRX_MODEL_VERIFY_LOW 1U
#define QRX_MODEL_VERIFY_STANDARD 2U
#define QRX_MODEL_VERIFY_HIGH 3U

typedef struct {
    uint32_t registry_version;
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char architecture[QRX_MODEL_MAX_ARCH];
    char runtime_id[QRX_MODEL_MAX_RUNTIME];
    char manifest_root[QRX_MODEL_ROOT_HEX+1];
    char tokenizer_root[QRX_MODEL_ROOT_HEX+1];
    char expert_manifest_root[QRX_MODEL_ROOT_HEX+1]; /* all-zero root for non-MoE */
    uint64_t min_memory_bytes;
    uint64_t min_storage_bytes;
    uint32_t verification_profile;
    char license_id[QRX_MODEL_MAX_LICENSE];
    uint8_t is_moe;
} QrxAiModelRecord;

typedef struct {
    QrxAiModelRecord records[64];
    uint32_t count;
} QrxAiModelRegistry;

typedef struct {
    uint32_t kernel_class;
    uint64_t cpu_feature_flags;
    uint32_t recommended_parallel_tasks;
    uint64_t scheduling_memory_bytes;
} QrxComputeDispatchProfile;

int qrx_compute_capabilities_validate(const QrxComputeCapabilities *caps);
int qrx_compute_task_validate(const QrxComputeTaskDescriptor *task);
int qrx_compute_provider_eligible(const QrxComputeCapabilities *caps, const QrxComputeTaskDescriptor *task);
uint64_t qrx_compute_privacy_fingerprint(const QrxComputeCapabilities *caps);
const char *qrx_compute_os_name(QrxComputeOs os);
const char *qrx_compute_arch_name(QrxComputeArch arch);

/* 0.0.9.1: local, privacy-safe native capability discovery. */
int qrx_compute_detect_local_capabilities(const char *provider_id, QrxComputeCapabilities *out);
uint64_t qrx_compute_detect_cpu_features(void);
int qrx_compute_select_dispatch_profile(const QrxComputeCapabilities *caps, QrxComputeDispatchProfile *out);
const char *qrx_compute_kernel_name(uint32_t kernel_class);

/* 0.0.9.2: versioned benchmark schema and deterministic normalization. */
int qrx_compute_benchmark_validate(const QrxComputeBenchmarkMetrics *metrics);
int qrx_compute_benchmark_normalize(const QrxComputeBenchmarkMetrics *metrics, QrxComputeNormalizedScore *out);
const char *qrx_compute_score_class_name(uint32_t class_id);

/* 0.0.9.3: deterministic AI model registry. */
int qrx_ai_model_validate(const QrxAiModelRecord *model);
int qrx_ai_model_commitment(const QrxAiModelRecord *model, char out_hex[65]);
int qrx_ai_model_registry_add(QrxAiModelRegistry *registry, const QrxAiModelRecord *model);
const QrxAiModelRecord *qrx_ai_model_registry_find(const QrxAiModelRegistry *registry, const char *model_id, const char *model_version);

/* 0.0.9.4 Compute Job Protocol & deterministic Job Graphs.
   AURA, Wallet, dApps and external API clients submit the same graph format.
   The graph describes work; it does not grant host privileges. */
#define QRX_COMPUTE_JOB_PROTOCOL_VERSION 1U
#define QRX_COMPUTE_JOB_MAX_NODES 64U
#define QRX_COMPUTE_JOB_MAX_EDGES 256U
#define QRX_COMPUTE_JOB_MAX_REF 129U
#define QRX_COMPUTE_JOB_MAX_KIND 48U
#define QRX_COMPUTE_JOB_MAX_RUNTIME 96U
#define QRX_COMPUTE_JOB_MAX_MODEL_ID 96U
#define QRX_COMPUTE_JOB_MAX_MODEL_VERSION 48U

#define QRX_JOB_CAP_NONE            0U
#define QRX_JOB_CAP_READ_INPUTS     (1U<<0)
#define QRX_JOB_CAP_WRITE_ARTIFACTS (1U<<1)
#define QRX_JOB_CAP_STREAM_OUTPUT   (1U<<2)
#define QRX_JOB_CAP_MODEL_INFERENCE (1U<<3)
#define QRX_JOB_CAP_SANDBOX_EXEC    (1U<<4)
#define QRX_JOB_CAP_ALLOWED_V1      (QRX_JOB_CAP_READ_INPUTS|QRX_JOB_CAP_WRITE_ARTIFACTS|QRX_JOB_CAP_STREAM_OUTPUT|QRX_JOB_CAP_MODEL_INFERENCE|QRX_JOB_CAP_SANDBOX_EXEC)

typedef enum {
    QRX_JOB_AI_INFERENCE=1,
    QRX_JOB_CODE_GENERATION=2,
    QRX_JOB_CODE_EXECUTION=3,
    QRX_JOB_DOCUMENT_GENERATION=4,
    QRX_JOB_DATA_ANALYSIS=5,
    QRX_JOB_IMAGE_GENERATION=6,
    QRX_JOB_COMPILE=7,
    QRX_JOB_TEST=8,
    QRX_JOB_CONVERSION=9,
    QRX_JOB_RESEARCH=10,
    QRX_JOB_CUSTOM_SANDBOXED=11
} QrxComputeJobType;

typedef struct {
    uint32_t node_id;
    QrxComputeJobType job_type;
    char runtime_id[QRX_COMPUTE_JOB_MAX_RUNTIME];
    char model_id[QRX_COMPUTE_JOB_MAX_MODEL_ID];
    char model_version[QRX_COMPUTE_JOB_MAX_MODEL_VERSION];
    char input_ref[QRX_COMPUTE_JOB_MAX_REF];
    char output_ref[QRX_COMPUTE_JOB_MAX_REF];
    uint64_t min_memory_bytes;
    uint64_t max_runtime_ms;
    uint64_t max_output_bytes;
    uint64_t max_fee_atoms;
    uint32_t capability_mask;
} QrxComputeJobNode;

typedef struct {
    uint32_t from_node;
    uint32_t to_node;
} QrxComputeJobEdge;

typedef struct {
    uint32_t protocol_version;
    char graph_id[129];
    char owner[160];
    uint64_t nonce;
    uint64_t max_total_fee_atoms;
    uint64_t expiry_height;
    uint32_t node_count;
    uint32_t edge_count;
    QrxComputeJobNode nodes[QRX_COMPUTE_JOB_MAX_NODES];
    QrxComputeJobEdge edges[QRX_COMPUTE_JOB_MAX_EDGES];
} QrxComputeJobGraph;

int qrx_compute_job_node_validate(const QrxComputeJobNode *node);
int qrx_compute_job_graph_validate(const QrxComputeJobGraph *graph);
int qrx_compute_job_graph_commitment(const QrxComputeJobGraph *graph, char out_hex[65]);
int qrx_compute_job_graph_topological_order(const QrxComputeJobGraph *graph, uint32_t out_node_ids[QRX_COMPUTE_JOB_MAX_NODES]);
const char *qrx_compute_job_type_name(QrxComputeJobType type);


/* 0.0.9.5 Compute Market, Quotes & Escrow.
   Quotes are bounded by the job's pre-approved maximum fee. Provider ranking is
   deterministic and uses price, normalized benchmark score, reliability,
   latency, model locality and verification capability. Benchmark provenance and
   useful-compute verification are hardened in later phases; a quote itself is
   never proof that work was performed. */
#define QRX_COMPUTE_MARKET_VERSION 1U
#define QRX_COMPUTE_MAX_QUOTES 128U
#define QRX_COMPUTE_BPS 10000U
#define QRX_FASTTRACK_PROVIDER_SHARE_BPS 9000U /* 90% of FastTrack surcharge rewards the provider that prioritizes the job */
#define QRX_FASTTRACK_NETWORK_SHARE_BPS 950U  /* 9.5% joins the ordinary network/fee pool */
#define QRX_FASTTRACK_DEV_SHARE_BPS 50U       /* 0.5% of FastTrack surcharge only */
#define QRX_FASTTRACK_MAX_PREMIUM_BPS 2500U   /* user-authorized surcharge may not exceed 25% of max compute budget */

#define QRX_COMPUTE_VERIFY_LOW      1U
#define QRX_COMPUTE_VERIFY_STANDARD 2U
#define QRX_COMPUTE_VERIFY_HIGH     3U

typedef struct {
    uint32_t market_version;
    char quote_id[129];
    char provider_id[129];
    char graph_commitment[65];
    uint32_t node_id;
    uint64_t price_atoms;
    uint64_t expiry_height;
    uint32_t normalized_compute_score;
    uint32_t reliability_bps;
    uint32_t estimated_latency_ms;
    uint32_t verification_level;
    uint8_t model_local;
    uint8_t fasttrack_available;
} QrxComputeQuote;

typedef struct {
    QrxComputeQuote quotes[QRX_COMPUTE_MAX_QUOTES];
    uint32_t count;
} QrxComputeQuoteBook;

typedef enum {
    QRX_COMPUTE_ESCROW_EMPTY=0,
    QRX_COMPUTE_ESCROW_LOCKED=1,
    QRX_COMPUTE_ESCROW_ASSIGNED=2,
    QRX_COMPUTE_ESCROW_SETTLED=3,
    QRX_COMPUTE_ESCROW_REFUNDED=4,
    QRX_COMPUTE_ESCROW_CANCELLED=5,
    QRX_COMPUTE_ESCROW_EXPIRED=6
} QrxComputeEscrowState;

typedef struct {
    uint32_t market_version;
    char graph_commitment[65];
    char owner[160];
    uint64_t max_compute_atoms;
    uint64_t fasttrack_fee_atoms;
    /* Optional user-authorized verification/challenge micro-reward budget.
       It is separate from provider compute price and FastTrack economics. */
    uint64_t verification_reward_atoms;
    uint64_t verification_reward_spent_atoms;
    uint64_t locked_atoms;
    uint64_t actual_compute_atoms;
    uint64_t fasttrack_dev_atoms;
    uint64_t fasttrack_net_atoms;
    uint64_t refund_atoms;
    uint64_t expiry_height;
    QrxComputeEscrowState state;
} QrxComputeEscrow;

int qrx_compute_quote_validate(const QrxComputeQuote *quote, const QrxComputeJobGraph *graph, uint64_t current_height);
int qrx_compute_quote_book_add(QrxComputeQuoteBook *book, const QrxComputeQuote *quote, const QrxComputeJobGraph *graph, uint64_t current_height);
int qrx_compute_quote_rank_score(const QrxComputeQuote *quote, const QrxComputeJobNode *node, uint32_t *out_score);
const QrxComputeQuote *qrx_compute_quote_select(const QrxComputeQuoteBook *book, const QrxComputeJobGraph *graph, uint32_t node_id, uint32_t min_verification_level, uint8_t require_model_local, uint8_t require_fasttrack, uint64_t current_height);
int qrx_compute_escrow_lock(QrxComputeEscrow *escrow, const QrxComputeJobGraph *graph, uint64_t fasttrack_fee_atoms, uint64_t current_height);
int qrx_compute_escrow_mark_assigned(QrxComputeEscrow *escrow);
int qrx_compute_escrow_settle(QrxComputeEscrow *escrow, uint64_t actual_compute_atoms, uint64_t current_height);
int qrx_compute_escrow_refund(QrxComputeEscrow *escrow, uint64_t current_height);
const char *qrx_compute_escrow_state_name(QrxComputeEscrowState state);

/* 0.0.9.6 Secure Execution Sandbox + resumable budget guard.
   This is the consensus/runtime-neutral policy layer. Platform executors must
   enforce this policy before launching untrusted code. Wallet keys and host
   secrets are never grantable capabilities. */
#define QRX_SANDBOX_POLICY_VERSION 1U
#define QRX_SANDBOX_MAX_MOUNTS 8U
#define QRX_SANDBOX_MAX_REF 129U
#define QRX_BUDGET_WARNING_BPS 8000U
#define QRX_BUDGET_CRITICAL_BPS 9500U

#define QRX_SANDBOX_ALLOW_INPUT_READ      (1U<<0)
#define QRX_SANDBOX_ALLOW_ARTIFACT_WRITE  (1U<<1)
#define QRX_SANDBOX_ALLOW_NETWORK         (1U<<2)
#define QRX_SANDBOX_ALLOW_SUBPROCESS      (1U<<3)
#define QRX_SANDBOX_ALLOW_MODEL_READ      (1U<<4)
#define QRX_SANDBOX_ALLOWED_V1 (QRX_SANDBOX_ALLOW_INPUT_READ|QRX_SANDBOX_ALLOW_ARTIFACT_WRITE|QRX_SANDBOX_ALLOW_NETWORK|QRX_SANDBOX_ALLOW_SUBPROCESS|QRX_SANDBOX_ALLOW_MODEL_READ)

typedef struct {
    uint32_t version;
    uint32_t permissions;
    uint64_t memory_limit_bytes;
    uint64_t cpu_time_limit_ms;
    uint64_t wall_time_limit_ms;
    uint64_t output_limit_bytes;
    uint32_t max_subprocesses;
    uint8_t wallet_keys_visible;   /* must always be 0 */
    uint8_t host_secrets_visible;  /* must always be 0 */
    uint8_t host_fs_visible;       /* must always be 0 */
    uint8_t unrestricted_network;  /* must always be 0; network uses allowlisted proxy later */
} QrxComputeSandboxPolicy;

typedef enum {
    QRX_BUDGET_RUNNING=1,
    QRX_BUDGET_WARNING=2,
    QRX_BUDGET_CHECKPOINTING=3,
    QRX_BUDGET_PAUSED_EXHAUSTED=4,
    QRX_BUDGET_EXTENDED=5,
    QRX_BUDGET_RESUMING=6,
    QRX_BUDGET_COMPLETED=7
} QrxComputeBudgetState;

typedef struct {
    uint64_t authorized_atoms;
    uint64_t verified_spent_atoms;
    uint64_t extension_atoms;
    uint64_t checkpoint_seq;
    char checkpoint_ref[QRX_SANDBOX_MAX_REF];
    QrxComputeBudgetState state;
} QrxComputeBudgetGuard;

int qrx_compute_sandbox_policy_default(const QrxComputeJobNode *node, QrxComputeSandboxPolicy *out);
int qrx_compute_sandbox_policy_validate(const QrxComputeJobNode *node, const QrxComputeSandboxPolicy *policy);
int qrx_compute_budget_guard_init(QrxComputeBudgetGuard *guard, uint64_t authorized_atoms);
int qrx_compute_budget_guard_charge(QrxComputeBudgetGuard *guard, uint64_t verified_total_atoms);
int qrx_compute_budget_guard_checkpoint(QrxComputeBudgetGuard *guard, const char *checkpoint_ref);
int qrx_compute_budget_guard_extend(QrxComputeBudgetGuard *guard, uint64_t additional_atoms);
int qrx_compute_budget_guard_resume(QrxComputeBudgetGuard *guard);
int qrx_compute_budget_guard_complete(QrxComputeBudgetGuard *guard);
const char *qrx_compute_budget_state_name(QrxComputeBudgetState state);


/* 0.0.9.7 Proof of Useful Compute (PoUC) verification foundation. */
#define QRX_POUC_VERSION 1U
#define QRX_POUC_MAX_PROVIDER 160U
#define QRX_POUC_MAX_RUNTIME 96U
#define QRX_POUC_HASH_HEX 64U
#define QRX_POUC_VERIFY_SPOT 1U
#define QRX_POUC_VERIFY_RANDOM 2U
#define QRX_POUC_VERIFY_REDUNDANT_2_OF_3 3U

typedef struct {
    uint32_t version;
    char graph_commitment[65];
    uint32_t node_id;
    char provider_id[QRX_POUC_MAX_PROVIDER];
    char input_commitment[65];
    char model_commitment[65];
    char runtime_id[QRX_POUC_MAX_RUNTIME];
    char execution_params_commitment[65];
    char result_commitment[65];
    uint64_t verified_compute_atoms;
    uint64_t started_height;
    uint64_t completed_height;
    uint32_t verification_mode;
} QrxPoucReceipt;

typedef struct {
    char receipt_commitment[65];
    uint32_t verifier_count;
    uint32_t matching_verifiers;
    uint8_t challenge_required;
    uint8_t accepted;
} QrxPoucVerification;

int qrx_pouc_receipt_validate(const QrxPoucReceipt *receipt);
int qrx_pouc_receipt_commitment(const QrxPoucReceipt *receipt, char out_hex[65]);
int qrx_pouc_verify_result(const QrxPoucReceipt *receipt, uint32_t verifier_count, uint32_t matching_verifiers, QrxPoucVerification *out);


/* 0.0.9.8 AURA Agent Runtime & Tool API.
   AURA is an orchestrator/client of the QRX Compute Job Protocol, never a
   privileged execution backdoor. Agent manifests bind model identity,
   capabilities and hard spend limits. Tool calls may create ordinary job
   nodes but cannot grant wallet-key, host-secret or unrestricted host access. */
#define QRX_AURA_RUNTIME_VERSION 1U
#define QRX_AURA_MAX_ID 129U
#define QRX_AURA_MAX_OWNER 160U
#define QRX_AURA_MAX_REF 129U
#define QRX_AURA_MAX_STEPS 1024U

#define QRX_AURA_CAP_CHAT             (1U<<0)
#define QRX_AURA_CAP_MODEL_INFERENCE  (1U<<1)
#define QRX_AURA_CAP_READ_ARTIFACT    (1U<<2)
#define QRX_AURA_CAP_WRITE_ARTIFACT   (1U<<3)
#define QRX_AURA_CAP_SUBMIT_JOB       (1U<<4)
#define QRX_AURA_CAP_STREAM_RESULT    (1U<<5)
#define QRX_AURA_CAP_RESEARCH         (1U<<6)
#define QRX_AURA_CAP_CODE             (1U<<7)
#define QRX_AURA_CAP_ALLOWED_V1 (QRX_AURA_CAP_CHAT|QRX_AURA_CAP_MODEL_INFERENCE|QRX_AURA_CAP_READ_ARTIFACT|QRX_AURA_CAP_WRITE_ARTIFACT|QRX_AURA_CAP_SUBMIT_JOB|QRX_AURA_CAP_STREAM_RESULT|QRX_AURA_CAP_RESEARCH|QRX_AURA_CAP_CODE)

typedef enum {
    QRX_AURA_TOOL_MODEL_INFERENCE=1,
    QRX_AURA_TOOL_READ_ARTIFACT=2,
    QRX_AURA_TOOL_WRITE_ARTIFACT=3,
    QRX_AURA_TOOL_SUBMIT_JOB=4,
    QRX_AURA_TOOL_JOB_STATUS=5,
    QRX_AURA_TOOL_STREAM_RESULT=6,
    QRX_AURA_TOOL_RESEARCH=7,
    QRX_AURA_TOOL_CODE_GENERATION=8,
    QRX_AURA_TOOL_CODE_EXECUTION=9,
    QRX_AURA_TOOL_COMPILE=10,
    QRX_AURA_TOOL_TEST=11
} QrxAuraToolType;

typedef struct {
    uint32_t version;
    char agent_id[QRX_AURA_MAX_ID];
    char owner[QRX_AURA_MAX_OWNER];
    char model_id[QRX_COMPUTE_JOB_MAX_MODEL_ID];
    char model_version[QRX_COMPUTE_JOB_MAX_MODEL_VERSION];
    uint32_t capability_mask;
    uint32_t max_steps;
    uint64_t max_job_fee_atoms;
    uint64_t max_session_fee_atoms;
    uint8_t require_user_approval_for_code_execution;
} QrxAuraAgentManifest;

typedef enum {
    QRX_AURA_SESSION_OPEN=1,
    QRX_AURA_SESSION_BUDGET_WARNING=2,
    QRX_AURA_SESSION_PAUSED=3,
    QRX_AURA_SESSION_COMPLETED=4,
    QRX_AURA_SESSION_CANCELLED=5
} QrxAuraSessionState;

typedef struct {
    uint32_t version;
    char session_id[QRX_AURA_MAX_ID];
    char agent_commitment[65];
    char owner[QRX_AURA_MAX_OWNER];
    uint64_t authorized_atoms;
    uint64_t spent_atoms;
    uint32_t step_count;
    QrxAuraSessionState state;
} QrxAuraSession;

typedef struct {
    uint32_t version;
    char call_id[QRX_AURA_MAX_ID];
    char session_id[QRX_AURA_MAX_ID];
    QrxAuraToolType tool_type;
    char input_ref[QRX_AURA_MAX_REF];
    char output_ref[QRX_AURA_MAX_REF];
    char runtime_id[QRX_COMPUTE_JOB_MAX_RUNTIME];
    uint64_t requested_fee_atoms;
    uint64_t max_runtime_ms;
    uint64_t max_output_bytes;
    uint8_t user_approved;
} QrxAuraToolCall;

int qrx_aura_agent_manifest_validate(const QrxAuraAgentManifest *manifest);
int qrx_aura_agent_manifest_commitment(const QrxAuraAgentManifest *manifest, char out_hex[65]);
int qrx_aura_session_open(const QrxAuraAgentManifest *manifest, const char *session_id, uint64_t authorized_atoms, QrxAuraSession *out);
int qrx_aura_session_charge(QrxAuraSession *session, uint64_t verified_total_atoms);
int qrx_aura_session_record_step(const QrxAuraAgentManifest *manifest, QrxAuraSession *session);
int qrx_aura_session_complete(QrxAuraSession *session);
int qrx_aura_session_cancel(QrxAuraSession *session);
int qrx_aura_tool_call_validate(const QrxAuraAgentManifest *manifest, const QrxAuraSession *session, const QrxAuraToolCall *call);
int qrx_aura_tool_call_commitment(const QrxAuraToolCall *call, char out_hex[65]);
int qrx_aura_tool_call_to_job_node(const QrxAuraAgentManifest *manifest, const QrxAuraSession *session, const QrxAuraToolCall *call, uint32_t node_id, QrxComputeJobNode *out);
const char *qrx_aura_tool_type_name(QrxAuraToolType type);
const char *qrx_aura_session_state_name(QrxAuraSessionState state);


/* 0.0.9.10 AURA Chat Protocol Foundation.
   Conversations are local/encrypted-client metadata plus content references;
   plaintext prompts are not consensus objects. Messages bind compute jobs and
   generated artifacts without making AURA a privileged compute path. */
#define QRX_AURA_CHAT_VERSION 1U
#define QRX_AURA_CHAT_MAX_TITLE 128U
#define QRX_AURA_CHAT_MAX_MESSAGES 512U
#define QRX_AURA_CHAT_MAX_ARTIFACTS 16U
#define QRX_AURA_CHAT_MAX_JOB_REF 129U

typedef enum {
    QRX_AURA_CHAT_ROLE_SYSTEM=1,
    QRX_AURA_CHAT_ROLE_USER=2,
    QRX_AURA_CHAT_ROLE_ASSISTANT=3,
    QRX_AURA_CHAT_ROLE_TOOL=4
} QrxAuraChatRole;

typedef enum {
    QRX_AURA_CHAT_MSG_PENDING=1,
    QRX_AURA_CHAT_MSG_STREAMING=2,
    QRX_AURA_CHAT_MSG_COMPLETE=3,
    QRX_AURA_CHAT_MSG_CANCELLED=4,
    QRX_AURA_CHAT_MSG_FAILED=5
} QrxAuraChatMessageState;

typedef struct {
    uint32_t version;
    char conversation_id[QRX_AURA_MAX_ID];
    char owner[QRX_AURA_MAX_OWNER];
    char session_id[QRX_AURA_MAX_ID];
    char title[QRX_AURA_CHAT_MAX_TITLE];
    char context_ref[QRX_AURA_MAX_REF];
    uint64_t max_conversation_fee_atoms;
    uint64_t spent_atoms;
    uint32_t message_count;
    uint8_t encrypted_context;
} QrxAuraConversation;

typedef struct {
    uint32_t version;
    char message_id[QRX_AURA_MAX_ID];
    char conversation_id[QRX_AURA_MAX_ID];
    char parent_message_id[QRX_AURA_MAX_ID]; /* empty for root */
    QrxAuraChatRole role;
    QrxAuraChatMessageState state;
    char content_ref[QRX_AURA_MAX_REF];
    char job_ref[QRX_AURA_CHAT_MAX_JOB_REF];
    char artifact_refs[QRX_AURA_CHAT_MAX_ARTIFACTS][QRX_AURA_MAX_REF];
    uint32_t artifact_count;
    uint64_t authorized_fee_atoms;
    uint64_t verified_fee_atoms;
    uint64_t stream_sequence;
} QrxAuraChatMessage;

int qrx_aura_conversation_open(const QrxAuraAgentManifest *manifest, const QrxAuraSession *session, const char *conversation_id, const char *title, const char *context_ref, uint64_t max_fee_atoms, QrxAuraConversation *out);
int qrx_aura_conversation_validate(const QrxAuraConversation *conversation);
int qrx_aura_chat_message_validate(const QrxAuraConversation *conversation, const QrxAuraChatMessage *message);
int qrx_aura_chat_message_commitment(const QrxAuraChatMessage *message, char out_hex[65]);
int qrx_aura_chat_begin_assistant(const QrxAuraConversation *conversation, const char *message_id, const char *parent_message_id, const char *input_ref, const char *output_ref, uint64_t fee_atoms, QrxAuraToolCall *out_call, QrxAuraChatMessage *out_message);
int qrx_aura_chat_stream_advance(QrxAuraChatMessage *message, uint64_t next_sequence);
int qrx_aura_chat_complete(QrxAuraConversation *conversation, QrxAuraChatMessage *message, uint64_t verified_fee_atoms, const char *job_ref);
int qrx_aura_chat_cancel(QrxAuraChatMessage *message);
int qrx_aura_chat_add_artifact(QrxAuraChatMessage *message, const char *artifact_ref);
int qrx_aura_chat_branch(const QrxAuraConversation *source, const QrxAuraChatMessage *from, const char *new_conversation_id, QrxAuraConversation *out);


/* 0.0.9.11 AURA Files / Projects / Artifact Creation.
   Artifacts are immutable metadata records pointing to QRX Drive objects.
   Project entries bind safe relative paths to artifact commitments. Plaintext
   file contents remain outside consensus metadata and can stay PRIVATE_PQ. */
#define QRX_AURA_ARTIFACT_VERSION 1U
#define QRX_AURA_PROJECT_VERSION 1U
#define QRX_AURA_ARTIFACT_MAX_NAME 192U
#define QRX_AURA_ARTIFACT_MAX_MIME 96U
#define QRX_AURA_ARTIFACT_MAX_EXT 16U
#define QRX_AURA_PROJECT_MAX_FILES 128U
#define QRX_AURA_PROJECT_MAX_PATH 256U

typedef enum {
    QRX_AURA_ARTIFACT_TEXT=1,
    QRX_AURA_ARTIFACT_DOCUMENT=2,
    QRX_AURA_ARTIFACT_SPREADSHEET=3,
    QRX_AURA_ARTIFACT_PRESENTATION=4,
    QRX_AURA_ARTIFACT_CODE=5,
    QRX_AURA_ARTIFACT_ARCHIVE=6,
    QRX_AURA_ARTIFACT_IMAGE=7,
    QRX_AURA_ARTIFACT_DATA=8,
    QRX_AURA_ARTIFACT_OTHER=9
} QrxAuraArtifactType;

typedef struct {
    uint32_t version;
    char artifact_id[QRX_AURA_MAX_ID];
    char owner[QRX_AURA_MAX_OWNER];
    char name[QRX_AURA_ARTIFACT_MAX_NAME];
    char mime_type[QRX_AURA_ARTIFACT_MAX_MIME];
    char extension[QRX_AURA_ARTIFACT_MAX_EXT];
    QrxAuraArtifactType type;
    char drive_ref[QRX_AURA_MAX_REF];
    char source_job_ref[QRX_AURA_CHAT_MAX_JOB_REF];
    char content_commitment[65];
    uint64_t size_bytes;
    uint8_t private_pq;
    uint8_t immutable;
} QrxAuraArtifact;

typedef struct {
    char path[QRX_AURA_PROJECT_MAX_PATH];
    char artifact_commitment[65];
} QrxAuraProjectFile;

typedef struct {
    uint32_t version;
    char project_id[QRX_AURA_MAX_ID];
    char owner[QRX_AURA_MAX_OWNER];
    char name[QRX_AURA_ARTIFACT_MAX_NAME];
    char root_ref[QRX_AURA_MAX_REF];
    QrxAuraProjectFile files[QRX_AURA_PROJECT_MAX_FILES];
    uint32_t file_count;
    uint8_t private_pq;
} QrxAuraProject;

int qrx_aura_artifact_validate(const QrxAuraArtifact *artifact);
int qrx_aura_artifact_commitment(const QrxAuraArtifact *artifact, char out_hex[65]);
int qrx_aura_project_validate(const QrxAuraProject *project);
int qrx_aura_project_add_file(QrxAuraProject *project, const char *relative_path, const QrxAuraArtifact *artifact);
int qrx_aura_project_commitment(const QrxAuraProject *project, char out_hex[65]);
int qrx_aura_artifact_prepare_write_call(const QrxAuraConversation *conversation, const QrxAuraArtifact *artifact, const char *call_id, const char *input_ref, uint64_t fee_atoms, QrxAuraToolCall *out_call);
int qrx_aura_artifact_attach_to_chat(QrxAuraChatMessage *message, const QrxAuraArtifact *artifact);


/* 0.0.9.12 AURA Code Generation, Build & Test Workspace.
   A workspace is an immutable project snapshot plus an explicit staged plan.
   Generated code may be proposed without host execution; compile/test stages
   require the normal AURA approval path and sandboxed compute jobs. */
#define QRX_AURA_CODE_WORKSPACE_VERSION 1U
#define QRX_AURA_CODE_MAX_STAGES 16U
#define QRX_AURA_CODE_MAX_LABEL 96U
#define QRX_AURA_CODE_MAX_RUNTIME 96U

#define QRX_AURA_CODE_STAGE_GENERATE (1U<<0)
#define QRX_AURA_CODE_STAGE_EXECUTE  (1U<<1)
#define QRX_AURA_CODE_STAGE_COMPILE  (1U<<2)
#define QRX_AURA_CODE_STAGE_TEST     (1U<<3)
#define QRX_AURA_CODE_STAGE_ALLOWED_V1 (QRX_AURA_CODE_STAGE_GENERATE|QRX_AURA_CODE_STAGE_EXECUTE|QRX_AURA_CODE_STAGE_COMPILE|QRX_AURA_CODE_STAGE_TEST)

typedef enum {
    QRX_AURA_CODE_PLAN_DRAFT=1,
    QRX_AURA_CODE_PLAN_APPROVED=2,
    QRX_AURA_CODE_PLAN_RUNNING=3,
    QRX_AURA_CODE_PLAN_COMPLETED=4,
    QRX_AURA_CODE_PLAN_FAILED=5,
    QRX_AURA_CODE_PLAN_CANCELLED=6
} QrxAuraCodePlanState;

typedef struct {
    uint32_t stage_id;
    uint32_t stage_mask; /* exactly one QRX_AURA_CODE_STAGE_* bit */
    char label[QRX_AURA_CODE_MAX_LABEL];
    char input_ref[QRX_AURA_MAX_REF];
    char output_ref[QRX_AURA_MAX_REF];
    char runtime_id[QRX_AURA_CODE_MAX_RUNTIME];
    uint64_t max_fee_atoms;
    uint64_t max_runtime_ms;
    uint64_t max_output_bytes;
    uint8_t requires_user_approval;
} QrxAuraCodeStage;

typedef struct {
    uint32_t version;
    char workspace_id[QRX_AURA_MAX_ID];
    char owner[QRX_AURA_MAX_OWNER];
    char project_commitment[65];
    char source_project_ref[QRX_AURA_MAX_REF];
    char patch_artifact_ref[QRX_AURA_MAX_REF];
    char build_log_ref[QRX_AURA_MAX_REF];
    char test_log_ref[QRX_AURA_MAX_REF];
    uint64_t max_total_fee_atoms;
    uint32_t stage_count;
    QrxAuraCodeStage stages[QRX_AURA_CODE_MAX_STAGES];
    QrxAuraCodePlanState state;
    uint8_t user_approved_execution;
} QrxAuraCodeWorkspace;

int qrx_aura_code_workspace_init(const QrxAuraProject *project, const char *workspace_id, const char *source_project_ref, uint64_t max_total_fee_atoms, QrxAuraCodeWorkspace *out);
int qrx_aura_code_workspace_validate(const QrxAuraCodeWorkspace *workspace);
int qrx_aura_code_workspace_add_stage(QrxAuraCodeWorkspace *workspace, uint32_t stage_mask, const char *label, const char *input_ref, const char *output_ref, const char *runtime_id, uint64_t max_fee_atoms, uint64_t max_runtime_ms, uint64_t max_output_bytes);
int qrx_aura_code_workspace_approve(QrxAuraCodeWorkspace *workspace, uint8_t approve_execution);
int qrx_aura_code_workspace_commitment(const QrxAuraCodeWorkspace *workspace, char out_hex[65]);
int qrx_aura_code_stage_prepare_tool_call(const QrxAuraCodeWorkspace *workspace, const QrxAuraConversation *conversation, uint32_t stage_index, const char *call_id, QrxAuraToolCall *out_call);
const char *qrx_aura_code_plan_state_name(QrxAuraCodePlanState state);


/* 0.0.9.13 Workspace Execution Lifecycle / Patch-Diff / Build-Test Results.
   Runtime state is kept separate from the immutable workspace plan. Every
   stage result is bound to output/log artifacts and actual charged compute. */
#define QRX_AURA_CODE_EXECUTION_VERSION 1U
#define QRX_AURA_CODE_RESULT_MAX_SUMMARY 192U

typedef enum {
    QRX_AURA_CODE_STAGE_PENDING=1,
    QRX_AURA_CODE_STAGE_RUNNING=2,
    QRX_AURA_CODE_STAGE_PASSED=3,
    QRX_AURA_CODE_STAGE_FAILED=4,
    QRX_AURA_CODE_STAGE_CANCELLED=5
} QrxAuraCodeStageStatus;

typedef struct {
    uint32_t stage_id;
    QrxAuraCodeStageStatus status;
    char output_ref[QRX_AURA_MAX_REF];
    char log_ref[QRX_AURA_MAX_REF];
    char result_commitment[65];
    char summary[QRX_AURA_CODE_RESULT_MAX_SUMMARY];
    int32_t exit_code;
    uint64_t charged_fee_atoms;
    uint64_t started_ms;
    uint64_t finished_ms;
} QrxAuraCodeStageResult;

typedef struct {
    uint32_t version;
    char execution_id[QRX_AURA_MAX_ID];
    char workspace_commitment[65];
    char owner[QRX_AURA_MAX_OWNER];
    uint32_t current_stage_index;
    uint32_t stage_count;
    QrxAuraCodeStageResult results[QRX_AURA_CODE_MAX_STAGES];
    char patch_artifact_ref[QRX_AURA_MAX_REF];
    char diff_artifact_ref[QRX_AURA_MAX_REF];
    char build_report_ref[QRX_AURA_MAX_REF];
    char test_report_ref[QRX_AURA_MAX_REF];
    uint64_t total_charged_atoms;
    QrxAuraCodePlanState state;
    uint8_t final_user_approved;
} QrxAuraCodeExecution;

int qrx_aura_code_execution_init(const QrxAuraCodeWorkspace *workspace, const char *execution_id, QrxAuraCodeExecution *out);
int qrx_aura_code_execution_validate(const QrxAuraCodeWorkspace *workspace, const QrxAuraCodeExecution *execution);
int qrx_aura_code_execution_start_stage(const QrxAuraCodeWorkspace *workspace, QrxAuraCodeExecution *execution, uint32_t stage_index, uint64_t started_ms);
int qrx_aura_code_execution_record_result(const QrxAuraCodeWorkspace *workspace, QrxAuraCodeExecution *execution, uint32_t stage_index, QrxAuraCodeStageStatus status, const char *output_ref, const char *log_ref, const char *result_commitment, const char *summary, int32_t exit_code, uint64_t charged_fee_atoms, uint64_t finished_ms);
int qrx_aura_code_execution_set_reports(QrxAuraCodeExecution *execution, const char *patch_ref, const char *diff_ref, const char *build_ref, const char *test_ref);
int qrx_aura_code_execution_finalize(const QrxAuraCodeWorkspace *workspace, QrxAuraCodeExecution *execution);
int qrx_aura_code_execution_user_approve(QrxAuraCodeExecution *execution, uint8_t approve);
int qrx_aura_code_execution_commitment(const QrxAuraCodeWorkspace *workspace, const QrxAuraCodeExecution *execution, char out_hex[65]);
const char *qrx_aura_code_stage_status_name(QrxAuraCodeStageStatus status);

#ifdef __cplusplus
}
#endif

/* 0.0.9.14 Heterogeneous MoE Pods / Traffic Optimization / Optimistic PoUC.
   Nodes expose variable expert slots; no fixed minimum expert count is required.
   Expert parallelism is preferred before tensor fragmentation. Verification may
   be optimistic and asynchronous; execution receipts remain challengeable. */
#define QRX_MOE_PLACEMENT_VERSION 1U
#define QRX_MOE_MAX_EXPERT_SLOTS 64U
#define QRX_MOE_MAX_REPLICAS 16U
#define QRX_MOE_MAX_POD_ID 96U
#define QRX_MOE_MAX_BACKEND 48U
#define QRX_MOE_MAX_MODEL_ID 96U

#define QRX_MOE_CAP_CPU       (1U<<0)
#define QRX_MOE_CAP_GPU       (1U<<1)
#define QRX_MOE_CAP_NPU       (1U<<2)
#define QRX_MOE_CAP_METAL     (1U<<3)
#define QRX_MOE_CAP_MLX       (1U<<4)
#define QRX_MOE_CAP_CUDA      (1U<<5)
#define QRX_MOE_CAP_NEON      (1U<<6)
#define QRX_MOE_CAP_AVX2      (1U<<7)
#define QRX_MOE_CAP_AVX512    (1U<<8)
#define QRX_MOE_CAP_BF16      (1U<<9)

#define QRX_MOE_ROLE_ROUTE        (1U<<0)
#define QRX_MOE_ROLE_EXPERT       (1U<<1)
#define QRX_MOE_ROLE_VERIFY       (1U<<2)
#define QRX_MOE_ROLE_PREPOST      (1U<<3)
#define QRX_MOE_ROLE_SMALL_MODEL  (1U<<4)
#define QRX_MOE_ROLE_CACHE        (1U<<5)
#define QRX_MOE_ROLE_AGGREGATE    (1U<<6)

#define QRX_POUC_OPT_MIN_CHALLENGE_BPS 50U
#define QRX_POUC_OPT_MAX_CHALLENGE_BPS 10000U

typedef enum { QRX_MOE_NODE_MICRO=1, QRX_MOE_NODE_LIGHT=2, QRX_MOE_NODE_STANDARD=3, QRX_MOE_NODE_HOT=4 } QrxMoeNodeClass;
typedef enum { QRX_MOE_RES_REMOTE=1, QRX_MOE_RES_NVME=2, QRX_MOE_RES_RAM=3, QRX_MOE_RES_VRAM=4 } QrxMoeResidency;
typedef enum { QRX_MOE_CACHE_COLD=1, QRX_MOE_CACHE_WARM=2, QRX_MOE_CACHE_HOT=3 } QrxMoeCacheState;
typedef enum { QRX_ACT_FP16=1, QRX_ACT_BF16=2, QRX_ACT_FP8=3, QRX_ACT_INT8=4, QRX_ACT_INT4=5 } QrxActivationEncoding;
typedef enum { QRX_POUC_OPT_FAST=1, QRX_POUC_OPT_STANDARD=2, QRX_POUC_OPT_HIGH=3, QRX_POUC_OPT_CONSENSUS=4 } QrxPoucOptimisticTier;

typedef struct {
    uint32_t version;
    char node_id[129];
    char pod_id[QRX_MOE_MAX_POD_ID];
    QrxComputeArch arch;
    QrxMoeNodeClass node_class;
    uint32_t capability_mask;
    uint32_t role_mask;
    uint64_t memory_total_bytes;
    uint64_t memory_available_bytes;
    uint64_t nvme_available_bytes;
    uint64_t accelerator_memory_available_bytes;
    uint32_t latency_us_to_pod;
    uint32_t bandwidth_mbps_to_pod;
    char backend[QRX_MOE_MAX_BACKEND];
} QrxMoeNodeProfile;

typedef struct {
    uint32_t version;
    char model_id[QRX_MOE_MAX_MODEL_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    uint32_t layer_id;
    uint32_t expert_id;
    uint32_t fragment_id; /* 0 = complete expert for this layer; >0 tensor fragment */
    uint32_t fragment_count;
    char content_root[65];
    uint64_t size_bytes;
} QrxMoeExpertFragment;

typedef struct {
    uint32_t version;
    char node_id[129];
    char pod_id[QRX_MOE_MAX_POD_ID];
    QrxMoeExpertFragment fragment;
    QrxMoeResidency residency;
    QrxMoeCacheState cache_state;
    uint32_t replica_ordinal;
    uint32_t measured_latency_us;
} QrxMoeExpertPlacement;

typedef struct {
    QrxPoucOptimisticTier tier;
    uint32_t challenge_bps;
    uint32_t redundant_workers;
    uint8_t async_verification;
    uint8_t allow_optimistic_forward;
} QrxPoucOptimisticPolicy;

int qrx_moe_node_profile_validate(const QrxMoeNodeProfile *profile);
QrxMoeNodeClass qrx_moe_classify_memory(uint64_t memory_total_bytes);
int qrx_moe_fragment_validate(const QrxMoeExpertFragment *fragment);
int qrx_moe_placement_validate(const QrxMoeNodeProfile *node, const QrxMoeExpertPlacement *placement);
int qrx_moe_placement_commitment(const QrxMoeExpertPlacement *placement, char out_hex[65]);
uint64_t qrx_moe_activation_wire_bytes(uint64_t element_count, QrxActivationEncoding encoding);
uint64_t qrx_moe_placement_score(const QrxMoeNodeProfile *node, const QrxMoeExpertPlacement *placement);
int qrx_pouc_optimistic_policy(QrxPoucOptimisticTier tier, uint32_t provider_reliability_bps, QrxPoucOptimisticPolicy *out);

/* 0.0.9.15 MoE Pod Coordinator / Co-Activation Locality / Predictive Prefetch.
   The coordinator never changes the model router decision. It only chooses
   where already-selected experts execute and which likely-next fragments are
   warmed ahead of time. All scoring is deterministic and integer-only. */
#define QRX_MOE_COORDINATOR_VERSION 1U
#define QRX_MOE_MAX_COACTIVATION 256U
#define QRX_MOE_MAX_PREFETCH 32U
#define QRX_MOE_MAX_SELECTED_EXPERTS 32U

typedef enum {
    QRX_MOE_PREFETCH_NONE=0,
    QRX_MOE_PREFETCH_NVME_TO_RAM=1,
    QRX_MOE_PREFETCH_RAM_TO_VRAM=2,
    QRX_MOE_PREFETCH_REMOTE_TO_NVME=3,
    QRX_MOE_PREFETCH_REMOTE_TO_RAM=4
} QrxMoePrefetchAction;

typedef struct {
    uint32_t version;
    char model_id[QRX_MOE_MAX_MODEL_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    uint32_t layer_id;
    uint32_t expert_a;
    uint32_t expert_b;
    uint64_t coactivation_count;
    uint64_t observation_count;
} QrxMoeCoactivationStat;

typedef struct {
    uint32_t version;
    char pod_id[QRX_MOE_MAX_POD_ID];
    uint32_t current_layer_id;
    uint32_t next_layer_id;
    uint32_t selected_experts[QRX_MOE_MAX_SELECTED_EXPERTS];
    uint32_t selected_count;
    uint32_t max_prefetch;
    uint64_t max_prefetch_bytes;
    QrxActivationEncoding activation_encoding;
} QrxMoeCoordinatorRequest;

typedef struct {
    uint32_t version;
    QrxMoeExpertPlacement placement;
    QrxMoePrefetchAction action;
    uint64_t locality_score;
    uint64_t predicted_bps;
    uint64_t estimated_wire_bytes;
} QrxMoePrefetchItem;

typedef struct {
    uint32_t version;
    char pod_id[QRX_MOE_MAX_POD_ID];
    uint32_t next_layer_id;
    QrxMoePrefetchItem items[QRX_MOE_MAX_PREFETCH];
    uint32_t item_count;
    uint64_t total_prefetch_bytes;
    uint64_t estimated_wire_bytes;
} QrxMoePrefetchPlan;

int qrx_moe_coactivation_validate(const QrxMoeCoactivationStat *stat);
uint32_t qrx_moe_coactivation_bps(const QrxMoeCoactivationStat *stat);
uint64_t qrx_moe_locality_score(const QrxMoeNodeProfile *node, const QrxMoeExpertPlacement *placement, uint32_t coactivation_bps);
int qrx_moe_coordinator_request_validate(const QrxMoeCoordinatorRequest *request);
int qrx_moe_prefetch_plan_add(const QrxMoeCoordinatorRequest *request, const QrxMoeNodeProfile *node, const QrxMoeExpertPlacement *placement, uint32_t predicted_bps, QrxMoePrefetchPlan *plan);
int qrx_moe_prefetch_plan_finalize(const QrxMoeCoordinatorRequest *request, QrxMoePrefetchPlan *plan);
int qrx_moe_prefetch_plan_commitment(const QrxMoePrefetchPlan *plan, char out_hex[65]);

/* 0.0.9.16 MoE Micro-Batching / Persistent Pod Streams / Hierarchical Aggregation.
   This is a deterministic protocol foundation only: it models compatible
   request batching, long-lived stream state and pod-local reduction plans.
   Transport implementations (QUIC/JACCL/RDMA/etc.) plug in below this layer. */
#define QRX_MOE_BATCH_VERSION 1U
#define QRX_MOE_MAX_BATCH_ITEMS 64U
#define QRX_MOE_MAX_AGG_ITEMS 32U
#define QRX_MOE_MAX_STREAM_ID 129U

#define QRX_MOE_STREAM_FEAT_COMPRESSION (1U<<0)
#define QRX_MOE_STREAM_FEAT_ZERO_COPY   (1U<<1)
#define QRX_MOE_STREAM_FEAT_RDMA        (1U<<2)
#define QRX_MOE_STREAM_FEAT_MULTIPLEX   (1U<<3)

typedef enum {
    QRX_MOE_STREAM_OPEN=1,
    QRX_MOE_STREAM_DRAINING=2,
    QRX_MOE_STREAM_CLOSED=3,
    QRX_MOE_STREAM_FAILED=4
} QrxMoeStreamState;

typedef struct {
    uint32_t version;
    char request_id[129];
    char session_id[129];
    char model_id[QRX_MOE_MAX_MODEL_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    uint32_t layer_id;
    uint32_t expert_id;
    uint32_t fragment_id;
    QrxActivationEncoding activation_encoding;
    uint32_t token_count;
    uint64_t activation_elements;
    uint64_t deadline_us;
} QrxMoeBatchItem;

typedef struct {
    uint32_t version;
    char pod_id[QRX_MOE_MAX_POD_ID];
    uint32_t layer_id;
    uint32_t expert_id;
    uint32_t fragment_id;
    QrxActivationEncoding activation_encoding;
    QrxMoeBatchItem items[QRX_MOE_MAX_BATCH_ITEMS];
    uint32_t item_count;
    uint32_t total_tokens;
    uint64_t total_activation_elements;
    uint64_t estimated_wire_bytes;
    uint64_t earliest_deadline_us;
} QrxMoeMicroBatch;

typedef struct {
    uint32_t version;
    char stream_id[QRX_MOE_MAX_STREAM_ID];
    char pod_id[QRX_MOE_MAX_POD_ID];
    char local_node_id[129];
    char remote_node_id[129];
    QrxMoeStreamState state;
    uint32_t feature_mask;
    QrxActivationEncoding activation_encoding;
    uint32_t max_inflight_batches;
    uint32_t inflight_batches;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t opened_us;
    uint64_t last_activity_us;
} QrxMoePersistentStream;

typedef struct {
    char node_id[129];
    uint64_t partial_result_bytes;
    uint64_t sequence;
} QrxMoeAggregateInput;

typedef struct {
    uint32_t version;
    char pod_id[QRX_MOE_MAX_POD_ID];
    char aggregator_node_id[129];
    uint32_t layer_id;
    uint32_t expert_id;
    QrxMoeAggregateInput inputs[QRX_MOE_MAX_AGG_ITEMS];
    uint32_t input_count;
    uint64_t local_input_bytes;
    uint64_t uplink_result_bytes;
} QrxMoeAggregationPlan;

int qrx_moe_batch_item_validate(const QrxMoeBatchItem *item);
int qrx_moe_microbatch_add(QrxMoeMicroBatch *batch, const char *pod_id, const QrxMoeBatchItem *item, uint32_t max_items, uint32_t max_tokens, uint64_t max_wire_bytes);
int qrx_moe_microbatch_finalize(QrxMoeMicroBatch *batch);
int qrx_moe_microbatch_commitment(const QrxMoeMicroBatch *batch, char out_hex[65]);
int qrx_moe_stream_open(QrxMoePersistentStream *stream, const char *stream_id, const char *pod_id, const char *local_node_id, const char *remote_node_id, QrxActivationEncoding encoding, uint32_t feature_mask, uint32_t max_inflight_batches, uint64_t now_us);
int qrx_moe_stream_begin_batch(QrxMoePersistentStream *stream, uint64_t wire_bytes, uint64_t now_us);
int qrx_moe_stream_complete_batch(QrxMoePersistentStream *stream, uint64_t received_bytes, uint64_t now_us);
int qrx_moe_stream_set_state(QrxMoePersistentStream *stream, QrxMoeStreamState state, uint64_t now_us);
int qrx_moe_aggregation_add(QrxMoeAggregationPlan *plan, const char *pod_id, const char *aggregator_node_id, uint32_t layer_id, uint32_t expert_id, const char *node_id, uint64_t partial_result_bytes, uint64_t sequence);
int qrx_moe_aggregation_finalize(QrxMoeAggregationPlan *plan, uint64_t uplink_result_bytes);
uint64_t qrx_moe_aggregation_wire_savings(const QrxMoeAggregationPlan *plan);
int qrx_moe_aggregation_commitment(const QrxMoeAggregationPlan *plan, char out_hex[65]);

/* 0.0.9.17 MoE Adaptive Batch Scheduler / Backpressure / Throughput Telemetry.
   Deterministic integer-only policy for deciding whether to dispatch now,
   briefly wait for a larger compatible batch, throttle/reroute overloaded
   workers, and account end-to-end useful throughput without relying on a
   specific transport backend. */
#define QRX_MOE_SCHED_VERSION 1U
#define QRX_MOE_MAX_REPLICA_CANDIDATES 16U
#define QRX_MOE_TELEMETRY_VERSION 1U

typedef enum {
    QRX_MOE_DISPATCH_WAIT=1,
    QRX_MOE_DISPATCH_NOW=2,
    QRX_MOE_DISPATCH_REROUTE=3,
    QRX_MOE_DISPATCH_REJECT=4
} QrxMoeDispatchDecision;

typedef enum {
    QRX_MOE_PRESSURE_NORMAL=1,
    QRX_MOE_PRESSURE_ELEVATED=2,
    QRX_MOE_PRESSURE_HIGH=3,
    QRX_MOE_PRESSURE_CRITICAL=4
} QrxMoePressureState;

typedef struct {
    uint32_t version;
    uint32_t min_batch_items;
    uint32_t target_batch_items;
    uint32_t max_batch_items;
    uint64_t max_wait_us;
    uint64_t deadline_guard_us;
    uint32_t elevated_queue_bps;
    uint32_t high_queue_bps;
    uint32_t critical_queue_bps;
} QrxMoeAdaptiveBatchPolicy;

typedef struct {
    uint32_t version;
    uint32_t queued_items;
    uint32_t compatible_items;
    uint32_t queue_capacity;
    uint32_t inflight_batches;
    uint32_t inflight_capacity;
    uint64_t oldest_item_age_us;
    uint64_t earliest_deadline_slack_us;
    uint8_t alternate_replica_available;
} QrxMoeSchedulerSnapshot;

typedef struct {
    QrxMoeDispatchDecision decision;
    QrxMoePressureState pressure;
    uint32_t selected_batch_items;
    uint64_t suggested_wait_us;
    uint32_t queue_pressure_bps;
    uint32_t inflight_pressure_bps;
} QrxMoeScheduleAdvice;

typedef struct {
    uint32_t version;
    char node_id[129];
    uint32_t queue_depth;
    uint32_t queue_capacity;
    uint32_t inflight_batches;
    uint32_t inflight_capacity;
    uint32_t latency_us;
    uint32_t bandwidth_mbps;
    uint32_t reliability_bps;
    QrxMoeResidency residency;
    QrxMoeCacheState cache_state;
} QrxMoeReplicaCandidate;

typedef struct {
    uint32_t version;
    char pod_id[QRX_MOE_MAX_POD_ID];
    uint64_t window_start_us;
    uint64_t window_end_us;
    uint64_t requests_completed;
    uint64_t tokens_completed;
    uint64_t activation_bytes_sent;
    uint64_t result_bytes_received;
    uint64_t local_aggregation_bytes;
    uint64_t wan_bytes;
    uint64_t compute_us;
    uint64_t queue_wait_us;
    uint64_t batch_count;
    uint64_t reroute_count;
    uint64_t rejected_count;
    uint64_t failed_count;
} QrxMoeThroughputTelemetry;

int qrx_moe_adaptive_policy_validate(const QrxMoeAdaptiveBatchPolicy *policy);
int qrx_moe_schedule_advice(const QrxMoeAdaptiveBatchPolicy *policy, const QrxMoeSchedulerSnapshot *snapshot, QrxMoeScheduleAdvice *out);
uint64_t qrx_moe_replica_candidate_score(const QrxMoeReplicaCandidate *candidate);
int qrx_moe_choose_replica(const QrxMoeReplicaCandidate *candidates, uint32_t count, uint32_t *selected_index);
int qrx_moe_telemetry_validate(const QrxMoeThroughputTelemetry *telemetry);
int qrx_moe_telemetry_record_batch(QrxMoeThroughputTelemetry *telemetry, uint64_t requests, uint64_t tokens, uint64_t sent_bytes, uint64_t received_bytes, uint64_t local_bytes, uint64_t wan_bytes, uint64_t compute_us, uint64_t queue_wait_us, uint8_t rerouted, uint8_t failed);
uint64_t qrx_moe_telemetry_tokens_per_second_milli(const QrxMoeThroughputTelemetry *telemetry);
uint64_t qrx_moe_telemetry_requests_per_second_milli(const QrxMoeThroughputTelemetry *telemetry);
uint32_t qrx_moe_telemetry_wan_share_bps(const QrxMoeThroughputTelemetry *telemetry);
int qrx_moe_telemetry_commitment(const QrxMoeThroughputTelemetry *telemetry, char out_hex[65]);

/* 0.0.9.18 Runtime Transport Adapters / Reproducible MoE Benchmark Harness.
   Transport/backend descriptors are capability contracts, not claims that a
   native runtime is present. NVIDIA Pascal/P40 is explicitly representable via
   CUDA SM 6.1; tensor-core-only kernels must not be selected for that profile. */
#define QRX_MOE_RUNTIME_VERSION 1U
#define QRX_MOE_BENCH_VERSION 1U
#define QRX_MOE_MAX_TRANSPORT_NAME 32U
#define QRX_MOE_MAX_DEVICE_NAME 96U
#define QRX_MOE_MAX_BENCH_NODES 256U

#define QRX_MOE_RUNTIME_FEAT_ASYNC       (1U<<0)
#define QRX_MOE_RUNTIME_FEAT_MULTIPLEX   (1U<<1)
#define QRX_MOE_RUNTIME_FEAT_ZERO_COPY   (1U<<2)
#define QRX_MOE_RUNTIME_FEAT_RDMA        (1U<<3)
#define QRX_MOE_RUNTIME_FEAT_COMPRESSION (1U<<4)

#define QRX_MOE_ACCEL_FEAT_FP16          (1U<<0)
#define QRX_MOE_ACCEL_FEAT_FP32          (1U<<1)
#define QRX_MOE_ACCEL_FEAT_INT8          (1U<<2)
#define QRX_MOE_ACCEL_FEAT_TENSOR_CORES  (1U<<3)
#define QRX_MOE_ACCEL_FEAT_UNIFIED_MEM   (1U<<4)
#define QRX_MOE_ACCEL_FEAT_MLX           (1U<<5)
#define QRX_MOE_ACCEL_FEAT_METAL         (1U<<6)
#define QRX_MOE_ACCEL_FEAT_CUDA          (1U<<7)

typedef enum {
    QRX_MOE_TRANSPORT_LOCAL=1,
    QRX_MOE_TRANSPORT_SHM=2,
    QRX_MOE_TRANSPORT_TCP=3,
    QRX_MOE_TRANSPORT_QUIC=4,
    QRX_MOE_TRANSPORT_MPI=5,
    QRX_MOE_TRANSPORT_JACCL_RDMA=6
} QrxMoeTransportKind;

typedef enum {
    QRX_MOE_BACKEND_CPU=1,
    QRX_MOE_BACKEND_MLX_METAL=2,
    QRX_MOE_BACKEND_CUDA=3,
    QRX_MOE_BACKEND_OTHER=4
} QrxMoeRuntimeBackend;

typedef struct {
    uint32_t version;
    QrxMoeTransportKind kind;
    char name[QRX_MOE_MAX_TRANSPORT_NAME];
    uint32_t feature_mask;
    uint32_t latency_us;
    uint32_t bandwidth_mbps;
    uint32_t max_inflight;
    uint32_t mtu_bytes;
    uint32_t per_message_overhead_bytes;
} QrxMoeTransportAdapter;

typedef struct {
    uint32_t version;
    QrxMoeRuntimeBackend backend;
    char device_name[QRX_MOE_MAX_DEVICE_NAME];
    uint32_t accelerator_features;
    uint64_t device_memory_bytes;
    uint32_t cuda_cc_major;
    uint32_t cuda_cc_minor;
    uint32_t max_batch_items;
} QrxMoeRuntimeDevice;

typedef struct {
    uint32_t version;
    char node_id[129];
    QrxMoeRuntimeDevice device;
    QrxMoeTransportAdapter transport;
    uint64_t standalone_milli_tokens_per_second;
    uint32_t availability_bps;
} QrxMoeBenchmarkNode;

typedef struct {
    uint32_t version;
    char scenario_id[129];
    uint64_t seed;
    uint64_t duration_us;
    uint32_t concurrency;
    uint32_t requested_batch_items;
    uint64_t activation_bytes_per_token;
    uint64_t result_bytes_per_token;
    uint32_t synthetic_fault_bps;
    QrxMoeBenchmarkNode nodes[QRX_MOE_MAX_BENCH_NODES];
    uint32_t node_count;
} QrxMoeBenchmarkScenario;

typedef struct {
    uint32_t version;
    uint64_t effective_nodes_milli;
    uint64_t raw_milli_tokens_per_second;
    uint64_t estimated_milli_tokens_per_second;
    uint64_t estimated_tokens_completed;
    uint64_t estimated_wire_bytes;
    uint64_t estimated_transport_us;
    uint64_t estimated_compute_us;
    uint32_t bottleneck_transport_index;
    uint32_t fault_loss_bps;
} QrxMoeBenchmarkResult;

int qrx_moe_transport_validate(const QrxMoeTransportAdapter *adapter);
int qrx_moe_runtime_device_validate(const QrxMoeRuntimeDevice *device);
int qrx_moe_runtime_is_nvidia_p40(const QrxMoeRuntimeDevice *device);
int qrx_moe_runtime_supports_tensor_core_kernel(const QrxMoeRuntimeDevice *device);
int qrx_moe_benchmark_scenario_validate(const QrxMoeBenchmarkScenario *scenario);
int qrx_moe_benchmark_run(const QrxMoeBenchmarkScenario *scenario, QrxMoeBenchmarkResult *out);
int qrx_moe_benchmark_commitment(const QrxMoeBenchmarkScenario *scenario, const QrxMoeBenchmarkResult *result, char out_hex[65]);

/* 0.0.9.19 Native Runtime Discovery / Worker Eligibility / Calibration.
   CUDA is discovered at runtime through the CUDA driver API when present, so
   qrxcore does not acquire a build-time CUDA dependency. Apple Silicon is
   reported as Metal-capable; MLX execution still requires an MLX worker adapter
   at runtime and is not inferred merely from the operating system. */
#define QRX_MOE_DISCOVERY_VERSION 1U
#define QRX_MOE_CALIBRATION_VERSION 1U
#define QRX_MOE_MAX_DISCOVERED_DEVICES 32U
#define QRX_MOE_MAX_RUNTIME_PATH 256U

#define QRX_MOE_KERNEL_FP32       (1U<<0)
#define QRX_MOE_KERNEL_FP16       (1U<<1)
#define QRX_MOE_KERNEL_INT8       (1U<<2)
#define QRX_MOE_KERNEL_TENSORCORE (1U<<3)
#define QRX_MOE_KERNEL_MLX        (1U<<4)
#define QRX_MOE_KERNEL_METAL      (1U<<5)

typedef enum {
    QRX_MOE_DISCOVERY_NONE=0,
    QRX_MOE_DISCOVERY_CPU_NATIVE=1,
    QRX_MOE_DISCOVERY_APPLE_NATIVE=2,
    QRX_MOE_DISCOVERY_CUDA_DRIVER=3
} QrxMoeDiscoverySource;

typedef struct {
    uint32_t version;
    QrxMoeDiscoverySource source;
    QrxMoeRuntimeDevice device;
    uint8_t runtime_present;
    uint8_t execution_adapter_ready;
    uint8_t reserved[2];
} QrxMoeDiscoveredDevice;

typedef struct {
    uint32_t version;
    QrxMoeDiscoveredDevice devices[QRX_MOE_MAX_DISCOVERED_DEVICES];
    uint32_t device_count;
    uint8_t cuda_driver_present;
    uint8_t apple_silicon;
    uint8_t metal_capable;
    uint8_t mlx_adapter_present;
} QrxMoeRuntimeInventory;

typedef struct {
    uint32_t version;
    QrxMoeRuntimeBackend backend;
    uint32_t required_kernel_features;
    uint64_t required_device_memory_bytes;
    uint32_t requested_batch_items;
} QrxMoeWorkerRequirement;

typedef struct {
    uint32_t version;
    char profile_id[129];
    QrxMoeRuntimeDevice device;
    uint64_t measured_at_unix_ms;
    uint64_t memory_bandwidth_bytes_per_second;
    uint64_t compute_ops_per_second;
    uint64_t model_milli_tokens_per_second;
    uint32_t model_batch_items;
    uint32_t sample_count;
    uint32_t transport_latency_us;
    uint32_t transport_bandwidth_mbps;
} QrxMoeCalibrationProfile;

int qrx_moe_runtime_discover_native(QrxMoeRuntimeInventory *out);
int qrx_moe_runtime_inventory_validate(const QrxMoeRuntimeInventory *inventory);
uint32_t qrx_moe_runtime_kernel_mask(const QrxMoeRuntimeDevice *device);
int qrx_moe_worker_eligible(const QrxMoeRuntimeDevice *device, const QrxMoeWorkerRequirement *requirement);
int qrx_moe_calibration_profile_validate(const QrxMoeCalibrationProfile *profile);
int qrx_moe_calibration_apply_to_benchmark(const QrxMoeCalibrationProfile *profile, QrxMoeBenchmarkNode *node);
int qrx_moe_calibration_commitment(const QrxMoeCalibrationProfile *profile, char out_hex[65]);

#define QRX_MOE_MAX_ADAPTER_NAME 64U
typedef int (*QrxMoeWorkerExecuteFn)(void *ctx, const void *input, size_t input_bytes, void *output, size_t output_capacity, size_t *output_bytes);
typedef struct {
    uint32_t version;
    char name[QRX_MOE_MAX_ADAPTER_NAME];
    QrxMoeRuntimeBackend backend;
    uint32_t kernel_mask;
    uint8_t ready;
    QrxMoeWorkerExecuteFn execute;
    void *ctx;
} QrxMoeWorkerAdapter;

int qrx_moe_worker_adapter_validate(const QrxMoeWorkerAdapter *adapter);
int qrx_moe_worker_adapter_execute(const QrxMoeWorkerAdapter *adapter, const QrxMoeRuntimeDevice *device, const QrxMoeWorkerRequirement *requirement, const void *input, size_t input_bytes, void *output, size_t output_capacity, size_t *output_bytes);
int qrx_moe_calibration_record_model_sample(QrxMoeCalibrationProfile *profile, uint64_t tokens_completed, uint64_t elapsed_us, uint32_t batch_items);
int qrx_moe_calibration_record_transport_probe(QrxMoeCalibrationProfile *profile, uint64_t payload_bytes, uint64_t round_trip_us);
