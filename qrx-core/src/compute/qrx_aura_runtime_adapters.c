#include "compute/qrx_aura_runtime_adapters.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define QRX_DLEXT ".dll"
#else
#include <unistd.h>
#ifdef __APPLE__
#define QRX_DLEXT ".dylib"
#else
#define QRX_DLEXT ".so"
#endif
#endif

static int path_exists(const char *p){
    if(!p||!p[0]) return 0;
#ifdef _WIN32
    DWORD a=GetFileAttributesA(p);return a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_DIRECTORY);
#else
    return access(p,R_OK)==0;
#endif
}
const char *qrx_aura_runtime_adapter_name(QrxAuraRuntimeAdapterKind k){
    switch(k){case QRX_AURA_ADAPTER_AUTO:return "AUTO";case QRX_AURA_ADAPTER_LLAMA_CPP_CPU:return "LLAMA_CPP_CPU";case QRX_AURA_ADAPTER_LLAMA_CPP_CUDA:return "LLAMA_CPP_CUDA";case QRX_AURA_ADAPTER_MLX_METAL:return "MLX_METAL";case QRX_AURA_ADAPTER_EXTERNAL:return "EXTERNAL";case QRX_AURA_ADAPTER_LLAMA_CPP_METAL:return "LLAMA_CPP_METAL";default:return "UNKNOWN";}
}
QrxMoeRuntimeBackend qrx_aura_runtime_adapter_backend(QrxAuraRuntimeAdapterKind k){
    switch(k){case QRX_AURA_ADAPTER_LLAMA_CPP_CPU:return QRX_MOE_BACKEND_CPU;case QRX_AURA_ADAPTER_LLAMA_CPP_CUDA:return QRX_MOE_BACKEND_CUDA;case QRX_AURA_ADAPTER_MLX_METAL:return QRX_MOE_BACKEND_MLX_METAL;case QRX_AURA_ADAPTER_LLAMA_CPP_METAL:return QRX_MOE_BACKEND_OTHER;default:return QRX_MOE_BACKEND_OTHER;}
}
int qrx_aura_runtime_adapter_parse(const char*n,QrxAuraRuntimeAdapterKind*out){if(!n||!out)return-1;for(int k=0;k<=5;k++)if(!strcmp(n,qrx_aura_runtime_adapter_name((QrxAuraRuntimeAdapterKind)k))){*out=(QrxAuraRuntimeAdapterKind)k;return 0;}return-1;}
static QrxAuraRuntimeAdapterKind auto_kind(const QrxMoeRuntimeDevice*d){if(!d)return QRX_AURA_ADAPTER_LLAMA_CPP_CPU;if(d->backend==QRX_MOE_BACKEND_CUDA)return QRX_AURA_ADAPTER_LLAMA_CPP_CUDA;if((d->accelerator_features&QRX_MOE_ACCEL_FEAT_METAL)&&d->backend!=QRX_MOE_BACKEND_MLX_METAL)return QRX_AURA_ADAPTER_LLAMA_CPP_METAL;if(d->backend==QRX_MOE_BACKEND_MLX_METAL)return QRX_AURA_ADAPTER_MLX_METAL;return QRX_AURA_ADAPTER_LLAMA_CPP_CPU;}
static const char *env_for(QrxAuraRuntimeAdapterKind k){switch(k){case QRX_AURA_ADAPTER_LLAMA_CPP_CPU:return "QRX_AURA_LLAMA_CPP_CPU_PLUGIN";case QRX_AURA_ADAPTER_LLAMA_CPP_CUDA:return "QRX_AURA_LLAMA_CPP_CUDA_PLUGIN";case QRX_AURA_ADAPTER_MLX_METAL:return "QRX_AURA_MLX_METAL_PLUGIN";case QRX_AURA_ADAPTER_EXTERNAL:return "QRX_AURA_EXTERNAL_PLUGIN";case QRX_AURA_ADAPTER_LLAMA_CPP_METAL:return "QRX_AURA_LLAMA_CPP_METAL_PLUGIN";default:return NULL;}}
static const char *base_for(QrxAuraRuntimeAdapterKind k){switch(k){case QRX_AURA_ADAPTER_LLAMA_CPP_CPU:return "qrx-aura-llama-cpu";case QRX_AURA_ADAPTER_LLAMA_CPP_CUDA:return "qrx-aura-llama-cuda";case QRX_AURA_ADAPTER_MLX_METAL:return "qrx-aura-mlx-metal";case QRX_AURA_ADAPTER_EXTERNAL:return "qrx-aura-external";case QRX_AURA_ADAPTER_LLAMA_CPP_METAL:return "qrx-aura-llama-metal";default:return NULL;}}
int qrx_aura_runtime_adapter_resolve(const QrxAuraRuntimeAdapterConfig *c, const QrxMoeRuntimeDevice *d,
                                     char out[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX],
                                     QrxAuraRuntimeAdapterKind *rkind) {
    if (!c || !out || c->version != QRX_AURA_RUNTIME_ADAPTER_VERSION) return -1;
    QrxAuraRuntimeAdapterKind k = c->kind == QRX_AURA_ADAPTER_AUTO ? auto_kind(d) : c->kind;
    if (k < QRX_AURA_ADAPTER_LLAMA_CPP_CPU || k > QRX_AURA_ADAPTER_LLAMA_CPP_METAL) return -1;

    QrxMoeRuntimeBackend want = qrx_aura_runtime_adapter_backend(k);
    if (k != QRX_AURA_ADAPTER_EXTERNAL && d && d->backend != want) return -2;
    if (k == QRX_AURA_ADAPTER_LLAMA_CPP_METAL && d && !(d->accelerator_features & QRX_MOE_ACCEL_FEAT_METAL)) return -2;

    if (c->explicit_path[0]) {
        if (strlen(c->explicit_path) >= QRX_AURA_RUNTIME_ADAPTER_PATH_MAX) return -1;
        memcpy(out, c->explicit_path, strlen(c->explicit_path) + 1);
        if (rkind) *rkind = k;
        return path_exists(out) ? 0 : -3;
    }

    const char *ev = env_for(k);
    const char *ep = ev ? getenv(ev) : NULL;
    if (ep && ep[0]) {
        if (strlen(ep) >= QRX_AURA_RUNTIME_ADAPTER_PATH_MAX) return -1;
        memcpy(out, ep, strlen(ep) + 1);
        if (rkind) *rkind = k;
        return path_exists(out) ? 0 : -3;
    }

    const char *base = base_for(k);
    if (!base) return -1;
    int n = c->adapter_dir[0]
        ? snprintf(out, QRX_AURA_RUNTIME_ADAPTER_PATH_MAX, "%s/%s%s", c->adapter_dir, base, QRX_DLEXT)
        : snprintf(out, QRX_AURA_RUNTIME_ADAPTER_PATH_MAX, "%s%s", base, QRX_DLEXT);
    if (n < 0 || (size_t)n >= QRX_AURA_RUNTIME_ADAPTER_PATH_MAX) return -1;
    if (rkind) *rkind = k;
    return path_exists(out) ? 0 : -3;
}
int qrx_aura_runtime_adapter_open(const QrxAuraRuntimeAdapterConfig*c,const QrxMoeRuntimeDevice*d,QrxStorageFs*fs,QrxAuraRuntimePluginHost*out,char path[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX]){QrxAuraRuntimeAdapterKind k;if(qrx_aura_runtime_adapter_resolve(c,d,path,&k))return-1;if(qrx_aura_runtime_plugin_open(path,d,fs,out))return-2;QrxMoeRuntimeBackend want=qrx_aura_runtime_adapter_backend(k);if(k!=QRX_AURA_ADAPTER_EXTERNAL&&out->api&&out->api->backend!=want){qrx_aura_runtime_plugin_close(out);return-3;}return 0;}
