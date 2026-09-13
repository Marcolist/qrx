#pragma once
#include <stddef.h>
#include <stdint.h>
#include <openssl/evp.h>
#include "compute/qrx_compute.h"
#include "compute/qrx_aura_provider_host.h"
#include "compute/qrx_aura_model_distribution.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QRX_AURA_RUNTIME_PACKAGE_VERSION 1u
#define QRX_AURA_RUNTIME_PACKAGE_MAX 128u
#define QRX_AURA_RUNTIME_PACKAGE_SIGNATURE_MAX 1024u
#define QRX_AURA_RUNTIME_PACKAGE_ID_MAX 128u
#define QRX_AURA_RUNTIME_PACKAGE_VERSION_MAX 64u
#define QRX_AURA_RUNTIME_PACKAGE_URI_MAX 512u
#define QRX_AURA_RUNTIME_PACKAGE_FILE_MAX 128u

typedef enum {
    QRX_AURA_PACKAGE_PLATFORM_ANY=0,
    QRX_AURA_PACKAGE_PLATFORM_LINUX=1,
    QRX_AURA_PACKAGE_PLATFORM_MACOS=2,
    QRX_AURA_PACKAGE_PLATFORM_WINDOWS=3
} QrxAuraPackagePlatform;

typedef enum {
    QRX_AURA_PACKAGE_ARCH_ANY=0,
    QRX_AURA_PACKAGE_ARCH_X86_64=1,
    QRX_AURA_PACKAGE_ARCH_AARCH64=2,
    QRX_AURA_PACKAGE_ARCH_ARM=3
} QrxAuraPackageArch;

typedef struct {
    uint32_t version;
    char publisher_id[129];
    char package_id[QRX_AURA_RUNTIME_PACKAGE_ID_MAX+1];
    char package_version[QRX_AURA_RUNTIME_PACKAGE_VERSION_MAX+1];
    uint64_t sequence;
    uint64_t valid_from_height;
    uint64_t valid_until_height;
    QrxAuraRuntimeAdapterKind adapter_kind;
    QrxAuraPackagePlatform platform;
    QrxAuraPackageArch arch;
    QrxMoeRuntimeBackend backend;
    uint32_t required_accelerator_features;
    uint64_t min_device_memory_bytes;
    uint32_t min_cuda_cc_major;
    uint32_t min_cuda_cc_minor;
    char content_root[65]; /* raw SHA3-256 of package file */
    char download_uri[QRX_AURA_RUNTIME_PACKAGE_URI_MAX];
    char install_filename[QRX_AURA_RUNTIME_PACKAGE_FILE_MAX];
} QrxAuraRuntimePackageAnnouncement;

typedef struct {
    QrxAuraRuntimePackageAnnouncement announcement;
    uint8_t *signature;
    size_t signature_len;
} QrxAuraRuntimePackageEntry;

typedef struct {
    uint32_t version;
    QrxAuraRuntimePackageEntry entries[QRX_AURA_RUNTIME_PACKAGE_MAX];
    uint32_t count;
    uint64_t revision;
} QrxAuraRuntimePackageCatalog;

typedef int (*QrxAuraRuntimePackageKeyLookupFn)(void *ctx,const char *publisher_id,EVP_PKEY **out_public_key);

typedef struct {
    uint32_t version;
    uint32_t device_index;
    QrxMoeRuntimeDevice device;
    QrxAuraRuntimePackageAnnouncement package;
    uint32_t score;
} QrxAuraRuntimeActivationPlan;

typedef int (*QrxAuraRuntimeActivateFn)(void *ctx,const QrxAuraProviderHostConfig *host,
                                       const QrxMoeRuntimeDevice *device,
                                       const QrxAuraRuntimePackageAnnouncement *package,
                                       const char *installed_plugin_path);

typedef int (*QrxAuraRuntimePackageFetchFn)(void *ctx,
                                           const QrxAuraRuntimePackageAnnouncement *package,
                                           char out_downloaded_path[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX]);

typedef struct {
    uint32_t version;
    QrxMoeRuntimeInventory inventory;
    QrxAuraRuntimeActivationPlan plan;
    char downloaded_path[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX];
    char installed_path[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX];
    uint8_t host_config_persisted;
} QrxAuraRuntimeOneClickResult;

QrxAuraPackagePlatform qrx_aura_runtime_package_current_platform(void);
QrxAuraPackageArch qrx_aura_runtime_package_current_arch(void);
int qrx_aura_runtime_package_hash(const QrxAuraRuntimePackageAnnouncement *announcement,uint8_t out_hash[32]);
int qrx_aura_runtime_package_sign(EVP_PKEY *private_key,const QrxAuraRuntimePackageAnnouncement *announcement,uint8_t **signature,size_t *signature_len);
int qrx_aura_runtime_package_verify(EVP_PKEY *public_key,const QrxAuraRuntimePackageAnnouncement *announcement,const uint8_t *signature,size_t signature_len);
void qrx_aura_runtime_package_catalog_init(QrxAuraRuntimePackageCatalog *catalog);
void qrx_aura_runtime_package_catalog_free(QrxAuraRuntimePackageCatalog *catalog);
int qrx_aura_runtime_package_catalog_ingest(QrxAuraRuntimePackageCatalog *catalog,const QrxAuraRuntimePackageAnnouncement *announcement,
                                            const uint8_t *signature,size_t signature_len,uint64_t height,
                                            QrxAuraRuntimePackageKeyLookupFn key_lookup,void *key_ctx);
size_t qrx_aura_runtime_package_catalog_prune(QrxAuraRuntimePackageCatalog *catalog,uint64_t height);
int qrx_aura_runtime_package_catalog_save_signed_file(const char *path,const QrxAuraRuntimePackageCatalog *catalog);
int qrx_aura_runtime_package_catalog_load_signed_file(const char *path,uint64_t height,
                                                      QrxAuraRuntimePackageKeyLookupFn key_lookup,void *key_ctx,
                                                      QrxAuraRuntimePackageCatalog *out_catalog);
int qrx_aura_runtime_package_select(const QrxAuraRuntimePackageCatalog *catalog,const QrxMoeRuntimeInventory *inventory,
                                    uint64_t height,QrxAuraPowerProfile power,QrxAuraRuntimeActivationPlan *out_plan);
int qrx_aura_runtime_package_select_for(const QrxAuraRuntimePackageCatalog *catalog,const QrxMoeRuntimeInventory *inventory,
                                        uint64_t height,QrxAuraPowerProfile power,QrxAuraPackagePlatform platform,
                                        QrxAuraPackageArch arch,QrxAuraRuntimeActivationPlan *out_plan);
int qrx_aura_runtime_package_file_root(const char *path,char out_hex[65]);
int qrx_aura_runtime_package_install_verified(const char *downloaded_path,const char *target_path,const char expected_root[65]);
int qrx_aura_runtime_package_activate_verified(QrxAuraProviderHostConfig *host,const QrxAuraRuntimeActivationPlan *plan,
                                               const char *downloaded_path,const char *target_path,
                                               QrxAuraRuntimeActivateFn activate,void *activate_ctx);
int qrx_aura_runtime_one_click_activate(QrxAuraProviderHostConfig *host,
                                        const QrxAuraRuntimePackageCatalog *catalog,
                                        uint64_t height,QrxAuraPowerProfile power,
                                        const char *install_dir,const char *host_config_path,
                                        QrxAuraRuntimePackageFetchFn fetch,void *fetch_ctx,
                                        QrxAuraRuntimeActivateFn activate,void *activate_ctx,
                                        QrxAuraRuntimeOneClickResult *out);

#ifdef __cplusplus
}
#endif
