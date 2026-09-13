#include "compute/qrx_aura_runtime_delivery.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define QRX_MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#define QRX_MKDIR(p) mkdir((p),0700)
#endif
#ifdef QRX_HAVE_CURL
#include <curl/curl.h>
#endif

static int ensure_dir(const char *p){
    if(!p||!p[0])return -1;
    if(QRX_MKDIR(p)==0||errno==EEXIST)return 0;
    return -1;
}
static int safe_name(const char *s){
    if(!s||!s[0])return 0;
    for(;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='-'||*s=='_'||*s=='.'))return 0;
    return 1;
}
static int work_path(const QrxAuraRuntimeDeliveryContext*c,const QrxAuraRuntimePackageAnnouncement*p,char out[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX]){
    if(!c||!p||!out||!safe_name(p->package_id)||!safe_name(p->install_filename))return -1;
#ifdef _WIN32
    int n=snprintf(out,QRX_AURA_RUNTIME_ADAPTER_PATH_MAX,"%s\\%s-%.16s-%s.download",c->work_dir,p->package_id,p->content_root,p->install_filename);
#else
    int n=snprintf(out,QRX_AURA_RUNTIME_ADAPTER_PATH_MAX,"%s/%s-%.16s-%s.download",c->work_dir,p->package_id,p->content_root,p->install_filename);
#endif
    return n<0||(size_t)n>=QRX_AURA_RUNTIME_ADAPTER_PATH_MAX?-1:0;
}
int qrx_aura_runtime_delivery_defaults(QrxAuraRuntimeDeliveryContext*out,const char*work){
    if(!out||!work||!work[0]||strlen(work)>=sizeof(out->work_dir))return -1;
    memset(out,0,sizeof(*out));out->version=QRX_AURA_RUNTIME_DELIVERY_VERSION;snprintf(out->work_dir,sizeof(out->work_dir),"%s",work);out->connect_timeout_seconds=20;out->allow_https_origin=1;return 0;
}
static QrxStorageFs *preferred_cache(QrxAuraRuntimeDeliveryContext*c){if(c->local_cache)return c->local_cache;if(c->drive)return c->drive->local_cache;return NULL;}
static int materialize(QrxStorageFs*fs,const char*root,const char*dst){if(!fs||!root||!dst||qrx_storage_fs_has(fs,root)!=1)return-1;if(qrx_storage_fs_get_file(fs,root,dst))return-1;char got[65];if(qrx_aura_runtime_package_file_root(dst,got)||strcmp(got,root)){remove(dst);return-1;}return 0;}
static int try_drive(QrxAuraRuntimeDeliveryContext*c,const QrxAuraRuntimePackageAnnouncement*p,const char*dst){
    QrxStorageFs*local=preferred_cache(c);if(local&&qrx_storage_fs_has(local,p->content_root)==1)return materialize(local,p->content_root,dst);
    if(!c->drive||!c->drive->local_cache||!c->drive->stat||!c->drive->fetch_range)return -1;
    uint64_t bytes=0;if(c->drive->stat(c->drive->remote_ctx,p->content_root,&bytes)||!bytes)return -1;
    if(qrx_aura_drive_asset_fetch(c->drive,p->content_root,bytes))return -1;
    if(c->local_cache&&c->local_cache!=c->drive->local_cache){unsigned char*b=NULL;size_t n=0;char got[65];if(qrx_storage_fs_read(c->drive->local_cache,p->content_root,&b,&n)||qrx_storage_fs_put(c->local_cache,b,n,got)||strcmp(got,p->content_root)){free(b);return-1;}free(b);local=c->local_cache;}else local=c->drive->local_cache;
    return materialize(local,p->content_root,dst);
}
#ifdef QRX_HAVE_CURL
static size_t wrfile(char*p,size_t s,size_t n,void*v){return fwrite(p,s,n,(FILE*)v);}
static int https_fetch(QrxAuraRuntimeDeliveryContext*c,const char*url,const char*dst){
    if(!c||!url||!dst||strncmp(url,"https://",8))return -1;FILE*f=fopen(dst,"ab+");if(!f)return -1;if(fseek(f,0,SEEK_END)){fclose(f);return-1;}long off=ftell(f);if(off<0){fclose(f);return-1;}CURL*h=curl_easy_init();if(!h){fclose(f);return-1;}curl_easy_setopt(h,CURLOPT_URL,url);/* Genesis hardening (Finding 4): HTTPS-only, including every redirect hop;
       bounded redirects; explicit TLS peer/host verification and minimum
       TLS 1.2. Without REDIR_PROTOCOLS a hostile release origin could
       redirect a signed runtime download to plain HTTP. */
#if defined(CURLOPT_PROTOCOLS_STR)
    curl_easy_setopt(h,CURLOPT_PROTOCOLS_STR,"https");
    curl_easy_setopt(h,CURLOPT_REDIR_PROTOCOLS_STR,"https");
#else
    curl_easy_setopt(h,CURLOPT_PROTOCOLS,(long)CURLPROTO_HTTPS);
    curl_easy_setopt(h,CURLOPT_REDIR_PROTOCOLS,(long)CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(h,CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(h,CURLOPT_MAXREDIRS,5L);curl_easy_setopt(h,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(h,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(h,CURLOPT_SSLVERSION,(long)CURL_SSLVERSION_TLSv1_2);curl_easy_setopt(h,CURLOPT_FAILONERROR,1L);curl_easy_setopt(h,CURLOPT_USERAGENT,"QRX-AURA-Runtime/0.0.9-genesis");curl_easy_setopt(h,CURLOPT_CONNECTTIMEOUT,(long)(c->connect_timeout_seconds?c->connect_timeout_seconds:20));if(c->transfer_timeout_seconds)curl_easy_setopt(h,CURLOPT_TIMEOUT,(long)c->transfer_timeout_seconds);curl_easy_setopt(h,CURLOPT_WRITEFUNCTION,wrfile);curl_easy_setopt(h,CURLOPT_WRITEDATA,f);if(off>0)curl_easy_setopt(h,CURLOPT_RESUME_FROM_LARGE,(curl_off_t)off);CURLcode rc=curl_easy_perform(h);curl_easy_cleanup(h);int frc=fflush(f)||fclose(f);return rc==CURLE_OK&&!frc?0:-1;
}
#endif
static int file_fetch(const char*uri,const char*dst){
    if(strncmp(uri,"file://",7))return-1;const char*src=uri+7;FILE*in=fopen(src,"rb"),*out=in?fopen(dst,"wb"):NULL;if(!in||!out){if(in)fclose(in);if(out)fclose(out);return-1;}unsigned char b[65536];int ok=1;while(ok){size_t n=fread(b,1,sizeof(b),in);if(n&&fwrite(b,1,n,out)!=n)ok=0;if(n<sizeof(b)){if(ferror(in))ok=0;break;}}if(fclose(out))ok=0;fclose(in);return ok?0:-1;
}
int qrx_aura_runtime_fetch_drive_then_origin(void*v,const QrxAuraRuntimePackageAnnouncement*p,char out[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX]){
    QrxAuraRuntimeDeliveryContext*c=(QrxAuraRuntimeDeliveryContext*)v;if(!c||c->version!=QRX_AURA_RUNTIME_DELIVERY_VERSION||!p||!out||!c->work_dir[0])return-1;if(ensure_dir(c->work_dir))return-2;char dst[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX];if(work_path(c,p,dst))return-2;remove(dst);
    if(!try_drive(c,p,dst)){snprintf(out,QRX_AURA_RUNTIME_ADAPTER_PATH_MAX,"%s",dst);return 0;}
    int rc=-1;
    if(!strncmp(p->download_uri,"https://",8)&&c->allow_https_origin){
#ifdef QRX_HAVE_CURL
        rc=https_fetch(c,p->download_uri,dst);
#else
        rc=-1;
#endif
    }else if(!strncmp(p->download_uri,"file://",7)&&c->allow_file_origin)rc=file_fetch(p->download_uri,dst);
    if(rc){remove(dst);return-3;}char got[65];if(qrx_aura_runtime_package_file_root(dst,got)||strcmp(got,p->content_root)){remove(dst);return-4;}
    QrxStorageFs*cache=preferred_cache(c);if(cache){char cr[65];if(qrx_storage_fs_put_file(cache,dst,cr)||strcmp(cr,p->content_root)){remove(dst);return-5;}}
    snprintf(out,QRX_AURA_RUNTIME_ADAPTER_PATH_MAX,"%s",dst);return 0;
}
int qrx_aura_runtime_single_publisher_lookup(void*v,const char*id,EVP_PKEY**out){QrxAuraRuntimeSinglePublisherKey*k=v;if(!k||!id||!out||!k->publisher_id||strcmp(id,k->publisher_id)||!k->public_key)return-1;if(EVP_PKEY_up_ref(k->public_key)!=1)return-1;*out=k->public_key;return 0;}
int qrx_aura_runtime_probe_activate(void*v,const QrxAuraProviderHostConfig*host,const QrxMoeRuntimeDevice*d,const QrxAuraRuntimePackageAnnouncement*p,const char*path){(void)host;(void)p;QrxAuraRuntimeProbeActivation*c=v;if(!c||!d||!path)return-1;qrx_aura_runtime_probe_activation_close(c);if(qrx_aura_runtime_plugin_open(path,d,c->model_cache,&c->plugin))return-2;c->plugin_open=1;return 0;}
void qrx_aura_runtime_probe_activation_close(QrxAuraRuntimeProbeActivation*c){if(!c)return;if(c->plugin_open)qrx_aura_runtime_plugin_close(&c->plugin);c->plugin_open=0;}
int qrx_aura_runtime_auto_bootstrap(const char*host_path,const char*catalog_path,uint64_t h,QrxAuraPowerProfile power,const char*install_dir,QrxAuraRuntimePackageKeyLookupFn lookup,void*keyctx,QrxAuraRuntimeDeliveryContext*delivery,QrxAuraRuntimeProbeActivation*probe,QrxAuraRuntimeOneClickResult*out){
    if(!host_path||!catalog_path||!install_dir||!lookup||!delivery||!probe||!out)return-1;QrxAuraProviderHostConfig host;if(qrx_aura_provider_host_config_load(host_path,&host))return-2;if(!host.enabled)return 1;if(host.runtime_adapter.kind!=QRX_AURA_ADAPTER_AUTO&&host.runtime_adapter.explicit_path[0])return 2;QrxAuraRuntimePackageCatalog cat;qrx_aura_runtime_package_catalog_init(&cat);int rc=qrx_aura_runtime_package_catalog_load_signed_file(catalog_path,h,lookup,keyctx,&cat);if(rc){qrx_aura_runtime_package_catalog_free(&cat);return-3;}rc=qrx_aura_runtime_one_click_activate(&host,&cat,h,power,install_dir,host_path,qrx_aura_runtime_fetch_drive_then_origin,delivery,qrx_aura_runtime_probe_activate,probe,out);qrx_aura_runtime_package_catalog_free(&cat);return rc?(-10+rc):0;
}
