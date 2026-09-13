#include "storage/qrx_drive_manifest.h"
#include "storage/qrx_drive_crypto.h"
#include <assert.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>
int main(void){QrxDriveManifest m;memset(&m,0,sizeof(m));m.version=1;strcpy(m.crypto_suite,QRX_DRIVE_CRYPTO_SUITE);strcpy(m.profile,"STANDARD");m.plaintext_bytes=1000;m.ciphertext_bytes=1016;m.data_shards=10;m.parity_shards=4;assert(RAND_bytes(m.ciphertext_root,64)==1);for(int i=0;i<14;i++)assert(RAND_bytes(m.shard_roots[i],64)==1);EVP_PKEY *k=NULL;assert(qrx_drive_manifest_generate_signing_key(&k)==0);uint8_t *sig=NULL;size_t slen=0;assert(qrx_drive_manifest_sign(k,&m,&sig,&slen)==0);assert(qrx_drive_manifest_verify(k,&m,sig,slen)==0);m.shard_roots[3][0]^=1;assert(qrx_drive_manifest_verify(k,&m,sig,slen)!=0);free(sig);EVP_PKEY_free(k);return 0;}
