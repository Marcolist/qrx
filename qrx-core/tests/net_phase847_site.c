#include "net/qrx_net_site.h"
#include "storage/qrx_drive_manifest.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(void){char tmp[]="/tmp/qrxsite-XXXXXX";assert(mkdtemp(tmp));QrxStorageFs *fs=NULL;assert(qrx_storage_fs_open(tmp,10*1024*1024,0,&fs)==0);EVP_PKEY*k=NULL;assert(qrx_drive_manifest_generate_signing_key(&k)==0);const uint8_t a[]="<html>hello</html>",b[]="body{}";QrxNetSiteFileInput in[]={{"index.html",a,sizeof(a)-1},{"css/app.css",b,sizeof(b)-1}};QrxNetPublishedSite s;assert(qrx_net_site_publish(fs,"example.qrx",5,100,in,2,k,&s)==0);assert(qrx_net_site_verify_file(&s.files[0],!strcmp(s.files[0].path,"css/app.css")?b:a,!strcmp(s.files[0].path,"css/app.css")?sizeof(b)-1:sizeof(a)-1)==0);QrxDomainRecord r;memset(&r,0,sizeof(r));strcpy(r.name,"example.qrx");strcpy(r.owner,"owner");r.expiry_height=1000;assert(qrx_domain_pubkey_commitment(k,r.publishing_key_commitment)==0);assert(qrx_net_site_manifest_hash(&s.manifest,r.web_manifest_root)==0);assert(qrx_net_site_verify(&s,&r,k,100)==0);s.signature[0]^=1;assert(qrx_net_site_verify(&s,&r,k,100)!=0);qrx_net_published_site_free(&s);EVP_PKEY_free(k);qrx_storage_fs_close(fs);return 0;}
