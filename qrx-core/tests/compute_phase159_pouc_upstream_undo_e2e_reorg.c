#include "compute/qrx_pouc_reorg.h"
#include "compute/qrx_pouc_undo.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int count_cb(const char*k,const char*v,uint32_t n,void*ctx){(void)k;(void)v;(void)n;(*(uint32_t*)ctx)++;return 0;}
static void put(QrxDBBatch*b,const char*k,const char*v){assert(!qrxdb_batch_put(b,k,v));}
static void tx(QrxDB*d,uint64_t h,const char*id,const char*type,const char**kv,size_t pairs){
    QrxDBBatch b;assert(!qrxdb_batch_begin(d,&b));
    for(size_t i=0;i<pairs;i++)put(&b,kv[i*2],kv[i*2+1]);
    assert(!qrx_pouc_tx_undo_stage(d,&b,id,type,h));
    assert(!qrxdb_batch_commit(&b));
}
static void geteq(QrxDB*d,const char*k,const char*want){char v[4096];assert(!qrxdb_get(d,k,v,sizeof(v)));assert(!strcmp(v,want));}
static void missing(QrxDB*d,const char*k){char v[32];assert(qrxdb_get(d,k,v,sizeof(v))!=0);}

int main(void){
    char dir[]="/tmp/qrx-phase159-XXXXXX";assert(mkdtemp(dir));QrxDB db;assert(!qrxdb_init(&db,dir));
    assert(!qrxdb_put(&db,"acct:balance:owner","10000"));
    assert(!qrxdb_put(&db,"acct:nonce:owner","7"));
    assert(!qrxdb_put(&db,"consensus:fee_pool:pending","50"));
    assert(!qrxdb_put(&db,"consensus:compute:escrow_pool","0"));
    assert(!qrxdb_put(&db,"state:untouched","alpha"));
    char root0[129];assert(!qrxdb_merkle_root_hex(&db,root0));

    const char *t1[]={
      "acct:balance:owner","6990","acct:nonce:owner","8","consensus:fee_pool:pending","60",
      "consensus:compute:escrow_pool","3000","consensus:compute:funding:graph1","height=10|locked=3000",
      "consensus:compute:escrow:graph1","LOCKED","tx:applied:tx-lock","height=10\napplied=1\n",
      "tx:loc:tx-lock","height=10","tx:payload:tx-lock","COMPUTE_ESCROW_LOCK","consensus:applytx:tx-lock","height=10\ntype=pouc-upstream-consensus\ncommitted=1\n"};
    tx(&db,10,"tx-lock","COMPUTE_ESCROW_LOCK",t1,sizeof(t1)/sizeof(t1[0])/2);
    const char *t2[]={"acct:balance:owner","6980","acct:nonce:owner","9","consensus:fee_pool:pending","70","consensus:compute:assignment:graph1","ASSIGNED-V1","consensus:compute:escrow:graph1","ASSIGNED","tx:applied:tx-assign","height=10\napplied=1\n"};
    tx(&db,10,"tx-assign","COMPUTE_ASSIGN",t2,sizeof(t2)/sizeof(t2[0])/2);
    const char *t3[]={"acct:balance:provider","990","consensus:fee_pool:pending","80","consensus:compute:receipt:r1","RECEIPT-V1","consensus:compute:receipt_by_graph:graph1","r1","consensus:compute:verifier_selection:r1","SELECT-V1","tx:applied:tx-receipt","height=11\napplied=1\n"};
    tx(&db,11,"tx-receipt","COMPUTE_RECEIPT",t3,sizeof(t3)/sizeof(t3[0])/2);
    char root11[129];assert(!qrxdb_merkle_root_hex(&db,root11));

    const char *t4[]={"acct:balance:v1","25","consensus:compute:escrow_pool","2975","consensus:compute:verification_aggregate:r1","1|1|1|0|3|0","consensus:compute:verify:r1:v1","match=1","tx:applied:tx-v1","height=12\napplied=1\n"};
    tx(&db,12,"tx-v1","COMPUTE_VERIFY",t4,sizeof(t4)/sizeof(t4[0])/2);
    const char *t5[]={"acct:balance:v2","25","consensus:compute:escrow_pool","2950","consensus:compute:verification_aggregate:r1","1|2|2|0|3|0","consensus:compute:verify:r1:v2","match=1","tx:applied:tx-v2","height=13\napplied=1\n"};
    tx(&db,13,"tx-v2","COMPUTE_VERIFY",t5,sizeof(t5)/sizeof(t5[0])/2);
    const char *t6[]={"acct:balance:c1","10","consensus:compute:escrow_pool","2940","consensus:compute:challenge_aggregate:r1","1|1|0","consensus:compute:challenge:r1","PENDING","consensus:compute:challenge_vote:r1:c1","pass=1","tx:applied:tx-c1","height=14\napplied=1\n"};
    tx(&db,14,"tx-c1","COMPUTE_CHALLENGE",t6,sizeof(t6)/sizeof(t6[0])/2);
    const char *t7[]={"acct:balance:provider","3690","acct:balance:owner","7270","consensus:compute:escrow_pool","0","consensus:compute:journal:graph1","PAYOUT","tx:applied:tx-settle","height=15\napplied=1\n"};
    tx(&db,15,"tx-settle","POUC_SETTLEMENT",t7,sizeof(t7)/sizeof(t7[0])/2);

    uint64_t g=qrxdb_generation(&db);QrxPoucCanonicalReorgReport r;assert(!qrx_pouc_canonical_reorg_hook(&db,11,&r));
    assert(r.atomic_tx_reverted==4);assert(qrxdb_generation(&db)==g+1);
    char root_after[129];assert(!qrxdb_merkle_root_hex(&db,root_after));assert(!strcmp(root_after,root11));
    geteq(&db,"consensus:compute:escrow_pool","3000");geteq(&db,"consensus:compute:assignment:graph1","ASSIGNED-V1");geteq(&db,"consensus:compute:receipt:r1","RECEIPT-V1");
    missing(&db,"consensus:compute:verification_aggregate:r1");missing(&db,"consensus:compute:challenge:r1");missing(&db,"tx:applied:tx-settle");assert(!qrxdb_chain_is_applied(&db,"tx-settle"));

    g=qrxdb_generation(&db);assert(!qrx_pouc_canonical_reorg_hook(&db,0,&r));assert(r.atomic_tx_reverted==3);assert(qrxdb_generation(&db)==g+1);
    assert(!qrxdb_merkle_root_hex(&db,root_after));assert(!strcmp(root_after,root0));
    geteq(&db,"acct:balance:owner","10000");geteq(&db,"acct:nonce:owner","7");geteq(&db,"consensus:fee_pool:pending","50");geteq(&db,"consensus:compute:escrow_pool","0");geteq(&db,"state:untouched","alpha");
    missing(&db,"consensus:compute:funding:graph1");missing(&db,"consensus:compute:assignment:graph1");missing(&db,"consensus:compute:receipt:r1");missing(&db,"tx:applied:tx-lock");assert(!qrxdb_chain_is_applied(&db,"tx-lock"));
    uint32_t n=0;assert(!qrxdb_scan_prefix(&db,QRX_POUC_TX_UNDO_PREFIX,count_cb,&n));assert(n==0);
    g=qrxdb_generation(&db);assert(!qrx_pouc_canonical_reorg_hook(&db,0,&r));assert(r.atomic_tx_reverted==0);assert(qrxdb_generation(&db)==g);
    assert(!qrxdb_verify(&db));assert(!qrxdb_compact(&db));assert(!qrxdb_merkle_root_hex(&db,root_after));assert(!strcmp(root_after,root0));missing(&db,"tx:applied:tx-lock");missing(&db,"consensus:compute:funding:graph1");
    assert(!qrxdb_close(&db));char cmd[1200];snprintf(cmd,sizeof(cmd),"rm -rf '%s'",dir);system(cmd);
    puts("compute phase 159 PoUC upstream undo/end-to-end reorg atomicity: PASS");return 0;
}
