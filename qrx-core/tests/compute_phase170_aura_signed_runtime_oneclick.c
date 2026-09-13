#include "compute/qrx_aura_runtime_packages.h"
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)

typedef struct { const char *id; EVP_PKEY *key; } KeyCtx;
static int lookup(void *ctx,const char *id,EVP_PKEY **out){KeyCtx*k=(KeyCtx*)ctx;if(strcmp(id,k->id))return -1;if(EVP_PKEY_up_ref(k->key)!=1)return -1;*out=k->key;return 0;}

typedef struct { char source[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX]; int calls; } FetchCtx;
static int fetch_runtime(void *ctx,const QrxAuraRuntimePackageAnnouncement *p,char out[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX]){FetchCtx*f=(FetchCtx*)ctx;f->calls++;if(!p||strncmp(p->download_uri,"qrxdrive://",11))return -1;snprintf(out,QRX_AURA_RUNTIME_ADAPTER_PATH_MAX,"%s",f->source);return 0;}

typedef struct { int calls; QrxMoeRuntimeBackend backend; char path[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX]; } ActivateCtx;
static int activate_runtime(void *ctx,const QrxAuraProviderHostConfig *host,const QrxMoeRuntimeDevice *dev,const QrxAuraRuntimePackageAnnouncement *pkg,const char *path){ActivateCtx*a=(ActivateCtx*)ctx;a->calls++;a->backend=dev->backend;snprintf(a->path,sizeof(a->path),"%s",path);if(host->runtime_adapter.kind!=pkg->adapter_kind)return -1;if(strcmp(host->runtime_adapter.explicit_path,path))return -1;return 0;}

static int write_blob(const char *path){FILE*f=fopen(path,"wb");if(!f)return -1;const char payload[]="QRX AURA signed runtime package fixture v1\n";int ok=fwrite(payload,1,sizeof(payload)-1,f)==sizeof(payload)-1;return fclose(f)||!ok?-1:0;}

