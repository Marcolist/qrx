#include "../src/compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static QrxMoeBatchItem item(const char *id,uint64_t deadline,uint64_t elems){
    QrxMoeBatchItem i; memset(&i,0,sizeof(i)); i.version=QRX_MOE_BATCH_VERSION;
    snprintf(i.request_id,sizeof(i.request_id),"%s",id); snprintf(i.session_id,sizeof(i.session_id),"s-%s",id);
    snprintf(i.model_id,sizeof(i.model_id),"kimi-k3"); snprintf(i.model_version,sizeof(i.model_version),"test");
    i.layer_id=12;i.expert_id=31;i.fragment_id=0;i.activation_encoding=QRX_ACT_INT8;i.token_count=1;i.activation_elements=elems;i.deadline_us=deadline; return i;
}
int main(void){
    QrxMoeMicroBatch b={0}; QrxMoeBatchItem a=item("a",2000,1024),c=item("c",1500,2048),bad=item("bad",2500,1); bad.expert_id=99;
    assert(qrx_moe_microbatch_add(&b,"pod-eu",&a,8,16,10000)==0);
    assert(qrx_moe_microbatch_add(&b,"pod-eu",&c,8,16,10000)==0);
    assert(qrx_moe_microbatch_add(&b,"pod-eu",&bad,8,16,10000)==-2);
    assert(b.item_count==2 && b.estimated_wire_bytes==3072);
    assert(qrx_moe_microbatch_finalize(&b)==0); assert(!strcmp(b.items[0].request_id,"c"));
    char h1[65],h2[65]; assert(qrx_moe_microbatch_commitment(&b,h1)==0); assert(qrx_moe_microbatch_commitment(&b,h2)==0); assert(!strcmp(h1,h2));

    QrxMoePersistentStream s; assert(qrx_moe_stream_open(&s,"stream-1","pod-eu","coord","worker",QRX_ACT_INT8,QRX_MOE_STREAM_FEAT_MULTIPLEX,2,100)==0);
    assert(qrx_moe_stream_begin_batch(&s,3072,110)==0); assert(qrx_moe_stream_begin_batch(&s,100,111)==0); assert(qrx_moe_stream_begin_batch(&s,100,112)==-1);
    assert(qrx_moe_stream_complete_batch(&s,512,120)==0); assert(qrx_moe_stream_complete_batch(&s,128,121)==0); assert(qrx_moe_stream_set_state(&s,QRX_MOE_STREAM_CLOSED,130)==0);
    assert(s.bytes_sent==3172 && s.bytes_received==640);

    QrxMoeAggregationPlan p={0}; assert(qrx_moe_aggregation_add(&p,"pod-eu","agg",12,31,"n2",1000,2)==0); assert(qrx_moe_aggregation_add(&p,"pod-eu","agg",12,31,"n1",1000,1)==0);
    assert(qrx_moe_aggregation_add(&p,"pod-eu","agg",12,31,"n1",1000,3)==-3); assert(qrx_moe_aggregation_finalize(&p,600)==0); assert(!strcmp(p.inputs[0].node_id,"n1")); assert(qrx_moe_aggregation_wire_savings(&p)==1400);
    assert(qrx_moe_aggregation_commitment(&p,h1)==0); assert(strlen(h1)==64);
    puts("compute_phase141_moe_batch_stream_aggregation: PASS"); return 0;
}
