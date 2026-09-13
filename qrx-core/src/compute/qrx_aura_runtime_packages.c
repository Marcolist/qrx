#include "compute/qrx_aura_runtime_packages.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/crypto.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define qrx_fsync _commit
#define qrx_fileno _fileno
#else
#include <unistd.h>
#define qrx_fsync fsync
#define qrx_fileno fileno
#endif

#define PACKAGE_DOMAIN "QRX/AURA/RUNTIME-PACKAGE/V1"

typedef struct { uint8_t *p; size_t n,cap; } Buf;
static int grow(Buf*b,size_t add){if(!b||SIZE_MAX-b->n<add)return-1;size_t need=b->n+add;if(need<=b->cap)return 0;size_t cap=b->cap?b->cap:512;while(cap<need){if(cap>SIZE_MAX/2)return-1;cap*=2;}uint8_t*p=realloc(b->p,cap);if(!p)return-1;b->p=p;b->cap=cap;return 0;}
static int put(Buf*b,const void*p,size_t n){if(grow(b,n))return-1;if(n)memcpy(b->p+b->n,p,n);b->n+=n;return 0;}
static int u32(Buf*b,uint32_t v){uint8_t x[4]={(uint8_t)(v>>24),(uint8_t)(v>>16),(uint8_t)(v>>8),(uint8_t)v};return put(b,x,4);}
static int u64(Buf*b,uint64_t v){uint8_t x[8];for(int i=7;i>=0;i--){x[i]=(uint8_t)v;v>>=8;}return put(b,x,8);}
static int str(Buf*b,const char*s,size_t cap){if(!s||!memchr(s,0,cap))return-1;size_t n=strlen(s);return n>UINT32_MAX||u32(b,(uint32_t)n)||put(b,s,n)?-1:0;}
static int hex64(const char*s){if(!s||strlen(s)!=64)return 0;for(size_t i=0;i<64;i++){char c=s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;}return 1;}
static void hex32(const uint8_t h[32],char out[65]){static const char x[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=x[h[i]>>4];out[i*2+1]=x[h[i]&15];}out[64]=0;}
static int hash_domain(const char*d,const void*p,size_t n,uint8_t out[32]){EVP_MD_CTX*m=EVP_MD_CTX_new();unsigned hn=0;if(!m)return-1;int ok=EVP_DigestInit_ex(m,EVP_sha3_256(),NULL)==1&&EVP_DigestUpdate(m,d,strlen(d))==1&&(!n||EVP_DigestUpdate(m,p,n)==1)&&EVP_DigestFinal_ex(m,out,&hn)==1&&hn==32;EVP_MD_CTX_free(m);return ok?0:-1;}
static int announcement_valid(const QrxAuraRuntimePackageAnnouncement*a,uint64_t h){
    if(!a||a->version!=QRX_AURA_RUNTIME_PACKAGE_VERSION||!a->publisher_id[0]||!a->package_id[0]||!a->package_version[0]||!a->sequence||a->valid_until_height<a->valid_from_height)return 0;
    if(!memchr(a->publisher_id,0,sizeof(a->publisher_id))||!memchr(a->package_id,0,sizeof(a->package_id))||!memchr(a->package_version,0,sizeof(a->package_version))||!memchr(a->download_uri,0,sizeof(a->download_uri))||!memchr(a->install_filename,0,sizeof(a->install_filename)))return 0;
    if(a->adapter_kind<QRX_AURA_ADAPTER_LLAMA_CPP_CPU||a->adapter_kind>QRX_AURA_ADAPTER_LLAMA_CPP_METAL||a->platform>QRX_AURA_PACKAGE_PLATFORM_WINDOWS||a->arch>QRX_AURA_PACKAGE_ARCH_ARM||a->backend<QRX_MOE_BACKEND_CPU||a->backend>QRX_MOE_BACKEND_OTHER||!hex64(a->content_root)||!a->install_filename[0])return 0;
    if(h&&(h<a->valid_from_height||h>a->valid_until_height))return 0;
    return 1;
}
static int payload(const QrxAuraRuntimePackageAnnouncement*a,Buf*b){
    if(!announcement_valid(a,0))return-1;
    return u32(b,a->version)||str(b,a->publisher_id,sizeof(a->publisher_id))||str(b,a->package_id,sizeof(a->package_id))||str(b,a->package_version,sizeof(a->package_version))||u64(b,a->sequence)||u64(b,a->valid_from_height)||u64(b,a->valid_until_height)||u32(b,(uint32_t)a->adapter_kind)||u32(b,(uint32_t)a->platform)||u32(b,(uint32_t)a->arch)||u32(b,(uint32_t)a->backend)||u32(b,a->required_accelerator_features)||u64(b,a->min_device_memory_bytes)||u32(b,a->min_cuda_cc_major)||u32(b,a->min_cuda_cc_minor)||str(b,a->content_root,sizeof(a->content_root))||str(b,a->download_uri,sizeof(a->download_uri))||str(b,a->install_filename,sizeof(a->install_filename))?-1:0;
}
QrxAuraPackagePlatform qrx_aura_runtime_package_current_platform(void){
#if defined(_WIN32)
    return QRX_AURA_PACKAGE_PLATFORM_WINDOWS;
#elif defined(__APPLE__)
    return QRX_AURA_PACKAGE_PLATFORM_MACOS;
#else
    return QRX_AURA_PACKAGE_PLATFORM_LINUX;
#endif
}
QrxAuraPackageArch qrx_aura_runtime_package_current_arch(void){
#if defined(__x86_64__) || defined(_M_X64)
    return QRX_AURA_PACKAGE_ARCH_X86_64;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return QRX_AURA_PACKAGE_ARCH_AARCH64;
#elif defined(__arm__) || defined(_M_ARM)
    return QRX_AURA_PACKAGE_ARCH_ARM;
#else
    return QRX_AURA_PACKAGE_ARCH_ANY;
#endif
}
int qrx_aura_runtime_package_hash(const QrxAuraRuntimePackageAnnouncement*a,uint8_t out[32]){if(!out)return-1;Buf b={0};if(payload(a,&b)){free(b.p);return-1;}int rc=hash_domain(PACKAGE_DOMAIN,b.p,b.n,out);free(b.p);return rc;}
int qrx_aura_runtime_package_sign(EVP_PKEY*k,const QrxAuraRuntimePackageAnnouncement*a,uint8_t**sig,size_t*sn){if(!k||!sig||!sn)return-1;*sig=NULL;*sn=0;uint8_t h[32];if(qrx_aura_runtime_package_hash(a,h))return-1;EVP_MD_CTX*m=EVP_MD_CTX_new();size_t n=0;uint8_t*p=NULL;int rc=-1;if(!m||EVP_DigestSignInit_ex(m,NULL,NULL,NULL,NULL,k,NULL)!=1||EVP_DigestSign(m,NULL,&n,h,32)!=1||!n||n>QRX_AURA_RUNTIME_PACKAGE_SIGNATURE_MAX)goto done;p=malloc(n);if(!p)goto done;if(EVP_DigestSign(m,p,&n,h,32)!=1)goto done;*sig=p;*sn=n;p=NULL;rc=0;done:OPENSSL_cleanse(h,sizeof(h));free(p);EVP_MD_CTX_free(m);return rc;}
int qrx_aura_runtime_package_verify(EVP_PKEY*k,const QrxAuraRuntimePackageAnnouncement*a,const uint8_t*sig,size_t sn){if(!k||!sig||!sn||sn>QRX_AURA_RUNTIME_PACKAGE_SIGNATURE_MAX)return-1;uint8_t h[32];if(qrx_aura_runtime_package_hash(a,h))return-1;EVP_MD_CTX*m=EVP_MD_CTX_new();int ok=m&&EVP_DigestVerifyInit_ex(m,NULL,NULL,NULL,NULL,k,NULL)==1&&EVP_DigestVerify(m,sig,sn,h,32)==1;OPENSSL_cleanse(h,sizeof(h));EVP_MD_CTX_free(m);return ok?0:-1;}
void qrx_aura_runtime_package_catalog_init(QrxAuraRuntimePackageCatalog*c){if(c){memset(c,0,sizeof(*c));c->version=QRX_AURA_RUNTIME_PACKAGE_VERSION;}}
void qrx_aura_runtime_package_catalog_free(QrxAuraRuntimePackageCatalog*c){if(!c)return;for(uint32_t i=0;i<c->count;i++)free(c->entries[i].signature);memset(c,0,sizeof(*c));}
static QrxAuraRuntimePackageEntry*slot(QrxAuraRuntimePackageCatalog*c,const QrxAuraRuntimePackageAnnouncement*a){for(uint32_t i=0;i<c->count;i++){QrxAuraRuntimePackageAnnouncement*b=&c->entries[i].announcement;if(!strcmp(a->package_id,b->package_id)&&a->platform==b->platform&&a->arch==b->arch&&a->adapter_kind==b->adapter_kind)return &c->entries[i];}return NULL;}
int qrx_aura_runtime_package_catalog_ingest(QrxAuraRuntimePackageCatalog*c,const QrxAuraRuntimePackageAnnouncement*a,const uint8_t*sig,size_t sn,uint64_t h,QrxAuraRuntimePackageKeyLookupFn lookup,void*ctx){if(!c||c->version!=1||!announcement_valid(a,h)||!sig||!sn||!lookup)return-1;EVP_PKEY*pk=NULL;if(lookup(ctx,a->publisher_id,&pk)||!pk||qrx_aura_runtime_package_verify(pk,a,sig,sn)){EVP_PKEY_free(pk);return-2;}EVP_PKEY_free(pk);QrxAuraRuntimePackageEntry*e=slot(c,a);if(e&&a->sequence<=e->announcement.sequence)return-3;if(!e){if(c->count>=QRX_AURA_RUNTIME_PACKAGE_MAX)return-4;e=&c->entries[c->count++];memset(e,0,sizeof(*e));}uint8_t*copy=malloc(sn);if(!copy)return-5;memcpy(copy,sig,sn);free(e->signature);e->announcement=*a;e->signature=copy;e->signature_len=sn;c->revision++;return 0;}
size_t qrx_aura_runtime_package_catalog_prune(QrxAuraRuntimePackageCatalog*c,uint64_t h){if(!c)return 0;uint32_t w=0;size_t n=0;for(uint32_t i=0;i<c->count;i++){if(c->entries[i].announcement.valid_until_height<h){free(c->entries[i].signature);n++;continue;}if(w!=i)c->entries[w]=c->entries[i];w++;}c->count=w;if(n)c->revision++;return n;}

