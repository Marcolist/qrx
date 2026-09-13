#include "compute/qrx_compute.h"
#include <stdio.h>
#include <string.h>

static QrxComputeJobGraph graph(void){
 QrxComputeJobGraph g; memset(&g,0,sizeof(g)); g.protocol_version=QRX_COMPUTE_JOB_PROTOCOL_VERSION;
 snprintf(g.graph_id,sizeof(g.graph_id),"market-job-1"); snprintf(g.owner,sizeof(g.owner),"qrx1owner"); g.nonce=9; g.max_total_fee_atoms=400000000ULL; g.expiry_height=5000; g.node_count=1;
 QrxComputeJobNode *n=&g.nodes[0]; n->node_id=1; n->job_type=QRX_JOB_AI_INFERENCE; snprintf(n->runtime_id,sizeof(n->runtime_id),"qrx-ai-v1"); snprintf(n->model_id,sizeof(n->model_id),"qrx/model"); snprintf(n->model_version,sizeof(n->model_version),"1"); snprintf(n->input_ref,sizeof(n->input_ref),"drive:prompt"); snprintf(n->output_ref,sizeof(n->output_ref),"drive:answer"); n->min_memory_bytes=1024; n->max_runtime_ms=60000; n->max_output_bytes=1024; n->max_fee_atoms=400000000ULL; n->capability_mask=QRX_JOB_CAP_READ_INPUTS|QRX_JOB_CAP_MODEL_INFERENCE|QRX_JOB_CAP_STREAM_OUTPUT; return g;
}
static QrxComputeQuote quote(const QrxComputeJobGraph *g,const char *id,const char *provider,uint64_t price,uint32_t score,uint32_t rel,uint32_t latency,uint8_t local){
 QrxComputeQuote q; memset(&q,0,sizeof(q)); q.market_version=QRX_COMPUTE_MARKET_VERSION; snprintf(q.quote_id,sizeof(q.quote_id),"%s",id); snprintf(q.provider_id,sizeof(q.provider_id),"%s",provider); qrx_compute_job_graph_commitment(g,q.graph_commitment); q.node_id=1; q.price_atoms=price; q.expiry_height=4500; q.normalized_compute_score=score; q.reliability_bps=rel; q.estimated_latency_ms=latency; q.verification_level=QRX_COMPUTE_VERIFY_STANDARD; q.model_local=local; q.fasttrack_available=1; return q;
}
int main(void){
 QrxComputeJobGraph g=graph(); if(qrx_compute_job_graph_validate(&g)){puts("graph invalid");return 1;}
 QrxComputeQuoteBook b={0}; QrxComputeQuote cheap=quote(&g,"q1","provider-cheap",200000000ULL,7000,9000,700,0); QrxComputeQuote good=quote(&g,"q2","provider-good",270000000ULL,18000,9950,40,1);
 if(qrx_compute_quote_book_add(&b,&cheap,&g,1000)||qrx_compute_quote_book_add(&b,&good,&g,1000)){puts("quote add failed");return 2;}
 const QrxComputeQuote *best=qrx_compute_quote_select(&b,&g,1,QRX_COMPUTE_VERIFY_STANDARD,1,0,1000); if(!best||strcmp(best->provider_id,"provider-good")){puts("selection failed");return 3;}
 QrxComputeQuote over=quote(&g,"q3","provider-over",500000000ULL,20000,10000,1,1); if(!qrx_compute_quote_validate(&over,&g,1000)){puts("over-budget quote accepted");return 4;}
 QrxComputeEscrow e; if(qrx_compute_escrow_lock(&e,&g,100000000ULL,1000)||e.locked_atoms!=500000000ULL){puts("lock failed");return 5;} if(qrx_compute_escrow_mark_assigned(&e)){puts("assign failed");return 6;}
 if(qrx_compute_escrow_settle(&e,270000000ULL,2000)){puts("settle failed");return 7;} if(e.refund_atoms!=130000000ULL||e.fasttrack_dev_atoms!=500000ULL||e.fasttrack_net_atoms!=9500000ULL){puts("settlement math failed");return 8;}
 QrxComputeEscrow too_fast; if(qrx_compute_escrow_lock(&too_fast,&g,100000001ULL,1000)!=-2){puts("fasttrack premium cap failed");return 9;}
 QrxComputeEscrow x; if(qrx_compute_escrow_lock(&x,&g,0,1000)||qrx_compute_escrow_refund(&x,6000)||x.state!=QRX_COMPUTE_ESCROW_EXPIRED||x.refund_atoms!=400000000ULL){puts("expiry refund failed");return 10;}
 puts("compute phase 131 market quotes escrow: PASS"); return 0;
}
