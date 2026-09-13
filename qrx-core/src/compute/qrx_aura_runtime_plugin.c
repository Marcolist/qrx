#include "compute/qrx_aura_runtime_plugin.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static int nz(const char *s,size_t n){ return s&&memchr(s,'\0',n)&&s[0]; }
static int api_valid(const QrxAuraRuntimePluginApiV1 *a){
    if(!a||a->abi_version!=QRX_AURA_RUNTIME_PLUGIN_ABI_VERSION||
       !nz(a->plugin_name,sizeof(a->plugin_name))||!nz(a->runtime_id,sizeof(a->runtime_id))) return 0;
    if(a->backend<QRX_MOE_BACKEND_CPU||a->backend>QRX_MOE_BACKEND_OTHER||!a->kernel_mask||
       !a->load_model||!a->execute||!a->unload_model) return 0;
    return 1;
}
#ifdef _WIN32
static void *lib_open(const char *p){ return (void*)LoadLibraryA(p); }
static void *lib_sym(void *h,const char *s){ return h?(void*)GetProcAddress((HMODULE)h,s):NULL; }
static void lib_close(void *h){ if(h) FreeLibrary((HMODULE)h); }
#else
static void *lib_open(const char *p){ return dlopen(p,RTLD_NOW|RTLD_LOCAL); }
static void *lib_sym(void *h,const char *s){ return h?dlsym(h,s):NULL; }
static void lib_close(void *h){ if(h) dlclose(h); }
#endif

int qrx_aura_runtime_plugin_open(const char *path,const QrxMoeRuntimeDevice *d,QrxStorageFs *cache,QrxAuraRuntimePluginHost *out){
    if(!path||!path[0]||!d||!out||qrx_moe_runtime_device_validate(d)) return -1;
    memset(out,0,sizeof(*out));
    void *h=lib_open(path);
    if(!h) return -2;
    void *sym=lib_sym(h,QRX_AURA_RUNTIME_PLUGIN_ENTRY);
    if(!sym){ lib_close(h); return -3; }
    QrxAuraRuntimePluginEntryFn entry=NULL;
    /* POSIX specifies dlsym as a generic symbol address; memcpy avoids a
       non-portable object-pointer/function-pointer cast diagnostic. */
    if(sizeof(entry)!=sizeof(sym)){ lib_close(h); return -3; }
    memcpy(&entry,&sym,sizeof(entry));
    const QrxAuraRuntimePluginApiV1 *a=entry();
    if(!api_valid(a)||a->backend!=d->backend||(a->probe&&a->probe(d))){ lib_close(h); return -4; }
    out->library_handle=h;out->api=a;out->cache_fs=cache;out->device=*d;
    return 0;
}

void qrx_aura_runtime_plugin_close(QrxAuraRuntimePluginHost *h){
    if(!h) return;
    if(h->api&&h->api->unload_model){
        for(uint32_t i=0;i<h->model_count;i++) if(h->models[i].model_handle) h->api->unload_model(h->models[i].model_handle);
    }
    lib_close(h->library_handle);
    memset(h,0,sizeof(*h));
}

static int model_slot(const QrxAuraRuntimePluginHost *h,const char *commit){
    if(!h||!commit) return -1;
    for(uint32_t i=0;i<h->model_count;i++) if(!strcmp(h->models[i].model_commitment,commit)) return (int)i;
    return -1;
}

int qrx_aura_runtime_plugin_load_model(QrxAuraRuntimePluginHost *h,const QrxAiModelRecord *m,uint32_t *out_slot){
    if(!h||!h->api||!m||qrx_ai_model_validate(m)||strcmp(m->runtime_id,h->api->runtime_id)||
       m->min_memory_bytes>h->device.device_memory_bytes) return -1;
    char mc[65];
    if(qrx_ai_model_commitment(m,mc)) return -1;
    int idx=model_slot(h,mc);
    if(idx>=0){ if(out_slot)*out_slot=(uint32_t)idx; return 0; }
    if(h->model_count>=QRX_AURA_RUNTIME_PLUGIN_MAX_MODELS) return -2;
    if((h->api->flags&QRX_AURA_RUNTIME_PLUGIN_FLAG_MODEL_CACHE_REQUIRED)&&
       (!h->cache_fs||qrx_storage_fs_has(h->cache_fs,m->manifest_root)!=1)) return -3;
    void *mh=NULL;
    if(h->api->load_model(m,h->cache_fs,&mh)||!mh) return -4;
    QrxAuraRuntimePluginModelSlot *slot=&h->models[h->model_count];
    memset(slot,0,sizeof(*slot));
    snprintf(slot->model_id,sizeof(slot->model_id),"%s",m->model_id);
    snprintf(slot->model_version,sizeof(slot->model_version),"%s",m->model_version);
    snprintf(slot->model_commitment,sizeof(slot->model_commitment),"%s",mc);
    slot->model_handle=mh;
    if(out_slot) *out_slot=h->model_count;
    h->model_count++;
    return 0;
}

int qrx_aura_runtime_plugin_execute(QrxAuraRuntimePluginHost *h,const QrxAiModelRecord *m,const QrxAuraRemoteFrame *r,
                                    const void *in,size_t in_n,void *out,size_t cap,size_t *out_n){
    if(!h||!m||!r||(in_n&&!in)||!out_n) return -1;
    uint32_t slot=0;
    if(qrx_aura_runtime_plugin_load_model(h,m,&slot)) return -2;
    if(slot>=h->model_count||!h->models[slot].model_handle) return -2;
    return h->api->execute(h->models[slot].model_handle,r,in,in_n,out,cap,out_n);
}
int qrx_aura_runtime_plugin_model_execute_adapter(void *ctx,const QrxAiModelRecord *m,const QrxAuraRemoteFrame *r,
                                                  const void *in,size_t in_n,void *out,size_t cap,size_t *out_n){
    return qrx_aura_runtime_plugin_execute((QrxAuraRuntimePluginHost*)ctx,m,r,in,in_n,out,cap,out_n);
}
const char *qrx_aura_runtime_plugin_name(const QrxAuraRuntimePluginHost *h){ return h&&h->api?h->api->plugin_name:NULL; }
const char *qrx_aura_runtime_plugin_runtime_id(const QrxAuraRuntimePluginHost *h){ return h&&h->api?h->api->runtime_id:NULL; }
