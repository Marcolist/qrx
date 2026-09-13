#include "compute/qrx_pouc_journal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define JR_PREFIX "pouc/journal/active/"
#define RP_PREFIX "pouc/journal/replay/"
#define ES_PREFIX "pouc/journal/escrow/"
#define BF_PREFIX "pouc/journal/before/"
#define HI_PREFIX "pouc/journal/history/"
#define HB_PREFIX "pouc/journal/history-before/"
#define HA_PREFIX "pouc/journal/history-after/"

static int hex64(const char*s){if(!s||strlen(s)!=64)return 0;for(size_t i=0;i<64;i++){char c=s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;}return 1;}
static int token_safe(const char*s){return s&&s[0]&&!strchr(s,'|')&&!strchr(s,'\n')&&!strchr(s,'\r');}
static void key(char*out,size_t n,const char*p,const char*id){snprintf(out,n,"%s%s",p,id);}
static void histkey(char*out,size_t n,const char*p,const char*r,uint64_t h,const char*d){snprintf(out,n,"%s%s/%020llu/%s",p,r,(unsigned long long)h,d);}

static QrxPoucJournalState outcome_state(QrxPoucSettlementOutcome o){
    switch(o){case QRX_POUC_SETTLEMENT_FROZEN:return QRX_POUC_JOURNAL_FROZEN;case QRX_POUC_SETTLEMENT_PAYOUT:return QRX_POUC_JOURNAL_PAYOUT;case QRX_POUC_SETTLEMENT_REJECTED:return QRX_POUC_JOURNAL_REJECTED;default:return QRX_POUC_JOURNAL_NONE;}
}

