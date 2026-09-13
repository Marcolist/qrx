#include "compute/qrx_pouc_undo.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define META_APPLIED "APPLIED"

static int safe_token(const char*s){return s&&s[0]&&!strchr(s,'|')&&!strchr(s,'\n')&&!strchr(s,'\r');}
static int starts(const char*s,const char*p){return s&&p&&!strncmp(s,p,strlen(p));}
static char hx(unsigned v){return (char)(v<10?'0'+v:'a'+(v-10));}
static int unhx(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static char*hex_encode(const char*p,size_t n){if(n>(SIZE_MAX-1)/2)return NULL;char*out=(char*)malloc(n*2+1);if(!out)return NULL;for(size_t i=0;i<n;i++){unsigned char c=(unsigned char)p[i];out[2*i]=hx(c>>4);out[2*i+1]=hx(c&15);}out[n*2]=0;return out;}
static char*hex_decode(const char*s,size_t*outn){if(!s)return NULL;size_t n=strlen(s);if(n&1)return NULL;char*out=(char*)malloc(n/2+1);if(!out)return NULL;for(size_t i=0;i<n;i+=2){int a=unhx(s[i]),b=unhx(s[i+1]);if(a<0||b<0){free(out);return NULL;}out[i/2]=(char)((a<<4)|b);}out[n/2]=0;if(outn)*outn=n/2;return out;}
static int key_seen(const QrxDBBatch*b,size_t upto,size_t at){for(size_t i=0;i<at&&i<upto;i++)if(b->entries[i].key_len==b->entries[at].key_len&&!memcmp(b->entries[i].key,b->entries[at].key,b->entries[at].key_len))return 1;return 0;}

static int snapshot_stage(QrxDB*d,QrxDBBatch*b,const char*base,uint32_t idx,const char*k,uint32_t klen){
    if(!d||!b||!base||!k||!klen||klen>4096)return -1;
    QrxDBView v;int ex=qrxdb_get_view(d,k,&v)==0;if(ex&&v.value_len>QRX_POUC_TX_UNDO_MAX_SNAPSHOT_VALUE)return -2;
    char*kh=hex_encode(k,klen),*vh=ex?hex_encode(v.value,v.value_len):strdup("");if(!kh||!vh){free(kh);free(vh);return -3;}
    size_t need=strlen(kh)+strlen(vh)+32;char*sv=(char*)malloc(need);if(!sv){free(kh);free(vh);return -3;}snprintf(sv,need,"%u|%s|%s",(unsigned)ex,kh,vh);
    char sk[640];snprintf(sk,sizeof(sk),"%s:snapshot:%04u",base,idx);int rc=qrxdb_batch_put(b,sk,sv);free(kh);free(vh);free(sv);return rc;
}

int qrx_pouc_tx_undo_stage(QrxDB*d,QrxDBBatch*b,const char*txid,const char*tt,uint64_t h){
    if(!d||!b||!b->active||b->db!=d||!safe_token(txid)||!safe_token(tt)||!h)return -1;
    size_t original=b->count;uint32_t unique=0;
    for(size_t i=0;i<original;i++)if(!starts(b->entries[i].key,QRX_POUC_TX_UNDO_PREFIX)&&!key_seen(b,original,i))unique++;
    if(!unique||unique>65535)return -2;
    char base[512];snprintf(base,sizeof(base),"%s%020llu:%020llu:%s",QRX_POUC_TX_UNDO_PREFIX,(unsigned long long)h,(unsigned long long)b->generation,txid);
    uint32_t idx=0;for(size_t i=0;i<original;i++){
        if(starts(b->entries[i].key,QRX_POUC_TX_UNDO_PREFIX)||key_seen(b,original,i))continue;
        if(snapshot_stage(d,b,base,idx++,b->entries[i].key,b->entries[i].key_len))return -3;
    }
    char meta[320];snprintf(meta,sizeof(meta),"%s|%llu|%llu|%u|%s|%s",META_APPLIED,(unsigned long long)h,(unsigned long long)b->generation,idx,tt,txid);
    return qrxdb_batch_put(b,base,meta);
}

typedef struct {QrxPoucTxUndoEntry *a;uint32_t n,cap;} UndoSet;
static int collect_meta(const char*k,const char*v,uint32_t vl,void*ctxp){(void)vl;UndoSet*s=(UndoSet*)ctxp;if(strstr(k,":snapshot:"))return 0;char st[16]={0},tt[64]={0},tx[129]={0};unsigned long long h=0,g=0;unsigned snaps=0;if(sscanf(v,"%15[^|]|%llu|%llu|%u|%63[^|]|%128s",st,&h,&g,&snaps,tt,tx)!=6||strcmp(st,META_APPLIED))return 0;if(!safe_token(tt)||!safe_token(tx)||!h||!g||!snaps)return -1;if(s->n==s->cap){uint32_t nc=s->cap?s->cap*2:64;if(nc>QRX_POUC_TX_UNDO_MAX_REVERT)nc=QRX_POUC_TX_UNDO_MAX_REVERT;if(nc<=s->cap)return -1;QrxPoucTxUndoEntry*na=(QrxPoucTxUndoEntry*)realloc(s->a,(size_t)nc*sizeof(*na));if(!na)return -1;s->a=na;s->cap=nc;}QrxPoucTxUndoEntry*e=&s->a[s->n++];memset(e,0,sizeof(*e));e->version=1;e->apply_height=h;e->apply_generation=g;e->snapshot_count=snaps;snprintf(e->txid,sizeof(e->txid),"%s",tx);snprintf(e->tx_type,sizeof(e->tx_type),"%s",tt);return 0;}
static int cmp_desc(const void*aa,const void*bb){const QrxPoucTxUndoEntry*a=aa,*b=bb;if(a->apply_generation<b->apply_generation)return 1;if(a->apply_generation>b->apply_generation)return -1;return strcmp(b->txid,a->txid);}
static int restore_snapshot(QrxDB*d,QrxDBBatch*b,const char*base,uint32_t idx){char sk[640];snprintf(sk,sizeof(sk),"%s:snapshot:%04u",base,idx);QrxDBView v;if(qrxdb_get_view(d,sk,&v))return -1;char*tmp=(char*)malloc((size_t)v.value_len+1);if(!tmp)return -2;memcpy(tmp,v.value,v.value_len);tmp[v.value_len]=0;char*p1=strchr(tmp,'|');if(!p1){free(tmp);return -3;}*p1=0;char*p2=strchr(p1+1,'|');if(!p2){free(tmp);return -3;}*p2=0;unsigned ex=(unsigned)strtoul(tmp,NULL,10);size_t kn=0,vn=0;char*k=hex_decode(p1+1,&kn),*old=hex_decode(p2+1,&vn);if(!k||!old||!kn||(ex>1)){free(k);free(old);free(tmp);return -4;}int rc=ex?qrxdb_batch_put(b,k,old):qrxdb_batch_delete(b,k);if(!rc)rc=qrxdb_batch_delete(b,sk);free(k);free(old);free(tmp);return rc;}

int qrx_pouc_tx_undo_revert_above_height(QrxDB*d,uint64_t canon,uint32_t*out){
    if(!d)return -1;UndoSet s={0};if(qrxdb_scan_prefix(d,QRX_POUC_TX_UNDO_PREFIX,collect_meta,&s)){free(s.a);return -2;}if(s.n>1)qsort(s.a,s.n,sizeof(s.a[0]),cmp_desc);uint32_t pending=0;for(uint32_t i=0;i<s.n;i++)if(s.a[i].apply_height>canon)pending++;if(!pending){free(s.a);if(out)*out=0;return 0;}
    QrxDBBatch b;if(qrxdb_batch_begin(d,&b)){free(s.a);return -3;}uint32_t done=0;
    for(uint32_t i=0;i<s.n;i++){QrxPoucTxUndoEntry*e=&s.a[i];if(e->apply_height<=canon)continue;char base[512];snprintf(base,sizeof(base),"%s%020llu:%020llu:%s",QRX_POUC_TX_UNDO_PREFIX,(unsigned long long)e->apply_height,(unsigned long long)e->apply_generation,e->txid);for(uint32_t j=e->snapshot_count;j>0;j--)if(restore_snapshot(d,&b,base,j-1)){qrxdb_batch_abort(&b);free(s.a);return -4;}if(qrxdb_batch_delete(&b,base)){qrxdb_batch_abort(&b);free(s.a);return -5;}done++;}
    if(qrxdb_batch_commit(&b)){qrxdb_batch_abort(&b);free(s.a);return -6;}free(s.a);if(out)*out=done;return 0;
}
