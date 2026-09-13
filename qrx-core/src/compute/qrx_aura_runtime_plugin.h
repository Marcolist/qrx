#pragma once
#include <stddef.h>
#include <stdint.h>
#include "compute/qrx_aura_remote_dispatch.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_AURA_RUNTIME_PLUGIN_ABI_VERSION 1u
#define QRX_AURA_RUNTIME_PLUGIN_ENTRY "qrx_aura_runtime_plugin_v1"
#define QRX_AURA_RUNTIME_PLUGIN_NAME_MAX 96u
#define QRX_AURA_RUNTIME_PLUGIN_MAX_MODELS 16u
#define QRX_AURA_RUNTIME_PLUGIN_FLAG_MODEL_CACHE_REQUIRED (1u<<0)
#define QRX_AURA_RUNTIME_PLUGIN_FLAG_MOE_FRAGMENT_INPUT     (1u<<1)
#define QRX_AURA_RUNTIME_PLUGIN_FLAG_STREAMING              (1u<<2)

/* Stable C ABI for production runtime adapters. A plugin may wrap llama.cpp,
 * MLX/Metal, CUDA-native engines or another inference runtime. QRX validates
 * the model registry/cache identity before calling into the plugin. */
typedef struct {
    uint32_t abi_version;
    char plugin_name[QRX_AURA_RUNTIME_PLUGIN_NAME_MAX];
    char runtime_id[QRX_MODEL_MAX_RUNTIME];
    QrxMoeRuntimeBackend backend;
    uint32_t kernel_mask;
    uint32_t flags;
    int (*probe)(const QrxMoeRuntimeDevice *device);
    int (*load_model)(const QrxAiModelRecord *model,QrxStorageFs *cache_fs,void **model_handle_out);
    int (*execute)(void *model_handle,const QrxAuraRemoteFrame *request,
                   const void *input,size_t input_bytes,
                   void *output,size_t output_capacity,size_t *output_bytes);
    void (*unload_model)(void *model_handle);
} QrxAuraRuntimePluginApiV1;

typedef const QrxAuraRuntimePluginApiV1 *(*QrxAuraRuntimePluginEntryFn)(void);

typedef struct {
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char model_commitment[65];
    void *model_handle;
} QrxAuraRuntimePluginModelSlot;

typedef struct {
    void *library_handle;
    const QrxAuraRuntimePluginApiV1 *api;
    QrxStorageFs *cache_fs; /* borrowed */
    QrxMoeRuntimeDevice device;
    QrxAuraRuntimePluginModelSlot models[QRX_AURA_RUNTIME_PLUGIN_MAX_MODELS];
    uint32_t model_count;
} QrxAuraRuntimePluginHost;

int qrx_aura_runtime_plugin_open(const char *path,const QrxMoeRuntimeDevice *device,QrxStorageFs *cache_fs,QrxAuraRuntimePluginHost *out);
void qrx_aura_runtime_plugin_close(QrxAuraRuntimePluginHost *host);
int qrx_aura_runtime_plugin_load_model(QrxAuraRuntimePluginHost *host,const QrxAiModelRecord *model,uint32_t *slot_out);
int qrx_aura_runtime_plugin_execute(QrxAuraRuntimePluginHost *host,const QrxAiModelRecord *model,const QrxAuraRemoteFrame *request,
                                    const void *input,size_t input_bytes,void *output,size_t output_capacity,size_t *output_bytes);
int qrx_aura_runtime_plugin_model_execute_adapter(void *ctx,const QrxAiModelRecord *model,const QrxAuraRemoteFrame *request,
                                                  const void *input,size_t input_bytes,void *output,size_t output_capacity,size_t *output_bytes);
const char *qrx_aura_runtime_plugin_name(const QrxAuraRuntimePluginHost *host);
const char *qrx_aura_runtime_plugin_runtime_id(const QrxAuraRuntimePluginHost *host);

#ifdef __cplusplus
}
#endif