static int escrow_encode(const QrxComputeEscrow*e,char*out,size_t n){
    if(!e||!out||n<640||!hex64(e->graph_commitment))return -1;
    int z=snprintf(out,n,"%u|%u|%llu|%llu|%llu|%llu|%llu|%llu|%llu|%llu|%llu|%llu|%s|%s",
        e->market_version,(unsigned)e->state,
        (unsigned long long)e->max_compute_atoms,
        (unsigned long long)e->fasttrack_fee_atoms,
        (unsigned long long)e->verification_reward_atoms,
        (unsigned long long)e->verification_reward_spent_atoms,
        (unsigned long long)e->locked_atoms,
        (unsigned long long)e->actual_compute_atoms,
        (unsigned long long)e->fasttrack_dev_atoms,
        (unsigned long long)e->fasttrack_net_atoms,
        (unsigned long long)e->refund_atoms,
        (unsigned long long)e->expiry_height,
        e->graph_commitment,e->owner);
    return z>0&&(size_t)z<n?0:-1;
}
static int escrow_decode(const char*s,QrxComputeEscrow*e){
    if(!s||!e)return -1;
    memset(e,0,sizeof(*e));
    unsigned mv=0,st=0;unsigned long long a=0,b=0,vr=0,vs=0,c=0,d=0,f=0,g=0,h=0,i=0;char gc[65]={0},owner[160]={0};
    int fields=sscanf(s,"%u|%u|%llu|%llu|%llu|%llu|%llu|%llu|%llu|%llu|%llu|%llu|%64[^|]|%159s",&mv,&st,&a,&b,&vr,&vs,&c,&d,&f,&g,&h,&i,gc,owner);
    if(fields==14){
        e->verification_reward_atoms=vr;e->verification_reward_spent_atoms=vs;
    }else{
        /* Backward-compatible decode for pre-0.0.9.31 escrow snapshots. */
        memset(e,0,sizeof(*e));mv=st=0;a=b=c=d=f=g=h=i=0;gc[0]=owner[0]=0;
        if(sscanf(s,"%u|%u|%llu|%llu|%llu|%llu|%llu|%llu|%llu|%llu|%64[^|]|%159s",&mv,&st,&a,&b,&c,&d,&f,&g,&h,&i,gc,owner)!=12)return -1;
        e->verification_reward_atoms=0;e->verification_reward_spent_atoms=0;
    }
    if(!hex64(gc))return -1;
    e->market_version=mv;e->state=(QrxComputeEscrowState)st;e->max_compute_atoms=a;e->fasttrack_fee_atoms=b;e->locked_atoms=c;e->actual_compute_atoms=d;e->fasttrack_dev_atoms=f;e->fasttrack_net_atoms=g;e->refund_atoms=h;e->expiry_height=i;snprintf(e->graph_commitment,sizeof(e->graph_commitment),"%s",gc);snprintf(e->owner,sizeof(e->owner),"%s",owner);return 0;
}
static int entry_encode(const QrxPoucJournalEntry*e,char*out,size_t n){
    if(!e||!out||!hex64(e->replay_key)||!hex64(e->receipt_commitment)||!hex64(e->decision_commitment)||!hex64(e->graph_commitment))return -1;
    int z=snprintf(out,n,"%u|%u|%llu|%llu|%llu|%llu|%u|%u|%s|%s|%s|%s|%s|%s|%s",
      e->version,(unsigned)e->state,(unsigned long long)e->applied_height,(unsigned long long)e->settlement_nonce,
      (unsigned long long)e->payout_compute_atoms,(unsigned long long)e->refund_atoms,(unsigned)e->slash_candidate,(unsigned)e->jail_candidate,
      e->replay_key,e->receipt_commitment,e->decision_commitment,e->graph_commitment,e->chain_id,e->genesis_hash,e->protocol_version);
    return z>0&&(size_t)z<n?0:-1;
}
static int entry_decode(const char*s,QrxPoucJournalEntry*e){
    if(!s||!e)return -1;memset(e,0,sizeof(*e));unsigned v=0,st=0,sl=0,jl=0;unsigned long long h=0,nn=0,p=0,r=0;
    char rk[65]={0},rc[65]={0},dc[65]={0},gc[65]={0},ci[QRX_POUC_MAINNET_CHAIN_ID_MAX+1]={0},gh[65]={0},pv[QRX_POUC_MAINNET_PROTOCOL_MAX+1]={0};
    if(sscanf(s,"%u|%u|%llu|%llu|%llu|%llu|%u|%u|%64[^|]|%64[^|]|%64[^|]|%64[^|]|%64[^|]|%64[^|]|%32s",
      &v,&st,&h,&nn,&p,&r,&sl,&jl,rk,rc,dc,gc,ci,gh,pv)!=15)return -1;
    if(!hex64(rk)||!hex64(rc)||!hex64(dc)||!hex64(gc)||!hex64(gh))return -1;
    e->version=v;e->state=(QrxPoucJournalState)st;e->applied_height=h;e->settlement_nonce=nn;e->payout_compute_atoms=p;e->refund_atoms=r;e->slash_candidate=(uint8_t)sl;e->jail_candidate=(uint8_t)jl;
    snprintf(e->replay_key,65,"%s",rk);snprintf(e->receipt_commitment,65,"%s",rc);snprintf(e->decision_commitment,65,"%s",dc);snprintf(e->graph_commitment,65,"%s",gc);snprintf(e->chain_id,sizeof(e->chain_id),"%s",ci);snprintf(e->genesis_hash,65,"%s",gh);snprintf(e->protocol_version,sizeof(e->protocol_version),"%s",pv);return 0;
}
static int getstr(QrxDB*db,const char*k,char*out,size_t n){int rc=qrxdb_get(db,k,out,n);return rc;}

int qrx_pouc_journal_get(QrxDB*db,const char*r,QrxPoucJournalEntry*out){
    if(!db||!hex64(r)||!out)return -1;char k[160],v[1024];key(k,sizeof(k),JR_PREFIX,r);if(getstr(db,k,v,sizeof(v)))return -2;return entry_decode(v,out);
}
int qrx_pouc_journal_get_escrow(QrxDB*db,const char*g,QrxComputeEscrow*out){
    if(!db||!hex64(g)||!out)return -1;char k[160],v[640];key(k,sizeof(k),ES_PREFIX,g);if(getstr(db,k,v,sizeof(v)))return -2;return escrow_decode(v,out);
}
int qrx_pouc_journal_replay_consumed(QrxDB*db,const char*r){
    if(!db||!hex64(r))return -1;char k[160],v[64];key(k,sizeof(k),RP_PREFIX,r);if(getstr(db,k,v,sizeof(v)))return 0;if(!strcmp(v,"REVERTED"))return 0;return 1;
}

