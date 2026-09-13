#include "compute/qrx_aura_fabric_gossip.h"
#include <openssl/crypto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POD_MAGIC "QRXAUPD1"
#define MODEL_MAGIC "QRXAUMD1"
#define POD_CACHE_MAGIC "QRXAUPC1"
#define MODEL_CACHE_MAGIC "QRXAUMC1"

typedef struct { uint8_t *p; size_t n,cap; } Buf;
typedef struct { const uint8_t *p; size_t n,o; } Rd;

static int bounded_text(const char*s,size_t cap,int allow_empty){
    if(!s||!memchr(s,'\0',cap)) return 0;
    if(!allow_empty&&!s[0]) return 0;
    for(size_t i=0;s[i];i++){
        unsigned char c=(unsigned char)s[i];
        if(c<0x21||c>0x7e||c=='|') return 0;
    }
    return 1;
}
static int hex64(const char*s){if(!s||strlen(s)!=64)return 0;for(int i=0;i<64;i++){char c=s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return 0;}return 1;}
static int endpoint_ok(const char*s){return bounded_text(s,QRX_AURA_GOSSIP_MAX_ENDPOINT,0)&&(!strncmp(s,"qrxp2p://",9)||!strncmp(s,"quic://",7));}
static int grow(Buf*b,size_t add){if(add>SIZE_MAX-b->n)return -1;size_t need=b->n+add;if(need<=b->cap)return 0;size_t c=b->cap?b->cap*2:512;while(c<need){if(c>SIZE_MAX/2){c=need;break;}c*=2;}uint8_t*p=realloc(b->p,c);if(!p)return -1;b->p=p;b->cap=c;return 0;}
static int put_bytes(Buf*b,const void*p,size_t n){if(grow(b,n))return -1;if(n)memcpy(b->p+b->n,p,n);b->n+=n;return 0;}
static int put_u8(Buf*b,uint8_t v){return put_bytes(b,&v,1);}
static int put_u16(Buf*b,uint16_t v){uint8_t x[2]={(uint8_t)(v>>8),(uint8_t)v};return put_bytes(b,x,2);}
static int put_u32(Buf*b,uint32_t v){uint8_t x[4]={(uint8_t)(v>>24),(uint8_t)(v>>16),(uint8_t)(v>>8),(uint8_t)v};return put_bytes(b,x,4);}
static int put_u64(Buf*b,uint64_t v){uint8_t x[8];for(int i=7;i>=0;i--){x[i]=(uint8_t)v;v>>=8;}return put_bytes(b,x,8);}
static int put_str(Buf*b,const char*s,size_t max){size_t n=strlen(s);if(n>max||n>UINT16_MAX)return -1;return put_u16(b,(uint16_t)n)||put_bytes(b,s,n);}
static int rd_bytes(Rd*r,void*out,size_t n){if(!r||n>r->n-r->o)return -1;if(out&&n)memcpy(out,r->p+r->o,n);r->o+=n;return 0;}
static int rd_u8(Rd*r,uint8_t*out){return rd_bytes(r,out,1);}
static int rd_u16(Rd*r,uint16_t*out){uint8_t x[2];if(rd_bytes(r,x,2))return -1;*out=((uint16_t)x[0]<<8)|x[1];return 0;}
static int rd_u32(Rd*r,uint32_t*out){uint8_t x[4];if(rd_bytes(r,x,4))return -1;*out=((uint32_t)x[0]<<24)|((uint32_t)x[1]<<16)|((uint32_t)x[2]<<8)|x[3];return 0;}
static int rd_u64(Rd*r,uint64_t*out){uint8_t x[8];if(rd_bytes(r,x,8))return -1;uint64_t v=0;for(int i=0;i<8;i++)v=(v<<8)|x[i];*out=v;return 0;}
static int rd_str(Rd*r,char*out,size_t cap){uint16_t n=0;if(rd_u16(r,&n)||!cap||n>=cap||rd_bytes(r,out,n))return -1;out[n]=0;return 0;}