static void sighex(const uint8_t *p,size_t n,char *out){static const char x[]="0123456789abcdef";for(size_t i=0;i<n;i++){out[i*2]=x[p[i]>>4];out[i*2+1]=x[p[i]&15];}out[n*2]=0;}
static int unhex(const char*s,uint8_t**out,size_t*n){if(!s||!out||!n)return-1;size_t z=strlen(s);if(!z||(z&1)||z/2>QRX_AURA_RUNTIME_PACKAGE_SIGNATURE_MAX)return-1;uint8_t*p=malloc(z/2);if(!p)return-1;for(size_t i=0;i<z;i+=2){int a=-1,b=-1;char ca=s[i],cb=s[i+1];if(ca>='0'&&ca<='9')a=ca-'0';else if(ca>='a'&&ca<='f')a=ca-'a'+10;else if(ca>='A'&&ca<='F')a=ca-'A'+10;if(cb>='0'&&cb<='9')b=cb-'0';else if(cb>='a'&&cb<='f')b=cb-'a'+10;else if(cb>='A'&&cb<='F')b=cb-'A'+10;if(a<0||b<0){free(p);return-1;}p[i/2]=(uint8_t)((a<<4)|b);}*out=p;*n=z/2;return 0;}
static int fields(char*line,char**f,size_t cap){size_t n=0;char*p=line;if(!line||!f||!cap)return-1;while(1){if(n>=cap)return-1;f[n++]=p;char*q=strchr(p,'|');if(!q)break;*q=0;p=q+1;}return (int)n;}
static int u64txt(const char*s,uint64_t*out){if(!s||!s[0]||!out)return-1;char*e=NULL;unsigned long long v=strtoull(s,&e,10);if(!e||*e)return-1;*out=(uint64_t)v;return 0;}
static int u32txt(const char*s,uint32_t*out){uint64_t v=0;if(u64txt(s,&v)||v>UINT32_MAX)return-1;*out=(uint32_t)v;return 0;}
int qrx_aura_runtime_package_catalog_save_signed_file(const char*path,const QrxAuraRuntimePackageCatalog*c){if(!path||!c||c->version!=QRX_AURA_RUNTIME_PACKAGE_VERSION)return-1;FILE*f=fopen(path,"wb");if(!f)return-2;if(fputs("QRXRUNTIME1\n",f)<0){fclose(f);return-3;}for(uint32_t i=0;i<c->count;i++){const QrxAuraRuntimePackageEntry*e=&c->entries[i];if(!announcement_valid(&e->announcement,0)||!e->signature||!e->signature_len){fclose(f);return-4;}size_t hn=e->signature_len*2;char*h=malloc(hn+1);if(!h){fclose(f);return-5;}sighex(e->signature,e->signature_len,h);const QrxAuraRuntimePackageAnnouncement*a=&e->announcement;int rc=fprintf(f,"%u|%s|%s|%s|%llu|%llu|%llu|%u|%u|%u|%u|%u|%llu|%u|%u|%s|%s|%s|%s\n",a->version,a->publisher_id,a->package_id,a->package_version,(unsigned long long)a->sequence,(unsigned long long)a->valid_from_height,(unsigned long long)a->valid_until_height,(unsigned)a->adapter_kind,(unsigned)a->platform,(unsigned)a->arch,(unsigned)a->backend,a->required_accelerator_features,(unsigned long long)a->min_device_memory_bytes,a->min_cuda_cc_major,a->min_cuda_cc_minor,a->content_root,a->download_uri,a->install_filename,h);free(h);if(rc<0){fclose(f);return-6;}}return fclose(f)?-7:0;}
int qrx_aura_runtime_package_catalog_load_signed_file(const char*path,uint64_t h,QrxAuraRuntimePackageKeyLookupFn lookup,void*ctx,QrxAuraRuntimePackageCatalog*out){if(!path||!lookup||!out)return-1;FILE*f=fopen(path,"rb");if(!f)return-2;char line[4096];if(!fgets(line,sizeof(line),f)||strcmp(line,"QRXRUNTIME1\n")){fclose(f);return-3;}QrxAuraRuntimePackageCatalog tmp;qrx_aura_runtime_package_catalog_init(&tmp);int rc=0;while(fgets(line,sizeof(line),f)){size_t n=strlen(line);while(n&&(line[n-1]=='\n'||line[n-1]=='\r'))line[--n]=0;if(!line[0]||line[0]=='#')continue;char*fv[19];int fn=fields(line,fv,19);if(fn!=19){rc=-4;break;}QrxAuraRuntimePackageAnnouncement a;memset(&a,0,sizeof(a));uint32_t u=0;if(u32txt(fv[0],&a.version)||strlen(fv[1])>=sizeof(a.publisher_id)||strlen(fv[2])>=sizeof(a.package_id)||strlen(fv[3])>=sizeof(a.package_version)||u64txt(fv[4],&a.sequence)||u64txt(fv[5],&a.valid_from_height)||u64txt(fv[6],&a.valid_until_height)||u32txt(fv[7],&u)){rc=-4;break;}a.adapter_kind=(QrxAuraRuntimeAdapterKind)u;if(u32txt(fv[8],&u)){rc=-4;break;}a.platform=(QrxAuraPackagePlatform)u;if(u32txt(fv[9],&u)){rc=-4;break;}a.arch=(QrxAuraPackageArch)u;if(u32txt(fv[10],&u)){rc=-4;break;}a.backend=(QrxMoeRuntimeBackend)u;if(u32txt(fv[11],&a.required_accelerator_features)||u64txt(fv[12],&a.min_device_memory_bytes)||u32txt(fv[13],&a.min_cuda_cc_major)||u32txt(fv[14],&a.min_cuda_cc_minor)||strlen(fv[15])>=sizeof(a.content_root)||strlen(fv[16])>=sizeof(a.download_uri)||strlen(fv[17])>=sizeof(a.install_filename)){rc=-4;break;}snprintf(a.publisher_id,sizeof(a.publisher_id),"%s",fv[1]);snprintf(a.package_id,sizeof(a.package_id),"%s",fv[2]);snprintf(a.package_version,sizeof(a.package_version),"%s",fv[3]);snprintf(a.content_root,sizeof(a.content_root),"%s",fv[15]);snprintf(a.download_uri,sizeof(a.download_uri),"%s",fv[16]);snprintf(a.install_filename,sizeof(a.install_filename),"%s",fv[17]);uint8_t*sig=NULL;size_t sn=0;if(unhex(fv[18],&sig,&sn)){rc=-4;break;}int ir=qrx_aura_runtime_package_catalog_ingest(&tmp,&a,sig,sn,h,lookup,ctx);free(sig);if(ir){rc=-5;break;}}fclose(f);if(rc){qrx_aura_runtime_package_catalog_free(&tmp);return rc;}*out=tmp;return 0;}
static int package_device_compatible(const QrxAuraRuntimePackageAnnouncement*p,const QrxMoeRuntimeDevice*d){
    if(!p||!d||qrx_moe_runtime_device_validate(d))return 0;
    if(p->adapter_kind==QRX_AURA_ADAPTER_LLAMA_CPP_METAL){if(!(d->accelerator_features&QRX_MOE_ACCEL_FEAT_METAL))return 0;}
    else if(p->backend==QRX_MOE_BACKEND_MLX_METAL){if(!(d->accelerator_features&QRX_MOE_ACCEL_FEAT_METAL))return 0;}
    else if(d->backend!=p->backend)return 0;
    /* Runtime package announcements store QRX_MOE_ACCEL_FEAT_* bits, not
       QRX_MOE_KERNEL_* bits.  Comparing them to kernel_mask() is incorrect
       because FP16/FP32/MLX/Metal use different bit positions in the two
       namespaces. */
    if((d->accelerator_features&p->required_accelerator_features)!=p->required_accelerator_features)return 0;
    if(d->device_memory_bytes<p->min_device_memory_bytes)return 0;
    if(p->backend==QRX_MOE_BACKEND_CUDA){if(d->cuda_cc_major<p->min_cuda_cc_major)return 0;if(d->cuda_cc_major==p->min_cuda_cc_major&&d->cuda_cc_minor<p->min_cuda_cc_minor)return 0;}
    return 1;
}
static uint32_t package_score(const QrxAuraRuntimePackageAnnouncement*p,const QrxMoeRuntimeDevice*d,QrxAuraPowerProfile power){uint64_t s=1000;switch(p->adapter_kind){case QRX_AURA_ADAPTER_MLX_METAL:s+=power==QRX_AURA_POWER_ECO?7000:9000;break;case QRX_AURA_ADAPTER_LLAMA_CPP_CUDA:s+=power==QRX_AURA_POWER_ECO?6000:10000;break;case QRX_AURA_ADAPTER_LLAMA_CPP_METAL:s+=power==QRX_AURA_POWER_ECO?6500:9500;break;case QRX_AURA_ADAPTER_LLAMA_CPP_CPU:s+=3000;break;default:s+=1000;break;}s+=d->device_memory_bytes>>30;if(qrx_moe_runtime_is_nvidia_p40(d))s+=250;return s>UINT32_MAX?UINT32_MAX:(uint32_t)s;}
int qrx_aura_runtime_package_select_for(const QrxAuraRuntimePackageCatalog*c,const QrxMoeRuntimeInventory*i,uint64_t h,QrxAuraPowerProfile power,QrxAuraPackagePlatform plat,QrxAuraPackageArch arch,QrxAuraRuntimeActivationPlan*out){if(!c||!i||!out||c->version!=1||qrx_moe_runtime_inventory_validate(i)||power<QRX_AURA_POWER_ECO||power>QRX_AURA_POWER_PERFORMANCE||plat>QRX_AURA_PACKAGE_PLATFORM_WINDOWS||arch>QRX_AURA_PACKAGE_ARCH_ARM)return-1;int found=0;QrxAuraRuntimeActivationPlan best;memset(&best,0,sizeof(best));for(uint32_t p=0;p<c->count;p++){const QrxAuraRuntimePackageAnnouncement*a=&c->entries[p].announcement;if(!announcement_valid(a,h)||(a->platform!=QRX_AURA_PACKAGE_PLATFORM_ANY&&a->platform!=plat)||(a->arch!=QRX_AURA_PACKAGE_ARCH_ANY&&a->arch!=arch))continue;for(uint32_t d=0;d<i->device_count;d++){if(!package_device_compatible(a,&i->devices[d].device))continue;uint32_t score=package_score(a,&i->devices[d].device,power);if(!found||score>best.score||(score==best.score&&strcmp(a->package_id,best.package.package_id)<0)){memset(&best,0,sizeof(best));best.version=1;best.device_index=d;best.device=i->devices[d].device;best.package=*a;best.score=score;found=1;}}}if(!found)return-2;if(best.package.backend==QRX_MOE_BACKEND_MLX_METAL){best.device.backend=QRX_MOE_BACKEND_MLX_METAL;best.device.accelerator_features|=QRX_MOE_ACCEL_FEAT_MLX|QRX_MOE_ACCEL_FEAT_METAL;}*out=best;return 0;}
int qrx_aura_runtime_package_select(const QrxAuraRuntimePackageCatalog*c,const QrxMoeRuntimeInventory*i,uint64_t h,QrxAuraPowerProfile power,QrxAuraRuntimeActivationPlan*out){return qrx_aura_runtime_package_select_for(c,i,h,power,qrx_aura_runtime_package_current_platform(),qrx_aura_runtime_package_current_arch(),out);}
int qrx_aura_runtime_package_file_root(const char*path,char out[65]){if(!path||!out)return-1;FILE*f=fopen(path,"rb");if(!f)return-2;EVP_MD_CTX*m=EVP_MD_CTX_new();uint8_t buf[65536],h[32];unsigned hn=0;int ok=m&&EVP_DigestInit_ex(m,EVP_sha3_256(),NULL)==1;while(ok){size_t n=fread(buf,1,sizeof(buf),f);if(n&&EVP_DigestUpdate(m,buf,n)!=1)ok=0;if(n<sizeof(buf)){if(ferror(f))ok=0;break;}}if(ok&&EVP_DigestFinal_ex(m,h,&hn)!=1)ok=0;EVP_MD_CTX_free(m);fclose(f);if(!ok||hn!=32)return-3;hex32(h,out);OPENSSL_cleanse(h,sizeof(h));return 0;}
static int replace_file(const char*s,const char*d){
#ifdef _WIN32
    return MoveFileExA(s,d,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?0:-1;
#else
    return rename(s,d);
#endif
}
int qrx_aura_runtime_package_install_verified(const char*src,const char*dst,const char expected[65]){if(!src||!dst||!hex64(expected))return-1;char root[65];if(qrx_aura_runtime_package_file_root(src,root)||strcmp(root,expected))return-2;size_t dn=strlen(dst);char*tmp=malloc(dn+5);if(!tmp)return-3;memcpy(tmp,dst,dn);memcpy(tmp+dn,".tmp",5);FILE*in=fopen(src,"rb"),*out=in?fopen(tmp,"wb"):NULL;if(!in||!out){if(in)fclose(in);if(out)fclose(out);free(tmp);return-4;}uint8_t buf[65536];int ok=1;while(ok){size_t n=fread(buf,1,sizeof(buf),in);if(n&&fwrite(buf,1,n,out)!=n)ok=0;if(n<sizeof(buf)){if(ferror(in))ok=0;break;}}if(fflush(out)||qrx_fsync(qrx_fileno(out)))ok=0;if(fclose(out))ok=0;fclose(in);if(!ok||replace_file(tmp,dst)){remove(tmp);free(tmp);return-5;}free(tmp);if(qrx_aura_runtime_package_file_root(dst,root)||strcmp(root,expected)){remove(dst);return-6;}return 0;}
int qrx_aura_runtime_package_activate_verified(QrxAuraProviderHostConfig*host,const QrxAuraRuntimeActivationPlan*plan,const char*src,const char*dst,QrxAuraRuntimeActivateFn fn,void*ctx){if(!host||!plan||plan->version!=1||!src||!dst||!fn)return-1;if(qrx_aura_runtime_package_install_verified(src,dst,plan->package.content_root))return-2;if(strlen(dst)>=sizeof(host->runtime_adapter.explicit_path)){remove(dst);return-3;}host->runtime_adapter.version=QRX_AURA_RUNTIME_ADAPTER_VERSION;host->runtime_adapter.kind=plan->package.adapter_kind;memcpy(host->runtime_adapter.explicit_path,dst,strlen(dst)+1);int rc=fn(ctx,host,&plan->device,&plan->package,dst);if(rc){host->runtime_adapter.kind=QRX_AURA_ADAPTER_AUTO;host->runtime_adapter.explicit_path[0]=0;return-4;}return 0;}


static int qrx_join_runtime_path(const char *dir,const char *file,char out[QRX_AURA_RUNTIME_ADAPTER_PATH_MAX]){
    if(!dir||!file||!file[0]||!out)return -1;
    size_t dn=strlen(dir),fn=strlen(file);
    if(dn+fn+2>QRX_AURA_RUNTIME_ADAPTER_PATH_MAX)return -1;
    if(!dn){memcpy(out,file,fn+1);return 0;}
#ifdef _WIN32
    const char sep='\\';
#else
    const char sep='/';
#endif
    memcpy(out,dir,dn);
    if(dir[dn-1]!='/'&&dir[dn-1]!='\\')out[dn++]=sep;
    memcpy(out+dn,file,fn+1);
    return 0;
}

int qrx_aura_runtime_one_click_activate(QrxAuraProviderHostConfig *host,
                                        const QrxAuraRuntimePackageCatalog *catalog,
                                        uint64_t height,QrxAuraPowerProfile power,
                                        const char *install_dir,const char *host_config_path,
                                        QrxAuraRuntimePackageFetchFn fetch,void *fetch_ctx,
                                        QrxAuraRuntimeActivateFn activate,void *activate_ctx,
                                        QrxAuraRuntimeOneClickResult *out){
    if(!host||!catalog||!install_dir||!fetch||!activate||!out)return -1;
    if(host->version!=QRX_AURA_PROVIDER_HOST_CONFIG_VERSION||catalog->version!=QRX_AURA_RUNTIME_PACKAGE_VERSION)return -1;
    QrxAuraRuntimeOneClickResult r;memset(&r,0,sizeof(r));r.version=1;
    if(qrx_moe_runtime_discover_native(&r.inventory))return -2;
    if(qrx_aura_runtime_package_select(catalog,&r.inventory,height,power,&r.plan))return -3;
    if(fetch(fetch_ctx,&r.plan.package,r.downloaded_path))return -4;
    if(!r.downloaded_path[0])return -4;
    if(qrx_join_runtime_path(install_dir,r.plan.package.install_filename,r.installed_path))return -5;
    QrxAuraRuntimeAdapterConfig old_adapter=host->runtime_adapter;
    if(qrx_aura_runtime_package_activate_verified(host,&r.plan,r.downloaded_path,r.installed_path,activate,activate_ctx)){
        host->runtime_adapter=old_adapter;
        return -6;
    }
    if(host_config_path&&host_config_path[0]){
        if(qrx_aura_provider_host_config_save(host_config_path,host)){
            host->runtime_adapter=old_adapter;
            return -7;
        }
        r.host_config_persisted=1;
    }
    *out=r;
    return 0;
}
