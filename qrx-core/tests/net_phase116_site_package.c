#include "net/qrx_net_site.h"
#include "net/qrx_net_registry.h"
#include "net/qrx_net_publisher.h"
#include "storage/qrx_drive_manifest.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void){
 char tmp[]="/tmp/qrxsite116-XXXXXX"; assert(mkdtemp(tmp));
 char fsdir[512],dbdir[512],pkg[512]; snprintf(fsdir,sizeof(fsdir),"%s/fs",tmp); snprintf(dbdir,sizeof(dbdir),"%s/db",tmp); snprintf(pkg,sizeof(pkg),"%s/site.qrxsite",tmp);
 QrxStorageFs *fs=NULL; assert(qrx_storage_fs_open(fsdir,16*1024*1024,0,&fs)==0);
 EVP_PKEY *k=NULL; assert(qrx_drive_manifest_generate_signing_key(&k)==0);
 const uint8_t a[]="<html>v1</html>",b[]="console.log('qrx')";
 QrxNetSiteFileInput in[]={{"index.html",a,sizeof(a)-1},{"js/app.js",b,sizeof(b)-1}};
 QrxNetPublishedSite s; assert(qrx_net_site_publish(fs,"Example.QRX",7,100,in,2,k,&s)==0);
 QrxDomainRecord r; memset(&r,0,sizeof(r)); strcpy(r.name,"example.qrx"); strcpy(r.owner,"alice"); r.expiry_height=1000;
 assert(qrx_domain_pubkey_commitment(k,r.publishing_key_commitment)==0); assert(qrx_net_site_manifest_hash(&s.manifest,r.web_manifest_root)==0);
 assert(qrx_net_site_verify_storage(fs,&s,&r,k,100)==0);
 assert(qrx_net_site_package_save(&s,pkg)==0);
 char catalog[512]; snprintf(catalog,sizeof(catalog),"%s/catalog",tmp); QrxNetSiteVersion vv; assert(qrx_net_publisher_archive(catalog,&s,&vv)==0); assert(vv.sequence==7); assert(qrx_net_publisher_archive(catalog,&s,NULL)!=0); QrxNetSiteVersion rb; assert(qrx_net_publisher_prepare_rollback(catalog,"EXAMPLE.QRX",7,&rb)==0); assert(memcmp(rb.manifest_root,r.web_manifest_root,64)==0);
 QrxNetPublishedSite l; assert(qrx_net_site_package_load(pkg,&l)==0); assert(qrx_net_site_verify_storage(fs,&l,&r,k,100)==0);
 /* Entry substitution is detected even when manifest+signature are untouched. */
 l.files[0].path[0]^=1; assert(qrx_net_site_verify(&l,&r,k,100)!=0); l.files[0].path[0]^=1;
 /* Provider/object tamper is detected by content hash + Merkle commitment. */
 unsigned char *d=NULL; size_t dn=0; assert(qrx_storage_fs_read(fs,l.files[0].object_id,&d,&dn)==0); d[0]^=1; assert(qrx_net_site_verify_file(&l.files[0],d,dn)!=0); free(d);
 qrx_net_published_site_free(&l); qrx_net_published_site_free(&s); EVP_PKEY_free(k); qrx_storage_fs_close(fs);
 /* Duplicate/case-normalized domain registration must fail at registry state. */
 QrxDB db; assert(qrxdb_init(&db,dbdir)==0); QrxDomainRecord rr;
 assert(qrx_net_registry_register(&db,"SameName.QRX","alice",NULL,NULL,NULL,10,1000,&rr)==0);
 assert(qrx_net_registry_register(&db,"samename.qrx","bob",NULL,NULL,NULL,11,1000,&rr)!=0);
 QrxDomainRecord got; assert(qrx_net_registry_get(&db,"SAMENAME.QRX",&got)==0); assert(strcmp(got.owner,"alice")==0);
 qrxdb_close(&db);
 return 0;
}