static int encode_pod_unsigned(const QrxAuraPodAnnouncement*a,Buf*b){
    const QrxAuraPodCapacity*p=&a->capacity;
    return put_u32(b,a->version)||put_u64(b,a->sequence)||put_u64(b,a->valid_from_height)||put_u64(b,a->valid_until_height)||put_str(b,a->service_endpoint,QRX_AURA_GOSSIP_MAX_ENDPOINT-1)||
      put_u32(b,p->version)||put_str(b,p->provider_id,QRX_GLOBE_PROVIDER_MAX)||put_str(b,p->pod_id,QRX_MOE_MAX_POD_ID-1)||put_str(b,p->region,QRX_GLOBE_REGION_MAX)||
      put_u32(b,(uint32_t)p->backend)||put_u32(b,p->accelerator_features)||put_u32(b,p->role_mask)||put_u64(b,p->usable_memory_bytes)||put_u64(b,p->free_memory_bytes)||put_u64(b,p->model_cache_free_bytes)||put_u64(b,p->measured_milli_tokens_per_second)||put_u32(b,p->network_egress_mbps)||put_u32(b,p->latency_ms)||put_u32(b,p->utilization_bps)||put_u32(b,p->reliability_bps)||put_u32(b,p->cache_hit_bps)||put_u32(b,p->expert_locality_bps)||put_u32(b,p->max_context_tokens)||put_u32(b,p->node_count)||put_u8(b,p->available)||put_str(b,p->calibration_commitment,64);
}
static int decode_pod_unsigned(const uint8_t*p,size_t n,QrxAuraPodAnnouncement*a){
    Rd r={p,n,0};uint32_t be=0;memset(a,0,sizeof(*a));
    if(rd_u32(&r,&a->version)||rd_u64(&r,&a->sequence)||rd_u64(&r,&a->valid_from_height)||rd_u64(&r,&a->valid_until_height)||rd_str(&r,a->service_endpoint,sizeof(a->service_endpoint))||rd_u32(&r,&a->capacity.version)||rd_str(&r,a->capacity.provider_id,sizeof(a->capacity.provider_id))||rd_str(&r,a->capacity.pod_id,sizeof(a->capacity.pod_id))||rd_str(&r,a->capacity.region,sizeof(a->capacity.region))||rd_u32(&r,&be)) return -1;
    a->capacity.backend=(QrxMoeRuntimeBackend)be;
    if(rd_u32(&r,&a->capacity.accelerator_features)||rd_u32(&r,&a->capacity.role_mask)||rd_u64(&r,&a->capacity.usable_memory_bytes)||rd_u64(&r,&a->capacity.free_memory_bytes)||rd_u64(&r,&a->capacity.model_cache_free_bytes)||rd_u64(&r,&a->capacity.measured_milli_tokens_per_second)||rd_u32(&r,&a->capacity.network_egress_mbps)||rd_u32(&r,&a->capacity.latency_ms)||rd_u32(&r,&a->capacity.utilization_bps)||rd_u32(&r,&a->capacity.reliability_bps)||rd_u32(&r,&a->capacity.cache_hit_bps)||rd_u32(&r,&a->capacity.expert_locality_bps)||rd_u32(&r,&a->capacity.max_context_tokens)||rd_u32(&r,&a->capacity.node_count)||rd_u8(&r,&a->capacity.available)||rd_str(&r,a->capacity.calibration_commitment,sizeof(a->capacity.calibration_commitment))||r.o!=r.n) return -1;
    return 0;
}
static int encode_model_unsigned(const QrxAuraModelProfileAnnouncement*a,Buf*b){
    const QrxAuraModelCapabilityProfile*m=&a->profile;
    return put_u32(b,a->version)||put_str(b,a->publisher_id,QRX_GLOBE_PROVIDER_MAX)||put_u64(b,a->sequence)||put_u64(b,a->valid_from_height)||put_u64(b,a->valid_until_height)||put_str(b,a->model_registry_commitment,64)||put_u32(b,m->version)||put_str(b,m->model_id,QRX_MODEL_MAX_ID-1)||put_str(b,m->model_version,QRX_MODEL_MAX_VERSION-1)||put_str(b,m->family,QRX_AURA_FABRIC_MAX_FAMILY-1)||put_str(b,m->runtime_id,QRX_MODEL_MAX_RUNTIME-1)||put_str(b,m->quantization,QRX_AURA_FABRIC_MAX_QUANT-1)||put_u32(b,(uint32_t)m->tier)||put_u64(b,m->min_memory_bytes)||put_u64(b,m->recommended_memory_bytes)||put_u64(b,m->min_storage_bytes)||put_u64(b,m->min_aggregate_milli_tokens_per_second)||put_u32(b,m->min_pods)||put_u32(b,m->min_network_mbps)||put_u32(b,m->max_latency_ms)||put_u32(b,m->min_reliability_bps)||put_u32(b,m->min_cache_hit_bps)||put_u32(b,m->min_expert_locality_bps)||put_u32(b,m->max_context_tokens)||put_u32(b,m->quality_bps)||put_u32(b,m->coding_bps)||put_u32(b,m->reasoning_bps)||put_u32(b,m->rag_bps)||put_u32(b,m->tool_use_bps)||put_u32(b,m->required_accelerator_features)||put_u8(b,m->is_moe)||put_u8(b,m->supports_single_device);
}
static int decode_model_unsigned(const uint8_t*p,size_t n,QrxAuraModelProfileAnnouncement*a){
    Rd r={p,n,0};uint32_t tier=0;memset(a,0,sizeof(*a));QrxAuraModelCapabilityProfile*m=&a->profile;
    if(rd_u32(&r,&a->version)||rd_str(&r,a->publisher_id,sizeof(a->publisher_id))||rd_u64(&r,&a->sequence)||rd_u64(&r,&a->valid_from_height)||rd_u64(&r,&a->valid_until_height)||rd_str(&r,a->model_registry_commitment,sizeof(a->model_registry_commitment))||rd_u32(&r,&m->version)||rd_str(&r,m->model_id,sizeof(m->model_id))||rd_str(&r,m->model_version,sizeof(m->model_version))||rd_str(&r,m->family,sizeof(m->family))||rd_str(&r,m->runtime_id,sizeof(m->runtime_id))||rd_str(&r,m->quantization,sizeof(m->quantization))||rd_u32(&r,&tier)) return -1;
    m->tier=(QrxAuraModelTier)tier;
    if(rd_u64(&r,&m->min_memory_bytes)||rd_u64(&r,&m->recommended_memory_bytes)||rd_u64(&r,&m->min_storage_bytes)||rd_u64(&r,&m->min_aggregate_milli_tokens_per_second)||rd_u32(&r,&m->min_pods)||rd_u32(&r,&m->min_network_mbps)||rd_u32(&r,&m->max_latency_ms)||rd_u32(&r,&m->min_reliability_bps)||rd_u32(&r,&m->min_cache_hit_bps)||rd_u32(&r,&m->min_expert_locality_bps)||rd_u32(&r,&m->max_context_tokens)||rd_u32(&r,&m->quality_bps)||rd_u32(&r,&m->coding_bps)||rd_u32(&r,&m->reasoning_bps)||rd_u32(&r,&m->rag_bps)||rd_u32(&r,&m->tool_use_bps)||rd_u32(&r,&m->required_accelerator_features)||rd_u8(&r,&m->is_moe)||rd_u8(&r,&m->supports_single_device)||r.o!=r.n) return -1;
    return 0;
}

