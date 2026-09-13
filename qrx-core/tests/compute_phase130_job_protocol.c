#include "compute/qrx_compute.h"
#include <stdio.h>
#include <string.h>

static QrxComputeJobNode node(uint32_t id,QrxComputeJobType type,const char *in,const char *out) {
    QrxComputeJobNode n; memset(&n,0,sizeof(n)); n.node_id=id; n.job_type=type;
    snprintf(n.runtime_id,sizeof(n.runtime_id),"qrx-sandbox-v1");
    snprintf(n.input_ref,sizeof(n.input_ref),"%s",in); snprintf(n.output_ref,sizeof(n.output_ref),"%s",out);
    n.min_memory_bytes=256ULL<<20; n.max_runtime_ms=60000; n.max_output_bytes=16ULL<<20; n.max_fee_atoms=1000;
    n.capability_mask=QRX_JOB_CAP_READ_INPUTS|QRX_JOB_CAP_WRITE_ARTIFACTS;
    if(type==QRX_JOB_CODE_EXECUTION||type==QRX_JOB_COMPILE||type==QRX_JOB_TEST||type==QRX_JOB_CUSTOM_SANDBOXED) n.capability_mask|=QRX_JOB_CAP_SANDBOX_EXEC;
    if(type==QRX_JOB_AI_INFERENCE||type==QRX_JOB_CODE_GENERATION||type==QRX_JOB_IMAGE_GENERATION||type==QRX_JOB_RESEARCH){snprintf(n.model_id,sizeof(n.model_id),"qrx/test-model");snprintf(n.model_version,sizeof(n.model_version),"1");n.capability_mask|=QRX_JOB_CAP_MODEL_INFERENCE|QRX_JOB_CAP_STREAM_OUTPUT;}
    return n;
}
static QrxComputeJobGraph graph(void) {
    QrxComputeJobGraph g; memset(&g,0,sizeof(g)); g.protocol_version=QRX_COMPUTE_JOB_PROTOCOL_VERSION;
    snprintf(g.graph_id,sizeof(g.graph_id),"jobgraph-001"); snprintf(g.owner,sizeof(g.owner),"qrx1owner"); g.nonce=7; g.expiry_height=100000; g.max_total_fee_atoms=5000;
    g.node_count=3; g.nodes[0]=node(20,QRX_JOB_CODE_GENERATION,"drive:prompt","drive:source"); g.nodes[1]=node(30,QRX_JOB_COMPILE,"drive:source","drive:binary"); g.nodes[2]=node(40,QRX_JOB_TEST,"drive:binary","drive:test-report");
    g.edge_count=2; g.edges[0]=(QrxComputeJobEdge){20,30}; g.edges[1]=(QrxComputeJobEdge){30,40}; return g;
}
int main(void) {
    QrxComputeJobGraph g=graph(); if(qrx_compute_job_graph_validate(&g)){puts("valid graph rejected");return 1;}
    uint32_t order[QRX_COMPUTE_JOB_MAX_NODES]; if(qrx_compute_job_graph_topological_order(&g,order)||order[0]!=20||order[1]!=30||order[2]!=40){puts("bad topo order");return 2;}
    char a[65],b[65]; if(qrx_compute_job_graph_commitment(&g,a)){puts("commit failed");return 3;}
    QrxComputeJobGraph reordered=graph(); QrxComputeJobNode t=reordered.nodes[0];reordered.nodes[0]=reordered.nodes[2];reordered.nodes[2]=t; QrxComputeJobEdge e=reordered.edges[0];reordered.edges[0]=reordered.edges[1];reordered.edges[1]=e;
    if(qrx_compute_job_graph_commitment(&reordered,b)||strcmp(a,b)){puts("commitment not canonical");return 4;}
    QrxComputeJobGraph cyc=graph(); cyc.edges[cyc.edge_count++]=(QrxComputeJobEdge){40,20}; if(!qrx_compute_job_graph_validate(&cyc)){puts("cycle accepted");return 5;}
    QrxComputeJobGraph over=graph(); over.max_total_fee_atoms=2000; if(!qrx_compute_job_graph_validate(&over)){puts("fee cap bypass");return 6;}
    QrxComputeJobGraph unsafe=graph(); unsafe.nodes[1].capability_mask&=~QRX_JOB_CAP_SANDBOX_EXEC; if(!qrx_compute_job_graph_validate(&unsafe)){puts("unsafe execution accepted");return 7;}
    puts("compute phase 130 job protocol: PASS"); return 0;
}