int main(void){
    EVP_PKEY *publisher=EVP_PKEY_Q_keygen(NULL,NULL,"ED25519");CHECK(publisher);KeyCtx keys={"qrx:runtime:official",publisher};
    char src[256],dst[256],cfgpath[256];long pid=(long)getpid();snprintf(src,sizeof(src),"phase170-runtime-%ld.pkg",pid);snprintf(dst,sizeof(dst),"phase170-runtime-%ld.plugin",pid);snprintf(cfgpath,sizeof(cfgpath),"phase170-host-%ld.conf",pid);remove(src);remove(dst);remove(cfgpath);CHECK(write_blob(src)==0);
    char root[65];CHECK(qrx_aura_runtime_package_file_root(src,root)==0&&strlen(root)==64);

    QrxAuraRuntimePackageAnnouncement p;memset(&p,0,sizeof(p));p.version=1;snprintf(p.publisher_id,sizeof(p.publisher_id),"%s",keys.id);snprintf(p.package_id,sizeof(p.package_id),"qrx-aura-llama-cpu-official");snprintf(p.package_version,sizeof(p.package_version),"2026.09.45");p.sequence=1;p.valid_from_height=100;p.valid_until_height=1000;p.adapter_kind=QRX_AURA_ADAPTER_LLAMA_CPP_CPU;p.platform=QRX_AURA_PACKAGE_PLATFORM_ANY;p.arch=QRX_AURA_PACKAGE_ARCH_ANY;p.backend=QRX_MOE_BACKEND_CPU;p.required_accelerator_features=QRX_MOE_ACCEL_FEAT_FP32;snprintf(p.content_root,sizeof(p.content_root),"%s",root);snprintf(p.download_uri,sizeof(p.download_uri),"qrxdrive://runtime/%s/%s",p.package_id,root);snprintf(p.install_filename,sizeof(p.install_filename),"%s",dst);
    uint8_t *sig=NULL;size_t sn=0;CHECK(qrx_aura_runtime_package_sign(publisher,&p,&sig,&sn)==0&&sig&&sn);
    QrxAuraRuntimePackageCatalog cat;qrx_aura_runtime_package_catalog_init(&cat);CHECK(qrx_aura_runtime_package_catalog_ingest(&cat,&p,sig,sn,100,lookup,&keys)==0);CHECK(qrx_aura_runtime_package_catalog_ingest(&cat,&p,sig,sn,100,lookup,&keys)==-3);free(sig);

    /* A higher-sequence tampered entry must fail verification rather than roll the catalog forward. */
    QrxAuraRuntimePackageAnnouncement bad=p;bad.sequence=2;bad.content_root[0]=bad.content_root[0]=='a'?'b':'a';CHECK(qrx_aura_runtime_package_sign(publisher,&p,&sig,&sn)==0);CHECK(qrx_aura_runtime_package_catalog_ingest(&cat,&bad,sig,sn,100,lookup,&keys)==-2);free(sig);

    QrxMoeRuntimeInventory inv;CHECK(qrx_moe_runtime_discover_native(&inv)==0);CHECK(inv.device_count>=1&&inv.devices[0].device.backend==QRX_MOE_BACKEND_CPU);
    QrxAuraRuntimeActivationPlan plan;CHECK(qrx_aura_runtime_package_select(&cat,&inv,100,QRX_AURA_POWER_BALANCED,&plan)==0);CHECK(plan.package.adapter_kind==QRX_AURA_ADAPTER_LLAMA_CPP_CPU&&plan.device.backend==QRX_MOE_BACKEND_CPU);

    QrxAuraProviderHostConfig host;qrx_aura_provider_host_config_defaults(&host);host.enabled=1;snprintf(host.provider_id,sizeof(host.provider_id),"provider-phase170");snprintf(host.pod_id,sizeof(host.pod_id),"pod-phase170");snprintf(host.network,sizeof(host.network),"alpha");snprintf(host.region,sizeof(host.region),"DE");snprintf(host.lease_journal_path,sizeof(host.lease_journal_path),"phase170.leases");snprintf(host.job_journal_path,sizeof(host.job_journal_path),"phase170.jobs");
    FetchCtx fc={{0},0};snprintf(fc.source,sizeof(fc.source),"%s",src);ActivateCtx ac={0};QrxAuraRuntimeOneClickResult result;
    CHECK(qrx_aura_runtime_one_click_activate(&host,&cat,100,QRX_AURA_POWER_BALANCED,".",cfgpath,fetch_runtime,&fc,activate_runtime,&ac,&result)==0);
    CHECK(fc.calls==1&&ac.calls==1&&result.host_config_persisted==1);CHECK(host.runtime_adapter.kind==QRX_AURA_ADAPTER_LLAMA_CPP_CPU);CHECK(strstr(host.runtime_adapter.explicit_path,dst)!=NULL);CHECK(ac.backend==QRX_MOE_BACKEND_CPU);
    char installed_root[65];CHECK(qrx_aura_runtime_package_file_root(host.runtime_adapter.explicit_path,installed_root)==0&&!strcmp(installed_root,root));
    QrxAuraProviderHostConfig loaded;CHECK(qrx_aura_provider_host_config_load(cfgpath,&loaded)==0);CHECK(loaded.runtime_adapter.kind==host.runtime_adapter.kind&&!strcmp(loaded.runtime_adapter.explicit_path,host.runtime_adapter.explicit_path));

    /* Corrupt source cannot replace a verified runtime. */
    FILE*f=fopen(src,"ab");CHECK(f);fputs("tamper",f);fclose(f);QrxAuraProviderHostConfig host2=host;host2.runtime_adapter.kind=QRX_AURA_ADAPTER_AUTO;host2.runtime_adapter.explicit_path[0]=0;QrxAuraRuntimeOneClickResult r2;CHECK(qrx_aura_runtime_one_click_activate(&host2,&cat,100,QRX_AURA_POWER_BALANCED,".",NULL,fetch_runtime,&fc,activate_runtime,&ac,&r2)==-6);CHECK(host2.runtime_adapter.kind==QRX_AURA_ADAPTER_AUTO);

    remove(src);remove(dst);remove(cfgpath);qrx_aura_runtime_package_catalog_free(&cat);EVP_PKEY_free(publisher);printf("phase170 AURA signed runtime one-click activation: PASS\n");return 0;
}