static int digest_payload(const char*domain,const uint8_t*p,size_t n,uint8_t out[32]){EVP_MD_CTX*c=EVP_MD_CTX_new();unsigned z=0;int rc=-1;if(c&&EVP_DigestInit_ex(c,EVP_sha3_256(),NULL)==1&&EVP_DigestUpdate(c,domain,strlen(domain))==1&&EVP_DigestUpdate(c,p,n)==1&&EVP_DigestFinal_ex(c,out,&z)==1&&z==32)rc=0;EVP_MD_CTX_free(c);return rc;}
static int sign_hash(EVP_PKEY*k,const uint8_t h[32],uint8_t**sig,size_t*sl){if(!k||!sig||!sl)return -1;*sig=NULL;*sl=0;EVP_MD_CTX*c=EVP_MD_CTX_new();size_t n=0;uint8_t*s=NULL;int rc=-1;if(!c||EVP_DigestSignInit_ex(c,NULL,NULL,NULL,NULL,k,NULL)!=1||EVP_DigestSign(c,NULL,&n,h,32)!=1||!n||n>QRX_AURA_GOSSIP_MAX_SIGNATURE)goto done;s=malloc(n);if(!s)goto done;if(EVP_DigestSign(c,s,&n,h,32)!=1)goto done;*sig=s;*sl=n;s=NULL;rc=0;done:free(s);EVP_MD_CTX_free(c);return rc;}
static int verify_hash(EVP_PKEY*k,const uint8_t h[32],const uint8_t*sig,size_t sl){if(!k||!sig||!sl||sl>QRX_AURA_GOSSIP_MAX_SIGNATURE)return -1;EVP_MD_CTX*c=EVP_MD_CTX_new();int rc=-1;if(c&&EVP_DigestVerifyInit_ex(c,NULL,NULL,NULL,NULL,k,NULL)==1&&EVP_DigestVerify(c,sig,sl,h,32)==1)rc=0;EVP_MD_CTX_free(c);return rc;}

int qrx_aura_pod_announcement_validate(const QrxAuraPodAnnouncement*a,uint64_t h){if(!a||a->version!=QRX_AURA_GOSSIP_VERSION||!a->sequence||a->valid_until_height<a->valid_from_height||h<a->valid_from_height||h>a->valid_until_height||a->valid_until_height-a->valid_from_height>1000000u||!endpoint_ok(a->service_endpoint)||qrx_aura_pod_capacity_validate(&a->capacity))return -1;if(!a->capacity.available)return -1;return 0;}
int qrx_aura_pod_announcement_hash(const QrxAuraPodAnnouncement*a,uint8_t out[32]){if(!a||!out)return -1;Buf b={0};int rc=encode_pod_unsigned(a,&b)?-1:digest_payload("QRX/AURA/POD-GOSSIP/V1",b.p,b.n,out);free(b.p);return rc;}
int qrx_aura_pod_announcement_sign(EVP_PKEY*k,const QrxAuraPodAnnouncement*a,uint8_t**sig,size_t*sl){uint8_t h[32];return qrx_aura_pod_announcement_hash(a,h)||sign_hash(k,h,sig,sl);}
int qrx_aura_pod_announcement_verify(EVP_PKEY*k,const QrxAuraPodAnnouncement*a,const uint8_t*sig,size_t sl){uint8_t h[32];return qrx_aura_pod_announcement_hash(a,h)||verify_hash(k,h,sig,sl);}

