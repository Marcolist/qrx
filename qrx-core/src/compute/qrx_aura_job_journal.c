#include "compute/qrx_aura_job_journal.h"
#include <errno.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define qrx_fileno _fileno
#define qrx_fsync _commit
#else
#include <unistd.h>
#define qrx_fileno fileno
#define qrx_fsync fsync
#endif
#define MAGIC "QRXAJR41"
#define DOMAIN "QRX/AURA/JOB-RESULT-JOURNAL/V1"
#define RESULT_DOMAIN "QRX/AURA/JOB-RESULT/V1"
static uint32_t be32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t be64(const uint8_t*p){uint64_t v=0;for(int i=0;i<8;i++)v=(v<<8)|p[i];return v;}
static void put32(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void put64(uint8_t*p,uint64_t v){for(int i=7;i>=0;i--){p[i]=(uint8_t)v;v>>=8;}}
static int hex64(const char*s){if(!s||strlen(s)!=64)return 0;for(int i=0;i<64;i++){char c=s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;}return 1;}
static void hex32(const uint8_t h[32],char out[65]){static const char*x="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=x[h[i]>>4];out[i*2+1]=x[h[i]&15];}out[64]=0;}
static int sha3(const char*d,const void*p,size_t n,uint8_t out[32]){EVP_MD_CTX*m=EVP_MD_CTX_new();unsigned hn=0;if(!m||EVP_DigestInit_ex(m,EVP_sha3_256(),NULL)!=1||EVP_DigestUpdate(m,d,strlen(d))!=1||(n&&EVP_DigestUpdate(m,p,n)!=1)||EVP_DigestFinal_ex(m,out,&hn)!=1||hn!=32){EVP_MD_CTX_free(m);return-1;}EVP_MD_CTX_free(m);return 0;}
static int result_hash(const void*p,size_t n,char out[65]){uint8_t h[32];if(sha3(RESULT_DOMAIN,p,n,h))return-1;hex32(h,out);OPENSSL_cleanse(h,sizeof(h));return 0;}
void qrx_aura_job_journal_init(QrxAuraJobJournal*j){if(j)memset(j,0,sizeof(*j));}
static void rec_free(QrxAuraJobRecord*r){if(!r)return;if(r->result){OPENSSL_cleanse(r->result,r->result_len);free(r->result);}memset(r,0,sizeof(*r));}
void qrx_aura_job_journal_free(QrxAuraJobJournal*j){if(!j)return;for(uint32_t i=0;i<j->count;i++)rec_free(&j->records[i]);memset(j,0,sizeof(*j));}
QrxAuraJobRecord*qrx_aura_job_journal_find(QrxAuraJobJournal*j,const char*r,const char*l,uint64_t s){if(!j||!r||!l)return NULL;for(uint32_t i=0;i<j->count;i++)if(j->records[i].sequence==s&&!strcmp(j->records[i].requester_id,r)&&!strcmp(j->records[i].lease_id,l))return &j->records[i];return NULL;}
const QrxAuraJobRecord*qrx_aura_job_journal_find_const(const QrxAuraJobJournal*j,const char*r,const char*l,uint64_t s){return qrx_aura_job_journal_find((QrxAuraJobJournal*)j,r,l,s);}
int qrx_aura_job_journal_prepare(QrxAuraJobJournal*j,const char*r,const char*l,uint64_t s,const char c[65]){if(!j||!r||!l||!s||strlen(r)>=QRX_AURA_JOB_ID_MAX||!hex64(l)||!hex64(c))return-1;QrxAuraJobRecord*e=qrx_aura_job_journal_find(j,r,l,s);if(e)return strcmp(e->request_commitment,c)?-2:0;if(j->count>=QRX_AURA_JOB_JOURNAL_MAX)return-3;e=&j->records[j->count++];memset(e,0,sizeof(*e));e->version=QRX_AURA_JOB_JOURNAL_VERSION;e->state=QRX_AURA_JOB_PREPARED;snprintf(e->requester_id,sizeof(e->requester_id),"%s",r);snprintf(e->lease_id,sizeof(e->lease_id),"%s",l);e->sequence=s;snprintf(e->request_commitment,65,"%s",c);memset(e->result_commitment,'0',64);e->result_commitment[64]=0;j->revision++;return 0;}
int qrx_aura_job_journal_commit(QrxAuraJobJournal*j,const char*r,const char*l,uint64_t s,const char c[65],uint32_t status,const void*result,size_t n){if(!j||(n&&!result)||n>QRX_AURA_JOB_JOURNAL_MAX_RESULT)return-1;QrxAuraJobRecord*e=qrx_aura_job_journal_find(j,r,l,s);if(!e||strcmp(e->request_commitment,c))return-2;uint8_t*p=NULL;if(n){p=malloc(n);if(!p)return-3;memcpy(p,result,n);}if(e->result){OPENSSL_cleanse(e->result,e->result_len);free(e->result);}e->result=p;e->result_len=n;e->result_status=status;e->state=QRX_AURA_JOB_COMMITTED;if(result_hash(result,n,e->result_commitment))return-4;j->revision++;return 0;}

typedef struct{uint8_t*p;size_t n,cap;}Buf;static int grow(Buf*b,size_t a){if(SIZE_MAX-b->n<a)return-1;size_t need=b->n+a;if(need<=b->cap)return 0;size_t c=b->cap?b->cap:512;while(c<need){if(c>SIZE_MAX/2){c=need;break;}c*=2;}uint8_t*q=realloc(b->p,c);if(!q)return-1;b->p=q;b->cap=c;return 0;}static int bp(Buf*b,const void*p,size_t n){if(grow(b,n))return-1;if(n)memcpy(b->p+b->n,p,n);b->n+=n;return 0;}static int b32(Buf*b,uint32_t v){uint8_t x[4];put32(x,v);return bp(b,x,4);}static int b64(Buf*b,uint64_t v){uint8_t x[8];put64(x,v);return bp(b,x,8);}static int bs(Buf*b,const char*s,size_t cap){if(!s||!memchr(s,0,cap))return-1;size_t n=strlen(s);return n>UINT32_MAX||b32(b,(uint32_t)n)||bp(b,s,n)?-1:0;}
typedef struct{const uint8_t*p;size_t n,o;}Rd;static int rg(Rd*r,void*out,size_t n){if(r->o>r->n||r->n-r->o<n)return-1;if(n)memcpy(out,r->p+r->o,n);r->o+=n;return 0;}static int r32(Rd*r,uint32_t*out){uint8_t x[4];if(rg(r,x,4))return-1;*out=be32(x);return 0;}static int r64(Rd*r,uint64_t*out){uint8_t x[8];if(rg(r,x,8))return-1;*out=be64(x);return 0;}static int rs(Rd*r,char*out,size_t cap){uint32_t n;if(r32(r,&n)||n>=cap||r->o>r->n||r->n-r->o<n)return-1;memcpy(out,r->p+r->o,n);out[n]=0;r->o+=n;return 0;}
static int payload(const QrxAuraJobJournal*j,uint8_t**out,size_t*outn){Buf b={0};if(b32(&b,QRX_AURA_JOB_JOURNAL_VERSION)||b64(&b,j->revision)||b32(&b,j->count)){free(b.p);return-1;}for(uint32_t i=0;i<j->count;i++){const QrxAuraJobRecord*e=&j->records[i];if(e->version!=1||(e->state!=1&&e->state!=2)||!hex64(e->lease_id)||!hex64(e->request_commitment)||!hex64(e->result_commitment)||e->result_len>QRX_AURA_JOB_JOURNAL_MAX_RESULT||b32(&b,e->version)||b32(&b,(uint32_t)e->state)||bs(&b,e->requester_id,sizeof(e->requester_id))||bs(&b,e->lease_id,sizeof(e->lease_id))||b64(&b,e->sequence)||bs(&b,e->request_commitment,65)||b32(&b,e->result_status)||b64(&b,(uint64_t)e->result_len)||bp(&b,e->result,e->result_len)||bs(&b,e->result_commitment,65)){free(b.p);return-1;}}*out=b.p;*outn=b.n;return 0;}
static int parse(const uint8_t*p,size_t n,QrxAuraJobJournal*out){Rd r={p,n,0};uint32_t v,c;uint64_t rev;if(r32(&r,&v)||r64(&r,&rev)||r32(&r,&c)||v!=1||c>QRX_AURA_JOB_JOURNAL_MAX)return-1;QrxAuraJobJournal j;qrx_aura_job_journal_init(&j);j.revision=rev;j.count=c;for(uint32_t i=0;i<c;i++){QrxAuraJobRecord*e=&j.records[i];uint32_t st;uint64_t rn;if(r32(&r,&e->version)||r32(&r,&st)||rs(&r,e->requester_id,sizeof(e->requester_id))||rs(&r,e->lease_id,sizeof(e->lease_id))||r64(&r,&e->sequence)||rs(&r,e->request_commitment,65)||r32(&r,&e->result_status)||r64(&r,&rn)||rn>QRX_AURA_JOB_JOURNAL_MAX_RESULT||rn>SIZE_MAX||r.o>r.n||r.n-r.o<(size_t)rn){qrx_aura_job_journal_free(&j);return-1;}e->state=(QrxAuraJobState)st;e->result_len=(size_t)rn;if(rn){e->result=malloc((size_t)rn);if(!e->result){qrx_aura_job_journal_free(&j);return-1;}if(rg(&r,e->result,(size_t)rn)){qrx_aura_job_journal_free(&j);return-1;}}if(rs(&r,e->result_commitment,65)||e->version!=1||(st!=1&&st!=2)||!hex64(e->lease_id)||!hex64(e->request_commitment)||!hex64(e->result_commitment)){qrx_aura_job_journal_free(&j);return-1;}char h[65];if(e->state==QRX_AURA_JOB_COMMITTED&&(result_hash(e->result,e->result_len,h)||strcmp(h,e->result_commitment))){qrx_aura_job_journal_free(&j);return-1;}}
    if(r.o!=r.n){qrx_aura_job_journal_free(&j);return-1;}qrx_aura_job_journal_free(out);*out=j;return 0;}
static int atomic_replace(const char*s,const char*d){
#ifdef _WIN32
return MoveFileExA(s,d,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?0:-1;
#else
return rename(s,d);
#endif
}
int qrx_aura_job_journal_save(const char*path,const QrxAuraJobJournal*j){if(!path||!path[0]||!j)return-1;uint8_t*p=NULL,h[32],len[8];size_t n=0;if(payload(j,&p,&n)||sha3(DOMAIN,p,n,h)){free(p);return-1;}char tmp[1400];if(snprintf(tmp,sizeof(tmp),"%s.tmp",path)>=(int)sizeof(tmp)){free(p);return-1;}FILE*f=fopen(tmp,"wb");if(!f){free(p);return-2;}put64(len,(uint64_t)n);int ok=fwrite(MAGIC,1,8,f)==8&&fwrite(len,1,8,f)==8&&(!n||fwrite(p,1,n,f)==n)&&fwrite(h,1,32,f)==32&&fflush(f)==0;int fd=qrx_fileno(f);if(ok&&(fd<0||qrx_fsync(fd)))ok=0;if(fclose(f))ok=0;free(p);OPENSSL_cleanse(h,sizeof(h));if(!ok){remove(tmp);return-3;}if(atomic_replace(tmp,path)){remove(tmp);return-4;}return 0;}
int qrx_aura_job_journal_load(const char*path,QrxAuraJobJournal*j){if(!path||!path[0]||!j)return-1;FILE*f=fopen(path,"rb");if(!f)return errno==ENOENT?1:-2;uint8_t hdr[16],stored[32],calc[32];if(fread(hdr,1,16,f)!=16||memcmp(hdr,MAGIC,8)){fclose(f);return-3;}uint64_t n=be64(hdr+8);if(n>64u*1024u*1024u||n>SIZE_MAX){fclose(f);return-3;}uint8_t*p=malloc((size_t)n?n:1);if(!p){fclose(f);return-4;}int rc=0;if((n&&fread(p,1,(size_t)n,f)!=(size_t)n)||fread(stored,1,32,f)!=32||fgetc(f)!=EOF)rc=-3;if(!rc&&(sha3(DOMAIN,p,(size_t)n,calc)||CRYPTO_memcmp(stored,calc,32)))rc=-5;if(!rc&&parse(p,(size_t)n,j))rc=-6;free(p);fclose(f);return rc;}
size_t qrx_aura_job_journal_compact(QrxAuraJobJournal*j,uint32_t keepc,uint32_t keepp){if(!j)return 0;uint32_t seen_c=0,seen_p=0;uint8_t keep[QRX_AURA_JOB_JOURNAL_MAX]={0};for(uint32_t x=j->count;x>0;x--){QrxAuraJobRecord*e=&j->records[x-1];if(e->state==QRX_AURA_JOB_COMMITTED){if(seen_c++<keepc)keep[x-1]=1;}else if(seen_p++<keepp)keep[x-1]=1;}uint32_t w=0;size_t removed=0;for(uint32_t i=0;i<j->count;i++){if(keep[i]){if(w!=i){j->records[w]=j->records[i];memset(&j->records[i],0,sizeof(j->records[i]));}w++;}else{rec_free(&j->records[i]);removed++;}}j->count=w;if(removed)j->revision++;return removed;}
