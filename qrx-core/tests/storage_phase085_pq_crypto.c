#include "storage/qrx_drive_crypto.h"
#include <assert.h>
#include <openssl/crypto.h>
#include <string.h>
#include <stdlib.h>
int main(void){EVP_PKEY *a=NULL,*b=NULL;assert(qrx_drive_generate_recipient_key(&a)==0);assert(qrx_drive_generate_recipient_key(&b)==0);uint8_t fk[32],out[32];assert(qrx_drive_random_file_key(fk)==0);QrxDriveKeyEnvelope e;assert(qrx_drive_wrap_file_key(a,fk,&e)==0);assert(qrx_drive_unwrap_file_key(a,&e,out)==0);assert(CRYPTO_memcmp(fk,out,32)==0);assert(qrx_drive_unwrap_file_key(b,&e,out)!=0);e.tag[0]^=1;assert(qrx_drive_unwrap_file_key(a,&e,out)!=0);e.tag[0]^=1;const uint8_t msg[]="QRX post-quantum encrypted storage object";const uint8_t aad[]="manifest:v1";QrxDriveCiphertext c;assert(qrx_drive_encrypt(fk,msg,sizeof(msg),aad,sizeof(aad),&c)==0);uint8_t *pt=NULL;size_t ptn=0;assert(qrx_drive_decrypt(fk,&c,aad,sizeof(aad),&pt,&ptn)==0);assert(ptn==sizeof(msg)&&memcmp(pt,msg,sizeof(msg))==0);OPENSSL_cleanse(pt,ptn);free(pt);c.tag[1]^=1;assert(qrx_drive_decrypt(fk,&c,aad,sizeof(aad),&pt,&ptn)!=0);qrx_drive_ciphertext_free(&c);qrx_drive_key_envelope_free(&e);EVP_PKEY_free(a);EVP_PKEY_free(b);OPENSSL_cleanse(fk,sizeof(fk));return 0;}