static int wire_encode(const char magic[8],Buf*payload,const uint8_t*sig,size_t sl,uint8_t**out,size_t*outn){if(!sig||!sl||sl>QRX_AURA_GOSSIP_MAX_SIGNATURE||!out||!outn||payload->n>QRX_AURA_GOSSIP_MAX_WIRE)return -1;Buf b={0};if(put_bytes(&b,magic,8)||put_u32(&b,(uint32_t)payload->n)||put_bytes(&b,payload->p,payload->n)||put_u32(&b,(uint32_t)sl)||put_bytes(&b,sig,sl)||b.n>QRX_AURA_GOSSIP_MAX_WIRE){free(b.p);return -1;}*out=b.p;*outn=b.n;return 0;}
static int wire_decode(const uint8_t*in,size_t n,const char magic[8],const uint8_t**payload,size_t*pn,uint8_t**sig,size_t*sl){if(!in||n<16||n>QRX_AURA_GOSSIP_MAX_WIRE||memcmp(in,magic,8)||!payload||!pn||!sig||!sl)return -1;Rd r={in+8,n-8,0};uint32_t pz=0,sz=0;if(rd_u32(&r,&pz)||!pz||pz>QRX_AURA_GOSSIP_MAX_WIRE||pz>r.n-r.o){return -1;}*payload=r.p+r.o;r.o+=pz;if(rd_u32(&r,&sz)||!sz||sz>QRX_AURA_GOSSIP_MAX_SIGNATURE||sz!=r.n-r.o)return -1;uint8_t*s=malloc(sz);if(!s)return -1;memcpy(s,r.p+r.o,sz);*pn=pz;*sig=s;*sl=sz;return 0;}
int qrx_aura_pod_wire_encode(const QrxAuraPodAnnouncement*a,const uint8_t*sig,size_t sl,uint8_t**out,size_t*outn){Buf b={0};if(!a||encode_pod_unsigned(a,&b))return -1;int rc=wire_encode(POD_MAGIC,&b,sig,sl,out,outn);free(b.p);return rc;}
int qrx_aura_pod_wire_decode(const uint8_t*in,size_t n,QrxAuraPodAnnouncement*a,uint8_t**sig,size_t*sl){const uint8_t*p=NULL;size_t pn=0;if(wire_decode(in,n,POD_MAGIC,&p,&pn,sig,sl))return -1;if(decode_pod_unsigned(p,pn,a)){free(*sig);*sig=NULL;*sl=0;return -1;}return 0;}

void qrx_aura_pod_gossip_init(QrxAuraPodGossipTable*t){if(t)memset(t,0,sizeof(*t));}
static void pod_entry_free(QrxAuraPodGossipEntry*e){if(e){free(e->signature);memset(e,0,sizeof(*e));}}
void qrx_aura_pod_gossip_free(QrxAuraPodGossipTable*t){if(!t)return;for(size_t i=0;i<t->count;i++)pod_entry_free(&t->entries[i]);free(t->entries);memset(t,0,sizeof(*t));}
static long pod_find_idx(const QrxAuraPodGossipTable*t,const char*id){if(!t||!id)return -1;for(size_t i=0;i<t->count;i++)if(!strcmp(t->entries[i].announcement.capacity.pod_id,id))return (long)i;return -1;}
const QrxAuraPodAnnouncement*qrx_aura_pod_gossip_find(const QrxAuraPodGossipTable*t,const char*id){long i=pod_find_idx(t,id);return i<0?NULL:&t->entries[i].announcement;}
static int pod_reserve(QrxAuraPodGossipTable*t,size_t n){if(n<=t->capacity)return 0;if(n>QRX_AURA_GOSSIP_MAX_PODS)return -1;size_t c=t->capacity?t->capacity*2:64;if(c<n)c=n;if(c>QRX_AURA_GOSSIP_MAX_PODS)c=QRX_AURA_GOSSIP_MAX_PODS;void*p=realloc(t->entries,c*sizeof(*t->entries));if(!p)return -1;t->entries=p;t->capacity=c;return 0;}
int qrx_aura_pod_gossip_ingest(QrxAuraPodGossipTable*t,const QrxAuraPodAnnouncement*a,const uint8_t*sig,size_t sl,uint64_t h,QrxAuraGossipKeyLookupFn lookup,void*ctx){if(!t||!a||!sig||!sl||!lookup||qrx_aura_pod_announcement_validate(a,h))return -1;EVP_PKEY*k=NULL;if(lookup(ctx,a->capacity.provider_id,&k)||!k)return -2;int vr=qrx_aura_pod_announcement_verify(k,a,sig,sl);EVP_PKEY_free(k);if(vr)return -3;long i=pod_find_idx(t,a->capacity.pod_id);if(i>=0){QrxAuraPodGossipEntry*e=&t->entries[i];if(strcmp(e->announcement.capacity.provider_id,a->capacity.provider_id)||a->sequence<=e->announcement.sequence)return -4;}uint8_t*s=malloc(sl);if(!s)return -5;memcpy(s,sig,sl);if(i<0){if(pod_reserve(t,t->count+1)){free(s);return -5;}i=(long)t->count++;memset(&t->entries[i],0,sizeof(t->entries[i]));}else free(t->entries[i].signature);t->entries[i].announcement=*a;t->entries[i].signature=s;t->entries[i].signature_len=sl;t->entries[i].learned_height=h;t->revision++;return 0;}
size_t qrx_aura_pod_gossip_prune(QrxAuraPodGossipTable*t,uint64_t h){if(!t)return 0;size_t w=0,r=0;for(size_t i=0;i<t->count;i++){if(h>t->entries[i].announcement.valid_until_height){pod_entry_free(&t->entries[i]);r++;continue;}if(w!=i)t->entries[w]=t->entries[i];w++;}t->count=w;if(r)t->revision++;return r;}