static int decision_equal(const QrxPoucSettlementDecision*a,const QrxPoucSettlementDecision*b){
    return a&&b&&a->version==b->version&&a->outcome==b->outcome&&a->activation_ready==b->activation_ready&&a->payout_allowed==b->payout_allowed&&a->slash_candidate==b->slash_candidate&&a->jail_candidate==b->jail_candidate&&a->payout_compute_atoms==b->payout_compute_atoms&&a->refund_atoms==b->refund_atoms&&!strcmp(a->receipt_commitment,b->receipt_commitment)&&!strcmp(a->replay_key,b->replay_key)&&!strcmp(a->decision_commitment,b->decision_commitment);
}

int qrx_pouc_journal_stage_escrow(QrxDBBatch*b,const QrxComputeEscrow*e){
    if(!b||!e||!hex64(e->graph_commitment)||!token_safe(e->owner))return -1;
    char k[160],v[640];if(escrow_encode(e,v,sizeof(v)))return -2;key(k,sizeof(k),ES_PREFIX,e->graph_commitment);return qrxdb_batch_put(b,k,v);
}
int qrx_pouc_journal_put_escrow(QrxDB*db,const QrxComputeEscrow*e){
    if(!db||!e)return -1;QrxDBBatch b;if(qrxdb_batch_begin(db,&b))return -2;if(qrx_pouc_journal_stage_escrow(&b,e)||qrxdb_batch_commit(&b)){qrxdb_batch_abort(&b);return -3;}return 0;
}

static int transition_after(const QrxPoucSettlementRequest*req,const QrxPoucSettlementDecision*d,const QrxComputeEscrow*before,QrxComputeEscrow*after){
    if(!req||!d||!before||!after)return -1;*after=*before;
    if(d->outcome==QRX_POUC_SETTLEMENT_PAYOUT)return qrx_pouc_mainnet_apply_payout(d,after,req->current_height);
    if(d->outcome==QRX_POUC_SETTLEMENT_REJECTED)return qrx_compute_escrow_refund(after,req->current_height);
    return d->outcome==QRX_POUC_SETTLEMENT_FROZEN?0:-2;
}

