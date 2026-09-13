#include "compute/qrx_aura_runtime_plugin.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define QRX_TEST_EXPORT __declspec(dllexport)
#else
#define QRX_TEST_EXPORT __attribute__((visibility("default")))
#endif

typedef struct { char model_id[QRX_MODEL_MAX_ID]; } TestModel;
static uint32_t be32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void put32(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static int probe(const QrxMoeRuntimeDevice*d){return (!d||d->backend!=QRX_MOE_BACKEND_CPU)?-1:0;}
static int load_model(const QrxAiModelRecord*m,QrxStorageFs*fs,void**out){(void)fs;if(!m||!out)return-1;TestModel*x=calloc(1,sizeof(*x));if(!x)return-1;strncpy(x->model_id,m->model_id,sizeof(x->model_id)-1);*out=x;return 0;}
static void unload_model(void*h){free(h);}
static int execute(void*h,const QrxAuraRemoteFrame*r,const void*input,size_t n,void*out,size_t cap,size_t*outn){
    (void)h;(void)r;if(!input||!out||!outn||n<96)return-1;const uint8_t*p=(const uint8_t*)input;
    if(memcmp(p,"QRXMF39\0",8)){
        if(n>cap)return-2;memcpy(out,input,n);*outn=n;return 0;
    }
    /* fragment envelope: magic8, version4, plan-string length4, plan64,
       strategy4, fragment_index4, fragment_count4 ... */
    if(be32(p+12)!=64||n<96)return-3;uint32_t idx=be32(p+84),cnt=be32(p+88);if(!cnt||idx>=cnt)return-3;
    const size_t need=8+4+64+20+12;if(cap<need)return-4;uint8_t*b=(uint8_t*)out;size_t o=0;
    memcpy(b+o,"QRXMC40\0",8);o+=8;put32(b+o,1);o+=4;memcpy(b+o,p+16,64);o+=64;
    put32(b+o,idx);o+=4;put32(b+o,cnt);o+=4;put32(b+o,0);o+=4;put32(b+o,3);o+=4;put32(b+o,8);o+=4;
    int32_t vals0[3]={100,200,300},vals1[3]={10,20,30};const int32_t*v=idx?vals1:vals0;
    for(int i=0;i<3;i++){put32(b+o,(uint32_t)v[i]);o+=4;}*outn=o;return 0;
}
static const QrxAuraRuntimePluginApiV1 API={
    QRX_AURA_RUNTIME_PLUGIN_ABI_VERSION,
    "QRX Phase165 Test Runtime",
    "qrx-test-runtime-v1",
    QRX_MOE_BACKEND_CPU,
    QRX_MOE_KERNEL_FP32,
    QRX_AURA_RUNTIME_PLUGIN_FLAG_MODEL_CACHE_REQUIRED|QRX_AURA_RUNTIME_PLUGIN_FLAG_MOE_FRAGMENT_INPUT,
    probe,load_model,execute,unload_model
};
QRX_TEST_EXPORT const QrxAuraRuntimePluginApiV1*qrx_aura_runtime_plugin_v1(void){return &API;}
