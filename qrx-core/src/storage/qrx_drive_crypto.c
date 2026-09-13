#include "storage/qrx_drive_crypto.h"

#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <stdlib.h>
#include <string.h>

static int derive_kek(const uint8_t *secret,size_t secret_len,uint8_t out[32]){
    static const unsigned char info[]="QRX-DRIVE-KEY-WRAP-V1";
    EVP_KDF *kdf=EVP_KDF_fetch(NULL,"HKDF",NULL); EVP_KDF_CTX *ctx=NULL; int rc=-1;
    if(!kdf||!secret||!secret_len||!out)goto done; ctx=EVP_KDF_CTX_new(kdf); if(!ctx)goto done;
    char digest[]="SHA3-256";
    OSSL_PARAM params[4];
    params[0]=OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,digest,0);
    params[1]=OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,(void*)secret,secret_len);
    params[2]=OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,(void*)info,sizeof(info)-1);
    params[3]=OSSL_PARAM_construct_end();
    if(EVP_KDF_derive(ctx,out,32,params)==1)rc=0;
done: EVP_KDF_CTX_free(ctx); EVP_KDF_free(kdf); return rc;
}

int qrx_drive_generate_recipient_key(EVP_PKEY **out){
    if(!out)return -1; *out=NULL;
    EVP_PKEY_CTX *ctx=EVP_PKEY_CTX_new_from_name(NULL,QRX_DRIVE_KEM_NAME,NULL); if(!ctx)return -1;
    int rc=-1; if(EVP_PKEY_keygen_init(ctx)!=1)goto done; if(EVP_PKEY_generate(ctx,out)!=1)goto done; rc=0;
done: EVP_PKEY_CTX_free(ctx); if(rc!=0){EVP_PKEY_free(*out);*out=NULL;} return rc;
}
int qrx_drive_random_file_key(uint8_t out[32]){return out&&RAND_bytes(out,32)==1?0:-1;}

static int aes_gcm_encrypt(const uint8_t key[32],const uint8_t *pt,size_t ptlen,const uint8_t *aad,size_t aadlen,uint8_t nonce[12],uint8_t tag[16],uint8_t **ctout){
    if(!key||(!pt&&ptlen)||(!aad&&aadlen)||!nonce||!tag||!ctout||ptlen>INT_MAX||aadlen>INT_MAX)return -1; *ctout=NULL;
    if(RAND_bytes(nonce,12)!=1)return -1; uint8_t *ct=(uint8_t*)malloc(ptlen?ptlen:1); if(!ct)return -1;
    EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new(); int len=0,total=0,rc=-1; if(!ctx)goto done;
    if(EVP_EncryptInit_ex(ctx,EVP_aes_256_gcm(),NULL,NULL,NULL)!=1||EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_SET_IVLEN,12,NULL)!=1||EVP_EncryptInit_ex(ctx,NULL,NULL,key,nonce)!=1)goto done;
    if(aadlen&&EVP_EncryptUpdate(ctx,NULL,&len,aad,(int)aadlen)!=1)goto done;
    if(ptlen&&EVP_EncryptUpdate(ctx,ct,&len,pt,(int)ptlen)!=1)goto done; total=len;
    if(EVP_EncryptFinal_ex(ctx,ct+total,&len)!=1)goto done; total+=len;
    if((size_t)total!=ptlen||EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_GET_TAG,16,tag)!=1)goto done;
    *ctout=ct; ct=NULL; rc=0;
done: EVP_CIPHER_CTX_free(ctx); free(ct); return rc;
}
static int aes_gcm_decrypt(const uint8_t key[32],const uint8_t *ct,size_t ctlen,const uint8_t *aad,size_t aadlen,const uint8_t nonce[12],const uint8_t tag[16],uint8_t **ptout){
    if(!key||(!ct&&ctlen)||(!aad&&aadlen)||!nonce||!tag||!ptout||ctlen>INT_MAX||aadlen>INT_MAX)return -1; *ptout=NULL;
    uint8_t *pt=(uint8_t*)malloc(ctlen?ctlen:1); if(!pt)return -1; EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new(); int len=0,total=0,rc=-1; if(!ctx)goto done;
    if(EVP_DecryptInit_ex(ctx,EVP_aes_256_gcm(),NULL,NULL,NULL)!=1||EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_SET_IVLEN,12,NULL)!=1||EVP_DecryptInit_ex(ctx,NULL,NULL,key,nonce)!=1)goto done;
    if(aadlen&&EVP_DecryptUpdate(ctx,NULL,&len,aad,(int)aadlen)!=1)goto done;
    if(ctlen&&EVP_DecryptUpdate(ctx,pt,&len,ct,(int)ctlen)!=1)goto done; total=len;
    if(EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_SET_TAG,16,(void*)tag)!=1)goto done;
    if(EVP_DecryptFinal_ex(ctx,pt+total,&len)!=1)goto done; total+=len; if((size_t)total!=ctlen)goto done;
    *ptout=pt;pt=NULL;rc=0;
