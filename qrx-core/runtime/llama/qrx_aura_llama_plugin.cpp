#include "compute/qrx_aura_runtime_plugin.h"
#include "compute/qrx_aura_remote_dispatch.h"
#include "llama.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#define QRX_EXPORT __declspec(dllexport)
#define QRX_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define QRX_EXPORT __attribute__((visibility("default")))
#define QRX_MKDIR(p) mkdir((p),0700)
#endif

#ifndef QRX_LLAMA_PLUGIN_KIND
#define QRX_LLAMA_PLUGIN_KIND 0
#endif

struct QrxLlamaModel {
    llama_model *model = nullptr;
    const llama_vocab *vocab = nullptr;
    std::string path;
};
static std::once_flag g_backend_once;
static void backend_init_once(){ llama_backend_init(); }

static const char *cache_dir(){
    const char *p=std::getenv("QRX_AURA_MODEL_MATERIALIZE_DIR");
    if(p&&*p)return p;
#ifdef _WIN32
    p=std::getenv("TEMP"); return (p&&*p)?p:".";
#else
    p=std::getenv("TMPDIR"); return (p&&*p)?p:"/tmp";
#endif
}
static int ensure_dir(const char *p){ if(!p||!*p)return -1; if(QRX_MKDIR(p)==0||errno==EEXIST)return 0; return -1; }
static int materialize_gguf(const QrxAiModelRecord *m,QrxStorageFs *fs,std::string &path){
    if(!m||!fs||qrx_storage_fs_has(fs,m->manifest_root)!=1)return -1;
    unsigned char *raw=nullptr;size_t raw_n=0;if(qrx_storage_fs_read(fs,m->manifest_root,&raw,&raw_n))return -1;
    QrxAuraModelBundleManifest b{};int rc=qrx_aura_model_bundle_decode(raw,raw_n,&b);free(raw);if(rc||strcmp(b.model_id,m->model_id)||strcmp(b.model_version,m->model_version))return -2;
    const QrxAuraModelBundleObject *w=nullptr;for(uint32_t i=0;i<b.object_count;i++)if(b.objects[i].kind==QRX_AURA_MODEL_OBJECT_WEIGHTS){if(w)return -3;w=&b.objects[i];}
    if(!w||qrx_storage_fs_has(fs,w->object_root)!=1)return -4;const char *d=cache_dir();ensure_dir(d);char p[1400];
#ifdef _WIN32
    int n=snprintf(p,sizeof(p),"%s\\qrx-aura-%s.gguf",d,w->object_root);
#else
    int n=snprintf(p,sizeof(p),"%s/qrx-aura-%s.gguf",d,w->object_root);
#endif
    if(n<0||(size_t)n>=sizeof(p))return -5;FILE *f=fopen(p,"rb");if(f)fclose(f);else if(qrx_storage_fs_get_file(fs,w->object_root,p))return -6;path=p;return 0;
}
static int probe(const QrxMoeRuntimeDevice *d){
    if(!d)return -1;
#if QRX_LLAMA_PLUGIN_KIND == 1
    return d->backend==QRX_MOE_BACKEND_CUDA && (d->accelerator_features&QRX_MOE_ACCEL_FEAT_CUDA)?0:-1;
#elif QRX_LLAMA_PLUGIN_KIND == 2
    return (d->accelerator_features&QRX_MOE_ACCEL_FEAT_METAL)?0:-1;
#else
    return d->backend==QRX_MOE_BACKEND_CPU?0:-1;
#endif
}
static int load_model(const QrxAiModelRecord *m,QrxStorageFs *fs,void **out){
    if(!m||!fs||!out||strcmp(m->runtime_id,"llama.cpp"))return -1;std::call_once(g_backend_once,backend_init_once);auto *h=new(std::nothrow) QrxLlamaModel();if(!h)return -2;
    if(materialize_gguf(m,fs,h->path)){delete h;return -3;}llama_model_params mp=llama_model_default_params();
#if QRX_LLAMA_PLUGIN_KIND == 1 || QRX_LLAMA_PLUGIN_KIND == 2
    mp.n_gpu_layers=999;
#else
    mp.n_gpu_layers=0;
#endif
    h->model=llama_model_load_from_file(h->path.c_str(),mp);if(!h->model){delete h;return -4;}h->vocab=llama_model_get_vocab(h->model);if(!h->vocab){llama_model_free(h->model);delete h;return -4;}*out=h;return 0;
}
static void unload_model(void *v){auto *h=(QrxLlamaModel*)v;if(!h)return;if(h->model)llama_model_free(h->model);delete h;}
static int append_piece(const llama_vocab *v,llama_token tok,void *out,size_t cap,size_t &used){char small[256];int n=llama_token_to_piece(v,tok,small,sizeof(small),0,true);if(n>=0){if(used+(size_t)n>cap)return -1;memcpy((unsigned char*)out+used,small,(size_t)n);used+=(size_t)n;return 0;}size_t need=(size_t)(-n);std::vector<char>b(need);n=llama_token_to_piece(v,tok,b.data(),(int)b.size(),0,true);if(n<0||used+(size_t)n>cap)return -1;memcpy((unsigned char*)out+used,b.data(),(size_t)n);used+=(size_t)n;return 0;}
static int execute(void *v,const QrxAuraRemoteFrame *r,const void *input,size_t input_n,void *out,size_t cap,size_t *out_n){
    (void)r;if(!v||(!input&&input_n)||!out||!out_n)return -1;if(input_n>=8&&!memcmp(input,"QRXMF39\0",8))return -20;auto *h=(QrxLlamaModel*)v;std::string prompt((const char*)input,input_n);int nt=-llama_tokenize(h->vocab,prompt.c_str(),prompt.size(),nullptr,0,true,true);if(nt<=0)return -2;std::vector<llama_token> toks((size_t)nt);if(llama_tokenize(h->vocab,prompt.c_str(),prompt.size(),toks.data(),(int)toks.size(),true,true)<0)return -2;
    llama_context_params cp=llama_context_default_params();cp.n_ctx=(uint32_t)std::max(2048,nt+512);cp.n_batch=(uint32_t)std::min(512,std::max(32,nt));cp.no_perf=false;llama_context *ctx=llama_init_from_model(h->model,cp);if(!ctx)return -3;auto sp=llama_sampler_chain_default_params();sp.no_perf=false;llama_sampler *smpl=llama_sampler_chain_init(sp);if(!smpl){llama_free(ctx);return -3;}llama_sampler_chain_add(smpl,llama_sampler_init_greedy());llama_batch batch=llama_batch_get_one(toks.data(),(int)toks.size());size_t used=0;int rc=0;for(int pos=0,generated=0;generated<512;){if(llama_decode(ctx,batch)){rc=-4;break;}pos+=batch.n_tokens;llama_token t=llama_sampler_sample(smpl,ctx,-1);if(llama_vocab_is_eog(h->vocab,t))break;if(append_piece(h->vocab,t,out,cap,used)){rc=-5;break;}batch=llama_batch_get_one(&t,1);generated++;}llama_sampler_free(smpl);llama_free(ctx);*out_n=used;return rc;
}