int qrx_pouc_journal_stage(QrxDB*db,QrxDBBatch*b,const QrxPoucSettlementRequest*req,const QrxPoucSettlementDecision*d,const QrxComputeEscrow*escrow,QrxComputeEscrow*after_out){
    if(!db||!b||!req||!d||!escrow||d->version!=QRX_POUC_MAINNET_VERSION||!hex64(d->replay_key)||!hex64(d->decision_commitment)||!token_safe(req->chain.chain_id)||!token_safe(req->chain.protocol_version)||!token_safe(escrow->owner))return -1;
    QrxPoucSettlementDecision expected;if(qrx_pouc_mainnet_decide(req,escrow,&expected)||!decision_equal(&expected,d))return -2;
    QrxPoucJournalEntry old;int oldrc=qrx_pouc_journal_get(db,d->replay_key,&old);
    if(oldrc==0){
        if(old.state==QRX_POUC_JOURNAL_PAYOUT||old.state==QRX_POUC_JOURNAL_REJECTED)return -3;
        if(old.state==QRX_POUC_JOURNAL_FROZEN&&d->outcome==QRX_POUC_SETTLEMENT_FROZEN){if(strcmp(old.decision_commitment,d->decision_commitment))return -4;if(after_out)*after_out=*escrow;return 0;}
        if(old.state!=QRX_POUC_JOURNAL_FROZEN&&old.state!=QRX_POUC_JOURNAL_REVERTED)return -5;
        if(strcmp(old.receipt_commitment,d->receipt_commitment)||strcmp(old.graph_commitment,escrow->graph_commitment)||strcmp(old.chain_id,req->chain.chain_id)||strcmp(old.genesis_hash,req->chain.genesis_hash)||strcmp(old.protocol_version,req->chain.protocol_version))return -6;
    } else if(oldrc!=-2)return -7;

    QrxComputeEscrow before=*escrow,after=*escrow;if(transition_after(req,d,&before,&after))return -8;
    QrxPoucJournalEntry je;memset(&je,0,sizeof(je));je.version=QRX_POUC_JOURNAL_VERSION;je.state=outcome_state(d->outcome);je.applied_height=req->current_height;je.settlement_nonce=req->settlement_nonce;je.payout_compute_atoms=d->payout_compute_atoms;je.refund_atoms=d->refund_atoms;je.slash_candidate=d->slash_candidate;je.jail_candidate=d->jail_candidate;
    snprintf(je.replay_key,65,"%s",d->replay_key);snprintf(je.receipt_commitment,65,"%s",d->receipt_commitment);snprintf(je.decision_commitment,65,"%s",d->decision_commitment);snprintf(je.graph_commitment,65,"%s",escrow->graph_commitment);snprintf(je.chain_id,sizeof(je.chain_id),"%s",req->chain.chain_id);snprintf(je.genesis_hash,65,"%s",req->chain.genesis_hash);snprintf(je.protocol_version,sizeof(je.protocol_version),"%s",req->chain.protocol_version);
    char ev[1024],be[640],ae[640],jk[160],rk[160],ek[160],bk[160],hik[320],hbk[320],hak[320];if(entry_encode(&je,ev,sizeof(ev))||escrow_encode(&before,be,sizeof(be))||escrow_encode(&after,ae,sizeof(ae)))return -9;
    key(jk,sizeof(jk),JR_PREFIX,d->replay_key);key(rk,sizeof(rk),RP_PREFIX,d->replay_key);key(ek,sizeof(ek),ES_PREFIX,escrow->graph_commitment);key(bk,sizeof(bk),BF_PREFIX,d->replay_key);
    histkey(hik,sizeof(hik),HI_PREFIX,d->replay_key,req->current_height,d->decision_commitment);histkey(hbk,sizeof(hbk),HB_PREFIX,d->replay_key,req->current_height,d->decision_commitment);histkey(hak,sizeof(hak),HA_PREFIX,d->replay_key,req->current_height,d->decision_commitment);
    if(qrxdb_batch_put(b,jk,ev)||qrxdb_batch_put(b,rk,qrx_pouc_journal_state_name(je.state))||qrxdb_batch_put(b,bk,be)||qrxdb_batch_put(b,ek,ae)||qrxdb_batch_put(b,hik,ev)||qrxdb_batch_put(b,hbk,be)||qrxdb_batch_put(b,hak,ae))return -10;
    if(after_out)*after_out=after;return 0;
}

int qrx_pouc_journal_apply(QrxDB*db,const QrxPoucSettlementRequest*req,const QrxPoucSettlementDecision*d,QrxComputeEscrow*escrow){
    if(!db||!req||!d||!escrow)return -1;QrxDBBatch b;QrxComputeEscrow after;if(qrxdb_batch_begin(db,&b))return -10;int rc=qrx_pouc_journal_stage(db,&b,req,d,escrow,&after);if(rc){qrxdb_batch_abort(&b);return rc;}if(qrxdb_batch_commit(&b)){qrxdb_batch_abort(&b);return -11;}*escrow=after;return 0;
}

typedef struct {uint64_t h;uint32_t n;QrxPoucJournalEntry e[QRX_POUC_JOURNAL_MAX_REVERT];} RevertCtx;
static int collect_revert(const char*k,const char*v,uint32_t n,void*ctxp){(void)k;RevertCtx*c=(RevertCtx*)ctxp;if(c->n>=QRX_POUC_JOURNAL_MAX_REVERT)return -1;char tmp[1024];if(n>=sizeof(tmp))return -1;memcpy(tmp,v,n);tmp[n]=0;QrxPoucJournalEntry e;if(entry_decode(tmp,&e))return -1;if(e.state!=QRX_POUC_JOURNAL_REVERTED&&e.applied_height>c->h)c->e[c->n++]=e;return 0;}

