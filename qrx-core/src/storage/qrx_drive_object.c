#include "storage/qrx_drive_object.h"
#include "storage/qrx_drive_crypto.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define OBJ_MAGIC "QRXOBJ1\0"
#define IDX_MAGIC "QRXIDX1\0"
#define CHK_MAGIC "QRXCHK1\0"
#define OBJ_MAGIC_LEN 8u
#define ID_BYTES 32u
#define SALT_BYTES 32u
#define REF_BYTES 40u

typedef struct { uint8_t id[ID_BYTES]; uint64_t bytes; } ObjRef;

typedef struct {
    uint64_t total_bytes;
    uint32_t chunk_bytes;
    uint32_t tree_level;
    uint8_t salt[SALT_BYTES];
    uint8_t root_id[ID_BYTES];
    QrxDriveKeyEnvelope env;
} ObjDescriptor;

static void p32(uint8_t *o,uint32_t v){o[0]=(uint8_t)(v>>24);o[1]=(uint8_t)(v>>16);o[2]=(uint8_t)(v>>8);o[3]=(uint8_t)v;}
static uint32_t g32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void p64(uint8_t *o,uint64_t v){for(int i=7;i>=0;i--){o[i]=(uint8_t)v;v>>=8;}}
static uint64_t g64(const uint8_t *p){uint64_t v=0;for(int i=0;i<8;i++)v=(v<<8)|p[i];return v;}
static int hexv(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static int hex_to_id(const char *s,uint8_t out[32]){if(!s||strlen(s)!=64)return -1;for(int i=0;i<32;i++){int a=hexv(s[i*2]),b=hexv(s[i*2+1]);if(a<0||b<0)return -1;out[i]=(uint8_t)((a<<4)|b);}return 0;}
static void id_to_hex(const uint8_t id[32],char out[65]){static const char h[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=h[id[i]>>4];out[i*2+1]=h[id[i]&15];}out[64]=0;}

static int sha3_256(const uint8_t *p,size_t n,uint8_t out[32]){EVP_MD_CTX*c=EVP_MD_CTX_new();unsigned z=0;int rc=-1;if(c&&EVP_DigestInit_ex(c,EVP_sha3_256(),NULL)==1&&(!n||EVP_DigestUpdate(c,p,n)==1)&&EVP_DigestFinal_ex(c,out,&z)==1&&z==32)rc=0;EVP_MD_CTX_free(c);return rc;}
static int verify_cas(const char *hex,const uint8_t *p,size_t n){uint8_t want[32],got[32];return hex_to_id(hex,want)==0&&sha3_256(p,n,got)==0&&CRYPTO_memcmp(want,got,32)==0?0:-1;}
static int regular_size(const char *path,uint64_t*out){struct stat st;if(!path||stat(path,&st)!=0)return -1;
#ifdef _WIN32
 if((st.st_mode&_S_IFREG)==0)return -1;
#else
 if(!S_ISREG(st.st_mode))return -1;
#endif
 if(st.st_size<0)return -1;*out=(uint64_t)st.st_size;return 0;}

static int ref_write(FILE*f,const ObjRef*r){uint8_t b[REF_BYTES];memcpy(b,r->id,32);p64(b+32,r->bytes);return fwrite(b,1,sizeof(b),f)==sizeof(b)?0:-1;}
static int ref_read(FILE*f,ObjRef*r){uint8_t b[REF_BYTES];size_t n=fread(b,1,sizeof(b),f);if(n==0&&feof(f))return 1;if(n!=sizeof(b))return -1;memcpy(r->id,b,32);r->bytes=g64(b+32);return 0;}

static int store_index_page(QrxStorageFs*fs,uint32_t level,const ObjRef *refs,uint32_t count,ObjRef*out){if(!fs||!refs||!count||count>QRX_DRIVE_OBJECT_INDEX_FANOUT||!out)return -1;size_t n=8+4+4+4+8+(size_t)count*REF_BYTES;uint8_t*b=calloc(n,1);if(!b)return -1;size_t o=0;memcpy(b+o,IDX_MAGIC,8);o+=8;p32(b+o,QRX_DRIVE_OBJECT_VERSION);o+=4;p32(b+o,level);o+=4;p32(b+o,count);o+=4;uint64_t sum=0;for(uint32_t i=0;i<count;i++){if(UINT64_MAX-sum<refs[i].bytes){free(b);return -1;}sum+=refs[i].bytes;}p64(b+o,sum);o+=8;for(uint32_t i=0;i<count;i++){memcpy(b+o,refs[i].id,32);o+=32;p64(b+o,refs[i].bytes);o+=8;}char id[65];int rc=qrx_storage_fs_put(fs,b,n,id);if(rc==0&&hex_to_id(id,out->id)==0){out->bytes=sum;}else rc=-1;OPENSSL_cleanse(b,n);free(b);return rc;}

static int build_next_level(QrxStorageFs*fs,FILE*in,uint64_t in_count,uint32_t out_level,FILE**out_file,uint64_t*out_count){if(!fs||!in||!out_file||!out_count)return -1;FILE*nxt=tmpfile();if(!nxt)return -1;rewind(in);uint64_t produced=0,consumed=0;while(consumed<in_count){ObjRef refs[QRX_DRIVE_OBJECT_INDEX_FANOUT];uint32_t c=0;while(c<QRX_DRIVE_OBJECT_INDEX_FANOUT&&consumed<in_count){int rr=ref_read(in,&refs[c]);if(rr!=0){fclose(nxt);return -1;}c++;consumed++;}ObjRef p;if(store_index_page(fs,out_level,refs,c,&p)!=0||ref_write(nxt,&p)!=0){fclose(nxt);return -1;}produced++;}fflush(nxt);*out_file=nxt;*out_count=produced;return 0;}

static int serialize_chunk(uint64_t index,uint32_t plain_len,const QrxDriveCiphertext*c,uint8_t**out,size_t*out_len){if(!c||!out||!out_len||c->ciphertext_len!=plain_len)return -1;size_t n=8+4+8+4+12+16+c->ciphertext_len;uint8_t*b=malloc(n?n:1);if(!b)return -1;size_t o=0;memcpy(b+o,CHK_MAGIC,8);o+=8;p32(b+o,QRX_DRIVE_OBJECT_VERSION);o+=4;p64(b+o,index);o+=8;p32(b+o,plain_len);o+=4;memcpy(b+o,c->nonce,12);o+=12;memcpy(b+o,c->tag,16);o+=16;if(c->ciphertext_len)memcpy(b+o,c->ciphertext,c->ciphertext_len);*out=b;*out_len=n;return 0;}
static void make_aad(const uint8_t salt[32],uint64_t idx,uint32_t n,uint8_t out[44]){memcpy(out,salt,32);p64(out+32,idx);p32(out+40,n);}

static int serialize_descriptor(const ObjDescriptor*d,uint8_t**out,size_t*out_len){if(!d||!out||!out_len||!d->env.kem_ciphertext||d->env.kem_ciphertext_len>UINT32_MAX)return -1;size_t n=8+4+8+4+4+32+32+4+32+12+16+d->env.kem_ciphertext_len;uint8_t*b=malloc(n);if(!b)return -1;size_t o=0;memcpy(b+o,OBJ_MAGIC,8);o+=8;p32(b+o,QRX_DRIVE_OBJECT_VERSION);o+=4;p64(b+o,d->total_bytes);o+=8;p32(b+o,d->chunk_bytes);o+=4;p32(b+o,d->tree_level);o+=4;memcpy(b+o,d->salt,32);o+=32;memcpy(b+o,d->root_id,32);o+=32;p32(b+o,(uint32_t)d->env.kem_ciphertext_len);o+=4;memcpy(b+o,d->env.wrapped_file_key,32);o+=32;memcpy(b+o,d->env.nonce,12);o+=12;memcpy(b+o,d->env.tag,16);o+=16;memcpy(b+o,d->env.kem_ciphertext,d->env.kem_ciphertext_len);*out=b;*out_len=n;return 0;}
static int parse_descriptor(const uint8_t*b,size_t n,ObjDescriptor*d){if(!b||!d||n<8+4+8+4+4+32+32+4+32+12+16||memcmp(b,OBJ_MAGIC,8)||g32(b+8)!=QRX_DRIVE_OBJECT_VERSION)return -1;memset(d,0,sizeof(*d));size_t o=12;d->total_bytes=g64(b+o);o+=8;d->chunk_bytes=g32(b+o);o+=4;d->tree_level=g32(b+o);o+=4;if(d->chunk_bytes<QRX_DRIVE_OBJECT_MIN_CHUNK_BYTES||d->chunk_bytes>QRX_DRIVE_OBJECT_MAX_CHUNK_BYTES||d->tree_level>32)return -1;memcpy(d->salt,b+o,32);o+=32;memcpy(d->root_id,b+o,32);o+=32;uint32_t kl=g32(b+o);o+=4;if((size_t)kl>n-o-60)return -1;memcpy(d->env.wrapped_file_key,b+o,32);o+=32;memcpy(d->env.nonce,b+o,12);o+=12;memcpy(d->env.tag,b+o,16);o+=16;if(n-o!=kl)return -1;d->env.kem_ciphertext=malloc(kl?kl:1);if(!d->env.kem_ciphertext)return -1;memcpy(d->env.kem_ciphertext,b+o,kl);d->env.kem_ciphertext_len=kl;return 0;}

int qrx_drive_object_put_file(QrxStorageFs*fs,const char*source,EVP_PKEY*recipient,uint32_t chunk_bytes,QrxDriveObjectInfo*out){if(!fs||!source||!recipient||!out)return -1;memset(out,0,sizeof(*out));if(!chunk_bytes)chunk_bytes=QRX_DRIVE_OBJECT_DEFAULT_CHUNK_BYTES;if(chunk_bytes<QRX_DRIVE_OBJECT_MIN_CHUNK_BYTES||chunk_bytes>QRX_DRIVE_OBJECT_MAX_CHUNK_BYTES)return -1;uint64_t total=0;if(regular_size(source,&total)!=0)return -1;FILE*in=fopen(source,"rb");if(!in)return -1;uint8_t file_key[32]={0},salt[32]={0};QrxDriveKeyEnvelope env={0};FILE*refs=tmpfile();uint8_t*plain=NULL;int rc=-1;uint64_t chunks=0,leaf_pages=0;if(!refs||qrx_drive_random_file_key(file_key)!=0||RAND_bytes(salt,32)!=1||qrx_drive_wrap_file_key(recipient,file_key,&env)!=0)goto done;plain=malloc(chunk_bytes);if(!plain)goto done;ObjRef group[QRX_DRIVE_OBJECT_INDEX_FANOUT];uint32_t gc=0;uint64_t idx=0,seen=0;for(;;){size_t got=fread(plain,1,chunk_bytes,in);if(got){if(got>UINT32_MAX||UINT64_MAX-seen<got)goto done;uint8_t aad[44];make_aad(salt,idx,(uint32_t)got,aad);QrxDriveCiphertext ct={0};if(qrx_drive_encrypt(file_key,plain,got,aad,sizeof(aad),&ct)!=0)goto done;uint8_t*blob=NULL;size_t blob_len=0;if(serialize_chunk(idx,(uint32_t)got,&ct,&blob,&blob_len)!=0){qrx_drive_ciphertext_free(&ct);goto done;}char cid[65];int pr=qrx_storage_fs_put(fs,blob,blob_len,cid);OPENSSL_cleanse(blob,blob_len);free(blob);qrx_drive_ciphertext_free(&ct);if(pr!=0||hex_to_id(cid,group[gc].id)!=0)goto done;group[gc].bytes=(uint64_t)got;gc++;chunks++;seen+=got;idx++;if(gc==QRX_DRIVE_OBJECT_INDEX_FANOUT){ObjRef page;if(store_index_page(fs,0,group,gc,&page)!=0||ref_write(refs,&page)!=0)goto done;leaf_pages++;gc=0;}}
        if(got<chunk_bytes){if(ferror(in))goto done;break;}}
    if(seen!=total)goto done;
    if(gc||leaf_pages==0){/* Empty files still get one empty leaf page. */if(gc==0){ObjRef empty={0};char eid[65];uint8_t z=0;if(qrx_storage_fs_put(fs,&z,0,eid)!=0||hex_to_id(eid,empty.id)!=0)goto done;empty.bytes=0;group[gc++]=empty;chunks=1;}ObjRef page;if(store_index_page(fs,0,group,gc,&page)!=0||ref_write(refs,&page)!=0)goto done;leaf_pages++;}
    fflush(refs);FILE*cur=refs;refs=NULL;uint64_t count=leaf_pages;uint32_t level=0;while(count>1){FILE*nxt=NULL;uint64_t nc=0;if(build_next_level(fs,cur,count,level+1,&nxt,&nc)!=0){fclose(cur);goto done;}fclose(cur);cur=nxt;count=nc;level++;}rewind(cur);ObjRef root;if(ref_read(cur,&root)!=0){fclose(cur);goto done;}fclose(cur);ObjDescriptor d={0};d.total_bytes=total;d.chunk_bytes=chunk_bytes;d.tree_level=level;memcpy(d.salt,salt,32);memcpy(d.root_id,root.id,32);d.env=env;memset(&env,0,sizeof(env));uint8_t*desc=NULL;size_t desc_len=0;if(serialize_descriptor(&d,&desc,&desc_len)!=0){qrx_drive_key_envelope_free(&d.env);goto done;}char oid[65];int sp=qrx_storage_fs_put(fs,desc,desc_len,oid);OPENSSL_cleanse(desc,desc_len);free(desc);qrx_drive_key_envelope_free(&d.env);if(sp!=0)goto done;out->plaintext_bytes=total;out->chunk_bytes=chunk_bytes;out->tree_level=level;out->chunk_count=chunks;snprintf(out->object_id,sizeof(out->object_id),"%s",oid);id_to_hex(root.id,out->root_index_id);rc=0;
done:if(refs)fclose(refs);if(in)fclose(in);if(plain){OPENSSL_cleanse(plain,chunk_bytes);free(plain);}qrx_drive_key_envelope_free(&env);OPENSSL_cleanse(file_key,sizeof(file_key));OPENSSL_cleanse(salt,sizeof(salt));return rc;}

static int emit_chunk(QrxStorageFs*fs,const ObjRef*r,const uint8_t key[32],const uint8_t salt[32],uint64_t index,FILE*out,uint64_t*written){char id[65];id_to_hex(r->id,id);uint8_t*b=NULL;size_t n=0;if(qrx_storage_fs_read(fs,id,&b,&n)!=0||verify_cas(id,b,n)!=0||n<8+4+8+4+12+16||memcmp(b,CHK_MAGIC,8)||g32(b+8)!=QRX_DRIVE_OBJECT_VERSION){free(b);return -1;}size_t o=12;uint64_t ci=g64(b+o);o+=8;uint32_t plen=g32(b+o);o+=4;if(ci!=index||r->bytes!=plen||n-o<28||(size_t)plen!=n-(o+28)){free(b);return -1;}QrxDriveCiphertext ct={0};memcpy(ct.nonce,b+o,12);o+=12;memcpy(ct.tag,b+o,16);o+=16;ct.ciphertext=b+o;ct.ciphertext_len=plen;uint8_t aad[44];make_aad(salt,index,plen,aad);uint8_t*pt=NULL;size_t ptn=0;int rc=qrx_drive_decrypt(key,&ct,aad,sizeof(aad),&pt,&ptn);if(rc==0&&(ptn!=plen||(ptn&&fwrite(pt,1,ptn,out)!=ptn)))rc=-1;if(rc==0){if(UINT64_MAX-*written<ptn)rc=-1;else *written+=ptn;}if(pt){OPENSSL_cleanse(pt,ptn);free(pt);}OPENSSL_cleanse(b,n);free(b);return rc;}

static int emit_index(QrxStorageFs*fs,const uint8_t idb[32],uint32_t expected_level,const uint8_t key[32],const uint8_t salt[32],FILE*out,uint64_t*chunk_index,uint64_t*written){char id[65];id_to_hex(idb,id);uint8_t*b=NULL;size_t n=0;if(qrx_storage_fs_read(fs,id,&b,&n)!=0||verify_cas(id,b,n)!=0||n<28||memcmp(b,IDX_MAGIC,8)||g32(b+8)!=QRX_DRIVE_OBJECT_VERSION){free(b);return -1;}uint32_t level=g32(b+12),count=g32(b+16);uint64_t sum=g64(b+20);if(level!=expected_level||!count||count>QRX_DRIVE_OBJECT_INDEX_FANOUT||n!=28+(size_t)count*REF_BYTES){free(b);return -1;}uint64_t local=0;size_t o=28;for(uint32_t i=0;i<count;i++){ObjRef r;memcpy(r.id,b+o,32);r.bytes=g64(b+o+32);o+=40;if(UINT64_MAX-local<r.bytes){free(b);return -1;}local+=r.bytes;int rc;if(level==0){rc=emit_chunk(fs,&r,key,salt,*chunk_index,out,written);(*chunk_index)++;}else rc=emit_index(fs,r.id,level-1,key,salt,out,chunk_index,written);if(rc!=0){free(b);return -1;}}free(b);return local==sum?0:-1;}

int qrx_drive_object_get_file(QrxStorageFs*fs,const char*object_id,EVP_PKEY*recipient,const char*dst,QrxDriveObjectInfo*info){if(!fs||!object_id||!recipient||!dst)return -1;uint8_t*b=NULL;size_t n=0;if(qrx_storage_fs_read(fs,object_id,&b,&n)!=0||verify_cas(object_id,b,n)!=0){free(b);return -1;}ObjDescriptor d={0};if(parse_descriptor(b,n,&d)!=0){free(b);return -1;}free(b);uint8_t key[32]={0};if(qrx_drive_unwrap_file_key(recipient,&d.env,key)!=0){qrx_drive_key_envelope_free(&d.env);return -1;}char tmp[4096];if(snprintf(tmp,sizeof(tmp),"%s.qrxpart",dst)>=(int)sizeof(tmp)){qrx_drive_key_envelope_free(&d.env);OPENSSL_cleanse(key,32);return -1;}FILE*out=fopen(tmp,"wb");if(!out){qrx_drive_key_envelope_free(&d.env);OPENSSL_cleanse(key,32);return -1;}uint64_t ci=0,written=0;int rc=emit_index(fs,d.root_id,d.tree_level,key,d.salt,out,&ci,&written);if(fflush(out)!=0)rc=-1;if(fclose(out)!=0)rc=-1;if(rc==0&&written!=d.total_bytes)rc=-1;if(rc==0){
#ifdef _WIN32
        remove(dst);
#endif
        if(rename(tmp,dst)!=0)rc=-1;}
    if(rc!=0)remove(tmp);if(rc==0&&info){memset(info,0,sizeof(*info));info->plaintext_bytes=d.total_bytes;info->chunk_bytes=d.chunk_bytes;info->tree_level=d.tree_level;info->chunk_count=ci;snprintf(info->object_id,sizeof(info->object_id),"%s",object_id);id_to_hex(d.root_id,info->root_index_id);}qrx_drive_key_envelope_free(&d.env);OPENSSL_cleanse(key,32);return rc;}
