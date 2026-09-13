#include "../src/compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    QrxMoeAdaptiveBatchPolicy p={0};p.version=QRX_MOE_SCHED_VERSION;p.min_batch_items=2;p.target_batch_items=8;p.max_batch_items=16;p.max_wait_us=5000;p.deadline_guard_us=500;p.elevated_queue_bps=5000;p.high_queue_bps=7500;p.critical_queue_bps=9500;
    assert(qrx_moe_adaptive_policy_validate(&p)==0);
    QrxMoeSchedulerSnapshot s={0};s.version=QRX_MOE_SCHED_VERSION;s.queued_items=2;s.compatible_items=2;s.queue_capacity=16;s.inflight_capacity=4;s.oldest_item_age_us=1000;s.earliest_deadline_slack_us=5000;
    QrxMoeScheduleAdvice a;assert(qrx_moe_schedule_advice(&p,&s,&a)==0);assert(a.decision==QRX_MOE_DISPATCH_WAIT&&a.suggested_wait_us==4000);
    s.compatible_items=8;assert(qrx_moe_schedule_advice(&p,&s,&a)==0&&a.decision==QRX_MOE_DISPATCH_NOW&&a.selected_batch_items==8);
    s.compatible_items=1;s.queued_items=16;s.inflight_batches=4;s.alternate_replica_available=1;assert(qrx_moe_schedule_advice(&p,&s,&a)==0&&a.decision==QRX_MOE_DISPATCH_REROUTE&&a.pressure==QRX_MOE_PRESSURE_CRITICAL);
    s.alternate_replica_available=0;assert(qrx_moe_schedule_advice(&p,&s,&a)==0&&a.decision==QRX_MOE_DISPATCH_REJECT);

    QrxMoeReplicaCandidate c[2];memset(c,0,sizeof(c));for(int i=0;i<2;i++){c[i].version=QRX_MOE_SCHED_VERSION;c[i].queue_capacity=10;c[i].inflight_capacity=4;c[i].bandwidth_mbps=10000;c[i].reliability_bps=9900;c[i].residency=QRX_MOE_RES_RAM;c[i].cache_state=QRX_MOE_CACHE_HOT;}snprintf(c[0].node_id,sizeof(c[0].node_id),"slow");c[0].queue_depth=9;c[0].inflight_batches=3;c[0].latency_us=8000;snprintf(c[1].node_id,sizeof(c[1].node_id),"fast");c[1].queue_depth=1;c[1].inflight_batches=1;c[1].latency_us=1000;uint32_t idx=99;assert(qrx_moe_choose_replica(c,2,&idx)==0&&idx==1);

    QrxMoeThroughputTelemetry t={0};t.version=QRX_MOE_TELEMETRY_VERSION;snprintf(t.pod_id,sizeof(t.pod_id),"pod-eu");t.window_start_us=1000000;t.window_end_us=3000000;
    assert(qrx_moe_telemetry_record_batch(&t,4,40,4000,2000,3000,1000,500000,10000,1,0)==0);assert(qrx_moe_telemetry_record_batch(&t,4,60,6000,2000,3000,1000,500000,20000,0,0)==0);
    assert(t.batch_count==2&&t.tokens_completed==100&&t.reroute_count==1);assert(qrx_moe_telemetry_tokens_per_second_milli(&t)==50000);assert(qrx_moe_telemetry_requests_per_second_milli(&t)==4000);assert(qrx_moe_telemetry_wan_share_bps(&t)==1428);
    char h1[65],h2[65];assert(qrx_moe_telemetry_commitment(&t,h1)==0);assert(qrx_moe_telemetry_commitment(&t,h2)==0&&!strcmp(h1,h2));
    puts("compute_phase142_moe_adaptive_scheduler_telemetry: PASS");return 0;
}
