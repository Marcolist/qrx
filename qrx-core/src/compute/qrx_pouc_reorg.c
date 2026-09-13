#include "compute/qrx_pouc_reorg.h"
#include "compute/qrx_pouc_journal.h"
#include "compute/qrx_pouc_liveness.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#define POOL_KEY "consensus:compute:escrow_pool"

typedef struct { QrxPoucRewardJournalEntry e[QRX_POUC_REWARD_JOURNAL_MAX_REVERT]; uint32_t n; } RewardSet;

static int safe(const char*s){return s&&s[0]&&!strchr(s,'|')&&!strchr(s,'\n')&&!strchr(s,'\r');}
static int hex64(const char*s){if(!s||strlen(s)!=64)return 0;for(int i=0;i<64;i++){char c=s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;}return 1;}
static int u64s(const char*s,uint64_t*out){if(!s||!*s||!out||*s=='-')return -1;errno=0;char*e=NULL;unsigned long long v=strtoull(s,&e,10);if(errno||!e||*e)return -1;*out=(uint64_t)v;return 0;}
static int dbu64(QrxDB*d,const char*k,uint64_t*out){char b[128];return qrxdb_get(d,k,b,sizeof(b))||u64s(b,out)?-1:0;}
static int putu64(QrxDBBatch*b,const char*k,uint64_t v){char x[64];snprintf(x,sizeof(x),"%llu",(unsigned long long)v);return qrxdb_batch_put(b,k,x);}

static int encode(const QrxPoucRewardJournalEntry*e,char*out,size_t n){
    if(!e||!out||e->version!=QRX_POUC_REORG_VERSION||!e->apply_height||!e->reward_atoms||
       (e->role!=1&&e->role!=2)||!safe(e->txid)||!hex64(e->receipt_commitment)||
       !hex64(e->graph_commitment)||!safe(e->verifier))return -1;
    int z=snprintf(out,n,"APPLIED|%llu|%u|%llu|%llu|%llu|%llu|%llu|%s|%s|%s|%s",
        (unsigned long long)e->apply_height,(unsigned)e->role,(unsigned long long)e->reward_atoms,
        (unsigned long long)e->pool_before,(unsigned long long)e->pool_after,
        (unsigned long long)e->verification_spent_before,(unsigned long long)e->verification_spent_after,
        e->txid,e->receipt_commitment,e->graph_commitment,e->verifier);
    return z>0&&(size_t)z<n?0:-1;
}
static int decode(const char*s,QrxPoucRewardJournalEntry*e){
    if(!s||!e)return -1;memset(e,0,sizeof(*e));char st[16]={0};unsigned role=0;unsigned long long h=0,r=0,pb=0,pa=0,sb=0,sa=0;
    if(sscanf(s,"%15[^|]|%llu|%u|%llu|%llu|%llu|%llu|%llu|%128[^|]|%64[^|]|%64[^|]|%159s",
       st,&h,&role,&r,&pb,&pa,&sb,&sa,e->txid,e->receipt_commitment,e->graph_commitment,e->verifier)!=12)return -1;
    if(strcmp(st,"APPLIED")&&strcmp(st,"REVERTED"))return -1;
    e->version=QRX_POUC_REORG_VERSION;e->apply_height=h;e->role=(uint8_t)role;e->reward_atoms=r;e->pool_before=pb;e->pool_after=pa;e->verification_spent_before=sb;e->verification_spent_after=sa;
    return (hex64(e->receipt_commitment)&&hex64(e->graph_commitment)&&safe(e->txid)&&safe(e->verifier))?0:-1;
}

int qrx_pouc_reward_journal_stage(QrxDB*d,QrxDBBatch*b,const QrxPoucPipelineEffect*e,const char*txid,uint64_t h){
    if(!d||!b||!e||!safe(txid)||!h||!e->reward_credit_atoms||
       (e->op!=QRX_POUC_PIPELINE_VERIFY&&e->op!=QRX_POUC_PIPELINE_CHALLENGE)||
       !hex64(e->receipt_commitment)||!safe(e->verifier)||!hex64(e->escrow_before.graph_commitment)||
       strcmp(e->escrow_before.graph_commitment,e->escrow_after.graph_commitment))return -1;
    if(e->escrow_after.verification_reward_spent_atoms<e->escrow_before.verification_reward_spent_atoms||
       e->escrow_after.verification_reward_spent_atoms-e->escrow_before.verification_reward_spent_atoms!=e->reward_credit_atoms)return -2;
    uint64_t pool=0;if(dbu64(d,POOL_KEY,&pool)||pool<e->reward_credit_atoms)return -3;
    QrxPoucRewardJournalEntry j;memset(&j,0,sizeof(j));j.version=QRX_POUC_REORG_VERSION;j.apply_height=h;j.role=e->reward_role;j.reward_atoms=e->reward_credit_atoms;j.pool_before=pool;j.pool_after=pool-e->reward_credit_atoms;j.verification_spent_before=e->escrow_before.verification_reward_spent_atoms;j.verification_spent_after=e->escrow_after.verification_reward_spent_atoms;snprintf(j.txid,sizeof(j.txid),"%s",txid);snprintf(j.receipt_commitment,65,"%s",e->receipt_commitment);snprintf(j.graph_commitment,65,"%s",e->escrow_before.graph_commitment);snprintf(j.verifier,sizeof(j.verifier),"%s",e->verifier);
    char k[512],v[1200];snprintf(k,sizeof(k),"%s%020llu:%s",QRX_POUC_REWARD_JOURNAL_PREFIX,(unsigned long long)h,txid);char old[16];if(qrxdb_get(d,k,old,sizeof(old))==0)return -4;if(encode(&j,v,sizeof(v)))return -5;return qrxdb_batch_put(b,k,v);
}