int qrx_aura_model_announcement_validate(const QrxAuraModelProfileAnnouncement*a,uint64_t h){if(!a||a->version!=QRX_AURA_GOSSIP_VERSION||!bounded_text(a->publisher_id,sizeof(a->publisher_id),0)||!a->sequence||a->valid_until_height<a->valid_from_height||h<a->valid_from_height||h>a->valid_until_height||a->valid_until_height-a->valid_from_height>1000000u||!hex64(a->model_registry_commitment)||qrx_aura_model_profile_validate(&a->profile))return -1;return 0;}
int qrx_aura_model_announcement_hash(const QrxAuraModelProfileAnnouncement*a,uint8_t out[32]){if(!a||!out)return -1;Buf b={0};int rc=encode_model_unsigned(a,&b)?-1:digest_payload("QRX/AURA/MODEL-PROFILE-GOSSIP/V1",b.p,b.n,out);free(b.p);return rc;}
int qrx_aura_model_announcement_sign(EVP_PKEY*k,const QrxAuraModelProfileAnnouncement*a,uint8_t**sig,size_t*sl){uint8_t h[32];return qrx_aura_model_announcement_hash(a,h)||sign_hash(k,h,sig,sl);}
int qrx_aura_model_announcement_verify(EVP_PKEY*k,const QrxAuraModelProfileAnnouncement*a,const uint8_t*sig,size_t sl){uint8_t h[32];return qrx_aura_model_announcement_hash(a,h)||verify_hash(k,h,sig,sl);}
int qrx_aura_model_wire_encode(const QrxAuraModelProfileAnnouncement*a,const uint8_t*sig,size_t sl,uint8_t**out,size_t*outn){Buf b={0};if(!a||encode_model_unsigned(a,&b))return -1;int rc=wire_encode(MODEL_MAGIC,&b,sig,sl,out,outn);free(b.p);return rc;}
int qrx_aura_model_wire_decode(const uint8_t*in,size_t n,QrxAuraModelProfileAnnouncement*a,uint8_t**sig,size_t*sl){const uint8_t*p=NULL;size_t pn=0;if(wire_decode(in,n,MODEL_MAGIC,&p,&pn,sig,sl))return -1;if(decode_model_unsigned(p,pn,a)){free(*sig);*sig=NULL;*sl=0;return -1;}return 0;}

void qrx_aura_model_gossip_init(QrxAuraModelGossipTable*t){if(t)memset(t,0,sizeof(*t));}
static void model_entry_free(QrxAuraModelGossipEntry*e){if(e){free(e->signature);memset(e,0,sizeof(*e));}}
void qrx_aura_model_gossip_free(QrxAuraModelGossipTable*t){if(!t)return;for(size_t i=0;i<t->count;i++)model_entry_free(&t->entries[i]);free(t->entries);memset(t,0,sizeof(*t));}
static long model_find_idx(const QrxAuraModelGossipTable*t,const char*id,const char*ver){if(!t||!id||!ver)return -1;for(size_t i=0;i<t->count;i++){const QrxAuraModelCapabilityProfile*m=&t->entries[i].announcement.profile;if(!strcmp(m->model_id,id)&&!strcmp(m->model_version,ver))return (long)i;}return -1;}
const QrxAuraModelProfileAnnouncement*qrx_aura_model_gossip_find(const QrxAuraModelGossipTable*t,const char*id,const char*ver){long i=model_find_idx(t,id,ver);return i<0?NULL:&t->entries[i].announcement;}
static int model_reserve(QrxAuraModelGossipTable*t,size_t n){if(n<=t->capacity)return 0;if(n>QRX_AURA_GOSSIP_MAX_MODELS)return -1;size_t c=t->capacity?t->capacity*2:32;if(c<n)c=n;if(c>QRX_AURA_GOSSIP_MAX_MODELS)c=QRX_AURA_GOSSIP_MAX_MODELS;void*p=realloc(t->entries,c*sizeof(*t->entries));if(!p)return -1;t->entries=p;t->capacity=c;return 0;}
int qrx_aura_model_gossip_ingest(QrxAuraModelGossipTable*t,const QrxAuraModelProfileAnnouncement*a,const uint8_t*sig,size_t sl,uint64_t h,QrxAuraGossipKeyLookupFn lookup,void*kctx,QrxAuraModelPublisherAuthorizeFn auth,void*actx){if(!t||!a||!sig||!sl||!lookup||qrx_aura_model_announcement_validate(a,h))return -1;if(auth&&auth(actx,a->publisher_id))return -2;EVP_PKEY*k=NULL;if(lookup(kctx,a->publisher_id,&k)||!k)return -3;int vr=qrx_aura_model_announcement_verify(k,a,sig,sl);EVP_PKEY_free(k);if(vr)return -4;long i=model_find_idx(t,a->profile.model_id,a->profile.model_version);if(i>=0&&a->sequence<=t->entries[i].announcement.sequence)return -5;uint8_t*s=malloc(sl);if(!s)return -6;memcpy(s,sig,sl);if(i<0){if(model_reserve(t,t->count+1)){free(s);return -6;}i=(long)t->count++;memset(&t->entries[i],0,sizeof(t->entries[i]));}else free(t->entries[i].signature);t->entries[i].announcement=*a;t->entries[i].signature=s;t->entries[i].signature_len=sl;t->entries[i].learned_height=h;t->revision++;return 0;}
size_t qrx_aura_model_gossip_prune(QrxAuraModelGossipTable*t,uint64_t h){if(!t)return 0;size_t w=0,r=0;for(size_t i=0;i<t->count;i++){if(h>t->entries[i].announcement.valid_until_height){model_entry_free(&t->entries[i]);r++;continue;}if(w!=i)t->entries[w]=t->entries[i];w++;}t->count=w;if(r)t->revision++;return r;}

