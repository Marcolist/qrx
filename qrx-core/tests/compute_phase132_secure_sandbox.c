#include "compute/qrx_compute.h"
#include <stdio.h>
#include <string.h>
int main(void){
 QrxComputeJobNode n; memset(&n,0,sizeof(n)); n.node_id=1; n.job_type=QRX_JOB_CODE_EXECUTION; n.min_memory_bytes=256*1024*1024ULL; n.max_runtime_ms=60000; n.max_output_bytes=16*1024*1024ULL; n.max_fee_atoms=400000000ULL; n.capability_mask=QRX_JOB_CAP_READ_INPUTS|QRX_JOB_CAP_WRITE_ARTIFACTS|QRX_JOB_CAP_SANDBOX_EXEC;
 QrxComputeSandboxPolicy p; if(qrx_compute_sandbox_policy_default(&n,&p)||p.wallet_keys_visible||p.host_fs_visible||!(p.permissions&QRX_SANDBOX_ALLOW_SUBPROCESS)){puts("sandbox default failed");return 1;}
 p.host_fs_visible=1; if(qrx_compute_sandbox_policy_validate(&n,&p)!=-2){puts("host fs escape accepted");return 2;} p.host_fs_visible=0; p.permissions|=QRX_SANDBOX_ALLOW_NETWORK; if(qrx_compute_sandbox_policy_validate(&n,&p)!=-4){puts("raw network accepted");return 3;}
 QrxComputeBudgetGuard g; if(qrx_compute_budget_guard_init(&g,400000000ULL))return 4;
 if(qrx_compute_budget_guard_charge(&g,320000000ULL)||g.state!=QRX_BUDGET_WARNING){puts("80 pct warning failed");return 5;}
 if(qrx_compute_budget_guard_charge(&g,380000000ULL)||g.state!=QRX_BUDGET_CHECKPOINTING){puts("95 pct checkpoint failed");return 6;}
 if(qrx_compute_budget_guard_checkpoint(&g,"drive:checkpoint:1")||g.checkpoint_seq!=1)return 7;
 if(qrx_compute_budget_guard_charge(&g,400000000ULL)||g.state!=QRX_BUDGET_PAUSED_EXHAUSTED){puts("exhaust pause failed");return 8;}
 if(qrx_compute_budget_guard_charge(&g,400000001ULL)!=-2){puts("overspend accepted");return 9;}
 if(qrx_compute_budget_guard_checkpoint(&g,"drive:checkpoint:2")||qrx_compute_budget_guard_extend(&g,200000000ULL)||g.authorized_atoms!=600000000ULL||g.state!=QRX_BUDGET_EXTENDED)return 10;
 if(qrx_compute_budget_guard_resume(&g)||g.state!=QRX_BUDGET_RESUMING)return 11;
 if(qrx_compute_budget_guard_charge(&g,470000000ULL)||qrx_compute_budget_guard_complete(&g)||g.state!=QRX_BUDGET_COMPLETED)return 12;
 puts("compute phase 132 secure sandbox budget guard: PASS"); return 0;
}