static int collect(const char*k,const char*v,uint32_t n,void*ctx){(void)k;RewardSet*s=(RewardSet*)ctx;if(s->n>=QRX_POUC_REWARD_JOURNAL_MAX_REVERT)return -1;char b[1200];if(n>=sizeof(b))return -1;memcpy(b,v,n);b[n]=0;if(!strncmp(b,"REVERTED|",9))return 0;QrxPoucRewardJournalEntry e;if(decode(b,&e))return -1;s->e[s->n++]=e;return 0;}
static int cmp_desc(const void*a,const void*b){const QrxPoucRewardJournalEntry*x=a,*y=b;if(x->apply_height<y->apply_height)return 1;if(x->apply_height>y->apply_height)return -1;return strcmp(y->txid,x->txid);}

int qrx_pouc_reward_journal_revert_above_height(QrxDB*d,uint64_t canon,uint32_t*out){
    if(!d)return -1;RewardSet s;memset(&s,0,sizeof(s));if(qrxdb_scan_prefix(d,QRX_POUC_REWARD_JOURNAL_PREFIX,collect,&s))return -2;qsort(s.e,s.n,sizeof(s.e[0]),cmp_desc);uint32_t reverted=0;
    for(uint32_t i=0;i<s.n;i++){QrxPoucRewardJournalEntry*e=&s.e[i];if(e->apply_height<=canon)continue;char bk[512],bv[128];snprintf(bk,sizeof(bk),"acct:balance:%s",e->verifier);uint64_t bal=0;if(dbu64(d,bk,&bal)||bal<e->reward_atoms)return -3;uint64_t pool=0;if(dbu64(d,POOL_KEY,&pool)||UINT64_MAX-pool<e->reward_atoms)return -4;QrxComputeEscrow esc;if(qrx_pouc_journal_get_escrow(d,e->graph_commitment,&esc))return -5;if(esc.verification_reward_spent_atoms<e->reward_atoms)return -6;esc.verification_reward_spent_atoms-=e->reward_atoms;
        QrxDBBatch b;if(qrxdb_batch_begin(d,&b))return -7;if(putu64(&b,bk,bal-e->reward_atoms)||putu64(&b,POOL_KEY,pool+e->reward_atoms)||qrx_pouc_journal_stage_escrow(&b,&esc)){qrxdb_batch_abort(&b);return -8;}char jk[512],jv[1200],rev[1200];snprintf(jk,sizeof(jk),"%s%020llu:%s",QRX_POUC_REWARD_JOURNAL_PREFIX,(unsigned long long)e->apply_height,e->txid);if(encode(e,jv,sizeof(jv))){qrxdb_batch_abort(&b);return -9;}snprintf(rev,sizeof(rev),"REVERTED|%s",jv+8);if(qrxdb_batch_put(&b,jk,rev)||qrxdb_batch_commit(&b)){qrxdb_batch_abort(&b);return -10;}reverted++;
    }
    if(out)*out=reverted;return 0;
}

int qrx_pouc_canonical_reorg_hook(QrxDB*d,uint64_t canon,QrxPoucCanonicalReorgReport*out){
    if(!d)return -1;QrxPoucCanonicalReorgReport r;memset(&r,0,sizeof(r));r.version=QRX_POUC_REORG_VERSION;r.canonical_height=canon;
    /* 0.0.9.34+ transactions carry a generic outer-apply undo journal that
       restores every key touched by the PoUC transaction in ONE WAL commit.
       Older specialized journals remain as backward-compatible fallbacks. */
    if(qrx_pouc_tx_undo_revert_above_height(d,canon,&r.atomic_tx_reverted))return -2;
    if(qrx_pouc_journal_revert_above_height(d,canon,&r.settlement_reverted))return -3;
    if(qrx_pouc_liveness_revert_above_height(d,canon,&r.liveness_reverted))return -4;
    if(qrx_pouc_reward_journal_revert_above_height(d,canon,&r.reward_reverted))return -5;
    if(out)*out=r;return 0;
}