static int cache_save_common(const char magic[8],const char*path,size_t count,int (*enc)(size_t,uint8_t**,size_t*,void*),void*ctx){if(!path||!enc)return -1;char tmp[1400];snprintf(tmp,sizeof(tmp),"%s.tmp",path);FILE*f=fopen(tmp,"wb");if(!f)return -1;if(fwrite(magic,1,8,f)!=8){fclose(f);remove(tmp);return -1;}for(size_t i=0;i<count;i++){uint8_t*w=NULL;size_t n=0;if(enc(i,&w,&n,ctx)||n>QRX_AURA_GOSSIP_MAX_WIRE){free(w);fclose(f);remove(tmp);return -1;}uint8_t z[4]={(uint8_t)(n>>24),(uint8_t)(n>>16),(uint8_t)(n>>8),(uint8_t)n};if(fwrite(z,1,4,f)!=4||fwrite(w,1,n,f)!=n){free(w);fclose(f);remove(tmp);return -1;}free(w);}if(fclose(f)){remove(tmp);return -1;}remove(path);return rename(tmp,path)==0?0:-1;}
static int pod_cache_enc(size_t i,uint8_t**w,size_t*n,void*ctx){QrxAuraPodGossipTable*t=ctx;return qrx_aura_pod_wire_encode(&t->entries[i].announcement,t->entries[i].signature,t->entries[i].signature_len,w,n);}
static int model_cache_enc(size_t i,uint8_t**w,size_t*n,void*ctx){QrxAuraModelGossipTable*t=ctx;return qrx_aura_model_wire_encode(&t->entries[i].announcement,t->entries[i].signature,t->entries[i].signature_len,w,n);}
int qrx_aura_pod_gossip_cache_save(const QrxAuraPodGossipTable*t,const char*path){return t?cache_save_common(POD_CACHE_MAGIC,path,t->count,pod_cache_enc,(void*)t):-1;}
int qrx_aura_model_gossip_cache_save(const QrxAuraModelGossipTable*t,const char*path){return t?cache_save_common(MODEL_CACHE_MAGIC,path,t->count,model_cache_enc,(void*)t):-1;}

static int read_record(FILE*f,uint8_t**wire,size_t*n){uint8_t z[4];size_t r=fread(z,1,4,f);if(!r)return 1;if(r!=4)return -1;uint32_t x=((uint32_t)z[0]<<24)|((uint32_t)z[1]<<16)|((uint32_t)z[2]<<8)|z[3];if(!x||x>QRX_AURA_GOSSIP_MAX_WIRE)return -1;uint8_t*b=malloc(x);if(!b)return -1;if(fread(b,1,x,f)!=x){free(b);return -1;}*wire=b;*n=x;return 0;}
int qrx_aura_pod_gossip_cache_load(QrxAuraPodGossipTable*t,const char*path,uint64_t h,QrxAuraGossipKeyLookupFn lookup,void*ctx){if(!t||!path||!lookup)return -1;FILE*f=fopen(path,"rb");if(!f)return -1;char m[8];if(fread(m,1,8,f)!=8||memcmp(m,POD_CACHE_MAGIC,8)){fclose(f);return -1;}int accepted=0;for(;;){uint8_t*w=NULL,*s=NULL;size_t n=0,sl=0;int rr=read_record(f,&w,&n);if(rr==1)break;if(rr){accepted=-1;break;}QrxAuraPodAnnouncement a;if(!qrx_aura_pod_wire_decode(w,n,&a,&s,&sl)&&!qrx_aura_pod_gossip_ingest(t,&a,s,sl,h,lookup,ctx))accepted++;free(s);free(w);}fclose(f);return accepted;}
int qrx_aura_model_gossip_cache_load(QrxAuraModelGossipTable*t,const char*path,uint64_t h,QrxAuraGossipKeyLookupFn lookup,void*kctx,QrxAuraModelPublisherAuthorizeFn auth,void*actx){if(!t||!path||!lookup)return -1;FILE*f=fopen(path,"rb");if(!f)return -1;char m[8];if(fread(m,1,8,f)!=8||memcmp(m,MODEL_CACHE_MAGIC,8)){fclose(f);return -1;}int accepted=0;for(;;){uint8_t*w=NULL,*s=NULL;size_t n=0,sl=0;int rr=read_record(f,&w,&n);if(rr==1)break;if(rr){accepted=-1;break;}QrxAuraModelProfileAnnouncement a;if(!qrx_aura_model_wire_decode(w,n,&a,&s,&sl)&&!qrx_aura_model_gossip_ingest(t,&a,s,sl,h,lookup,kctx,auth,actx))accepted++;free(s);free(w);}fclose(f);return accepted;}