typedef struct {uint64_t h;QrxPoucJournalEntry best;int have;char best_key[320];} HistCtx;
static int collect_history(const char*k,const char*v,uint32_t n,void*ctxp){HistCtx*c=(HistCtx*)ctxp;char tmp[1024];if(n>=sizeof(tmp))return -1;memcpy(tmp,v,n);tmp[n]=0;QrxPoucJournalEntry e;if(entry_decode(tmp,&e))return -1;if(e.applied_height<=c->h&&(!c->have||e.applied_height>c->best.applied_height)){c->best=e;c->have=1;snprintf(c->best_key,sizeof(c->best_key),"%s",k);}return 0;}

int qrx_pouc_journal_revert_above_height(QrxDB*db,uint64_t h,uint32_t*out){
    if(!db)return -1;RevertCtx c;memset(&c,0,sizeof(c));c.h=h;if(qrxdb_scan_prefix(db,JR_PREFIX,collect_revert,&c))return -2;if(!c.n){if(out)*out=0;return 0;}
    QrxDBBatch b;if(qrxdb_batch_begin(db,&b))return -3;
    for(uint32_t i=0;i<c.n;i++){
        QrxPoucJournalEntry cur=c.e[i];char prefix[200];snprintf(prefix,sizeof(prefix),"%s%s/",HI_PREFIX,cur.replay_key);HistCtx hc;memset(&hc,0,sizeof(hc));hc.h=h;if(qrxdb_scan_prefix(db,prefix,collect_history,&hc)){qrxdb_batch_abort(&b);return -4;}
        char jk[160],rk[160],ek[160],ev[1024],src[320],av[640];key(jk,sizeof(jk),JR_PREFIX,cur.replay_key);key(rk,sizeof(rk),RP_PREFIX,cur.replay_key);key(ek,sizeof(ek),ES_PREFIX,cur.graph_commitment);
        if(hc.have){
            if(entry_encode(&hc.best,ev,sizeof(ev))){qrxdb_batch_abort(&b);return -5;}
            const char *suffix=hc.best_key+strlen(HI_PREFIX);snprintf(src,sizeof(src),"%s%s",HA_PREFIX,suffix);if(getstr(db,src,av,sizeof(av))){qrxdb_batch_abort(&b);return -6;}
            if(qrxdb_batch_put(&b,jk,ev)||qrxdb_batch_put(&b,rk,qrx_pouc_journal_state_name(hc.best.state))||qrxdb_batch_put(&b,ek,av)){qrxdb_batch_abort(&b);return -7;}
        }else{
            char eprefix[200];snprintf(eprefix,sizeof(eprefix),"%s%s/",HI_PREFIX,cur.replay_key);HistCtx first;memset(&first,0,sizeof(first));first.h=UINT64_MAX;if(qrxdb_scan_prefix(db,eprefix,collect_history,&first)||!first.have){qrxdb_batch_abort(&b);return -8;}
            /* collect_history with UINT64_MAX returns latest; walk backwards is not available, so use the stable pre-transition snapshot for the replay key. */
            char bk[160];key(bk,sizeof(bk),BF_PREFIX,cur.replay_key);if(getstr(db,bk,av,sizeof(av))){qrxdb_batch_abort(&b);return -9;}
            cur.state=QRX_POUC_JOURNAL_REVERTED;cur.applied_height=h;if(entry_encode(&cur,ev,sizeof(ev))){qrxdb_batch_abort(&b);return -10;}
            if(qrxdb_batch_put(&b,jk,ev)||qrxdb_batch_put(&b,rk,"REVERTED")||qrxdb_batch_put(&b,ek,av)){qrxdb_batch_abort(&b);return -11;}
        }
    }
    if(qrxdb_batch_commit(&b)){qrxdb_batch_abort(&b);return -12;}if(out)*out=c.n;return 0;
}
const char*qrx_pouc_journal_state_name(QrxPoucJournalState s){switch(s){case QRX_POUC_JOURNAL_FROZEN:return "FROZEN";case QRX_POUC_JOURNAL_PAYOUT:return "PAYOUT";case QRX_POUC_JOURNAL_REJECTED:return "REJECTED";case QRX_POUC_JOURNAL_REVERTED:return "REVERTED";default:return "NONE";}}
