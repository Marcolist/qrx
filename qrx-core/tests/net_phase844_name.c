#include "net/qrx_net_name.h"
#include "storage/qrx_drive_manifest.h"
#include <assert.h>
#include <string.h>
int main(void){char n[254];assert(qrx_domain_normalize("LatteMining.QRX",n)==0&&!strcmp(n,"lattemining.qrx"));assert(qrx_domain_normalize("-bad.qrx",n)!=0);assert(qrx_domain_normalize("bad_.qrx",n)!=0);assert(qrx_domain_normalize("x.qrx",n)==0);QrxDomainPrice p;assert(qrx_domain_price("x.qrx",1000,0,&p)==0&&p.mode==QRX_DOMAIN_PRICE_AUCTION_REQUIRED);assert(qrx_domain_price("abc.qrx",1000,0,&p)==0&&p.annual_atoms==100000);assert(qrx_domain_price("normal.qrx",1000,25,&p)==0&&p.annual_atoms==1000&&p.reservation_bond_atoms==3000);QrxDomainFeeSplit f;assert(qrx_domain_fee_split(100000,&f)==0&&f.development_atoms==500&&f.registry_atoms==99500);EVP_PKEY *k=NULL;assert(qrx_drive_manifest_generate_signing_key(&k)==0);uint8_t c1[64],c2[64];assert(qrx_domain_pubkey_commitment(k,c1)==0&&qrx_domain_pubkey_commitment(k,c2)==0&&!memcmp(c1,c2,64));EVP_PKEY_free(k);return 0;}