static int collect_live_pods(const QrxAuraPodGossipTable*t,uint64_t h,QrxAuraPodCapacity**out,size_t*n){*out=NULL;*n=0;if(!t)return 0;QrxAuraPodCapacity*p=calloc(t->count?t->count:1,sizeof(*p));if(!p)return -1;size_t z=0;for(size_t i=0;i<t->count;i++){const QrxAuraPodAnnouncement*a=&t->entries[i].announcement;if(h<a->valid_from_height||h>a->valid_until_height||!a->capacity.available)continue;p[z++]=a->capacity;}*out=p;*n=z;return 0;}
static int collect_live_models(const QrxAuraModelGossipTable*t,uint64_t h,QrxAuraModelCapabilityProfile**out,size_t*n){*out=NULL;*n=0;if(!t)return 0;QrxAuraModelCapabilityProfile*m=calloc(t->count?t->count:1,sizeof(*m));if(!m)return -1;size_t z=0;for(size_t i=0;i<t->count;i++){const QrxAuraModelProfileAnnouncement*a=&t->entries[i].announcement;if(h<a->valid_from_height||h>a->valid_until_height)continue;m[z++]=a->profile;}*out=m;*n=z;return 0;}
int qrx_aura_gossip_live_snapshot(const QrxAuraPodGossipTable*p,const QrxAuraModelGossipTable*m,uint64_t h,size_t privacy,QrxAuraFabricSnapshot*out,QrxAuraGlobeCell**cells,size_t*cn){QrxAuraPodCapacity*pv=NULL;QrxAuraModelCapabilityProfile*mv=NULL;size_t pn=0,mn=0;if(collect_live_pods(p,h,&pv,&pn)||collect_live_models(m,h,&mv,&mn)){free(pv);free(mv);return -1;}int rc=qrx_aura_fabric_build(pv,pn,mv,mn,privacy,out,cells,cn);free(pv);free(mv);return rc;}
int qrx_aura_gossip_live_route(const QrxAuraPodGossipTable*p,const QrxAuraModelGossipTable*m,uint64_t h,const QrxAuraRouteRequest*r,QrxAuraRouteDecision*out){QrxAuraPodCapacity*pv=NULL;QrxAuraModelCapabilityProfile*mv=NULL;size_t pn=0,mn=0;if(collect_live_pods(p,h,&pv,&pn)||collect_live_models(m,h,&mv,&mn)){free(pv);free(mv);return -1;}int rc=qrx_aura_route_model(r,mv,mn,pv,pn,out);free(pv);free(mv);return rc;}
int qrx_aura_gossip_resource_globe(const QrxAuraPodGossipTable*t,uint64_t h,size_t privacy,QrxResourceGlobeCell**out,size_t*n){if(!out||!n)return -1;*out=NULL;*n=0;if(!t)return qrx_resource_globe_build(NULL,0,privacy,out,n);QrxResourceGlobeObservation*o=calloc(t->count?t->count:1,sizeof(*o));if(!o)return -1;size_t z=0;for(size_t i=0;i<t->count;i++){const QrxAuraPodAnnouncement*a=&t->entries[i].announcement;const QrxAuraPodCapacity*p=&a->capacity;if(h<a->valid_from_height||h>a->valid_until_height||!p->available)continue;QrxResourceGlobeObservation*x=&o[z++];x->version=QRX_RESOURCE_GLOBE_VERSION;snprintf(x->provider_id,sizeof(x->provider_id),"%s",p->provider_id);snprintf(x->region,sizeof(x->region),"%s",p->region);x->layer_mask=QRX_GLOBE_LAYER_NETWORK;if(p->role_mask&QRX_AURA_POD_ROLE_INFERENCE)x->layer_mask|=QRX_GLOBE_LAYER_AI;if(p->model_cache_free_bytes)x->layer_mask|=QRX_GLOBE_LAYER_MODEL_CACHE;x->ai_milli_tokens_per_second=p->measured_milli_tokens_per_second;x->model_cache_free_bytes=p->model_cache_free_bytes;x->network_egress_mbps=p->network_egress_mbps;x->latency_ms=p->latency_ms;x->utilization_bps=p->utilization_bps;x->reliability_bps=p->reliability_bps;x->demand_bps=p->utilization_bps;}int rc=qrx_resource_globe_build(o,z,privacy,out,n);free(o);return rc;}

/* 0.0.9.38 anti-entropy digest/merge. Roots are independent of local table
 * insertion order so peers can cheaply detect divergence before paginated pull. */
