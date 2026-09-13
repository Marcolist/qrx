#include "net/qrx_net_browser.h"
#include "resource/qrx_storage_consensus.h"
#include "storage/qrx_storage_network.h"
#include <openssl/crypto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#define B_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define B_MKDIR(p) mkdir((p),0700)
#endif
static int b_mk(const char*p){return B_MKDIR(p)==0||errno==EEXIST?0:-1;}
static void b_hex64(const uint8_t in[64],char out[129]){static const char*x="0123456789abcdef";for(int i=0;i<64;i++){out[i*2]=x[in[i]>>4];out[i*2+1]=x[in[i]&15];}out[128]=0;}
static int b_root_nonzero(const uint8_t r[64]){uint8_t z=0;for(int i=0;i<64;i++)z|=r[i];return z!=0;}
static int b_req_path(const char*in,char out[QRX_NET_SITE_PATH_MAX+1]){if(!in||!*in||!strcmp(in,"/")){strcpy(out,"index.html");return 0;}while(*in=='/')in++;size_t n=strcspn(in,"?#");if(!n||n>QRX_NET_SITE_PATH_MAX)return -1;memcpy(out,in,n);out[n]=0;if(out[n-1]=='/'){if(n+10>QRX_NET_SITE_PATH_MAX)return -1;strcat(out,"index.html");}if(strstr(out,"..")||strchr(out,'\\'))return -1;for(const unsigned char*p=(const unsigned char*)out;*p;p++)if(*p<32)return -1;return 0;}
typedef struct{QrxDB*db;const uint8_t*root;uint64_t h;char cid[129];QrxStorageContractState c;uint64_t best;} FindCtx;
static int b_find_cb(const char*k,const char*v,uint32_t value_len,void*vp){(void)v;(void)value_len;FindCtx*c=vp;const char*pre="storage/contract/";size_t pl=strlen(pre);if(strncmp(k,pre,pl))return 0;const char*cid=k+pl;if(!*cid||strlen(cid)>128)return 0;QrxStorageContractState s;if(qrx_storage_contract_get(c->db,cid,&s))return 0;if(s.status!=QRX_STORAGE_CONTRACT_ACTIVE||s.end_height<c->h||s.shard_count!=14||strcmp(s.profile,"STANDARD")||CRYPTO_memcmp(s.manifest_root,c->root,64))return 0;if(!c->cid[0]||s.start_height>c->best){snprintf(c->cid,sizeof(c->cid),"%s",cid);c->c=s;c->best=s.start_height;}return 0;}
static int b_find_contract(QrxDB*db,const uint8_t root[64],uint64_t h,char cid[129],QrxStorageContractState*out){FindCtx c={db,root,h,{0},{0},0};if(qrxdb_scan_prefix(db,"storage/contract/",b_find_cb,&c)||!c.cid[0])return -1;strcpy(cid,c.cid);if(out)*out=c.c;return 0;}
static int b_extract_verified(const char*dist,const QrxDomainRecord*r,uint64_t h,const char*req,const char*outpath,uint64_t*seq){QrxNetPublishedSite s;unsigned char*d=NULL;size_t n=0;if(qrx_net_publisher_extract_file(dist,req,&s,&d,&n))return -1;int rc=qrx_net_site_verify(&s,r,NULL,h);if(!rc){FILE*f=fopen(outpath,"wb");if(!f)rc=-1;else{if(n&&fwrite(d,1,n,f)!=n)rc=-1;if(fclose(f)!=0)rc=-1;if(rc)remove(outpath);}}if(!rc&&seq)*seq=s.manifest.sequence;free(d);qrx_net_published_site_free(&s);return rc;}
int qrx_net_browser_fetch(QrxDB*db,QrxStorageDiscoveryTable*d,const char*domain,const char*request,uint64_t h,const char*cache,QrxNetBrowserFetchResult*out){
    if(!db||!d||!domain||!cache||!out)return -1;memset(out,0,sizeof(*out));QrxDomainRecord r;if(qrx_net_registry_get(db,domain,&r)||!qrx_domain_is_active(&r,h)||!b_root_nonzero(r.web_manifest_root)||!b_root_nonzero(r.publishing_key_commitment))return -1;
    if(b_req_path(request,out->request_path))return -1;snprintf(out->domain,sizeof(out->domain),"%s",r.name);b_hex64(r.web_manifest_root,out->manifest_root_hex);QrxStorageContractState c;if(b_find_contract(db,r.web_manifest_root,h,out->contract_id,&c))return -1;
    QrxShardProviderSource src[QRX_STORAGE_MAX_FETCH_SOURCES];size_t n=0;
    char d1[1400],d2[1400],dist[1400],tmp[1450],files[1450],fpath[1500];if(b_mk(cache)||snprintf(d1,sizeof(d1),"%s/qrx",cache)>=(int)sizeof(d1)||b_mk(d1)||snprintf(d2,sizeof(d2),"%s/%s",d1,r.name)>=(int)sizeof(d2)||b_mk(d2)||snprintf(dist,sizeof(dist),"%s/%s.qrxweb",d2,out->manifest_root_hex)>=(int)sizeof(dist))return -1;
    int valid=0;QrxNetPublishedSite vs;if(qrx_net_publisher_verify_distribution(dist,&r,h,&vs)==0){valid=1;out->from_cache=1;out->site_sequence=vs.manifest.sequence;qrx_net_published_site_free(&vs);}else remove(dist);
    if(!valid){if(qrx_storage_discovery_sources_for_contract(d,db,out->contract_id,c.shard_count,h,src,QRX_STORAGE_MAX_FETCH_SOURCES,&n)||n<10)return -1;out->active_sources=(uint32_t)n;if(c.logical_bytes>SIZE_MAX)return -1;snprintf(tmp,sizeof(tmp),"%s.part",dist);remove(tmp);QrxStorageNetworkFetchCtx fc={out->contract_id,5000,10000};QrxMultiFetchStats st={0};if(qrx_storage_stream_reconstruct_to_file(src,n,(size_t)c.shard_bytes,10,4,(size_t)c.logical_bytes,QRX_STORAGE_DEFAULT_FETCH_RANGE,qrx_storage_network_fetch_range,&fc,tmp,&st))return -1;if(qrx_net_publisher_verify_distribution(tmp,&r,h,&vs)){remove(tmp);return -1;}out->site_sequence=vs.manifest.sequence;qrx_net_published_site_free(&vs);if(rename(tmp,dist)){remove(tmp);return -1;}out->bytes_received=st.bytes_received;}
    if(snprintf(files,sizeof(files),"%s/files-%s",d2,out->manifest_root_hex)>=(int)sizeof(files)||b_mk(files))return -1;char safe[QRX_NET_SITE_PATH_MAX+1];snprintf(safe,sizeof(safe),"%s",out->request_path);for(char*p=safe;*p;p++)if(*p=='/')*p='_';if(snprintf(fpath,sizeof(fpath),"%s/%s",files,safe)>=(int)sizeof(fpath))return -1;if(b_extract_verified(dist,&r,h,out->request_path,fpath,&out->site_sequence))return -1;snprintf(out->distribution_cache_path,sizeof(out->distribution_cache_path),"%s",dist);snprintf(out->file_cache_path,sizeof(out->file_cache_path),"%s",fpath);return 0;
}