done: EVP_CIPHER_CTX_free(ctx); if(pt){OPENSSL_cleanse(pt,ctlen);free(pt);} return rc;
}

int qrx_drive_encrypt(const uint8_t key[32],const uint8_t *plaintext,size_t plaintext_len,const uint8_t *aad,size_t aad_len,QrxDriveCiphertext *out){
    if(!out)return -1; memset(out,0,sizeof(*out)); if(aes_gcm_encrypt(key,plaintext,plaintext_len,aad,aad_len,out->nonce,out->tag,&out->ciphertext)!=0)return -1; out->ciphertext_len=plaintext_len; return 0;
}
int qrx_drive_decrypt(const uint8_t key[32],const QrxDriveCiphertext *c,const uint8_t *aad,size_t aad_len,uint8_t **pt,size_t *ptlen){
    if(!c||!pt||!ptlen)return -1; if(aes_gcm_decrypt(key,c->ciphertext,c->ciphertext_len,aad,aad_len,c->nonce,c->tag,pt)!=0)return -1; *ptlen=c->ciphertext_len; return 0;
}
void qrx_drive_ciphertext_free(QrxDriveCiphertext *v){if(!v)return; if(v->ciphertext){OPENSSL_cleanse(v->ciphertext,v->ciphertext_len);free(v->ciphertext);}memset(v,0,sizeof(*v));}

int qrx_drive_wrap_file_key(EVP_PKEY *recipient_public,const uint8_t file_key[32],QrxDriveKeyEnvelope *out){
    if(!recipient_public||!file_key||!out)return -1; memset(out,0,sizeof(*out)); EVP_PKEY_CTX *ctx=EVP_PKEY_CTX_new_from_pkey(NULL,recipient_public,NULL); if(!ctx)return -1;
    uint8_t *kemct=NULL,*secret=NULL,*wrapped=NULL; size_t kemctlen=0,secretlen=0; uint8_t kek[32]; int rc=-1;
    if(EVP_PKEY_encapsulate_init(ctx,NULL)!=1||EVP_PKEY_encapsulate(ctx,NULL,&kemctlen,NULL,&secretlen)!=1||!kemctlen||!secretlen)goto done;
    kemct=(uint8_t*)malloc(kemctlen);secret=(uint8_t*)malloc(secretlen);if(!kemct||!secret)goto done;
    if(EVP_PKEY_encapsulate(ctx,kemct,&kemctlen,secret,&secretlen)!=1||derive_kek(secret,secretlen,kek)!=0)goto done;
    if(aes_gcm_encrypt(kek,file_key,32,NULL,0,out->nonce,out->tag,&wrapped)!=0)goto done;
    memcpy(out->wrapped_file_key,wrapped,32); out->kem_ciphertext=kemct;out->kem_ciphertext_len=kemctlen;kemct=NULL;rc=0;
done: EVP_PKEY_CTX_free(ctx); if(secret){OPENSSL_cleanse(secret,secretlen);free(secret);}OPENSSL_cleanse(kek,sizeof(kek)); if(wrapped){OPENSSL_cleanse(wrapped,32);free(wrapped);}free(kemct); if(rc!=0)qrx_drive_key_envelope_free(out); return rc;
}
int qrx_drive_unwrap_file_key(EVP_PKEY *recipient_private,const QrxDriveKeyEnvelope *env,uint8_t file_key_out[32]){
    if(!recipient_private||!env||!env->kem_ciphertext||!env->kem_ciphertext_len||!file_key_out)return -1; EVP_PKEY_CTX *ctx=EVP_PKEY_CTX_new_from_pkey(NULL,recipient_private,NULL); if(!ctx)return -1;
    size_t secretlen=0;uint8_t *secret=NULL,*pt=NULL;uint8_t kek[32];int rc=-1;
    if(EVP_PKEY_decapsulate_init(ctx,NULL)!=1||EVP_PKEY_decapsulate(ctx,NULL,&secretlen,env->kem_ciphertext,env->kem_ciphertext_len)!=1||!secretlen)goto done;
    secret=(uint8_t*)malloc(secretlen);if(!secret)goto done; if(EVP_PKEY_decapsulate(ctx,secret,&secretlen,env->kem_ciphertext,env->kem_ciphertext_len)!=1||derive_kek(secret,secretlen,kek)!=0)goto done;
    if(aes_gcm_decrypt(kek,env->wrapped_file_key,32,NULL,0,env->nonce,env->tag,&pt)!=0)goto done; memcpy(file_key_out,pt,32);rc=0;
done:EVP_PKEY_CTX_free(ctx);if(secret){OPENSSL_cleanse(secret,secretlen);free(secret);}if(pt){OPENSSL_cleanse(pt,32);free(pt);}OPENSSL_cleanse(kek,sizeof(kek));return rc;
}
void qrx_drive_key_envelope_free(QrxDriveKeyEnvelope *env){if(!env)return;if(env->kem_ciphertext){OPENSSL_cleanse(env->kem_ciphertext,env->kem_ciphertext_len);free(env->kem_ciphertext);}OPENSSL_cleanse(env,sizeof(*env));}