static int gossip_hash_cmp(const void *aa,const void *bb){return memcmp(aa,bb,32);}
static int gossip_root_from_hashes(uint8_t (*hashes)[32],size_t n,const char *domain,char out[65]){
    qsort(hashes,n,32,gossip_hash_cmp);EVP_MD_CTX*m=EVP_MD_CTX_new();uint8_t h[32];unsigned hn=0;if(!m)return-1;
    uint8_t cnt[8];uint64_t v=(uint64_t)n;for(int i=7;i>=0;i--){cnt[i]=(uint8_t)v;v>>=8;}
    int ok=EVP_DigestInit_ex(m,EVP_sha3_256(),NULL)==1&&EVP_DigestUpdate(m,domain,strlen(domain))==1&&EVP_DigestUpdate(m,cnt,8)==1;
    for(size_t i=0;ok&&i<n;i++)ok=EVP_DigestUpdate(m,hashes[i],32)==1;
    ok=ok&&EVP_DigestFinal_ex(m,h,&hn)==1&&hn==32;EVP_MD_CTX_free(m);if(!ok)return-1;static const char x[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=x[h[i]>>4];out[i*2+1]=x[h[i]&15];}out[64]=0;return 0;
}
int qrx_aura_gossip_digest(const QrxAuraPodGossipTable *p,const QrxAuraModelGossipTable *m,uint64_t h,QrxAuraGossipDigest *out){
    if(!p||!m||!out) return -1;
    uint8_t (*ph)[32]=calloc(p->count?p->count:1,32);
    uint8_t (*mh)[32]=calloc(m->count?m->count:1,32);
    if(!ph||!mh){free(ph);free(mh);return -2;}
    size_t pn=0,mn=0;
    for(size_t i=0;i<p->count;i++){
        const QrxAuraPodAnnouncement *a=&p->entries[i].announcement;
        if(h<a->valid_from_height||h>a->valid_until_height||!a->capacity.available) continue;
        if(qrx_aura_pod_announcement_hash(a,ph[pn])){free(ph);free(mh);return -3;}
        pn++;
    }
    for(size_t i=0;i<m->count;i++){
        const QrxAuraModelProfileAnnouncement *a=&m->entries[i].announcement;
        if(h<a->valid_from_height||h>a->valid_until_height) continue;
        if(qrx_aura_model_announcement_hash(a,mh[mn])){free(ph);free(mh);return -3;}
        mn++;
    }
    memset(out,0,sizeof(*out));
    out->version=QRX_AURA_GOSSIP_VERSION;out->pod_count=pn;out->model_count=mn;
    if(gossip_root_from_hashes(ph,pn,"QRX/AURA/ANTI-ENTROPY/PODS/V1",out->pod_root)||
       gossip_root_from_hashes(mh,mn,"QRX/AURA/ANTI-ENTROPY/MODELS/V1",out->model_root)){
        free(ph);free(mh);return -4;
    }
    free(ph);free(mh);
    EVP_MD_CTX *x=EVP_MD_CTX_new();uint8_t dig[32];unsigned dn=0;
    const char *domain="QRX/AURA/ANTI-ENTROPY/COMBINED/V1";
    if(!x||EVP_DigestInit_ex(x,EVP_sha3_256(),NULL)!=1||EVP_DigestUpdate(x,domain,strlen(domain))!=1||
       EVP_DigestUpdate(x,out->pod_root,64)!=1||EVP_DigestUpdate(x,out->model_root,64)!=1||
       EVP_DigestFinal_ex(x,dig,&dn)!=1||dn!=32){if(x)EVP_MD_CTX_free(x);return -4;}
    EVP_MD_CTX_free(x);
    static const char hx[]="0123456789abcdef";
    for(int i=0;i<32;i++){out->combined_root[i*2]=hx[dig[i]>>4];out->combined_root[i*2+1]=hx[dig[i]&15];}
    out->combined_root[64]=0;
    return 0;
}
int qrx_aura_gossip_anti_entropy_merge(QrxAuraPodGossipTable *dp,QrxAuraModelGossipTable *dm,
                                       const QrxAuraPodGossipTable *sp,const QrxAuraModelGossipTable *sm,
                                       uint64_t h,QrxAuraGossipKeyLookupFn pkey,void *pctx,
                                       QrxAuraGossipKeyLookupFn mkey,void *mctx,QrxAuraModelPublisherAuthorizeFn auth,void *actx,
                                       size_t *pu,size_t *mu){
    if(!dp||!dm||!sp||!sm||!pkey||!mkey||!auth) return -1;
    size_t pc=0,mc=0;
    for(size_t i=0;i<sp->count;i++){
        const QrxAuraPodGossipEntry *e=&sp->entries[i];
        if(h<e->announcement.valid_from_height||h>e->announcement.valid_until_height||!e->announcement.capacity.available) continue;
        const QrxAuraPodAnnouncement *old=qrx_aura_pod_gossip_find(dp,e->announcement.capacity.pod_id);
        if(old&&old->sequence>=e->announcement.sequence){
            if(old->sequence==e->announcement.sequence){uint8_t a[32],b[32];if(qrx_aura_pod_announcement_hash(old,a)||qrx_aura_pod_announcement_hash(&e->announcement,b)||memcmp(a,b,32))return -2;}
            continue;
        }
        if(qrx_aura_pod_gossip_ingest(dp,&e->announcement,e->signature,e->signature_len,h,pkey,pctx)) return -3;
        pc++;
    }
    for(size_t i=0;i<sm->count;i++){
        const QrxAuraModelGossipEntry *e=&sm->entries[i];
        if(h<e->announcement.valid_from_height||h>e->announcement.valid_until_height) continue;
        const QrxAuraModelProfileAnnouncement *old=qrx_aura_model_gossip_find(dm,e->announcement.profile.model_id,e->announcement.profile.model_version);
        if(old&&old->sequence>=e->announcement.sequence){
            if(old->sequence==e->announcement.sequence){uint8_t a[32],b[32];if(qrx_aura_model_announcement_hash(old,a)||qrx_aura_model_announcement_hash(&e->announcement,b)||memcmp(a,b,32))return -4;}
            continue;
        }
        if(qrx_aura_model_gossip_ingest(dm,&e->announcement,e->signature,e->signature_len,h,mkey,mctx,auth,actx)) return -5;
        mc++;
    }
    if(pu) *pu=pc;
    if(mu) *mu=mc;
    return 0;
}
