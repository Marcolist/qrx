#pragma once
#include <stddef.h>
#include "compute/qrx_aura_runtime_plugin.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_AURA_RUNTIME_ADAPTER_VERSION 1u
#define QRX_AURA_RUNTIME_ADAPTER_PATH_MAX 1024u

typedef enum {
    QRX_AURA_ADAPTER_AUTO=0,
    QRX_AURA_ADAPTER_LLAMA_CPP_CPU=1,
    QRX_AURA_ADAPTER_LLAMA_CPP_CUDA=2,
    QRX_AURA_ADAPTER_MLX_METAL=3,
    QRX_AURA_ADAPTER_EXTERNAL=4,
    /* llama.cpp using Apple Metal. Backend stays OTHER because Metal is an
       accelerator transport, not MLX; this avoids pretending GGUF is MLX. */
    QRX_AURA_ADAPTER_LLAMA_CPP_METAL=5
} QrxAuraRuntimeAdapterKind;

typedef struct {
    uint32_t version;
    QrxAuraRuntimeAdapterKind kind;
    char explicit_path[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX];
    char adapter_dir[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX];
} QrxAuraRuntimeAdapterConfig;

const char *qrx_aura_runtime_adapter_name(QrxAuraRuntimeAdapterKind kind);
QrxMoeRuntimeBackend qrx_aura_runtime_adapter_backend(QrxAuraRuntimeAdapterKind kind);
int qrx_aura_runtime_adapter_parse(const char *name,QrxAuraRuntimeAdapterKind *out);
int qrx_aura_runtime_adapter_resolve(const QrxAuraRuntimeAdapterConfig *cfg,const QrxMoeRuntimeDevice *device,
                                     char out_path[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX],QrxAuraRuntimeAdapterKind *resolved_kind);
int qrx_aura_runtime_adapter_open(const QrxAuraRuntimeAdapterConfig *cfg,const QrxMoeRuntimeDevice *device,
                                  QrxStorageFs *cache_fs,QrxAuraRuntimePluginHost *out,char resolved_path[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX]);
#ifdef __cplusplus
}
#endif