#if QRX_LLAMA_PLUGIN_KIND == 1
#define QRX_PLUGIN_NAME "QRX AURA llama.cpp CUDA"
#define QRX_PLUGIN_BACKEND QRX_MOE_BACKEND_CUDA
#define QRX_PLUGIN_KERNELS (QRX_MOE_KERNEL_FP32|QRX_MOE_KERNEL_FP16|QRX_MOE_KERNEL_INT8|QRX_MOE_KERNEL_TENSORCORE)
#elif QRX_LLAMA_PLUGIN_KIND == 2
#define QRX_PLUGIN_NAME "QRX AURA llama.cpp Metal"
#define QRX_PLUGIN_BACKEND QRX_MOE_BACKEND_OTHER
#define QRX_PLUGIN_KERNELS (QRX_MOE_KERNEL_FP32|QRX_MOE_KERNEL_FP16|QRX_MOE_KERNEL_INT8|QRX_MOE_KERNEL_METAL)
#else
#define QRX_PLUGIN_NAME "QRX AURA llama.cpp CPU"
#define QRX_PLUGIN_BACKEND QRX_MOE_BACKEND_CPU
#define QRX_PLUGIN_KERNELS (QRX_MOE_KERNEL_FP32|QRX_MOE_KERNEL_FP16|QRX_MOE_KERNEL_INT8)
#endif
static const QrxAuraRuntimePluginApiV1 API={QRX_AURA_RUNTIME_PLUGIN_ABI_VERSION,QRX_PLUGIN_NAME,"llama.cpp",QRX_PLUGIN_BACKEND,QRX_PLUGIN_KERNELS,QRX_AURA_RUNTIME_PLUGIN_FLAG_MODEL_CACHE_REQUIRED,probe,load_model,execute,unload_model};
extern "C" QRX_EXPORT const QrxAuraRuntimePluginApiV1 *qrx_aura_runtime_plugin_v1(void){return &API;}
