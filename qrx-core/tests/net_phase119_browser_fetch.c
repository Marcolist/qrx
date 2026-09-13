#include "net/qrx_net_browser.h"
#include "net/qrx_net_resolver.h"
#include "storage/qrx_drive_manifest.h"
#include "resource/qrx_storage_consensus.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static void wr(const char*p,const char*s){FILE*f=fopen(p,"wb");assert(f);assert(fwrite(s,1,strlen(s),f)==strlen(s));assert(fclose(f)==0);}
static uint64_t fsz(const char*p){struct stat st;assert(stat(p,&st)==0);return (uint64_t)st.st_size;}
static void cp(const char*a,const char*b){FILE*i=fopen(a,"rb"),*o=fopen(b,"wb");assert(i&&o);char x[65536];for(;;){size_t n=fread(x,1,sizeof(x),i);if(n)assert(fwrite(x,1,n,o)==n);if(n<sizeof(x)){assert(!ferror(i));break;}}assert(fclose(i)==0);assert(fclose(o)==0);}
static void hx(const uint8_t*b,char*out){static const char*x="0123456789abcdef";for(int i=0;i<64;i++){out[i*2]=x[b[i]>>4];out[i*2+1]=x[b[i]&15];}out[128]=0;}
int main(void){
 char tmp[]="/tmp/qrxnet119-XXXXXX";assert(mkdtemp(tmp));char web[512],fsd[512],cat[512],idx[512],cache[512];snprintf(web,sizeof(web),"%s/web",tmp);snprintf(fsd,sizeof(fsd),"%s/fs",tmp);snprintf(cat,sizeof(cat),"%s/catalog",tmp);snprintf(cache,sizeof(cache),"%s/cache",tmp);assert(!mkdir(web,0700));snprintf(idx,sizeof(idx),"%s/index.html",web);wr(idx,"<html><body>verified-qrx-browser</body></html>");
 QrxStorageFs*fs=NULL;assert(qrx_storage_fs_open(fsd,0,0,&fs)==0);EVP_PKEY*k=NULL;assert(qrx_drive_manifest_generate_signing_key(&k)==0);QrxNetPublishJob j;assert(qrx_net_publisher_publish_directory(cat,fs,web,"browse.qrx",2,100,k,&j)==0);
 QrxNetPublishedSite ps;assert(qrx_net_site_package_load(j.package_path,&ps)==0);assert(ps.publishing_public_der_len>0);QrxDomainRecord dr;memset(&dr,0,sizeof(dr));strcpy(dr.name,"browse.qrx");strcpy(dr.owner,"owner");dr.created_height=100;dr.expiry_height=1000;dr.sequence=1;assert(qrx_domain_pubkey_commitment(k,dr.publishing_key_commitment)==0);memcpy(dr.web_manifest_root,j.manifest_root,64);assert(qrx_net_site_verify(&ps,&dr,NULL,101)==0);ps.publishing_public_der[ps.publishing_public_der_len/2]^=1;assert(qrx_net_site_verify(&ps,&dr,NULL,101)!=0);qrx_net_published_site_free(&ps);
 QrxDB db;assert(qrxdb_init(&db,tmp)==0);QrxDomainRecord rr;assert(qrx_net_registry_register(&db,"browse.qrx","owner",NULL,j.manifest_root,dr.publishing_key_commitment,100,1000,&rr)==0);
 char mh[129],payload[1024];hx(j.manifest_root,mh);uint64_t logical=fsz(j.distribution_path);snprintf(payload,sizeof(payload),"contract_id=web119;profile=STANDARD;logical_bytes=%llu;end_height=900;base_atoms_per_gib_epoch=10000;manifest_root_hex=%s",(unsigned long long)logical,mh);QrxServiceEconomicEffect e;assert(qrx_storage_consensus_prepare(tmp,"STORAGE_CONTRACT_CREATE","owner","owner",100000,payload,"contract119",101,&e)==0);QrxDBBatch b;assert(qrxdb_batch_begin(&db,&b)==0);assert(qrx_storage_consensus_stage(&db,&b,tmp,"STORAGE_CONTRACT_CREATE","owner","owner",100000,payload,"contract119",101)==0);assert(qrxdb_batch_commit(&b)==0);
 assert(!mkdir(cache,0700));char qd[512],dd[512],dst[700];snprintf(qd,sizeof(qd),"%s/qrx",cache);assert(!mkdir(qd,0700));snprintf(dd,sizeof(dd),"%s/browse.qrx",qd);assert(!mkdir(dd,0700));snprintf(dst,sizeof(dst),"%s/%s.qrxweb",dd,mh);cp(j.distribution_path,dst);
 QrxStorageDiscoveryTable disc;qrx_storage_discovery_init(&disc);QrxNetBrowserFetchResult br;assert(qrx_net_browser_fetch(&db,&disc,"BROWSE.QRX","/",101,cache,&br)==0);assert(br.from_cache==1&&!strcmp(br.request_path,"index.html")&&!strcmp(br.contract_id,"web119"));FILE*f=fopen(br.file_cache_path,"rb");assert(f);char txt[128]={0};assert(fread(txt,1,sizeof(txt)-1,f)>0);fclose(f);assert(strstr(txt,"verified-qrx-browser"));
 /* Cache tampering is never served; without live providers refetch must fail. */
 f=fopen(dst,"r+b");assert(f);assert(fseek(f,-1,SEEK_END)==0);int c=fgetc(f);assert(c!=EOF);assert(fseek(f,-1,SEEK_END)==0);fputc(c^1,f);fclose(f);assert(qrx_net_browser_fetch(&db,&disc,"browse.qrx","/",101,cache,&br)!=0);
 QrxBrowserResolution r;assert(qrx_browser_resolve_input("browse.qrx/docs",&r)==0&&r.route==QRX_BROWSER_ROUTE_QRX&&!r.dns_allowed);assert(qrx_browser_resolve_input("qrx://browse.qrx/",&r)==0&&r.route==QRX_BROWSER_ROUTE_QRX&&!r.dns_allowed);assert(qrx_browser_resolve_input("https://example.com/",&r)==0&&r.route==QRX_BROWSER_ROUTE_WWW&&r.dns_allowed);
 qrx_storage_discovery_free(&disc);qrxdb_close(&db);EVP_PKEY_free(k);qrx_storage_fs_close(fs);puts("PASS: QRX browser cache is chain/root/key/signature/content verified; tampered cache fails closed and .qrx never enables DNS");return 0;
}
