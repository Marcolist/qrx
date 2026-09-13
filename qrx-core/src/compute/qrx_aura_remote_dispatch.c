#include "compute/qrx_aura_remote_dispatch.h"
#include "compute/qrx_aura_relay.h"
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#endif
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define qrx_close_socket closesocket
#define qrx_fileno _fileno
#define qrx_fsync _commit
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#define qrx_close_socket close
#define qrx_fileno fileno
#define qrx_fsync fsync
#endif

#ifdef _WIN32
static SRWLOCK g_remote_sequence_lock=SRWLOCK_INIT;
static void remote_sequence_lock(void){AcquireSRWLockExclusive(&g_remote_sequence_lock);}
static void remote_sequence_unlock(void){ReleaseSRWLockExclusive(&g_remote_sequence_lock);}
#else
static pthread_mutex_t g_remote_sequence_lock=PTHREAD_MUTEX_INITIALIZER;
static void remote_sequence_lock(void){pthread_mutex_lock(&g_remote_sequence_lock);}
static void remote_sequence_unlock(void){pthread_mutex_unlock(&g_remote_sequence_lock);}
#endif

#define REMOTE_FRAME_DOMAIN "QRX/AURA/REMOTE-FRAME/V1"
#define REMOTE_PAYLOAD_DOMAIN "QRX/AURA/REMOTE-PAYLOAD/V1"
#define REMOTE_LEASE_DOMAIN "QRX/AURA/REMOTE-LEASE/V1"
#define MODEL_BUNDLE_MAGIC "QRXMB38\0"
#define MODEL_BUNDLE_MAGIC_LEN 8u

