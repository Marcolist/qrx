#include "bitcoin/qrx_btc_spv.h"
#include "qrxdb.h"
#include <openssl/sha.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static int failures=0;
#define CHECK(C,M) do{if(!(C)){fprintf(stderr,"FAIL: %s\n",M);failures++;}else fprintf(stdout,"PASS: %s\n",M);}while(0)

static int hexv(int c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static int decode(const char*h,unsigned char*out,size_t n){if(strlen(h)!=n*2)return -1;for(size_t i=0;i<n;i++){int a=hexv(h[i*2]),b=hexv(h[i*2+1]);if(a<0||b<0)return -1;out[i]=(unsigned char)((a<<4)|b);}return 0;}
static void encode(const unsigned char*in,size_t n,char*out){static const char x[]="0123456789abcdef";for(size_t i=0;i<n;i++){out[i*2]=x[in[i]>>4];out[i*2+1]=x[in[i]&15];}out[n*2]=0;}
static void rev32(const unsigned char*in,unsigned char*out){for(int i=0;i<32;i++)out[i]=in[31-i];}
static void display_to_internal(const char*h,unsigned char*out){unsigned char b[32];decode(h,b,32);rev32(b,out);}
static uint32_t rd32le(const unsigned char*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void wr32le(unsigned char*p,uint32_t v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static int regtest_pow_ok(const unsigned char raw[80]){unsigned char a[32],b[32],r[32];SHA256(raw,80,a);SHA256(a,32,b);rev32(b,r);return r[0]<0x80;}
static void make_child(const char*prev,uint32_t timestamp,uint32_t bits,unsigned char raw[80]){
    memset(raw,0,80);wr32le(raw,1);display_to_internal(prev,raw+4);for(int i=0;i<32;i++)raw[36+i]=(unsigned char)(i+1);wr32le(raw+68,timestamp);wr32le(raw+72,bits);
    if(bits!=0x207fffffU){wr32le(raw+76,0);return;} for(uint32_t n=0;;n++){wr32le(raw+76,n);if(regtest_pow_ok(raw))break;if(n==UINT32_MAX)abort();}
}
static int stage(QrxDB*db,const char*net,const char*hex,int commit,QrxBtcSpvHeaderInfo*out,char*err,size_t esz){QrxDBBatch b;if(qrxdb_batch_begin(db,&b))return -99;int best=0;int rc=qrx_btc_spv_stage_header(db,&b,net,hex,out,&best,err,esz);if(rc||!commit){qrxdb_batch_abort(&b);return rc;}if(qrxdb_batch_commit(&b)){qrxdb_batch_abort(&b);return -98;}return 0;}

int main(void){
    uint32_t out=0;
    CHECK(qrx_btc_spv_retarget_bits(0x1d00ffffU,0x1d00ffffU,1209600,&out)==0&&out==0x1d00ffffU,"Bitcoin Core retarget vector: exact two weeks keeps difficulty");
    CHECK(qrx_btc_spv_retarget_bits(0x1c0ffff0U,0x1d00ffffU,302400,&out)==0&&out==0x1c03fffcU,"Bitcoin Core retarget vector: 1/4 clamp");
    CHECK(qrx_btc_spv_retarget_bits(0x1c0ffff0U,0x1d00ffffU,4838400,&out)==0&&out==0x1c3fffc0U,"Bitcoin Core retarget vector: 4x clamp");
    CHECK(qrx_btc_spv_retarget_bits(0x1d00ffffU,0x1d00ffffU,4838400,&out)==0&&out==0x1d00ffffU,"Bitcoin Core retarget vector: pow-limit cap");
    CHECK(qrx_btc_spv_funding_policy_valid(100,200,0,0,1)==1,"funding policy accepts first proof inside QRX deadline");
    CHECK(qrx_btc_spv_funding_policy_valid(201,200,0,0,1)==0,"funding policy rejects proof after QRX deadline");
    CHECK(qrx_btc_spv_funding_policy_valid(100,200,1,0,1)==0,"funding policy rejects already locked proof");
    CHECK(qrx_btc_spv_funding_policy_valid(100,200,0,1,1)==0,"funding policy rejects replacement funding txid");
    CHECK(qrx_btc_spv_funding_policy_valid(100,200,0,0,0)==0,"funding policy rejects terminal/non-awaiting session");

    char td[]="/tmp/qrx-spv-hardening-XXXXXX";
#ifndef _WIN32
    CHECK(mkdtemp(td)!=NULL,"create isolated SPV QRXDB");
#else
    snprintf(td,sizeof(td),"qrx-spv-hardening-test");
#endif
    QrxDB db;char err[512]={0};CHECK(qrxdb_init(&db,td)==0,"initialize QRXDB");
    CHECK(qrx_btc_spv_init(&db,"mainnet",err,sizeof(err))==0,"initialize embedded Bitcoin mainnet genesis");
    QrxBtcSpvHeaderInfo best;CHECK(qrx_btc_spv_get_best(&db,"mainnet",&best,err,sizeof(err))==0,"load Bitcoin mainnet genesis");
    CHECK(strcmp(best.hash,"000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f")==0,"Bitcoin Core chainparams genesis header/hash reference vector");
    qrxdb_close(&db);

    char tr[]="/tmp/qrx-spv-regtest-XXXXXX";
#ifndef _WIN32
    CHECK(mkdtemp(tr)!=NULL,"create isolated regtest SPV QRXDB");
#endif
    CHECK(qrxdb_init(&db,tr)==0,"initialize regtest QRXDB");
    CHECK(qrx_btc_spv_init(&db,"regtest",err,sizeof(err))==0,"initialize Bitcoin regtest genesis");
    CHECK(qrx_btc_spv_get_best(&db,"regtest",&best,err,sizeof(err))==0,"load regtest genesis");
    uint32_t gt=best.timestamp;

    unsigned char raw[80];char hx[161];make_child(best.hash,gt+1,0x207fffffU,raw);encode(raw,80,hx);
    QrxBtcSpvHeaderInfo child;memset(err,0,sizeof(err));CHECK(stage(&db,"regtest",hx,1,&child,err,sizeof(err))==0,"accept valid mined regtest child header");

    memset(err,0,sizeof(err));CHECK(stage(&db,"regtest","00",0,NULL,err,sizeof(err))!=0,"reject wrong-length header");
    char nonhex[161];memset(nonhex,'g',160);nonhex[160]=0;memset(err,0,sizeof(err));CHECK(stage(&db,"regtest",nonhex,0,NULL,err,sizeof(err))!=0,"reject non-hex header");

    make_child(child.hash,child.timestamp,0x207fffffU,raw);encode(raw,80,hx);memset(err,0,sizeof(err));CHECK(stage(&db,"regtest",hx,0,NULL,err,sizeof(err))!=0&&strstr(err,"median-time-past")!=NULL,"reject timestamp at/below MTP");
    make_child(child.hash,child.timestamp+1,0x207ffffeU,raw);encode(raw,80,hx);memset(err,0,sizeof(err));CHECK(stage(&db,"regtest",hx,0,NULL,err,sizeof(err))!=0&&strstr(err,"difficulty bits mismatch")!=NULL,"reject unexpected difficulty bits");
    make_child("1111111111111111111111111111111111111111111111111111111111111111",child.timestamp+2,0x207fffffU,raw);encode(raw,80,hx);memset(err,0,sizeof(err));CHECK(stage(&db,"regtest",hx,0,NULL,err,sizeof(err))!=0&&strstr(err,"previous Bitcoin header")!=NULL,"reject unknown-parent header");

    /* Deterministic malformed-input fuzz: valid length/hex plus arbitrary data must be rejected
       without crashes, state commits or unbounded work. */
    for(unsigned i=0;i<256;i++){
        for(size_t j=0;j<80;j++)raw[j]=(unsigned char)((i*131U+j*17U+0x5aU)&0xffU);
        encode(raw,80,hx);memset(err,0,sizeof(err));int rc=stage(&db,"regtest",hx,0,NULL,err,sizeof(err));
        if(rc==0){fprintf(stderr,"FAIL: malformed fuzz vector %u unexpectedly accepted\n",i);failures++;break;}
    }
    if(!failures)fprintf(stdout,"PASS: 256 deterministic malformed header fuzz vectors rejected without crash\n");
    qrxdb_close(&db);
#ifndef _WIN32
    char cmd[512];snprintf(cmd,sizeof(cmd),"rm -rf %s %s",td,tr);(void)system(cmd);
#endif
    if(failures){fprintf(stderr,"SPV hardening failures=%d\n",failures);return 1;}
    puts("PASS: Phase 7.1 Bitcoin SPV hardening vectors/retarget/malformed suite");return 0;
}