static int is_hex64(const char *s) {
    if (!s || strlen(s) != 64) return 0;
    for (size_t i=0;i<64;i++) {
        char c=s[i];
        if (!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return 0;
    }
    return 1;
}
static int bounded_text(const char *s,size_t cap,int empty_ok) {
    if (!s || !memchr(s,'\0',cap)) return 0;
    if (!empty_ok && !s[0]) return 0;
    for (size_t i=0;s[i];i++) {
        unsigned char c=(unsigned char)s[i];
        if (c<0x20 || c>0x7e) return 0;
    }
    return 1;
}
static void hex32(const uint8_t h[32],char out[65]) {
    static const char x[]="0123456789abcdef";
    for (size_t i=0;i<32;i++) { out[i*2]=x[h[i]>>4];out[i*2+1]=x[h[i]&15]; }
    out[64]=0;
}
static int sha3_bytes(const char *domain,const void *p,size_t n,char out[65]) {
    if (!domain || (n && !p) || !out) return -1;
    EVP_MD_CTX *m=EVP_MD_CTX_new(); uint8_t h[32]; unsigned hn=0;
    if (!m || EVP_DigestInit_ex(m,EVP_sha3_256(),NULL)!=1 ||
        EVP_DigestUpdate(m,domain,strlen(domain))!=1 ||
        (n && EVP_DigestUpdate(m,p,n)!=1) ||
        EVP_DigestFinal_ex(m,h,&hn)!=1 || hn!=32) { if(m)EVP_MD_CTX_free(m); return -1; }
    EVP_MD_CTX_free(m); hex32(h,out); return 0;
}
static int sha3_raw(const char *domain,const void *p,size_t n,uint8_t out[32]) {
    if (!domain || (n && !p) || !out) return -1;
    EVP_MD_CTX *m=EVP_MD_CTX_new(); unsigned hn=0;
    if (!m || EVP_DigestInit_ex(m,EVP_sha3_256(),NULL)!=1 ||
        EVP_DigestUpdate(m,domain,strlen(domain))!=1 ||
        (n && EVP_DigestUpdate(m,p,n)!=1) ||
        EVP_DigestFinal_ex(m,out,&hn)!=1 || hn!=32) { if(m)EVP_MD_CTX_free(m); return -1; }
    EVP_MD_CTX_free(m); return 0;
}
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t be64(const uint8_t *p){uint64_t v=0;for(int i=0;i<8;i++)v=(v<<8)|p[i];return v;}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void put64(uint8_t *p,uint64_t v){for(int i=7;i>=0;i--){p[i]=(uint8_t)v;v>>=8;}}

typedef struct { uint8_t *p; size_t n,cap; } Buf;
static int bgrow(Buf*b,size_t add){if(!b||SIZE_MAX-b->n<add)return-1;size_t need=b->n+add;if(need<=b->cap)return 0;size_t c=b->cap?b->cap:512;while(c<need){if(c>SIZE_MAX/2){c=need;break;}c*=2;}uint8_t*q=realloc(b->p,c);if(!q)return-1;b->p=q;b->cap=c;return 0;}
static int bput(Buf*b,const void*p,size_t n){if(bgrow(b,n))return-1;if(n)memcpy(b->p+b->n,p,n);b->n+=n;return 0;}
static int b32(Buf*b,uint32_t v){uint8_t x[4];put32(x,v);return bput(b,x,4);}
static int b64(Buf*b,uint64_t v){uint8_t x[8];put64(x,v);return bput(b,x,8);}
static int bs(Buf*b,const char*s,size_t cap){if(!s||!memchr(s,'\0',cap))return-1;size_t n=strlen(s);if(n>UINT32_MAX)return-1;return b32(b,(uint32_t)n)||bput(b,s,n);}

typedef struct { const uint8_t *p; size_t n,o; } Rd;
static int rget(Rd*r,void*out,size_t n){if(!r||r->o>r->n||r->n-r->o<n)return-1;if(n)memcpy(out,r->p+r->o,n);r->o+=n;return 0;}
static int r32(Rd*r,uint32_t*out){uint8_t x[4];if(rget(r,x,4))return-1;*out=be32(x);return 0;}
static int r64(Rd*r,uint64_t*out){uint8_t x[8];if(rget(r,x,8))return-1;*out=be64(x);return 0;}
static int rs(Rd*r,char*out,size_t cap){uint32_t n=0;if(r32(r,&n)||n>=cap||r->o>r->n||r->n-r->o<n)return-1;memcpy(out,r->p+r->o,n);out[n]=0;r->o+=n;return 0;}

static int secure_aad(const QrxAuraRemoteFrame *f,uint8_t **out,size_t *outn){
    if(!f||!out||!outn) return -1;
    Buf b={0};
    if(b32(&b,f->version)||b32(&b,f->kind)||b32(&b,f->status)||bs(&b,f->signer_id,sizeof(f->signer_id))||b64(&b,f->sequence)||b64(&b,f->height)||
       bs(&b,f->admission_commitment,sizeof(f->admission_commitment))||bs(&b,f->provider_id,sizeof(f->provider_id))||bs(&b,f->pod_id,sizeof(f->pod_id))||bs(&b,f->lease_id,sizeof(f->lease_id))||
       b64(&b,f->lease_expires_height)||b32(&b,f->compute_threads)||b64(&b,f->network_egress_mbps)||b32(&b,f->node_id)||b64(&b,f->required_memory_bytes)||b64(&b,f->max_output_bytes)||
       b32(&b,f->fragment_index)||b32(&b,f->fragment_count)||bs(&b,f->runtime_id,sizeof(f->runtime_id))||bs(&b,f->model_id,sizeof(f->model_id))||bs(&b,f->model_version,sizeof(f->model_version))||
       bs(&b,f->model_commitment,sizeof(f->model_commitment))){free(b.p);return -1;}
    *out=b.p;*outn=b.n;return 0;
}
int qrx_aura_secure_session_from_secret(const void *secret,size_t secret_len,const char *requester_id,const char *provider_id,QrxAuraSecureSession *out){
    if(!secret||!secret_len||!requester_id||!provider_id||!out||!bounded_text(requester_id,QRX_GLOBE_PROVIDER_MAX+1,0)||!bounded_text(provider_id,QRX_GLOBE_PROVIDER_MAX+1,0))return -1;
    EVP_MD_CTX*m=EVP_MD_CTX_new();unsigned hn=0;static const char dom[]="QRX/AURA/SECURE-KEY/V1";if(!m)return -1;
    if(EVP_DigestInit_ex(m,EVP_sha3_256(),NULL)!=1||EVP_DigestUpdate(m,dom,sizeof(dom)-1)!=1||EVP_DigestUpdate(m,requester_id,strlen(requester_id))!=1||EVP_DigestUpdate(m,"\0",1)!=1||EVP_DigestUpdate(m,provider_id,strlen(provider_id))!=1||EVP_DigestUpdate(m,"\0",1)!=1||EVP_DigestUpdate(m,secret,secret_len)!=1||EVP_DigestFinal_ex(m,out->key,&hn)!=1||hn!=32){EVP_MD_CTX_free(m);OPENSSL_cleanse(out,sizeof(*out));return -1;}EVP_MD_CTX_free(m);
    char sid[65];Buf b={0};if(bs(&b,requester_id,QRX_GLOBE_PROVIDER_MAX+1)||bs(&b,provider_id,QRX_GLOBE_PROVIDER_MAX+1)||bput(&b,out->key,sizeof(out->key))||sha3_bytes("QRX/AURA/SECURE-SESSION-ID/V1",b.p,b.n,sid)){free(b.p);OPENSSL_cleanse(out,sizeof(*out));return -1;}free(b.p);out->version=QRX_AURA_SECURE_CHANNEL_VERSION;snprintf(out->session_id,sizeof(out->session_id),"%s",sid);return 0;
}
int qrx_aura_secure_session_x25519(EVP_PKEY *local_private,EVP_PKEY *remote_public,const char *requester_id,const char *provider_id,QrxAuraSecureSession *out){
    if(!local_private||!remote_public||!out) return -1;
    EVP_PKEY_CTX*c=EVP_PKEY_CTX_new(local_private,NULL);
    size_t n=0;
    uint8_t*secret=NULL;
    int rc=-1;
    if(!c||EVP_PKEY_derive_init(c)!=1||EVP_PKEY_derive_set_peer(c,remote_public)!=1||EVP_PKEY_derive(c,NULL,&n)!=1||!n||n>1024) goto done;
    secret=malloc(n);
    if(!secret) goto done;
    if(EVP_PKEY_derive(c,secret,&n)!=1) goto done;
    rc=qrx_aura_secure_session_from_secret(secret,n,requester_id,provider_id,out);
done:
    if(secret){OPENSSL_cleanse(secret,n);free(secret);}
    EVP_PKEY_CTX_free(c);
    return rc;
}

static int pq_hybrid_key_ok(EVP_PKEY *key){
    if(!key) return 0;
    return EVP_PKEY_is_a(key,QRX_AURA_PQ_HYBRID_KEM_NAME)==1;
}
int qrx_aura_pq_hybrid_kem_generate(EVP_PKEY **private_key_out){
    if(!private_key_out) return -1;
    *private_key_out=NULL;
    EVP_PKEY_CTX *ctx=EVP_PKEY_CTX_new_from_name(NULL,QRX_AURA_PQ_HYBRID_KEM_NAME,NULL);
    EVP_PKEY *key=NULL;
    if(!ctx||EVP_PKEY_keygen_init(ctx)!=1||EVP_PKEY_generate(ctx,&key)!=1||!pq_hybrid_key_ok(key)){
        EVP_PKEY_CTX_free(ctx);EVP_PKEY_free(key);return -2;
    }
    EVP_PKEY_CTX_free(ctx);*private_key_out=key;return 0;
}
int qrx_aura_pq_hybrid_encapsulate(EVP_PKEY *remote_public,const char *requester_id,const char *provider_id,
                                   uint8_t **ciphertext_out,size_t *ciphertext_len_out,QrxAuraSecureSession *session_out){
    if(!remote_public||!pq_hybrid_key_ok(remote_public)||!requester_id||!provider_id||!ciphertext_out||!ciphertext_len_out||!session_out) return -1;
    *ciphertext_out=NULL;*ciphertext_len_out=0;memset(session_out,0,sizeof(*session_out));
    EVP_PKEY_CTX *ctx=EVP_PKEY_CTX_new(remote_public,NULL);size_t ct_n=0,ss_n=0;uint8_t *ct=NULL,*ss=NULL;int rc=-2;
    if(!ctx||EVP_PKEY_encapsulate_init(ctx,NULL)!=1||EVP_PKEY_encapsulate(ctx,NULL,&ct_n,NULL,&ss_n)!=1||!ct_n||!ss_n||ct_n>8192||ss_n>1024) goto done;
    ct=malloc(ct_n);ss=malloc(ss_n);if(!ct||!ss){rc=-3;goto done;}
    if(EVP_PKEY_encapsulate(ctx,ct,&ct_n,ss,&ss_n)!=1) goto done;
    if(qrx_aura_secure_session_from_secret(ss,ss_n,requester_id,provider_id,session_out)) goto done;
    *ciphertext_out=ct;*ciphertext_len_out=ct_n;ct=NULL;rc=0;
done:
    if(ss){OPENSSL_cleanse(ss,ss_n);free(ss);}free(ct);EVP_PKEY_CTX_free(ctx);
    if(rc) OPENSSL_cleanse(session_out,sizeof(*session_out));
    return rc;
}
int qrx_aura_pq_hybrid_decapsulate(EVP_PKEY *local_private,const void *ciphertext,size_t ciphertext_len,
                                   const char *requester_id,const char *provider_id,QrxAuraSecureSession *session_out){
    if(!local_private||!pq_hybrid_key_ok(local_private)||!ciphertext||!ciphertext_len||ciphertext_len>8192||!requester_id||!provider_id||!session_out) return -1;
    memset(session_out,0,sizeof(*session_out));EVP_PKEY_CTX *ctx=EVP_PKEY_CTX_new(local_private,NULL);size_t ss_n=0;uint8_t *ss=NULL;int rc=-2;
    if(!ctx||EVP_PKEY_decapsulate_init(ctx,NULL)!=1||EVP_PKEY_decapsulate(ctx,NULL,&ss_n,ciphertext,ciphertext_len)!=1||!ss_n||ss_n>1024) goto done;
    ss=malloc(ss_n);if(!ss){rc=-3;goto done;}
    if(EVP_PKEY_decapsulate(ctx,ss,&ss_n,ciphertext,ciphertext_len)!=1) goto done;
    if(qrx_aura_secure_session_from_secret(ss,ss_n,requester_id,provider_id,session_out)) goto done;
    rc=0;
done:
    if(ss){OPENSSL_cleanse(ss,ss_n);free(ss);}EVP_PKEY_CTX_free(ctx);if(rc)OPENSSL_cleanse(session_out,sizeof(*session_out));return rc;
}
void qrx_aura_pq_session_cache_init(QrxAuraPqSessionCache *cache){if(cache)memset(cache,0,sizeof(*cache));}
static long pq_session_find(QrxAuraPqSessionCache *cache,const char *local_id,const char *remote_id){if(!cache||!local_id||!remote_id)return -1;for(uint32_t i=0;i<cache->count;i++)if(!strcmp(cache->entries[i].local_id,local_id)&&!strcmp(cache->entries[i].remote_id,remote_id))return (long)i;return -1;}
int qrx_aura_pq_session_cache_put(QrxAuraPqSessionCache *cache,const char *local_id,const char *remote_id,const QrxAuraSecureSession *session,uint64_t created_height,uint64_t expires_height){
    if(!cache||!bounded_text(local_id,QRX_GLOBE_PROVIDER_MAX+1,0)||!bounded_text(remote_id,QRX_GLOBE_PROVIDER_MAX+1,0)||!session||session->version!=QRX_AURA_SECURE_CHANNEL_VERSION||!is_hex64(session->session_id)||!created_height||expires_height<created_height)return -1;
    long idx=pq_session_find(cache,local_id,remote_id);if(idx<0){if(cache->count>=QRX_AURA_PQ_SESSION_CACHE_MAX)return -2;idx=(long)cache->count++;}
    QrxAuraPqSessionEntry *e=&cache->entries[idx];OPENSSL_cleanse(e,sizeof(*e));snprintf(e->local_id,sizeof(e->local_id),"%s",local_id);snprintf(e->remote_id,sizeof(e->remote_id),"%s",remote_id);e->session=*session;e->created_height=created_height;e->expires_height=expires_height;cache->revision++;return 0;
}
size_t qrx_aura_pq_session_cache_prune(QrxAuraPqSessionCache *cache,uint64_t current_height){if(!cache)return 0;size_t w=0,n=0;for(uint32_t i=0;i<cache->count;i++){QrxAuraPqSessionEntry *e=&cache->entries[i];if(e->expires_height<current_height){OPENSSL_cleanse(e,sizeof(*e));n++;continue;}if(w!=i)cache->entries[w]=cache->entries[i];w++;}for(size_t i=w;i<cache->count;i++)OPENSSL_cleanse(&cache->entries[i],sizeof(cache->entries[i]));cache->count=(uint32_t)w;if(n)cache->revision++;return n;}
int qrx_aura_pq_session_cache_get(QrxAuraPqSessionCache *cache,const char *local_id,const char *remote_id,uint64_t current_height,QrxAuraSecureSession *session_out){if(!cache||!session_out)return -1;qrx_aura_pq_session_cache_prune(cache,current_height);long idx=pq_session_find(cache,local_id,remote_id);if(idx<0)return -2;*session_out=cache->entries[idx].session;return 0;}
int qrx_aura_secure_payload_is_envelope(const void *payload,size_t payload_len){return payload&&payload_len>=8&&memcmp(payload,QRX_AURA_SECURE_ENVELOPE_MAGIC,8)==0;}
int qrx_aura_secure_payload_seal(const QrxAuraSecureSession *session,const QrxAuraRemoteFrame *frame,const void *plaintext,size_t plaintext_len,uint8_t **sealed_out,size_t *sealed_len_out){
    const size_t overhead=8+4+64+QRX_AURA_SECURE_NONCE_BYTES+8+QRX_AURA_SECURE_TAG_BYTES;if(!session||session->version!=QRX_AURA_SECURE_CHANNEL_VERSION||!is_hex64(session->session_id)||!frame||(plaintext_len&&!plaintext)||!sealed_out||!sealed_len_out||plaintext_len>QRX_AURA_REMOTE_MAX_PAYLOAD-overhead)return -1;
    uint8_t*aad=NULL;size_t aadn=0;if(secure_aad(frame,&aad,&aadn))return -1;uint8_t nonce[QRX_AURA_SECURE_NONCE_BYTES];if(RAND_bytes(nonce,sizeof(nonce))!=1){free(aad);return -1;}uint8_t*out=malloc(overhead+plaintext_len);if(!out){free(aad);return -1;}memcpy(out,QRX_AURA_SECURE_ENVELOPE_MAGIC,8);put32(out+8,QRX_AURA_SECURE_CHANNEL_VERSION);memcpy(out+12,session->session_id,64);memcpy(out+76,nonce,sizeof(nonce));put64(out+88,(uint64_t)plaintext_len);uint8_t*cipher=out+96;uint8_t*tag=cipher+plaintext_len;
    EVP_CIPHER_CTX*c=EVP_CIPHER_CTX_new();int len=0,total=0,ok=c!=NULL;if(ok&&EVP_EncryptInit_ex(c,EVP_aes_256_gcm(),NULL,NULL,NULL)!=1)ok=0;if(ok&&EVP_CIPHER_CTX_ctrl(c,EVP_CTRL_GCM_SET_IVLEN,sizeof(nonce),NULL)!=1)ok=0;if(ok&&EVP_EncryptInit_ex(c,NULL,NULL,session->key,nonce)!=1)ok=0;if(ok&&aadn&&EVP_EncryptUpdate(c,NULL,&len,aad,(int)aadn)!=1)ok=0;if(ok&&plaintext_len&&EVP_EncryptUpdate(c,cipher,&len,plaintext,(int)plaintext_len)!=1)ok=0;if(ok)total=len;if(ok&&EVP_EncryptFinal_ex(c,cipher+total,&len)!=1)ok=0;if(ok)total+=len;if(ok&&(size_t)total!=plaintext_len)ok=0;if(ok&&EVP_CIPHER_CTX_ctrl(c,EVP_CTRL_GCM_GET_TAG,QRX_AURA_SECURE_TAG_BYTES,tag)!=1)ok=0;EVP_CIPHER_CTX_free(c);free(aad);OPENSSL_cleanse(nonce,sizeof(nonce));if(!ok){OPENSSL_cleanse(out,overhead+plaintext_len);free(out);return -2;}*sealed_out=out;*sealed_len_out=overhead+plaintext_len;return 0;
}
int qrx_aura_secure_payload_open(const QrxAuraSecureSession *session,const QrxAuraRemoteFrame *frame,const void *sealed,size_t sealed_len,uint8_t **plaintext_out,size_t *plaintext_len_out){
    const size_t overhead=8+4+64+QRX_AURA_SECURE_NONCE_BYTES+8+QRX_AURA_SECURE_TAG_BYTES;if(!session||session->version!=QRX_AURA_SECURE_CHANNEL_VERSION||!is_hex64(session->session_id)||!frame||!sealed||sealed_len<overhead||!plaintext_out||!plaintext_len_out)return -1;const uint8_t*in=sealed;if(memcmp(in,QRX_AURA_SECURE_ENVELOPE_MAGIC,8)||be32(in+8)!=QRX_AURA_SECURE_CHANNEL_VERSION||memcmp(in+12,session->session_id,64))return -2;uint64_t pn=be64(in+88);if(pn>SIZE_MAX||pn+overhead!=sealed_len)return -2;uint8_t*aad=NULL;size_t aadn=0;if(secure_aad(frame,&aad,&aadn))return -2;uint8_t nonce[QRX_AURA_SECURE_NONCE_BYTES];memcpy(nonce,in+76,sizeof(nonce));uint8_t*out=malloc((size_t)pn? (size_t)pn:1);if(!out){free(aad);return -1;}const uint8_t*cipher=in+96;const uint8_t*tag=cipher+(size_t)pn;EVP_CIPHER_CTX*c=EVP_CIPHER_CTX_new();int len=0,total=0,ok=c!=NULL;if(ok&&EVP_DecryptInit_ex(c,EVP_aes_256_gcm(),NULL,NULL,NULL)!=1)ok=0;if(ok&&EVP_CIPHER_CTX_ctrl(c,EVP_CTRL_GCM_SET_IVLEN,sizeof(nonce),NULL)!=1)ok=0;if(ok&&EVP_DecryptInit_ex(c,NULL,NULL,session->key,nonce)!=1)ok=0;if(ok&&aadn&&EVP_DecryptUpdate(c,NULL,&len,aad,(int)aadn)!=1)ok=0;if(ok&&pn&&EVP_DecryptUpdate(c,out,&len,cipher,(int)pn)!=1)ok=0;if(ok)total=len;if(ok&&EVP_CIPHER_CTX_ctrl(c,EVP_CTRL_GCM_SET_TAG,QRX_AURA_SECURE_TAG_BYTES,(void*)tag)!=1)ok=0;if(ok&&EVP_DecryptFinal_ex(c,out+total,&len)!=1)ok=0;if(ok)total+=len;if(ok&&(uint64_t)total!=pn)ok=0;EVP_CIPHER_CTX_free(c);free(aad);OPENSSL_cleanse(nonce,sizeof(nonce));if(!ok){OPENSSL_cleanse(out,(size_t)pn);free(out);return -4;}*plaintext_out=out;*plaintext_len_out=(size_t)pn;return 0;
}

static int frame_basic_validate(const QrxAuraRemoteFrame *f) {
    if (!f || f->version!=QRX_AURA_REMOTE_VERSION || f->kind<QRX_AURA_REMOTE_KIND_LEASE_OPEN || f->kind>QRX_AURA_REMOTE_KIND_SESSION_OPEN) return -1;
    if (f->status>QRX_AURA_REMOTE_STATUS_UNSUPPORTED || f->payload_len>QRX_AURA_REMOTE_MAX_PAYLOAD || (f->payload_len&&!f->payload)) return -1;
    if (!bounded_text(f->signer_id,sizeof(f->signer_id),0) || !bounded_text(f->provider_id,sizeof(f->provider_id),0) || !bounded_text(f->pod_id,sizeof(f->pod_id),0)) return -1;
    if (!is_hex64(f->admission_commitment) || !is_hex64(f->lease_id) || !is_hex64(f->model_commitment) || !is_hex64(f->payload_commitment)) return -1;
    if (!bounded_text(f->runtime_id,sizeof(f->runtime_id),1) || !bounded_text(f->model_id,sizeof(f->model_id),1) || !bounded_text(f->model_version,sizeof(f->model_version),1)) return -1;
    if (!f->sequence || !f->height) return -1;
    if (f->fragment_count && f->fragment_index>=f->fragment_count) return -1;
    return 0;
}
static int frame_canonical(const QrxAuraRemoteFrame *f,uint8_t **out,size_t *outn) {
    if (!f||!out||!outn) return -1;
    char pc[65];
    if (sha3_bytes(REMOTE_PAYLOAD_DOMAIN,f->payload,f->payload_len,pc) || strcmp(pc,f->payload_commitment)) return -2;
    if (frame_basic_validate(f)) return -3;
    Buf b={0};
    if (b32(&b,f->version)||b32(&b,f->kind)||b32(&b,f->status)||
        bs(&b,f->signer_id,sizeof(f->signer_id))||b64(&b,f->sequence)||b64(&b,f->height)||
        bs(&b,f->admission_commitment,sizeof(f->admission_commitment))||bs(&b,f->provider_id,sizeof(f->provider_id))||bs(&b,f->pod_id,sizeof(f->pod_id))||bs(&b,f->lease_id,sizeof(f->lease_id))||
        b64(&b,f->lease_expires_height)||b32(&b,f->compute_threads)||b64(&b,f->network_egress_mbps)||b32(&b,f->node_id)||b64(&b,f->required_memory_bytes)||b64(&b,f->max_output_bytes)||
        b32(&b,f->fragment_index)||b32(&b,f->fragment_count)||bs(&b,f->runtime_id,sizeof(f->runtime_id))||bs(&b,f->model_id,sizeof(f->model_id))||bs(&b,f->model_version,sizeof(f->model_version))||
        bs(&b,f->model_commitment,sizeof(f->model_commitment))||bs(&b,f->payload_commitment,sizeof(f->payload_commitment))||b64(&b,(uint64_t)f->payload_len)||bput(&b,f->payload,f->payload_len)) { free(b.p); return -4; }
    *out=b.p;*outn=b.n;return 0;
}
void qrx_aura_remote_frame_free(QrxAuraRemoteFrame *f){if(!f)return;free(f->payload);memset(f,0,sizeof(*f));}
int qrx_aura_remote_frame_commitment(const QrxAuraRemoteFrame*f,char out[65]){uint8_t*b=NULL;size_t n=0;if(frame_canonical(f,&b,&n))return-1;int rc=sha3_bytes(REMOTE_FRAME_DOMAIN,b,n,out);free(b);return rc;}
int qrx_aura_remote_frame_sign(EVP_PKEY*k,const QrxAuraRemoteFrame*f,uint8_t**sig,size_t*sn){if(!k||!f||!sig||!sn)return-1;char h[65];if(qrx_aura_remote_frame_commitment(f,h))return-2;EVP_MD_CTX*m=EVP_MD_CTX_new();if(!m)return-2;size_t n=0;if(EVP_DigestSignInit(m,NULL,NULL,NULL,k)!=1||EVP_DigestSign(m,NULL,&n,(const unsigned char*)h,64)!=1||!n||n>QRX_AURA_REMOTE_MAX_SIGNATURE){EVP_MD_CTX_free(m);return-2;}uint8_t*s=malloc(n);if(!s){EVP_MD_CTX_free(m);return-2;}if(EVP_DigestSign(m,s,&n,(const unsigned char*)h,64)!=1){free(s);EVP_MD_CTX_free(m);return-2;}EVP_MD_CTX_free(m);*sig=s;*sn=n;return 0;}
int qrx_aura_remote_frame_verify(EVP_PKEY*k,const QrxAuraRemoteFrame*f,const uint8_t*sig,size_t sn){if(!k||!f||!sig||!sn||sn>QRX_AURA_REMOTE_MAX_SIGNATURE)return-1;char h[65];if(qrx_aura_remote_frame_commitment(f,h))return-2;EVP_MD_CTX*m=EVP_MD_CTX_new();if(!m)return-2;int rc=EVP_DigestVerifyInit(m,NULL,NULL,NULL,k)==1&&EVP_DigestVerify(m,sig,sn,(const unsigned char*)h,64)==1?0:-3;EVP_MD_CTX_free(m);return rc;}
int qrx_aura_remote_wire_encode(const QrxAuraRemoteFrame*f,const uint8_t*sig,size_t sn,uint8_t**out,size_t*outn){if(!f||!sig||!sn||sn>QRX_AURA_REMOTE_MAX_SIGNATURE||!out||!outn)return-1;uint8_t*c=NULL;size_t cn=0;if(frame_canonical(f,&c,&cn)||cn>UINT32_MAX){free(c);return-2;}size_t total=8+4+cn+4+sn;if(total>QRX_AURA_REMOTE_MAX_PAYLOAD+65536u){free(c);return-2;}uint8_t*w=malloc(total);if(!w){free(c);return-2;}memcpy(w,QRX_AURA_REMOTE_IO_MAGIC,8);put32(w+8,(uint32_t)cn);memcpy(w+12,c,cn);put32(w+12+cn,(uint32_t)sn);memcpy(w+16+cn,sig,sn);free(c);*out=w;*outn=total;return 0;}
int qrx_aura_remote_wire_decode(const uint8_t*in,size_t n,QrxAuraRemoteFrame*f,uint8_t**sig,size_t*sn){if(!in||!f||!sig||!sn||n<16||memcmp(in,QRX_AURA_REMOTE_IO_MAGIC,8))return-1;uint32_t cn=be32(in+8);if((size_t)cn+16>n)return-2;uint32_t sl=be32(in+12+cn);if(!sl||sl>QRX_AURA_REMOTE_MAX_SIGNATURE||16u+(size_t)cn+(size_t)sl!=n)return-2;memset(f,0,sizeof(*f));Rd r={in+12,cn,0};uint64_t pn=0;if(r32(&r,&f->version)||r32(&r,&f->kind)||r32(&r,&f->status)||rs(&r,f->signer_id,sizeof(f->signer_id))||r64(&r,&f->sequence)||r64(&r,&f->height)||rs(&r,f->admission_commitment,sizeof(f->admission_commitment))||rs(&r,f->provider_id,sizeof(f->provider_id))||rs(&r,f->pod_id,sizeof(f->pod_id))||rs(&r,f->lease_id,sizeof(f->lease_id))||r64(&r,&f->lease_expires_height)||r32(&r,&f->compute_threads)||r64(&r,&f->network_egress_mbps)||r32(&r,&f->node_id)||r64(&r,&f->required_memory_bytes)||r64(&r,&f->max_output_bytes)||r32(&r,&f->fragment_index)||r32(&r,&f->fragment_count)||rs(&r,f->runtime_id,sizeof(f->runtime_id))||rs(&r,f->model_id,sizeof(f->model_id))||rs(&r,f->model_version,sizeof(f->model_version))||rs(&r,f->model_commitment,sizeof(f->model_commitment))||rs(&r,f->payload_commitment,sizeof(f->payload_commitment))||r64(&r,&pn)||pn>QRX_AURA_REMOTE_MAX_PAYLOAD||pn>SIZE_MAX||r.o>r.n||r.n-r.o!=(size_t)pn){qrx_aura_remote_frame_free(f);return-3;}if(pn){f->payload=malloc((size_t)pn);if(!f->payload){qrx_aura_remote_frame_free(f);return-4;}memcpy(f->payload,r.p+r.o,(size_t)pn);}f->payload_len=(size_t)pn;if(frame_basic_validate(f)){qrx_aura_remote_frame_free(f);return-5;}char pc[65];if(sha3_bytes(REMOTE_PAYLOAD_DOMAIN,f->payload,f->payload_len,pc)||strcmp(pc,f->payload_commitment)){qrx_aura_remote_frame_free(f);return-5;}uint8_t*s=malloc(sl);if(!s){qrx_aura_remote_frame_free(f);return-4;}memcpy(s,in+16+cn,sl);*sig=s;*sn=sl;return 0;}

static int send_all(int fd,const void*p,size_t n){const uint8_t*b=p;while(n){int w=(int)send(fd,(const char*)b,(int)(n>INT_MAX?INT_MAX:n),0);if(w<=0)return-1;b+=w;n-=(size_t)w;}return 0;}
static int recv_all(int fd,void*p,size_t n){uint8_t*b=p;while(n){int r=(int)recv(fd,(char*)b,(int)(n>INT_MAX?INT_MAX:n),0);if(r<=0)return-1;b+=r;n-=(size_t)r;}return 0;}
int qrx_aura_remote_send_fd(int fd,const QrxAuraRemoteFrame*f,EVP_PKEY*k){if(fd<0||!f||!k)return-1;uint8_t*sig=NULL,*w=NULL;size_t sn=0,wn=0;if(qrx_aura_remote_frame_sign(k,f,&sig,&sn)||qrx_aura_remote_wire_encode(f,sig,sn,&w,&wn)){free(sig);free(w);return-1;}free(sig);if(wn>UINT32_MAX){free(w);return-1;}uint8_t h[4];put32(h,(uint32_t)wn);int rc=send_all(fd,h,4)||send_all(fd,w,wn)?-1:0;free(w);return rc;}
int qrx_aura_remote_recv_fd(int fd,QrxAuraRemoteFrame*f,uint8_t**sig,size_t*sn){if(fd<0||!f||!sig||!sn)return-1;uint8_t h[4];if(recv_all(fd,h,4))return-1;uint32_t n=be32(h);if(n<16||n>QRX_AURA_REMOTE_MAX_PAYLOAD+65536u)return-1;uint8_t*w=malloc(n);if(!w)return-1;if(recv_all(fd,w,n)){free(w);return-1;}int rc=qrx_aura_remote_wire_decode(w,n,f,sig,sn);free(w);return rc;}

void qrx_aura_reservation_leases_init(QrxAuraReservationLeaseTable*t){if(t)memset(t,0,sizeof(*t));}
const QrxAuraReservationLease*qrx_aura_reservation_lease_find(const QrxAuraReservationLeaseTable*t,const char*id){if(!t||!id)return NULL;for(uint32_t i=0;i<t->count;i++)if(!strcmp(t->entries[i].lease_id,id))return &t->entries[i];return NULL;}
static QrxAuraReservationLease*lease_mut(QrxAuraReservationLeaseTable*t,const char*id){if(!t||!id)return NULL;for(uint32_t i=0;i<t->count;i++)if(!strcmp(t->entries[i].lease_id,id))return &t->entries[i];return NULL;}

#define LEASE_JOURNAL_MAGIC "QRXALJ39"
#define LEASE_JOURNAL_DOMAIN "QRX/AURA/LEASE-JOURNAL/V1"
#define LEASE_JOURNAL_VERSION 1u

static int lease_entry_encode(Buf*b,const QrxAuraReservationLease*e){
    if(!b||!e||e->version!=QRX_AURA_REMOTE_VERSION||e->state<QRX_AURA_LEASE_ACTIVE||e->state>QRX_AURA_LEASE_EXPIRED||!is_hex64(e->lease_id)||!is_hex64(e->admission_commitment)||!is_hex64(e->model_commitment))return -1;
    return b32(b,e->version)||b32(b,e->state)||bs(b,e->lease_id,sizeof(e->lease_id))||bs(b,e->requester_id,sizeof(e->requester_id))||bs(b,e->admission_commitment,sizeof(e->admission_commitment))||bs(b,e->provider_id,sizeof(e->provider_id))||bs(b,e->pod_id,sizeof(e->pod_id))||bs(b,e->model_commitment,sizeof(e->model_commitment))||b32(b,e->node_id)||b64(b,e->opened_height)||b64(b,e->expires_height)||b64(b,e->last_sequence)||b32(b,e->compute_threads)||b64(b,e->network_egress_mbps)||b64(b,e->required_memory_bytes)||b64(b,e->max_output_bytes)||b32(b,e->fragment_index)||b32(b,e->fragment_count)||bs(b,e->runtime_id,sizeof(e->runtime_id))||bs(b,e->model_id,sizeof(e->model_id))||bs(b,e->model_version,sizeof(e->model_version));
}
static int lease_entry_decode(Rd*r,QrxAuraReservationLease*e){
    if(!r||!e) return -1;
    memset(e,0,sizeof(*e));
    if(r32(r,&e->version)||r32(r,&e->state)||rs(r,e->lease_id,sizeof(e->lease_id))||rs(r,e->requester_id,sizeof(e->requester_id))||rs(r,e->admission_commitment,sizeof(e->admission_commitment))||rs(r,e->provider_id,sizeof(e->provider_id))||rs(r,e->pod_id,sizeof(e->pod_id))||rs(r,e->model_commitment,sizeof(e->model_commitment))||r32(r,&e->node_id)||r64(r,&e->opened_height)||r64(r,&e->expires_height)||r64(r,&e->last_sequence)||r32(r,&e->compute_threads)||r64(r,&e->network_egress_mbps)||r64(r,&e->required_memory_bytes)||r64(r,&e->max_output_bytes)||r32(r,&e->fragment_index)||r32(r,&e->fragment_count)||rs(r,e->runtime_id,sizeof(e->runtime_id))||rs(r,e->model_id,sizeof(e->model_id))||rs(r,e->model_version,sizeof(e->model_version)))return -1;
    if(e->version!=QRX_AURA_REMOTE_VERSION||e->state<QRX_AURA_LEASE_ACTIVE||e->state>QRX_AURA_LEASE_EXPIRED||!is_hex64(e->lease_id)||!is_hex64(e->admission_commitment)||!is_hex64(e->model_commitment)||!e->requester_id[0]||!e->provider_id[0]||!e->pod_id[0]||!e->node_id||!e->compute_threads||!e->fragment_count||e->fragment_index>=e->fragment_count||!e->runtime_id[0]||!e->model_id[0]||!e->model_version[0])return -1;
    return 0;
}
static int lease_table_payload(const QrxAuraReservationLeaseTable*t,uint8_t**out,size_t*outn){
    if(!t||!out||!outn||t->count>QRX_AURA_REMOTE_MAX_LEASES) return -1;
    Buf b={0};
    if(b32(&b,LEASE_JOURNAL_VERSION)||b64(&b,t->revision)||b32(&b,t->count)){free(b.p);return -1;}
    for(uint32_t i=0;i<t->count;i++) if(lease_entry_encode(&b,&t->entries[i])){free(b.p);return -1;}
    *out=b.p;
    *outn=b.n;
    return 0;
}
static int lease_table_parse(const uint8_t*p,size_t n,QrxAuraReservationLeaseTable*t){
    if(!p||!t) return -1;
    Rd r={p,n,0};
    uint32_t v=0,count=0;
    uint64_t rev=0;
    if(r32(&r,&v)||r64(&r,&rev)||r32(&r,&count)||v!=LEASE_JOURNAL_VERSION||count>QRX_AURA_REMOTE_MAX_LEASES) return -1;
    QrxAuraReservationLeaseTable tmp;
    memset(&tmp,0,sizeof(tmp));
    tmp.count=count;
    tmp.revision=rev;
    for(uint32_t i=0;i<count;i++) if(lease_entry_decode(&r,&tmp.entries[i])) return -1;
    if(r.o!=r.n) return -1;
    *t=tmp;
    return 0;
}
static int lease_journal_atomic_replace(const char*src,const char*dst){
#ifdef _WIN32
    return MoveFileExA(src,dst,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?0:-1;
#else
    return rename(src,dst);
#endif
}
int qrx_aura_reservation_leases_journal_save(const char*path,const QrxAuraReservationLeaseTable*t){
    if(!path||!path[0]||!t) return -1;
    uint8_t*p=NULL;
    size_t n=0;
    if(lease_table_payload(t,&p,&n)||n>16u*1024u*1024u){free(p);return -1;}
    uint8_t h[32];
    if(sha3_raw(LEASE_JOURNAL_DOMAIN,p,n,h)){free(p);return -1;}
    char tmp[1400];
    if(snprintf(tmp,sizeof(tmp),"%s.tmp",path)>=(int)sizeof(tmp)){free(p);return -1;}
    FILE*f=fopen(tmp,"wb");
    if(!f){free(p);return -2;}
    uint8_t len[8];
    put64(len,(uint64_t)n);
    int ok=fwrite(LEASE_JOURNAL_MAGIC,1,8,f)==8&&
           fwrite(len,1,8,f)==8&&
           (!n||fwrite(p,1,n,f)==n)&&
           fwrite(h,1,32,f)==32&&fflush(f)==0;
    int fd=qrx_fileno(f);
    if(ok&&(fd<0||qrx_fsync(fd)!=0)) ok=0;
    if(fclose(f)!=0) ok=0;
    free(p);
    OPENSSL_cleanse(h,sizeof(h));
    if(!ok){remove(tmp);return -3;}
    if(lease_journal_atomic_replace(tmp,path)){remove(tmp);return -4;}
    return 0;
}
int qrx_aura_reservation_leases_journal_load(const char*path,QrxAuraReservationLeaseTable*t){
    if(!path||!path[0]||!t) return -1;
    FILE*f=fopen(path,"rb");
    if(!f) return errno==ENOENT?1:-2;
    uint8_t hdr[16],stored[32],calc[32];
    if(fread(hdr,1,sizeof(hdr),f)!=sizeof(hdr)||memcmp(hdr,LEASE_JOURNAL_MAGIC,8)){fclose(f);return -3;}
    uint64_t n=be64(hdr+8);
    if(n>16u*1024u*1024u||n>SIZE_MAX){fclose(f);return -3;}
    uint8_t*p=malloc((size_t)n?n:1);
    if(!p){fclose(f);return -4;}
    int rc=0;
    if((n&&fread(p,1,(size_t)n,f)!=(size_t)n)||fread(stored,1,32,f)!=32||fgetc(f)!=EOF) rc=-3;
    if(!rc&&(sha3_raw(LEASE_JOURNAL_DOMAIN,p,(size_t)n,calc)||CRYPTO_memcmp(stored,calc,32))) rc=-5;
    if(!rc&&lease_table_parse(p,(size_t)n,t)) rc=-6;
    OPENSSL_cleanse(calc,sizeof(calc));
    free(p);
    fclose(f);
    return rc;
}
static QrxAuraRuntimeBinding*lease_binding(QrxAuraRuntimeBinding*b,size_t n,const QrxAuraReservationLease*e){if(!b||!e)return NULL;for(size_t i=0;i<n;i++)if(!strcmp(b[i].provider_id,e->provider_id)&&!strcmp(b[i].pod_id,e->pod_id))return &b[i];return NULL;}
int qrx_aura_reservation_leases_journal_recover(const char*path,QrxAuraReservationLeaseTable*t,QrxAuraRuntimeBinding*b,size_t bn,uint64_t h,size_t*rec_out,size_t*exp_out){
    if(rec_out) *rec_out=0;
    if(exp_out) *exp_out=0;
    if(!path||!t||!b||!bn) return -1;
    QrxAuraReservationLeaseTable loaded;
    memset(&loaded,0,sizeof(loaded));
    int lr=qrx_aura_reservation_leases_journal_load(path,&loaded);
    if(lr==1){qrx_aura_reservation_leases_init(t);return 0;}
    if(lr) return -2;
    uint32_t recovered[QRX_AURA_REMOTE_MAX_LEASES];
    size_t rn=0,en=0;
    for(uint32_t i=0;i<loaded.count;i++){
        QrxAuraReservationLease*e=&loaded.entries[i];
        if(e->state!=QRX_AURA_LEASE_ACTIVE) continue;
        if(e->expires_height<h){e->state=QRX_AURA_LEASE_EXPIRED;loaded.revision++;en++;continue;}
        QrxAuraRuntimeBinding*rb=lease_binding(b,bn,e);
        if(!rb||!rb->provider_runtime||qrx_resource_provider_runtime_reserve(rb->provider_runtime,0,0,e->compute_threads,e->network_egress_mbps)){
            for(size_t j=0;j<rn;j++){
                QrxAuraReservationLease*x=&loaded.entries[recovered[j]];
                QrxAuraRuntimeBinding*rr=lease_binding(b,bn,x);
                if(rr&&rr->provider_runtime) qrx_resource_provider_runtime_release(rr->provider_runtime,0,0,x->compute_threads,x->network_egress_mbps);
            }
            return -3;
        }
        recovered[rn++]=i;
    }
    *t=loaded;
    if(en&&qrx_aura_reservation_leases_journal_save(path,t)){
        for(size_t j=0;j<rn;j++){
            QrxAuraReservationLease*x=&t->entries[recovered[j]];
            QrxAuraRuntimeBinding*rr=lease_binding(b,bn,x);
            if(rr&&rr->provider_runtime) qrx_resource_provider_runtime_release(rr->provider_runtime,0,0,x->compute_threads,x->network_egress_mbps);
        }
        memset(t,0,sizeof(*t));
        return -4;
    }
    if(rec_out) *rec_out=rn;
    if(exp_out) *exp_out=en;
    return 0;
}
static void active_totals(const QrxAuraReservationLeaseTable*t,const QrxAuraRuntimeBinding*b,uint64_t*threads,uint64_t*net){*threads=0;*net=0;if(!t||!b)return;for(uint32_t i=0;i<t->count;i++){const QrxAuraReservationLease*e=&t->entries[i];if(e->state==QRX_AURA_LEASE_ACTIVE&&!strcmp(e->provider_id,b->provider_id)&&!strcmp(e->pod_id,b->pod_id)){*threads+=e->compute_threads;*net+=e->network_egress_mbps;}}}
static int lease_table_restore_runtime(QrxAuraReservationLeaseTable*t,const QrxAuraReservationLeaseTable*before,QrxAuraRuntimeBinding*b){uint64_t ct=0,cn=0,bt=0,bn=0;active_totals(t,b,&ct,&cn);active_totals(before,b,&bt,&bn);if(ct>bt||cn>bn){uint32_t td=(uint32_t)(ct>bt?ct-bt:0);uint64_t nd=cn>bn?cn-bn:0;if(qrx_resource_provider_runtime_release(b->provider_runtime,0,0,td,nd))return -1;}if(bt>ct||bn>cn){uint32_t td=(uint32_t)(bt>ct?bt-ct:0);uint64_t nd=bn>cn?bn-cn:0;if(qrx_resource_provider_runtime_reserve(b->provider_runtime,0,0,td,nd))return -1;}*t=*before;return 0;}
int qrx_aura_reservation_leases_reap_durable(QrxAuraReservationLeaseTable*t,QrxAuraRuntimeBinding*b,size_t bn,uint64_t h,const char*path,size_t*out){if(out)*out=0;if(!t||!b||!bn||!path||!path[0])return -1;QrxAuraReservationLeaseTable*before=malloc(sizeof(*before));if(!before)return -1;*before=*t;size_t n=qrx_aura_reservation_leases_reap(t,b,bn,h);if(n&&qrx_aura_reservation_leases_journal_save(path,t)){int rr=0;for(size_t i=0;i<bn;i++)if(lease_table_restore_runtime(t,before,&b[i])){rr=-1;break;}free(before);return rr?-3:-2;}free(before);if(out)*out=n;return 0;}
static int lease_request_valid(const QrxAuraRemoteFrame*r,QrxAuraRuntimeBinding*b,uint64_t h){
    if(!r||!b||!b->provider_runtime||r->status||strcmp(r->provider_id,b->provider_id)||strcmp(r->pod_id,b->pod_id)||!is_hex64(r->admission_commitment)||!is_hex64(r->model_commitment)||!is_hex64(r->lease_id)||!r->compute_threads||!r->node_id||!r->fragment_count||r->fragment_index>=r->fragment_count||!bounded_text(r->runtime_id,sizeof(r->runtime_id),0)||!bounded_text(r->model_id,sizeof(r->model_id),0)||!bounded_text(r->model_version,sizeof(r->model_version),0)||r->required_memory_bytes>b->device.device_memory_bytes||r->max_output_bytes>QRX_AURA_REMOTE_MAX_PAYLOAD||r->lease_expires_height<=h||r->lease_expires_height-h>QRX_AURA_REMOTE_MAX_LEASE_BLOCKS||r->height>h||h-r->height>8) return -1;
    if(b->provider_runtime->status!=QRX_PROVIDER_RUNTIME_SERVING||strcmp(b->provider_runtime->provider_id,b->provider_id)||!(b->provider_runtime->active_plan.enabled_mask&QRX_PROVIDER_ENABLE_COMPUTE)) return -1;
    return 0;
}
static int lease_immutable_matches(const QrxAuraReservationLease*e,const QrxAuraRemoteFrame*r){
    if(!e||!r) return 0;
    return !strcmp(e->requester_id,r->signer_id)&&!strcmp(e->admission_commitment,r->admission_commitment)&&!strcmp(e->provider_id,r->provider_id)&&!strcmp(e->pod_id,r->pod_id)&&!strcmp(e->model_commitment,r->model_commitment)&&e->node_id==r->node_id&&e->compute_threads==r->compute_threads&&e->network_egress_mbps==r->network_egress_mbps&&e->required_memory_bytes==r->required_memory_bytes&&e->max_output_bytes==r->max_output_bytes&&e->fragment_index==r->fragment_index&&e->fragment_count==r->fragment_count&&!strcmp(e->runtime_id,r->runtime_id)&&!strcmp(e->model_id,r->model_id)&&!strcmp(e->model_version,r->model_version);
}
int qrx_aura_reservation_lease_open(QrxAuraReservationLeaseTable*t,QrxAuraRuntimeBinding*b,const QrxAuraRemoteFrame*r,uint64_t h,QrxAuraReservationLease*out){
    if(!t||!b||!r||r->kind!=QRX_AURA_REMOTE_KIND_LEASE_OPEN||lease_request_valid(r,b,h)) return -1;
    QrxAuraReservationLease*e=lease_mut(t,r->lease_id);
    if(e){
        if(e->state==QRX_AURA_LEASE_ACTIVE&&e->last_sequence==r->sequence&&e->expires_height==r->lease_expires_height&&lease_immutable_matches(e,r)){if(out)*out=*e;return 0;}
        return -2;
    }
    if(t->count>=QRX_AURA_REMOTE_MAX_LEASES) return -3;
    if(qrx_resource_provider_runtime_reserve(b->provider_runtime,0,0,r->compute_threads,r->network_egress_mbps)) return -4;
    e=&t->entries[t->count++];memset(e,0,sizeof(*e));
    e->version=QRX_AURA_REMOTE_VERSION;e->state=QRX_AURA_LEASE_ACTIVE;snprintf(e->lease_id,sizeof(e->lease_id),"%s",r->lease_id);snprintf(e->requester_id,sizeof(e->requester_id),"%s",r->signer_id);snprintf(e->admission_commitment,65,"%s",r->admission_commitment);snprintf(e->provider_id,sizeof(e->provider_id),"%s",r->provider_id);snprintf(e->pod_id,sizeof(e->pod_id),"%s",r->pod_id);snprintf(e->model_commitment,65,"%s",r->model_commitment);snprintf(e->runtime_id,sizeof(e->runtime_id),"%s",r->runtime_id);snprintf(e->model_id,sizeof(e->model_id),"%s",r->model_id);snprintf(e->model_version,sizeof(e->model_version),"%s",r->model_version);e->node_id=r->node_id;e->opened_height=h;e->expires_height=r->lease_expires_height;e->last_sequence=r->sequence;e->compute_threads=r->compute_threads;e->network_egress_mbps=r->network_egress_mbps;e->required_memory_bytes=r->required_memory_bytes;e->max_output_bytes=r->max_output_bytes;e->fragment_index=r->fragment_index;e->fragment_count=r->fragment_count;t->revision++;if(out)*out=*e;return 0;
}
int qrx_aura_reservation_lease_renew(QrxAuraReservationLeaseTable*t,const QrxAuraRemoteFrame*r,uint64_t h,QrxAuraReservationLease*out){
    if(!t||!r||r->kind!=QRX_AURA_REMOTE_KIND_LEASE_RENEW||r->height>h||h-r->height>8) return -1;
    QrxAuraReservationLease*e=lease_mut(t,r->lease_id);
    if(!e||e->state!=QRX_AURA_LEASE_ACTIVE||e->expires_height<h) return -2;
    if(!lease_immutable_matches(e,r)) return -3;
    if(r->sequence<=e->last_sequence) return -4;
    if(r->lease_expires_height<=e->expires_height||r->lease_expires_height<=h||r->lease_expires_height-h>QRX_AURA_REMOTE_MAX_LEASE_BLOCKS) return -5;
    e->expires_height=r->lease_expires_height;e->last_sequence=r->sequence;t->revision++;if(out)*out=*e;return 0;
}
int qrx_aura_reservation_lease_release(QrxAuraReservationLeaseTable*t,QrxAuraRuntimeBinding*b,const QrxAuraRemoteFrame*r){if(!t||!b||!b->provider_runtime||!r||r->kind!=QRX_AURA_REMOTE_KIND_LEASE_RELEASE)return-1;QrxAuraReservationLease*e=lease_mut(t,r->lease_id);if(!e)return-2;if(!lease_immutable_matches(e,r)||strcmp(e->provider_id,b->provider_id)||strcmp(e->pod_id,b->pod_id))return-3;if(e->state!=QRX_AURA_LEASE_ACTIVE)return e->last_sequence==r->sequence?0:-4;if(r->sequence<=e->last_sequence)return-4;int rc=qrx_resource_provider_runtime_release(b->provider_runtime,0,0,e->compute_threads,e->network_egress_mbps);if(rc)return-5;e->last_sequence=r->sequence;e->state=QRX_AURA_LEASE_RELEASED;t->revision++;return 0;}
size_t qrx_aura_reservation_leases_reap(QrxAuraReservationLeaseTable*t,QrxAuraRuntimeBinding*b,size_t bn,uint64_t h){if(!t||!b)return 0;size_t n=0;for(uint32_t i=0;i<t->count;i++){QrxAuraReservationLease*e=&t->entries[i];if(e->state!=QRX_AURA_LEASE_ACTIVE||e->expires_height>=h)continue;QrxAuraRuntimeBinding*rb=NULL;for(size_t j=0;j<bn;j++)if(!strcmp(b[j].provider_id,e->provider_id)&&!strcmp(b[j].pod_id,e->pod_id)){rb=&b[j];break;}if(!rb||!rb->provider_runtime)continue;if(!qrx_resource_provider_runtime_release(rb->provider_runtime,0,0,e->compute_threads,e->network_egress_mbps)){e->state=QRX_AURA_LEASE_EXPIRED;t->revision++;n++;}}return n;}

static int cache_ready(QrxAuraModelCacheCatalog*c,const char*pod,const char*id,const char*ver,const char*commit){
    const QrxAuraModelCacheEntry*e=qrx_aura_model_cache_find(c,pod,id,ver);
    return e&&e->verified&&!strcmp(e->model_commitment,commit);
}
static int cache_mark_ready(QrxAuraModelCacheCatalog*c,const QrxAuraRemoteFrame*r,uint64_t bytes,uint64_t h){
    if(!c||!r||!bytes) return -1;
    QrxAuraModelCacheEntry*e=NULL;
    for(uint32_t i=0;i<c->count;i++)if(!strcmp(c->entries[i].pod_id,r->pod_id)&&!strcmp(c->entries[i].model_id,r->model_id)&&!strcmp(c->entries[i].model_version,r->model_version)){e=&c->entries[i];break;}
    if(!e){if(c->count>=QRX_AURA_CACHE_MAX_ENTRIES)return -1;e=&c->entries[c->count++];memset(e,0,sizeof(*e));}
    e->version=QRX_AURA_LIVE_DISPATCH_VERSION;snprintf(e->pod_id,sizeof(e->pod_id),"%s",r->pod_id);snprintf(e->model_id,sizeof(e->model_id),"%s",r->model_id);snprintf(e->model_version,sizeof(e->model_version),"%s",r->model_version);snprintf(e->model_commitment,sizeof(e->model_commitment),"%s",r->model_commitment);e->bytes_present=bytes;e->verified_height=h;e->verified=1;c->revision++;return 0;
}
static int server_cache_ensure(QrxAuraRemoteServerContext*s,const QrxAuraRemoteFrame*r,uint64_t h){
    if(!s||!r||!s->model_cache)return -1;
    if(cache_ready(s->model_cache,r->pod_id,r->model_id,r->model_version,r->model_commitment))return 0;
    if(!s->model_registry||!s->model_fetch)return -2;
    const QrxAiModelRecord*m=qrx_ai_model_registry_find(s->model_registry,r->model_id,r->model_version);if(!m)return -3;char mc[65];if(qrx_ai_model_commitment(m,mc)||strcmp(mc,r->model_commitment)||strcmp(m->runtime_id,r->runtime_id))return -3;
    QrxAuraModelCachePlacement p;memset(&p,0,sizeof(p));snprintf(p.pod_id,sizeof(p.pod_id),"%s",r->pod_id);snprintf(p.provider_id,sizeof(p.provider_id),"%s",r->provider_id);p.bytes_to_place=m->is_moe?((m->min_storage_bytes+(r->fragment_count?r->fragment_count:1)-1)/(r->fragment_count?r->fragment_count:1)):m->min_storage_bytes;if(!p.bytes_to_place)p.bytes_to_place=1;
    char got[65]={0};if(s->model_fetch(s->model_fetch_ctx,m,&p,got)||strcmp(got,mc))return -4;return cache_mark_ready(s->model_cache,r,p.bytes_to_place,h);
}
static void response_base(QrxAuraRemoteFrame *resp,const QrxAuraRemoteFrame *req,QrxAuraRemoteServerContext*s,uint64_t h,uint32_t status){
    memset(resp,0,sizeof(*resp));resp->version=QRX_AURA_REMOTE_VERSION;resp->kind=req->kind;resp->status=status;snprintf(resp->signer_id,sizeof(resp->signer_id),"%s",s->binding->provider_id);resp->sequence=req->sequence;resp->height=h;snprintf(resp->admission_commitment,65,"%s",req->admission_commitment);snprintf(resp->provider_id,sizeof(resp->provider_id),"%s",s->binding->provider_id);snprintf(resp->pod_id,sizeof(resp->pod_id),"%s",s->binding->pod_id);snprintf(resp->lease_id,65,"%s",req->lease_id);resp->node_id=req->node_id;resp->fragment_index=req->fragment_index;resp->fragment_count=req->fragment_count;snprintf(resp->runtime_id,sizeof(resp->runtime_id),"%s",req->runtime_id);snprintf(resp->model_id,sizeof(resp->model_id),"%s",req->model_id);snprintf(resp->model_version,sizeof(resp->model_version),"%s",req->model_version);snprintf(resp->model_commitment,65,"%s",req->model_commitment);sha3_bytes(REMOTE_PAYLOAD_DOMAIN,NULL,0,resp->payload_commitment);
}
static QrxAuraReservationLeaseTable*server_snapshot(QrxAuraRemoteServerContext*s){if(!s||!s->lease_journal_path||!s->lease_journal_path[0])return NULL;QrxAuraReservationLeaseTable*x=malloc(sizeof(*x));if(x)*x=*s->leases;return x;}
static int server_persist_or_restore(QrxAuraRemoteServerContext*s,QrxAuraReservationLeaseTable*before){
    if(!s) return -1;
    if(!s->lease_journal_path||!s->lease_journal_path[0]) return s->require_durable_leases?-1:0;
    if(qrx_aura_reservation_leases_journal_save(s->lease_journal_path,s->leases)==0) return 0;
    if(before&&lease_table_restore_runtime(s->leases,before,s->binding)) return -2;
    return -1;
}
static int server_replace_payload(QrxAuraRemoteFrame*f,uint8_t*p,size_t n){if(!f||(n&&!p)||n>QRX_AURA_REMOTE_MAX_PAYLOAD)return -1;free(f->payload);f->payload=p;f->payload_len=n;return sha3_bytes(REMOTE_PAYLOAD_DOMAIN,p,n,f->payload_commitment);}
static int server_secure_session_get(QrxAuraRemoteServerContext*s,const char*remote,uint64_t h,QrxAuraSecureSession*out){
    if(!s||!remote||!out)return -1;
    if(s->secure_session_lookup&&!s->secure_session_lookup(s->secure_session_ctx,s->binding->provider_id,remote,out))return 0;
    if(s->pq_session_cache&&!qrx_aura_pq_session_cache_get(s->pq_session_cache,s->binding->provider_id,remote,h,out))return 0;
    return -1;
}
static int server_kem_public_raw(EVP_PKEY*k,uint8_t**out,size_t*outn){
    if(!k||!pq_hybrid_key_ok(k)||!out||!outn)return -1;
    size_t n=0;
    if(EVP_PKEY_get_raw_public_key(k,NULL,&n)!=1||!n||n>8192)return -1;
    uint8_t*b=malloc(n);if(!b)return -1;
    if(EVP_PKEY_get_raw_public_key(k,b,&n)!=1){free(b);return -1;}
    *out=b;*outn=n;return 0;
}
static int job_request_commitment(const QrxAuraRemoteFrame*r,char out[65]){
    if(!r||!out)return -1;Buf b={0};
    if(bs(&b,r->signer_id,sizeof(r->signer_id))||bs(&b,r->provider_id,sizeof(r->provider_id))||bs(&b,r->pod_id,sizeof(r->pod_id))||
       bs(&b,r->admission_commitment,65)||bs(&b,r->lease_id,65)||b64(&b,r->sequence)||b32(&b,r->compute_threads)||b64(&b,r->network_egress_mbps)||
       b32(&b,r->node_id)||b64(&b,r->required_memory_bytes)||b64(&b,r->max_output_bytes)||b32(&b,r->fragment_index)||b32(&b,r->fragment_count)||
       bs(&b,r->runtime_id,sizeof(r->runtime_id))||bs(&b,r->model_id,sizeof(r->model_id))||bs(&b,r->model_version,sizeof(r->model_version))||
       bs(&b,r->model_commitment,65)||bs(&b,r->payload_commitment,65)){free(b.p);return -1;}
    int rc=sha3_bytes("QRX/AURA/JOB-REQUEST/V1",b.p,b.n,out);free(b.p);return rc;
}
static int job_persist(QrxAuraRemoteServerContext*s){
    if(!s||!s->job_journal)return s&&s->require_durable_jobs?-1:0;
    if(!s->job_journal_path||!s->job_journal_path[0])return s->require_durable_jobs?-1:0;
    return qrx_aura_job_journal_save(s->job_journal_path,s->job_journal);
}
int qrx_aura_remote_serve_fd(int fd,QrxAuraRemoteServerContext*s){
    if(fd<0||!s||!s->binding||!s->leases||!s->requester_key_lookup||!s->provider_private_key||!s->height_fn)return -1;
    if(s->require_durable_leases&&(!s->lease_journal_path||!s->lease_journal_path[0]))return -1;
    if(s->require_durable_jobs&&(!s->job_journal||!s->job_journal_path||!s->job_journal_path[0]))return -1;
    QrxAuraRemoteFrame req={0},resp={0};uint8_t*sig=NULL;size_t sn=0;int rc=qrx_aura_remote_recv_fd(fd,&req,&sig,&sn);if(rc)return -2;
    EVP_PKEY*pk=NULL;uint64_t h=s->height_fn(s->height_ctx);uint32_t status=QRX_AURA_REMOTE_STATUS_INVALID;
    if(s->lease_journal_path&&s->lease_journal_path[0]){size_t reaped=0;if(qrx_aura_reservation_leases_reap_durable(s->leases,s->binding,1,h,s->lease_journal_path,&reaped)){free(sig);qrx_aura_remote_frame_free(&req);return -3;}}
    else (void)qrx_aura_reservation_leases_reap(s->leases,s->binding,1,h);
    if(!s->requester_key_lookup(s->requester_key_ctx,req.signer_id,&pk)&&pk&&!qrx_aura_remote_frame_verify(pk,&req,sig,sn)){
        if(req.status!=QRX_AURA_REMOTE_STATUS_OK)status=QRX_AURA_REMOTE_STATUS_INVALID;
        else if(req.height>h||h-req.height>8)status=QRX_AURA_REMOTE_STATUS_EXPIRED;
        else if(strcmp(req.provider_id,s->binding->provider_id)||strcmp(req.pod_id,s->binding->pod_id))status=QRX_AURA_REMOTE_STATUS_INVALID;
        else status=QRX_AURA_REMOTE_STATUS_OK;
    } else status=QRX_AURA_REMOTE_STATUS_UNAUTHORIZED;
    EVP_PKEY_free(pk);free(sig);

    int request_secure=0;QrxAuraSecureSession session;memset(&session,0,sizeof(session));
    if(status==QRX_AURA_REMOTE_STATUS_OK&&req.kind==QRX_AURA_REMOTE_KIND_DISPATCH){
        request_secure=qrx_aura_secure_payload_is_envelope(req.payload,req.payload_len);
        if(s->require_secure_dispatch&&!request_secure)status=QRX_AURA_REMOTE_STATUS_UNAUTHORIZED;
        if(request_secure){
            if(server_secure_session_get(s,req.signer_id,h,&session))status=QRX_AURA_REMOTE_STATUS_UNAUTHORIZED;
            else {uint8_t*plain=NULL;size_t pn=0;if(qrx_aura_secure_payload_open(&session,&req,req.payload,req.payload_len,&plain,&pn)||server_replace_payload(&req,plain,pn)){free(plain);status=QRX_AURA_REMOTE_STATUS_UNAUTHORIZED;}}
        }
    }
    response_base(&resp,&req,s,h,status);
    if(status==QRX_AURA_REMOTE_STATUS_OK){
        if(req.kind==QRX_AURA_REMOTE_KIND_SESSION_KEM_GET){
            uint8_t*der=NULL;size_t dern=0;
            if(!s->hybrid_kem_private_key||server_kem_public_raw(s->hybrid_kem_private_key,&der,&dern))resp.status=QRX_AURA_REMOTE_STATUS_UNSUPPORTED;
            else if(server_replace_payload(&resp,der,dern)){free(der);resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;}
        } else if(req.kind==QRX_AURA_REMOTE_KIND_SESSION_OPEN){
            QrxAuraSecureSession hs;memset(&hs,0,sizeof(hs));
            uint64_t ttl=s->pq_session_ttl_blocks?s->pq_session_ttl_blocks:QRX_AURA_PQ_SESSION_DEFAULT_TTL_BLOCKS;
            if(ttl>QRX_AURA_PQ_SESSION_MAX_TTL_BLOCKS)ttl=QRX_AURA_PQ_SESSION_MAX_TTL_BLOCKS;
            if(!s->hybrid_kem_private_key||!s->pq_session_cache||!req.payload_len||UINT64_MAX-h<ttl||qrx_aura_pq_hybrid_decapsulate(s->hybrid_kem_private_key,req.payload,req.payload_len,req.signer_id,s->binding->provider_id,&hs))resp.status=QRX_AURA_REMOTE_STATUS_UNAUTHORIZED;
            else {uint64_t exp=h+ttl;if(qrx_aura_pq_session_cache_put(s->pq_session_cache,s->binding->provider_id,req.signer_id,&hs,h,exp))resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;else{uint8_t*sid=malloc(64);if(!sid)resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;else{memcpy(sid,hs.session_id,64);if(server_replace_payload(&resp,sid,64)){free(sid);resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;}else resp.lease_expires_height=exp;}}}
            OPENSSL_cleanse(&hs,sizeof(hs));
        } else if(req.kind==QRX_AURA_REMOTE_KIND_LEASE_OPEN){
            QrxAuraReservationLeaseTable*before=server_snapshot(s);QrxAuraReservationLease l;int x=qrx_aura_reservation_lease_open(s->leases,s->binding,&req,h,&l);
            if(x)resp.status=x==-4?QRX_AURA_REMOTE_STATUS_BUSY:QRX_AURA_REMOTE_STATUS_INVALID;else if(server_persist_or_restore(s,before))resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;else resp.lease_expires_height=l.expires_height;free(before);
        } else if(req.kind==QRX_AURA_REMOTE_KIND_LEASE_RENEW){
            QrxAuraReservationLeaseTable*before=server_snapshot(s);QrxAuraReservationLease l;int x=qrx_aura_reservation_lease_renew(s->leases,&req,h,&l);
            if(x)resp.status=x==-2?QRX_AURA_REMOTE_STATUS_EXPIRED:QRX_AURA_REMOTE_STATUS_INVALID;else if(server_persist_or_restore(s,before))resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;else resp.lease_expires_height=l.expires_height;free(before);
        } else if(req.kind==QRX_AURA_REMOTE_KIND_LEASE_RELEASE){
            QrxAuraReservationLeaseTable*before=server_snapshot(s);int x=qrx_aura_reservation_lease_release(s->leases,s->binding,&req);if(x)resp.status=QRX_AURA_REMOTE_STATUS_INVALID;else if(server_persist_or_restore(s,before))resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;free(before);
        } else if(req.kind==QRX_AURA_REMOTE_KIND_DISPATCH){
            QrxAuraReservationLease*e=lease_mut(s->leases,req.lease_id);char jc[65]={0};
            QrxAuraJobRecord*jr=NULL;int exact_prepared=0;
            if(job_request_commitment(&req,jc))resp.status=QRX_AURA_REMOTE_STATUS_INVALID;
            else if(s->job_journal){
                jr=qrx_aura_job_journal_find(s->job_journal,req.signer_id,req.lease_id,req.sequence);
                if(jr&&strcmp(jr->request_commitment,jc))resp.status=QRX_AURA_REMOTE_STATUS_UNAUTHORIZED;
                else if(jr&&jr->state==QRX_AURA_JOB_COMMITTED){
                    resp.status=jr->result_status;
                    if(jr->result_len){uint8_t*copy=malloc(jr->result_len);if(!copy)resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;else{memcpy(copy,jr->result,jr->result_len);resp.payload=copy;resp.payload_len=jr->result_len;sha3_bytes(REMOTE_PAYLOAD_DOMAIN,copy,jr->result_len,resp.payload_commitment);}}
                } else if(jr&&jr->state==QRX_AURA_JOB_PREPARED) exact_prepared=1;
            }
            if(resp.status==QRX_AURA_REMOTE_STATUS_OK&&!(jr&&jr->state==QRX_AURA_JOB_COMMITTED)){
                if(!e||e->state!=QRX_AURA_LEASE_ACTIVE||e->expires_height<h)resp.status=QRX_AURA_REMOTE_STATUS_EXPIRED;
                else if(!lease_immutable_matches(e,&req))resp.status=QRX_AURA_REMOTE_STATUS_UNAUTHORIZED;
                else if(req.sequence<e->last_sequence||(req.sequence==e->last_sequence&&!exact_prepared))resp.status=QRX_AURA_REMOTE_STATUS_UNAUTHORIZED;
                else if(server_cache_ensure(s,&req,h))resp.status=QRX_AURA_REMOTE_STATUS_INVALID;
                else if(!s->binding->local)resp.status=QRX_AURA_REMOTE_STATUS_UNSUPPORTED;
                else {
                    if(!jr&&s->job_journal){if(qrx_aura_job_journal_prepare(s->job_journal,req.signer_id,req.lease_id,req.sequence,jc)||job_persist(s))resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;else jr=qrx_aura_job_journal_find(s->job_journal,req.signer_id,req.lease_id,req.sequence);}
                    if(resp.status==QRX_AURA_REMOTE_STATUS_OK&&req.sequence>e->last_sequence){
                        QrxAuraReservationLeaseTable*before=server_snapshot(s);e->last_sequence=req.sequence;s->leases->revision++;
                        if(server_persist_or_restore(s,before))resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;free(before);
                    }
                    if(resp.status==QRX_AURA_REMOTE_STATUS_OK){
                        size_t cap=(size_t)(req.max_output_bytes&&req.max_output_bytes<=QRX_AURA_REMOTE_MAX_PAYLOAD?req.max_output_bytes:QRX_AURA_REMOTE_MAX_PAYLOAD);uint8_t*out=malloc(cap?cap:1);size_t outn=0;int x=-1;
                        const QrxAiModelRecord*model=s->model_registry?qrx_ai_model_registry_find(s->model_registry,req.model_id,req.model_version):NULL;
                        if(s->model_execute){if(model)x=s->model_execute(s->model_execute_ctx,model,&req,req.payload,req.payload_len,out,cap,&outn);}
                        else if(!qrx_moe_worker_adapter_validate(&s->binding->adapter)&&!qrx_moe_runtime_device_validate(&s->binding->device)){QrxMoeWorkerRequirement wr;memset(&wr,0,sizeof(wr));wr.version=QRX_MOE_DISCOVERY_VERSION;wr.backend=s->binding->device.backend;wr.required_device_memory_bytes=req.required_memory_bytes;wr.requested_batch_items=1;wr.required_kernel_features=s->binding->adapter.kernel_mask;x=qrx_moe_worker_adapter_execute(&s->binding->adapter,&s->binding->device,&wr,req.payload,req.payload_len,out,cap,&outn);}
                        if(x||outn>cap){free(out);resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;if(s->job_journal&&jr){(void)qrx_aura_job_journal_commit(s->job_journal,req.signer_id,req.lease_id,req.sequence,jc,resp.status,NULL,0);(void)job_persist(s);}}
                        else {resp.payload=out;resp.payload_len=outn;sha3_bytes(REMOTE_PAYLOAD_DOMAIN,out,outn,resp.payload_commitment);if(s->job_journal&&jr){if(qrx_aura_job_journal_commit(s->job_journal,req.signer_id,req.lease_id,req.sequence,jc,resp.status,out,outn)||job_persist(s)){resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;free(resp.payload);resp.payload=NULL;resp.payload_len=0;sha3_bytes(REMOTE_PAYLOAD_DOMAIN,NULL,0,resp.payload_commitment);}}}
                    }
                }
            }
        }
    }
    if(resp.status==QRX_AURA_REMOTE_STATUS_OK&&request_secure&&resp.payload_len){uint8_t*sealed=NULL;size_t sealedn=0;if(qrx_aura_secure_payload_seal(&session,&resp,resp.payload,resp.payload_len,&sealed,&sealedn)||server_replace_payload(&resp,sealed,sealedn)){free(sealed);resp.status=QRX_AURA_REMOTE_STATUS_EXECUTION;free(resp.payload);resp.payload=NULL;resp.payload_len=0;sha3_bytes(REMOTE_PAYLOAD_DOMAIN,NULL,0,resp.payload_commitment);}}
    rc=qrx_aura_remote_send_fd(fd,&resp,s->provider_private_key);OPENSSL_cleanse(&session,sizeof(session));qrx_aura_remote_frame_free(&resp);qrx_aura_remote_frame_free(&req);return rc;
}

static int parse_endpoint(const char*e,char*h,size_t hc,char*p,size_t pc){
    if(!e||!h||!p||strncmp(e,"qrxp2p://",9)) return -1;
    const char*s=e+9,*host_start=s,*host_end=NULL,*port_start=NULL;
    if(*s=='['){host_start=s+1;host_end=strchr(host_start,']');if(!host_end||host_end[1]!=':'||!host_end[2])return-1;port_start=host_end+2;}
    else {const char*c=strrchr(s,':');if(!c||c==s||!c[1])return-1;host_end=c;port_start=c+1;}
    size_t hn=(size_t)(host_end-host_start);if(!hn||hn>=hc||strlen(port_start)>=pc)return-1;
    char*end=NULL;long port=strtol(port_start,&end,10);if(!end||*end||port<1||port>65535)return-1;
    memcpy(h,host_start,hn);h[hn]=0;snprintf(p,pc,"%ld",port);return 0;
}
static int connect_endpoint(const char*e){if(e&&!strncmp(e,"qrxrelay://",11))return qrx_aura_relay_client_connect(e);char h[256],p[16];if(parse_endpoint(e,h,sizeof(h),p,sizeof(p)))return-1;struct addrinfo hints,*res=NULL,*it;memset(&hints,0,sizeof(hints));hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;if(getaddrinfo(h,p,&hints,&res))return-1;int fd=-1;for(it=res;it;it=it->ai_next){fd=(int)socket(it->ai_family,it->ai_socktype,it->ai_protocol);if(fd<0)continue;if(connect(fd,it->ai_addr,(socklen_t)it->ai_addrlen)==0)break;qrx_close_socket(fd);fd=-1;}freeaddrinfo(res);return fd;}
int qrx_aura_remote_client_add_peer(QrxAuraRemoteClientContext*c,const char*provider,const char*pod,const char*ep){if(!c||!provider||!pod||!ep||c->peer_count>=QRX_AURA_REMOTE_MAX_PEERS||strlen(provider)>QRX_GLOBE_PROVIDER_MAX||strlen(pod)>=QRX_MOE_MAX_POD_ID||strlen(ep)>=QRX_AURA_GOSSIP_MAX_ENDPOINT)return-1;QrxAuraRemotePeer*p=&c->peers[c->peer_count++];memset(p,0,sizeof(*p));snprintf(p->provider_id,sizeof(p->provider_id),"%s",provider);snprintf(p->pod_id,sizeof(p->pod_id),"%s",pod);snprintf(p->endpoint,sizeof(p->endpoint),"%s",ep);return 0;}
int qrx_aura_remote_client_set_peer_secure_session(QrxAuraRemoteClientContext*c,const char*provider,const char*pod,const QrxAuraSecureSession*s){if(!c||!provider||!pod||!s||s->version!=QRX_AURA_SECURE_CHANNEL_VERSION||!is_hex64(s->session_id))return -1;for(uint32_t i=0;i<c->peer_count;i++)if(!strcmp(c->peers[i].provider_id,provider)&&!strcmp(c->peers[i].pod_id,pod)){c->peers[i].secure_session=*s;c->peers[i].secure_enabled=1;return 0;}return -2;}
static QrxAuraRemotePeer*peer_find_mut(QrxAuraRemoteClientContext*c,const char*provider,const char*pod){if(!c)return NULL;for(uint32_t i=0;i<c->peer_count;i++)if(!strcmp(c->peers[i].provider_id,provider)&&!strcmp(c->peers[i].pod_id,pod))return &c->peers[i];return NULL;}
static const QrxAuraRemotePeer*peer_find(const QrxAuraRemoteClientContext*c,const char*provider,const char*pod){return peer_find_mut((QrxAuraRemoteClientContext*)c,provider,pod);}
int qrx_aura_remote_client_enable_peer_pq_hybrid(QrxAuraRemoteClientContext*c,const char*provider,const char*pod,int enable){QrxAuraRemotePeer*p=peer_find_mut(c,provider,pod);if(!p)return-1;p->auto_hybrid_session=enable?1:0;return 0;}
int qrx_aura_remote_client_set_peer_hybrid_kem_public(QrxAuraRemoteClientContext*c,const char*provider,const char*pod,EVP_PKEY*k){QrxAuraRemotePeer*p=peer_find_mut(c,provider,pod);if(!p||!k||!pq_hybrid_key_ok(k))return-1;if(EVP_PKEY_up_ref(k)!=1)return-1;EVP_PKEY_free(p->hybrid_kem_public_key);p->hybrid_kem_public_key=k;return 0;}
void qrx_aura_remote_client_cleanup(QrxAuraRemoteClientContext*c){if(!c)return;for(uint32_t i=0;i<c->peer_count;i++){EVP_PKEY_free(c->peers[i].hybrid_kem_public_key);c->peers[i].hybrid_kem_public_key=NULL;OPENSSL_cleanse(&c->peers[i].secure_session,sizeof(c->peers[i].secure_session));c->peers[i].secure_enabled=0;} }
static int make_lease_id(const QrxAuraAdmission*a,const QrxAuraAdmissionPod*p,const char*requester,uint64_t seq,char out[65]){Buf b={0};if(bs(&b,a->admission_commitment,65)||bs(&b,p->provider_id,sizeof(p->provider_id))||bs(&b,p->pod_id,sizeof(p->pod_id))||bs(&b,requester,QRX_GLOBE_PROVIDER_MAX+1)||b64(&b,seq)){free(b.p);return-1;}int rc=sha3_bytes(REMOTE_LEASE_DOMAIN,b.p,b.n,out);free(b.p);return rc;}
static int remote_roundtrip(const QrxAuraRemotePeer*p,QrxAuraRemoteClientContext*c,QrxAuraRemoteFrame*req,QrxAuraRemoteFrame*resp){int fd=connect_endpoint(p->endpoint);if(fd<0)return-1;if(qrx_aura_remote_send_fd(fd,req,c->requester_private_key)){qrx_close_socket(fd);return-1;}uint8_t*sig=NULL;size_t sn=0;int rc=qrx_aura_remote_recv_fd(fd,resp,&sig,&sn);qrx_close_socket(fd);if(rc){free(sig);return-1;}EVP_PKEY*pk=NULL;if(c->provider_key_lookup(c->provider_key_ctx,resp->signer_id,&pk)||!pk||qrx_aura_remote_frame_verify(pk,resp,sig,sn)){EVP_PKEY_free(pk);free(sig);qrx_aura_remote_frame_free(resp);return-2;}EVP_PKEY_free(pk);free(sig);uint64_t dh=resp->height>c->current_height?resp->height-c->current_height:c->current_height-resp->height;if(dh>8||resp->sequence!=req->sequence||resp->kind!=req->kind||strcmp(resp->signer_id,req->provider_id)||strcmp(resp->provider_id,req->provider_id)||strcmp(resp->pod_id,req->pod_id)||strcmp(resp->admission_commitment,req->admission_commitment)||strcmp(resp->lease_id,req->lease_id)||resp->node_id!=req->node_id||resp->fragment_index!=req->fragment_index||resp->fragment_count!=req->fragment_count||strcmp(resp->model_commitment,req->model_commitment)){qrx_aura_remote_frame_free(resp);return-2;}if(req->kind==QRX_AURA_REMOTE_KIND_DISPATCH&&resp->status==QRX_AURA_REMOTE_STATUS_OK){int enc=qrx_aura_secure_payload_is_envelope(resp->payload,resp->payload_len);if(p->secure_enabled){if(!enc&&resp->payload_len){qrx_aura_remote_frame_free(resp);return-3;}if(enc){uint8_t*plain=NULL;size_t pn=0;if(qrx_aura_secure_payload_open(&p->secure_session,resp,resp->payload,resp->payload_len,&plain,&pn)||server_replace_payload(resp,plain,pn)){free(plain);qrx_aura_remote_frame_free(resp);return-3;}}}else if(enc){qrx_aura_remote_frame_free(resp);return-3;}}return 0;}

static uint64_t client_next_sequence(QrxAuraRemoteClientContext*c){uint64_t v=0;remote_sequence_lock();if(c&&c->sequence<UINT64_MAX)v=++c->sequence;remote_sequence_unlock();return v;}
static void fill_zero_commitment(char out[65]){memset(out,'0',64);out[64]=0;}
static int init_session_frame(QrxAuraRemoteFrame *r,QrxAuraRemoteClientContext *c,const QrxAuraRemotePeer *p,
                              uint32_t kind,uint64_t seq,const void *payload,size_t payload_n){
    if(!r||!c||!p||!seq||(payload_n&&!payload)||payload_n>8192) return -1;
    memset(r,0,sizeof(*r));
    r->version=QRX_AURA_REMOTE_VERSION;r->kind=kind;r->status=QRX_AURA_REMOTE_STATUS_OK;
    snprintf(r->signer_id,sizeof(r->signer_id),"%s",c->requester_id);
    r->sequence=seq;r->height=c->current_height;
    snprintf(r->provider_id,sizeof(r->provider_id),"%s",p->provider_id);
    snprintf(r->pod_id,sizeof(r->pod_id),"%s",p->pod_id);
    fill_zero_commitment(r->admission_commitment);fill_zero_commitment(r->lease_id);fill_zero_commitment(r->model_commitment);
    if(payload_n){
        r->payload=malloc(payload_n);if(!r->payload)return -1;
        memcpy(r->payload,payload,payload_n);r->payload_len=payload_n;
    }
    if(sha3_bytes(REMOTE_PAYLOAD_DOMAIN,r->payload,r->payload_len,r->payload_commitment)){
        qrx_aura_remote_frame_free(r);return -1;
    }
    return 0;
}
static int client_fetch_hybrid_kem(QrxAuraRemoteClientContext *c,QrxAuraRemotePeer *p){
    if(!c||!p) return -1;
    if(p->hybrid_kem_public_key) return 0;
    uint64_t seq=client_next_sequence(c);if(!seq)return -1;
    QrxAuraRemoteFrame req={0},resp={0};
    if(init_session_frame(&req,c,p,QRX_AURA_REMOTE_KIND_SESSION_KEM_GET,seq,NULL,0)) return -1;
    int rc=remote_roundtrip(p,c,&req,&resp);qrx_aura_remote_frame_free(&req);
    if(rc||resp.status!=QRX_AURA_REMOTE_STATUS_OK||!resp.payload||!resp.payload_len||resp.payload_len>8192){
        qrx_aura_remote_frame_free(&resp);return -2;
    }
    EVP_PKEY *k=EVP_PKEY_new_raw_public_key_ex(NULL,QRX_AURA_PQ_HYBRID_KEM_NAME,NULL,resp.payload,resp.payload_len);
    if(!k||!pq_hybrid_key_ok(k)){
        EVP_PKEY_free(k);qrx_aura_remote_frame_free(&resp);return -3;
    }
    EVP_PKEY_free(p->hybrid_kem_public_key);p->hybrid_kem_public_key=k;qrx_aura_remote_frame_free(&resp);
    return 0;
}
static int client_open_hybrid_session(QrxAuraRemoteClientContext *c,QrxAuraRemotePeer *p){
    if(!c||!p) return -1;
    if(p->secure_enabled) return 0;
    if(client_fetch_hybrid_kem(c,p)) return -2;
    uint8_t *ct=NULL;size_t ctn=0;QrxAuraSecureSession sess;memset(&sess,0,sizeof(sess));
    if(qrx_aura_pq_hybrid_encapsulate(p->hybrid_kem_public_key,c->requester_id,p->provider_id,&ct,&ctn,&sess)) return -3;
    uint64_t seq=client_next_sequence(c);
    if(!seq){OPENSSL_cleanse(&sess,sizeof(sess));free(ct);return -3;}
    QrxAuraRemoteFrame req={0},resp={0};
    if(init_session_frame(&req,c,p,QRX_AURA_REMOTE_KIND_SESSION_OPEN,seq,ct,ctn)){
        OPENSSL_cleanse(&sess,sizeof(sess));free(ct);return -3;
    }
    free(ct);int rc=remote_roundtrip(p,c,&req,&resp);qrx_aura_remote_frame_free(&req);
    if(rc||resp.status!=QRX_AURA_REMOTE_STATUS_OK||resp.payload_len!=64||!resp.payload||
       CRYPTO_memcmp(resp.payload,sess.session_id,64)){
        qrx_aura_remote_frame_free(&resp);OPENSSL_cleanse(&sess,sizeof(sess));return -4;
    }
    p->secure_session=sess;p->secure_enabled=1;qrx_aura_remote_frame_free(&resp);OPENSSL_cleanse(&sess,sizeof(sess));
    return 0;
}
static int init_req(QrxAuraRemoteFrame*r,QrxAuraRemoteClientContext*c,const QrxAuraAdmission*a,const QrxComputeJobNode*n,const QrxAuraAdmissionPod*p,uint32_t kind,uint32_t idx,uint32_t count,const char*lease,uint64_t seq,const void*payload,size_t payload_n){if(!r||!c||!a||!n||!p||!lease||(payload_n&&!payload)||payload_n>QRX_AURA_REMOTE_MAX_PAYLOAD)return-1;memset(r,0,sizeof(*r));r->version=QRX_AURA_REMOTE_VERSION;r->kind=kind;snprintf(r->signer_id,sizeof(r->signer_id),"%s",c->requester_id);r->sequence=seq;r->height=c->current_height;snprintf(r->admission_commitment,65,"%s",a->admission_commitment);snprintf(r->provider_id,sizeof(r->provider_id),"%s",p->provider_id);snprintf(r->pod_id,sizeof(r->pod_id),"%s",p->pod_id);snprintf(r->lease_id,65,"%s",lease);uint64_t lb=c->lease_blocks?c->lease_blocks:QRX_AURA_REMOTE_DEFAULT_LEASE_BLOCKS;if(UINT64_MAX-c->current_height<lb)return-1;r->lease_expires_height=c->current_height+lb;if(r->lease_expires_height>a->expires_height)r->lease_expires_height=a->expires_height;r->compute_threads=p->compute_threads?p->compute_threads:1;r->network_egress_mbps=p->network_egress_mbps;r->node_id=n->node_id;r->required_memory_bytes=n->min_memory_bytes;r->max_output_bytes=n->max_output_bytes?n->max_output_bytes:QRX_AURA_REMOTE_MAX_PAYLOAD;r->fragment_index=idx;r->fragment_count=count;snprintf(r->runtime_id,sizeof(r->runtime_id),"%s",n->runtime_id);snprintf(r->model_id,sizeof(r->model_id),"%s",n->model_id);snprintf(r->model_version,sizeof(r->model_version),"%s",n->model_version);snprintf(r->model_commitment,65,"%s",a->model_commitment);if(payload_n){r->payload=malloc(payload_n);if(!r->payload)return-1;memcpy(r->payload,payload,payload_n);r->payload_len=payload_n;}if(sha3_bytes(REMOTE_PAYLOAD_DOMAIN,r->payload,r->payload_len,r->payload_commitment)){qrx_aura_remote_frame_free(r);return-1;}return 0;}

static int secure_request_for_peer(const QrxAuraRemotePeer*p,QrxAuraRemoteFrame*r){if(!p||!r||!p->secure_enabled)return 0;uint8_t*sealed=NULL;size_t sn=0;if(qrx_aura_secure_payload_seal(&p->secure_session,r,r->payload,r->payload_len,&sealed,&sn))return -1;return server_replace_payload(r,sealed,sn);}
typedef struct {QrxAuraRemoteClientContext*c;const QrxAuraAdmission*a;const QrxComputeJobNode*n;const void*input;size_t input_n;uint32_t idx;uint64_t seq_base;uint8_t*out;size_t outn;int rc;} DispatchWorker;
static void dispatch_worker_run(DispatchWorker*w){
    QrxAuraRemoteClientContext*c=w->c;const QrxAuraAdmissionPod*ap=&w->a->pods[w->idx];const QrxAuraRemotePeer*peer=peer_find(c,ap->provider_id,ap->pod_id);if(!peer){w->rc=-1;return;}
    uint8_t*frag=NULL;size_t fn=0;if(w->a->pod_count>1){if(!c->fragment_input||c->fragment_input(c->fragment_input_ctx,w->idx,w->a->pod_count,w->input,w->input_n,&frag,&fn)){w->rc=-2;return;}}else{frag=malloc(w->input_n?w->input_n:1);if(!frag){w->rc=-2;return;}if(w->input_n)memcpy(frag,w->input,w->input_n);fn=w->input_n;}
    uint64_t seq=w->seq_base;char lease[65];if(make_lease_id(w->a,ap,c->requester_id,seq,lease)){free(frag);w->rc=-3;return;}QrxAuraRemoteFrame req={0},resp={0};
    if(init_req(&req,c,w->a,w->n,ap,QRX_AURA_REMOTE_KIND_LEASE_OPEN,w->idx,w->a->pod_count,lease,seq,NULL,0)){free(frag);w->rc=-3;return;}
    if(req.lease_expires_height<=c->current_height||remote_roundtrip(peer,c,&req,&resp)||resp.status!=QRX_AURA_REMOTE_STATUS_OK){qrx_aura_remote_frame_free(&req);qrx_aura_remote_frame_free(&resp);free(frag);w->rc=-4;return;}qrx_aura_remote_frame_free(&req);qrx_aura_remote_frame_free(&resp);
    seq=w->seq_base+1;if(init_req(&req,c,w->a,w->n,ap,QRX_AURA_REMOTE_KIND_DISPATCH,w->idx,w->a->pod_count,lease,seq,frag,fn)||secure_request_for_peer(peer,&req)){free(frag);qrx_aura_remote_frame_free(&req);w->rc=-5;return;}free(frag);
    if(remote_roundtrip(peer,c,&req,&resp)||resp.status!=QRX_AURA_REMOTE_STATUS_OK){qrx_aura_remote_frame_free(&req);qrx_aura_remote_frame_free(&resp);w->rc=-5;}else{w->out=resp.payload;w->outn=resp.payload_len;resp.payload=NULL;resp.payload_len=0;qrx_aura_remote_frame_free(&req);qrx_aura_remote_frame_free(&resp);w->rc=0;}
    seq=w->seq_base+2;if(init_req(&req,c,w->a,w->n,ap,QRX_AURA_REMOTE_KIND_LEASE_RELEASE,w->idx,w->a->pod_count,lease,seq,NULL,0))return;if(!remote_roundtrip(peer,c,&req,&resp))qrx_aura_remote_frame_free(&resp);qrx_aura_remote_frame_free(&req);
}
#ifdef _WIN32
static DWORD WINAPI dispatch_worker_thread(LPVOID v){dispatch_worker_run((DispatchWorker*)v);return 0;}
#else
static void*dispatch_worker_thread(void*v){dispatch_worker_run((DispatchWorker*)v);return NULL;}
#endif

int qrx_aura_remote_distributed_dispatch(void*v,const QrxAuraAdmission*a,const QrxComputeJobNode*n,const void*input,size_t input_n,void*out,size_t outcap,size_t*outn){
    QrxAuraRemoteClientContext*c=v;
    if(!c||!a||!n||!outn||!c->requester_id[0]||!c->requester_private_key||!c->provider_key_lookup||!a->active||!a->pod_count||a->pod_count>QRX_AURA_ADMISSION_MAX_PODS) return -1;
    if(a->pod_count>1&&(!c->fragment_input||!c->aggregate)) return -2;
    for(uint32_t i=0;i<a->pod_count;i++){QrxAuraRemotePeer*p=peer_find_mut(c,a->pods[i].provider_id,a->pods[i].pod_id);if(!p)return -2;if(!p->secure_enabled&&p->auto_hybrid_session&&client_open_hybrid_session(c,p))return -2;if(c->require_secure_peers&&!p->secure_enabled)return -2;}
    uint64_t add=(uint64_t)a->pod_count*3ULL,seq0=0;
    remote_sequence_lock();
    if(UINT64_MAX-c->sequence < add){remote_sequence_unlock();return -2;}
    seq0=c->sequence+1;c->sequence+=add;
    remote_sequence_unlock();
    DispatchWorker w[QRX_AURA_ADMISSION_MAX_PODS];
#ifdef _WIN32
    HANDLE th[QRX_AURA_ADMISSION_MAX_PODS];
#else
    pthread_t th[QRX_AURA_ADMISSION_MAX_PODS];
#endif
    uint32_t launched=0;
    for(uint32_t i=0;i<a->pod_count;i++){
        memset(&w[i],0,sizeof(w[i]));
        w[i].c=c;w[i].a=a;w[i].n=n;w[i].input=input;w[i].input_n=input_n;w[i].idx=i;w[i].seq_base=seq0+(uint64_t)i*3ULL;w[i].rc=-99;
#ifdef _WIN32
        th[i]=CreateThread(NULL,0,dispatch_worker_thread,&w[i],0,NULL);if(!th[i]) break;
#else
        if(pthread_create(&th[i],NULL,dispatch_worker_thread,&w[i])) break;
#endif
        launched++;
    }
    for(uint32_t i=0;i<launched;i++){
#ifdef _WIN32
        WaitForSingleObject(th[i],INFINITE);CloseHandle(th[i]);
#else
        pthread_join(th[i],NULL);
#endif
    }
    if(launched!=a->pod_count){for(uint32_t i=0;i<launched;i++)free(w[i].out);return -3;}
    for(uint32_t i=0;i<a->pod_count;i++) if(w[i].rc){for(uint32_t j=0;j<a->pod_count;j++)free(w[j].out);return -4;}
    if(a->pod_count==1){
        if(w[0].outn>outcap){free(w[0].out);return -5;}
        if(w[0].outn) memcpy(out,w[0].out,w[0].outn);
        *outn=w[0].outn;free(w[0].out);return 0;
    }
    const uint8_t*parts[QRX_AURA_ADMISSION_MAX_PODS];size_t sizes[QRX_AURA_ADMISSION_MAX_PODS];
    for(uint32_t i=0;i<a->pod_count;i++){parts[i]=w[i].out;sizes[i]=w[i].outn;}
    int rc=c->aggregate(c->aggregate_ctx,parts,sizes,a->pod_count,out,outcap,outn);
    for(uint32_t i=0;i<a->pod_count;i++) free(w[i].out);
    return rc?-6:0;
}

/* ---- QRX Drive model bundle ------------------------------------------------ */
static int model_obj_valid(const QrxAuraModelBundleObject*o){return o&&o->kind>=QRX_AURA_MODEL_OBJECT_WEIGHTS&&o->kind<=QRX_AURA_MODEL_OBJECT_SOURCE_MAP&&o->bytes&&is_hex64(o->object_root);}
int qrx_aura_model_bundle_encode(const QrxAuraModelBundleManifest*m,uint8_t**out,size_t*outn){if(!m||!out||!outn||m->version!=QRX_AURA_MODEL_BUNDLE_VERSION||!bounded_text(m->model_id,sizeof(m->model_id),0)||!bounded_text(m->model_version,sizeof(m->model_version),0)||!m->object_count||m->object_count>QRX_AURA_REMOTE_MAX_BUNDLE_OBJECTS)return-1;uint64_t sum=0;Buf b={0};if(bput(&b,MODEL_BUNDLE_MAGIC,MODEL_BUNDLE_MAGIC_LEN)||b32(&b,m->version)||bs(&b,m->model_id,sizeof(m->model_id))||bs(&b,m->model_version,sizeof(m->model_version))||b64(&b,m->total_bytes)||b32(&b,m->object_count)){free(b.p);return-1;}for(uint32_t i=0;i<m->object_count;i++){if(!model_obj_valid(&m->objects[i])||UINT64_MAX-sum<m->objects[i].bytes){free(b.p);return-1;}sum+=m->objects[i].bytes;if(b32(&b,m->objects[i].kind)||b64(&b,m->objects[i].bytes)||bs(&b,m->objects[i].object_root,65)){free(b.p);return-1;}}if(sum!=m->total_bytes){free(b.p);return-1;}*out=b.p;*outn=b.n;return 0;}
int qrx_aura_model_bundle_decode(const uint8_t*in,size_t n,QrxAuraModelBundleManifest*m){if(!in||!m||n<MODEL_BUNDLE_MAGIC_LEN||memcmp(in,MODEL_BUNDLE_MAGIC,MODEL_BUNDLE_MAGIC_LEN))return-1;memset(m,0,sizeof(*m));Rd r={in+MODEL_BUNDLE_MAGIC_LEN,n-MODEL_BUNDLE_MAGIC_LEN,0};if(r32(&r,&m->version)||rs(&r,m->model_id,sizeof(m->model_id))||rs(&r,m->model_version,sizeof(m->model_version))||r64(&r,&m->total_bytes)||r32(&r,&m->object_count)||m->version!=QRX_AURA_MODEL_BUNDLE_VERSION||!m->object_count||m->object_count>QRX_AURA_REMOTE_MAX_BUNDLE_OBJECTS)return-1;uint64_t sum=0;for(uint32_t i=0;i<m->object_count;i++){QrxAuraModelBundleObject*o=&m->objects[i];if(r32(&r,&o->kind)||r64(&r,&o->bytes)||rs(&r,o->object_root,65)||!model_obj_valid(o)||UINT64_MAX-sum<o->bytes)return-1;sum+=o->bytes;}return r.o==r.n&&sum==m->total_bytes?0:-1;}
int qrx_aura_model_bundle_root(const QrxAuraModelBundleManifest*m,char out[65]){uint8_t*b=NULL;size_t n=0;if(qrx_aura_model_bundle_encode(m,&b,&n))return-1;EVP_MD_CTX*x=EVP_MD_CTX_new();uint8_t h[32];unsigned hn=0;int ok=x&&EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)==1&&EVP_DigestUpdate(x,b,n)==1&&EVP_DigestFinal_ex(x,h,&hn)==1&&hn==32;if(x)EVP_MD_CTX_free(x);free(b);if(!ok)return-1;hex32(h,out);return 0;}
static int fetch_object(QrxAuraDriveModelFetchContext*c,const char*root,uint64_t declared){if(qrx_storage_fs_has(c->local_cache,root)==1)return 0;if(!c->stat||!c->fetch_range)return-1;uint64_t total=0;if(c->stat(c->remote_ctx,root,&total)||!total||total!=declared)return-2;char tmp[1024];FILE*f=NULL;
#ifdef _WIN32
    char d[MAX_PATH],t[MAX_PATH];DWORD dn=GetTempPathA(MAX_PATH,d);if(!dn||dn>=MAX_PATH||!GetTempFileNameA(d,"QMD",0,t))return-3;snprintf(tmp,sizeof(tmp),"%s",t);f=fopen(tmp,"wb");
#else
    snprintf(tmp,sizeof(tmp),"/tmp/qrx-model-XXXXXX");int fd=mkstemp(tmp);if(fd>=0)f=fdopen(fd,"wb");
#endif
    if(!f) return -3;
    size_t range=c->range_bytes?c->range_bytes:128u*1024u;
    if(range>4u*1024u*1024u) range=4u*1024u*1024u;
    uint64_t off=0;int rc=0;
    while(off<total){
        size_t want=(size_t)((total-off)<range?(total-off):range);uint8_t*b=NULL;size_t n=0;
        if(c->fetch_range(c->remote_ctx,root,off,want,&b,&n)||n!=want||fwrite(b,1,n,f)!=n){free(b);rc=-4;break;}
        free(b);off+=n;
    }
    if(fflush(f)||fclose(f)) rc=-4;
    if(!rc){char got[65];if(qrx_storage_fs_put_file(c->local_cache,tmp,got)||strcmp(got,root))rc=-5;}
    remove(tmp);return rc;}
int qrx_aura_drive_asset_fetch(QrxAuraDriveModelFetchContext*c,const char*root,uint64_t declared){if(!c||!root||!declared)return-1;return fetch_object(c,root,declared);}
static int fetch_manifest_bytes(QrxAuraDriveModelFetchContext*c,const char*root,uint8_t**out,size_t*outn){if(qrx_storage_fs_has(c->local_cache,root)==1)return qrx_storage_fs_read(c->local_cache,root,out,outn);uint64_t n=0;if(!c->stat||c->stat(c->remote_ctx,root,&n)||!n||n>4u*1024u*1024u)return-1;if(fetch_object(c,root,n))return-1;return qrx_storage_fs_read(c->local_cache,root,out,outn);}
static int bundle_registry_roots_valid(const QrxAuraModelBundleManifest *mf,const QrxAiModelRecord *m){
    if(!mf||!m) return -1;
    int tokenizer=0,expert_manifest=m->is_moe?0:1,compute=0;
    for(uint32_t i=0;i<mf->object_count;i++){
        const QrxAuraModelBundleObject *o=&mf->objects[i];
        if(o->kind==QRX_AURA_MODEL_OBJECT_TOKENIZER&&!strcmp(o->object_root,m->tokenizer_root)) tokenizer=1;
        if(o->kind==QRX_AURA_MODEL_OBJECT_EXPERT_MANIFEST&&m->is_moe&&!strcmp(o->object_root,m->expert_manifest_root)) expert_manifest=1;
        if(o->kind==QRX_AURA_MODEL_OBJECT_WEIGHTS||o->kind==QRX_AURA_MODEL_OBJECT_EXPERT_PACK) compute++;
    }
    return tokenizer&&expert_manifest&&compute?0:-1;
}
int qrx_aura_drive_model_fetch(void*v,const QrxAiModelRecord*m,const QrxAuraModelCachePlacement*p,char out[65]){
    QrxAuraDriveModelFetchContext*c=v;
    if(!c||!c->local_cache||!m||!p||!out||!is_hex64(m->manifest_root)||!p->bytes_to_place) return -1;
    char mc[65];if(qrx_ai_model_commitment(m,mc)) return -1;
    uint8_t*b=NULL;size_t n=0;if(fetch_manifest_bytes(c,m->manifest_root,&b,&n)) return -2;
    QrxAuraModelBundleManifest mf;
    if(qrx_aura_model_bundle_decode(b,n,&mf)){free(b);return -3;}
    free(b);
    char root[65];
    if(qrx_aura_model_bundle_root(&mf,root)||strcmp(root,m->manifest_root)||strcmp(mf.model_id,m->model_id)||strcmp(mf.model_version,m->model_version)||bundle_registry_roots_valid(&mf,m)) return -4;

    /* Every execution pod needs small control metadata locally. Large weight /
     * expert objects are then selected deterministically per pod. */
    uint64_t fetched=0;
    for(uint32_t i=0;i<mf.object_count;i++){
        QrxAuraModelBundleObject*o=&mf.objects[i];
        if(o->kind!=QRX_AURA_MODEL_OBJECT_TOKENIZER&&o->kind!=QRX_AURA_MODEL_OBJECT_CONFIG&&o->kind!=QRX_AURA_MODEL_OBJECT_EXPERT_MANIFEST&&o->kind!=QRX_AURA_MODEL_OBJECT_SOURCE_MAP) continue;
        if(fetch_object(c,o->object_root,o->bytes)) return -5;
        fetched=UINT64_MAX-fetched<o->bytes?UINT64_MAX:fetched+o->bytes;
    }

    uint32_t candidates[QRX_AURA_REMOTE_MAX_BUNDLE_OBJECTS],cn=0;
    for(uint32_t i=0;i<mf.object_count;i++) if(mf.objects[i].kind==QRX_AURA_MODEL_OBJECT_WEIGHTS||mf.objects[i].kind==QRX_AURA_MODEL_OBJECT_EXPERT_PACK) candidates[cn++]=i;
    if(!cn) return -6;
    uint8_t hh[32];EVP_MD_CTX*x=EVP_MD_CTX_new();unsigned hn=0;
    if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1||EVP_DigestUpdate(x,p->pod_id,strlen(p->pod_id))!=1||EVP_DigestUpdate(x,mc,64)!=1||EVP_DigestFinal_ex(x,hh,&hn)!=1||hn!=32){if(x)EVP_MD_CTX_free(x);return -6;}
    EVP_MD_CTX_free(x);uint32_t start=be32(hh)%cn;
    uint32_t placed_compute=0;
    for(uint32_t k=0;k<cn&&(fetched<p->bytes_to_place||!placed_compute);k++){
        QrxAuraModelBundleObject*o=&mf.objects[candidates[(start+k)%cn]];
        if(fetch_object(c,o->object_root,o->bytes)) return -7;
        fetched=UINT64_MAX-fetched<o->bytes?UINT64_MAX:fetched+o->bytes;placed_compute++;
    }
    if(fetched<p->bytes_to_place) return -8;
    snprintf(out,65,"%s",mc);return 0;
}
