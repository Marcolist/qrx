#include "compute/qrx_aura_remote_dispatch.h"
#include "compute/qrx_aura_fabric_gossip.h"
#include "resource/qrx_resource_provider_runtime.h"
#include "storage/qrx_storage_fs.h"
#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define MIB (1024ULL*1024ULL)
#define GIB (1024ULL*1024ULL*1024ULL)
#define H 500ULL

typedef struct {
    const char *id[16];
    EVP_PKEY *key[16];
    size_t count;
} KeyRing;

static void hexfill(char out[65],char c){for(int i=0;i<64;i++)out[i]=c;out[64]=0;}
static EVP_PKEY *new_key(const char *name){
    EVP_PKEY_CTX *c=EVP_PKEY_CTX_new_from_name(NULL,name,NULL);EVP_PKEY *k=NULL;
    assert(c&&EVP_PKEY_keygen_init(c)==1&&EVP_PKEY_generate(c,&k)==1&&k);EVP_PKEY_CTX_free(c);return k;
}
static void ring_add(KeyRing*r,const char*id,EVP_PKEY*k){assert(r&&id&&k&&r->count<16);r->id[r->count]=id;r->key[r->count]=k;r->count++;}
static int key_lookup(void *ctx,const char *id,EVP_PKEY **out){
    KeyRing*r=(KeyRing*)ctx;if(!r||!id||!out)return -1;
    for(size_t i=0;i<r->count;i++)if(!strcmp(r->id[i],id)){assert(EVP_PKEY_up_ref(r->key[i])==1);*out=r->key[i];return 0;}return -1;
}
static int authorize_model(void *ctx,const char *id){const char*want=(const char*)ctx;return want&&id&&!strcmp(want,id)?0:-1;}
static int sha3_domain(const char *domain,const void *data,size_t n,char out[65]){
    EVP_MD_CTX*m=EVP_MD_CTX_new();unsigned char h[32];unsigned hn=0;assert(m);
    int ok=EVP_DigestInit_ex(m,EVP_sha3_256(),NULL)==1&&EVP_DigestUpdate(m,domain,strlen(domain))==1&&(!n||EVP_DigestUpdate(m,data,n)==1)&&EVP_DigestFinal_ex(m,h,&hn)==1&&hn==32;
    EVP_MD_CTX_free(m);if(!ok)return -1;static const char hx[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=hx[h[i]>>4];out[i*2+1]=hx[h[i]&15];}out[64]=0;return 0;
}

static QrxResourceProviderRuntime make_runtime(const char *provider,uint32_t threads,uint64_t net){
    QrxResourceProviderPlan plan;memset(&plan,0,sizeof(plan));plan.version=QRX_RESOURCE_PROVIDER_VERSION;snprintf(plan.region,sizeof(plan.region),"EU-WEST");
    plan.enabled_mask=QRX_PROVIDER_ENABLE_COMPUTE|QRX_PROVIDER_ENABLE_AI|QRX_PROVIDER_ENABLE_MODEL_CACHE|QRX_PROVIDER_ENABLE_NETWORK;
    plan.compute_threads=threads;plan.network_egress_mbps=net;plan.model_cache_bytes=8*GIB;plan.primary_action=QRX_PROVIDER_ENABLE_COMPUTE;plan.opportunity_score=8000;plan.expected_utilization_bps=4000;
    QrxResourceProviderRuntime rt;assert(qrx_resource_provider_runtime_init(&rt,provider,"alphanet",&plan)==0);assert(qrx_resource_provider_runtime_mark_ready(&rt)==0);
    QrxResourceProviderMarketRegistration reg;assert(qrx_resource_provider_market_register(&rt,NULL,H,&reg)==0);assert(qrx_resource_provider_runtime_start_serving(&rt)==0);return rt;
}
static int worker(void*ctx,const void*in,size_t in_n,void*out,size_t cap,size_t*out_n){
    (void)ctx;const char*p="REMOTE:";size_t pn=strlen(p);if(cap<pn+in_n)return -1;memcpy(out,p,pn);if(in_n)memcpy((unsigned char*)out+pn,in,in_n);*out_n=pn+in_n;return 0;
}
static QrxAuraRuntimeBinding make_binding(const char *provider,const char *pod,QrxResourceProviderRuntime *rt){
    QrxAuraRuntimeBinding b;memset(&b,0,sizeof(b));b.version=QRX_AURA_LIVE_DISPATCH_VERSION;snprintf(b.provider_id,sizeof(b.provider_id),"%s",provider);snprintf(b.pod_id,sizeof(b.pod_id),"%s",pod);b.provider_runtime=rt;b.local=1;
    b.device.version=QRX_MOE_RUNTIME_VERSION;b.device.backend=QRX_MOE_BACKEND_CPU;snprintf(b.device.device_name,sizeof(b.device.device_name),"phase163-cpu");b.device.accelerator_features=QRX_MOE_ACCEL_FEAT_FP32;b.device.device_memory_bytes=64*GIB;b.device.max_batch_items=8;
    b.adapter.version=QRX_MOE_DISCOVERY_VERSION;snprintf(b.adapter.name,sizeof(b.adapter.name),"phase163-worker");b.adapter.backend=QRX_MOE_BACKEND_CPU;b.adapter.kernel_mask=QRX_MOE_KERNEL_FP32;b.adapter.ready=1;b.adapter.execute=worker;
    assert(qrx_moe_runtime_device_validate(&b.device)==0);assert(qrx_moe_worker_adapter_validate(&b.adapter)==0);return b;
}
static QrxAuraRemoteFrame lease_frame(uint32_t kind,uint64_t seq,uint64_t height,uint64_t exp,const char *requester,const char *provider,const char *pod,const char *lease){
    QrxAuraRemoteFrame f;memset(&f,0,sizeof(f));f.version=QRX_AURA_REMOTE_VERSION;f.kind=kind;snprintf(f.signer_id,sizeof(f.signer_id),"%s",requester);f.sequence=seq;f.height=height;hexfill(f.admission_commitment,'a');snprintf(f.provider_id,sizeof(f.provider_id),"%s",provider);snprintf(f.pod_id,sizeof(f.pod_id),"%s",pod);snprintf(f.lease_id,sizeof(f.lease_id),"%s",lease);f.lease_expires_height=exp;f.compute_threads=2;f.network_egress_mbps=100;f.node_id=7;f.required_memory_bytes=1*MIB;f.max_output_bytes=4096;f.fragment_count=1;snprintf(f.runtime_id,sizeof(f.runtime_id),"qrx-llm-runtime-v1");snprintf(f.model_id,sizeof(f.model_id),"qrx/phase163");snprintf(f.model_version,sizeof(f.model_version),"1");hexfill(f.model_commitment,'b');assert(sha3_domain("QRX/AURA/REMOTE-PAYLOAD/V1",NULL,0,f.payload_commitment)==0);return f;
}

static void test_frame_crypto(void){
    EVP_PKEY *k=new_key("Ed25519");char lease[65];hexfill(lease,'c');QrxAuraRemoteFrame f=lease_frame(QRX_AURA_REMOTE_KIND_LEASE_OPEN,1,H,H+16,"requester-1","provider-1","pod-1",lease);
    const char payload[]="phase163-wire";f.payload=malloc(sizeof(payload)-1);assert(f.payload);memcpy(f.payload,payload,sizeof(payload)-1);f.payload_len=sizeof(payload)-1;assert(sha3_domain("QRX/AURA/REMOTE-PAYLOAD/V1",f.payload,f.payload_len,f.payload_commitment)==0);
    char commitment[65];assert(qrx_aura_remote_frame_commitment(&f,commitment)==0);assert(!strcmp(commitment,"47cfe8c5095fc8cc1eeef65f728f62076e48b4290320325f3e78c2e08b8f70ce"));
    uint8_t *sig=NULL,*wire=NULL,*sig2=NULL;size_t sn=0,wn=0,sn2=0;assert(qrx_aura_remote_frame_sign(k,&f,&sig,&sn)==0);assert(qrx_aura_remote_frame_verify(k,&f,sig,sn)==0);assert(qrx_aura_remote_wire_encode(&f,sig,sn,&wire,&wn)==0);
    QrxAuraRemoteFrame d;assert(qrx_aura_remote_wire_decode(wire,wn,&d,&sig2,&sn2)==0);assert(sn2==sn&&!memcmp(sig,sig2,sn));assert(qrx_aura_remote_frame_verify(k,&d,sig2,sn2)==0);assert(d.payload_len==f.payload_len&&!memcmp(d.payload,f.payload,f.payload_len));
    sig2[0]^=1;assert(qrx_aura_remote_frame_verify(k,&d,sig2,sn2)!=0);sig2[0]^=1;d.payload[0]^=1;assert(qrx_aura_remote_frame_verify(k,&d,sig2,sn2)!=0);
    qrx_aura_remote_frame_free(&d);qrx_aura_remote_frame_free(&f);free(sig);free(sig2);free(wire);EVP_PKEY_free(k);
}

static void test_leases(void){
    const char *provider="provider-lease",*pod="pod-lease",*requester="requester-lease";QrxResourceProviderRuntime rt=make_runtime(provider,8,1000);QrxAuraRuntimeBinding b=make_binding(provider,pod,&rt);QrxAuraReservationLeaseTable t;qrx_aura_reservation_leases_init(&t);char lease[65];hexfill(lease,'d');
    QrxAuraRemoteFrame open=lease_frame(QRX_AURA_REMOTE_KIND_LEASE_OPEN,10,H,H+16,requester,provider,pod,lease);QrxAuraReservationLease l;
    assert(qrx_aura_reservation_lease_open(&t,&b,&open,H,&l)==0);assert(rt.active_jobs==1&&rt.reserved_compute_threads==2&&rt.reserved_network_egress_mbps==100);assert(l.last_sequence==10);
    assert(qrx_aura_reservation_lease_open(&t,&b,&open,H,&l)==0);assert(rt.active_jobs==1); /* idempotent exact retry */
    QrxAuraRemoteFrame mutated=open;mutated.required_memory_bytes+=1;assert(qrx_aura_reservation_lease_open(&t,&b,&mutated,H,&l)!=0);assert(rt.active_jobs==1); /* same lease/seq but changed immutable request is not an exact retry */
    QrxAuraRemoteFrame renew=open;renew.kind=QRX_AURA_REMOTE_KIND_LEASE_RENEW;renew.sequence=11;renew.lease_expires_height=H+24;assert(qrx_aura_reservation_lease_renew(&t,&renew,H+1,&l)==0&&l.expires_height==H+24&&l.last_sequence==11);assert(qrx_aura_reservation_lease_renew(&t,&renew,H+1,&l)!=0); /* replay */
    QrxAuraRemoteFrame badrenew=renew;badrenew.sequence=12;badrenew.model_version[0]='2';assert(qrx_aura_reservation_lease_renew(&t,&badrenew,H+1,&l)!=0);
    QrxAuraRemoteFrame rel=renew;rel.kind=QRX_AURA_REMOTE_KIND_LEASE_RELEASE;rel.sequence=12;assert(qrx_aura_reservation_lease_release(&t,&b,&rel)==0);assert(rt.active_jobs==0&&rt.reserved_compute_threads==0&&rt.reserved_network_egress_mbps==0);assert(qrx_aura_reservation_lease_release(&t,&b,&rel)==0); /* exact release retry */
    char lease2[65];hexfill(lease2,'e');QrxAuraRemoteFrame open2=lease_frame(QRX_AURA_REMOTE_KIND_LEASE_OPEN,20,H+2,H+5,requester,provider,pod,lease2);assert(qrx_aura_reservation_lease_open(&t,&b,&open2,H+2,&l)==0);assert(rt.active_jobs==1);assert(qrx_aura_reservation_leases_reap(&t,&b,1,H+6)==1);assert(rt.active_jobs==0&&rt.reserved_compute_threads==0);const QrxAuraReservationLease*x=qrx_aura_reservation_lease_find(&t,lease2);assert(x&&x->state==QRX_AURA_LEASE_EXPIRED);
}

#ifndef _WIN32
typedef struct {int listen_fd;int connections;QrxAuraRemoteServerContext *server;int rc;} ServerThread;
static void *server_thread(void *v){ServerThread*s=(ServerThread*)v;s->rc=0;for(int i=0;i<s->connections;i++){int fd=accept(s->listen_fd,NULL,NULL);if(fd<0){s->rc=-1;break;}if(qrx_aura_remote_serve_fd(fd,s->server)!=0)s->rc=-2;close(fd);}close(s->listen_fd);return NULL;}
static int open_loopback(uint16_t *port){int fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);int one=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));struct sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=0;assert(bind(fd,(struct sockaddr*)&a,sizeof(a))==0);assert(listen(fd,8)==0);socklen_t n=sizeof(a);assert(getsockname(fd,(struct sockaddr*)&a,&n)==0);*port=ntohs(a.sin_port);return fd;}
static uint64_t fixed_height(void *ctx){return *(uint64_t*)ctx;}
static void test_real_remote_dispatch(void){
    const char *provider="provider-remote",*pod="pod-remote",*requester="requester-remote";EVP_PKEY *reqk=new_key("Ed25519"),*provk=new_key("ML-DSA-65");KeyRing reqring={0},provring={0};ring_add(&reqring,requester,reqk);ring_add(&provring,provider,provk);
    QrxResourceProviderRuntime rt=make_runtime(provider,8,1000);QrxAuraRuntimeBinding b=make_binding(provider,pod,&rt);QrxAuraReservationLeaseTable leases;qrx_aura_reservation_leases_init(&leases);QrxAuraModelCacheCatalog cache;qrx_aura_model_cache_catalog_init(&cache);cache.count=1;cache.entries[0].version=QRX_AURA_LIVE_DISPATCH_VERSION;snprintf(cache.entries[0].pod_id,sizeof(cache.entries[0].pod_id),"%s",pod);snprintf(cache.entries[0].model_id,sizeof(cache.entries[0].model_id),"qrx/phase163");snprintf(cache.entries[0].model_version,sizeof(cache.entries[0].model_version),"1");hexfill(cache.entries[0].model_commitment,'b');cache.entries[0].bytes_present=64*MIB;cache.entries[0].verified_height=H;cache.entries[0].verified=1;
    uint64_t h=H;QrxAuraRemoteServerContext sc;memset(&sc,0,sizeof(sc));sc.binding=&b;sc.leases=&leases;sc.model_cache=&cache;sc.requester_key_lookup=key_lookup;sc.requester_key_ctx=&reqring;sc.provider_private_key=provk;sc.height_fn=fixed_height;sc.height_ctx=&h;
    uint16_t port=0;int lfd=open_loopback(&port);ServerThread st={lfd,3,&sc,0};pthread_t th;assert(pthread_create(&th,NULL,server_thread,&st)==0);
    QrxAuraAdmission ad;memset(&ad,0,sizeof(ad));ad.version=QRX_AURA_LIVE_DISPATCH_VERSION;hexfill(ad.route.decision_commitment,'a');hexfill(ad.model_commitment,'b');hexfill(ad.graph_commitment,'c');ad.node_id=7;ad.admitted_height=H;ad.expires_height=H+32;ad.pod_count=1;ad.active=1;snprintf(ad.pods[0].provider_id,sizeof(ad.pods[0].provider_id),"%s",provider);snprintf(ad.pods[0].pod_id,sizeof(ad.pods[0].pod_id),"%s",pod);ad.pods[0].compute_threads=2;ad.pods[0].network_egress_mbps=100;assert(qrx_aura_admission_commitment(&ad,ad.admission_commitment)==0);
    QrxComputeJobNode node;memset(&node,0,sizeof(node));node.node_id=7;node.job_type=QRX_JOB_AI_INFERENCE;snprintf(node.runtime_id,sizeof(node.runtime_id),"qrx-llm-runtime-v1");snprintf(node.model_id,sizeof(node.model_id),"qrx/phase163");snprintf(node.model_version,sizeof(node.model_version),"1");node.min_memory_bytes=1*MIB;node.max_output_bytes=4096;
    QrxAuraRemoteClientContext cc;memset(&cc,0,sizeof(cc));snprintf(cc.requester_id,sizeof(cc.requester_id),"%s",requester);cc.requester_private_key=reqk;cc.provider_key_lookup=key_lookup;cc.provider_key_ctx=&provring;cc.current_height=H;cc.lease_blocks=16;char ep[128];snprintf(ep,sizeof(ep),"qrxp2p://127.0.0.1:%u",(unsigned)port);assert(qrx_aura_remote_client_add_peer(&cc,provider,pod,ep)==0);
    char out[128];size_t outn=0;const char *input="reason-this";assert(qrx_aura_remote_distributed_dispatch(&cc,&ad,&node,input,strlen(input),out,sizeof(out),&outn)==0);assert(outn==strlen("REMOTE:reason-this")&&!memcmp(out,"REMOTE:reason-this",outn));assert(pthread_join(th,NULL)==0&&st.rc==0);assert(rt.active_jobs==0&&rt.reserved_compute_threads==0&&rt.reserved_network_egress_mbps==0);assert(leases.count==1&&leases.entries[0].state==QRX_AURA_LEASE_RELEASED&&leases.entries[0].last_sequence==3);
    EVP_PKEY_free(reqk);EVP_PKEY_free(provk);
}
#endif

typedef struct {QrxStorageFs *fs;} CasCtx;
static int cas_stat(void *v,const char *root,uint64_t *bytes){CasCtx*c=(CasCtx*)v;unsigned char*b=NULL;size_t n=0;if(qrx_storage_fs_read(c->fs,root,&b,&n))return -1;free(b);*bytes=n;return 0;}
static int cas_fetch(void *v,const char *root,uint64_t off,size_t len,uint8_t **out,size_t *outn){CasCtx*c=(CasCtx*)v;return qrx_storage_fs_read_range(c->fs,root,off,len,out,outn);}
static void add_bundle_obj(QrxStorageFs *remote,QrxAuraModelBundleManifest *mf,uint32_t kind,const void *p,size_t n,char root_out[65]){
    assert(mf->object_count<QRX_AURA_REMOTE_MAX_BUNDLE_OBJECTS);assert(qrx_storage_fs_put(remote,p,n,root_out)==0);QrxAuraModelBundleObject*o=&mf->objects[mf->object_count++];o->kind=kind;o->bytes=n;snprintf(o->object_root,sizeof(o->object_root),"%s",root_out);mf->total_bytes+=n;
}
static void test_drive_model_fetch(void){
    char rd[]="/tmp/qrx163-remote-XXXXXX",ld[]="/tmp/qrx163-local-XXXXXX";assert(mkdtemp(rd)&&mkdtemp(ld));QrxStorageFs *remote=NULL,*local=NULL;assert(qrx_storage_fs_open(rd,0,0,&remote)==0);assert(qrx_storage_fs_open(ld,0,0,&local)==0);
    QrxAuraModelBundleManifest mf;memset(&mf,0,sizeof(mf));mf.version=QRX_AURA_MODEL_BUNDLE_VERSION;snprintf(mf.model_id,sizeof(mf.model_id),"deepseek/reasoning-compatible");snprintf(mf.model_version,sizeof(mf.model_version),"profile-1");char tok[65],cfg[65],weights[65];const char td[]="tokenizer-phase163",cd[]="config-phase163",wd[]="weights-phase163-reasoning";add_bundle_obj(remote,&mf,QRX_AURA_MODEL_OBJECT_TOKENIZER,td,sizeof(td)-1,tok);add_bundle_obj(remote,&mf,QRX_AURA_MODEL_OBJECT_CONFIG,cd,sizeof(cd)-1,cfg);add_bundle_obj(remote,&mf,QRX_AURA_MODEL_OBJECT_WEIGHTS,wd,sizeof(wd)-1,weights);
    uint8_t *mb=NULL;size_t mn=0;assert(qrx_aura_model_bundle_encode(&mf,&mb,&mn)==0);char manifest[65],root2[65];assert(qrx_storage_fs_put(remote,mb,mn,manifest)==0);assert(qrx_aura_model_bundle_root(&mf,root2)==0&&!strcmp(manifest,root2));free(mb);
    QrxAiModelRecord model;memset(&model,0,sizeof(model));model.registry_version=QRX_MODEL_REGISTRY_VERSION;snprintf(model.model_id,sizeof(model.model_id),"%s",mf.model_id);snprintf(model.model_version,sizeof(model.model_version),"%s",mf.model_version);snprintf(model.architecture,sizeof(model.architecture),"dense-reasoner");snprintf(model.runtime_id,sizeof(model.runtime_id),"qrx-llm-runtime-v1");snprintf(model.manifest_root,65,"%s",manifest);snprintf(model.tokenizer_root,65,"%s",tok);memset(model.expert_manifest_root,'0',64);model.expert_manifest_root[64]=0;model.min_memory_bytes=1*MIB;model.min_storage_bytes=mf.total_bytes;model.verification_profile=QRX_MODEL_VERIFY_HIGH;snprintf(model.license_id,sizeof(model.license_id),"registry-supplied-license");model.is_moe=0;assert(qrx_ai_model_validate(&model)==0);char mc[65];assert(qrx_ai_model_commitment(&model,mc)==0);
    QrxAuraModelCachePlacement p;memset(&p,0,sizeof(p));snprintf(p.pod_id,sizeof(p.pod_id),"pod-model-fetch");snprintf(p.provider_id,sizeof(p.provider_id),"provider-model-fetch");p.bytes_to_place=mf.total_bytes;CasCtx rc={remote};QrxAuraDriveModelFetchContext fc={local,cas_stat,cas_fetch,&rc,5};char got[65];assert(qrx_aura_drive_model_fetch(&fc,&model,&p,got)==0&&!strcmp(got,mc));assert(qrx_storage_fs_has(local,manifest)==1&&qrx_storage_fs_has(local,tok)==1&&qrx_storage_fs_has(local,cfg)==1&&qrx_storage_fs_has(local,weights)==1);
    QrxAiModelRecord bad=model;hexfill(bad.tokenizer_root,'f');assert(qrx_ai_model_validate(&bad)==0);assert(qrx_aura_drive_model_fetch(&fc,&bad,&p,got)==-4);
    qrx_storage_fs_close(local);qrx_storage_fs_close(remote);
}

static QrxAuraPodCapacity pod(const char *provider,const char *id){
    QrxAuraPodCapacity p;memset(&p,0,sizeof(p));p.version=QRX_AURA_FABRIC_VERSION;snprintf(p.provider_id,sizeof(p.provider_id),"%s",provider);snprintf(p.pod_id,sizeof(p.pod_id),"%s",id);snprintf(p.region,sizeof(p.region),"EU-WEST");p.backend=QRX_MOE_BACKEND_CPU;p.accelerator_features=QRX_MOE_ACCEL_FEAT_FP32;p.role_mask=QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_MODEL_CACHE|QRX_AURA_POD_ROLE_VERIFY;p.usable_memory_bytes=32*GIB;p.free_memory_bytes=24*GIB;p.model_cache_free_bytes=16*GIB;p.measured_milli_tokens_per_second=8000;p.network_egress_mbps=500;p.latency_ms=20;p.utilization_bps=2000;p.reliability_bps=9900;p.cache_hit_bps=8500;p.expert_locality_bps=8500;p.max_context_tokens=32768;p.node_count=1;p.available=1;hexfill(p.calibration_commitment,'1');assert(qrx_aura_pod_capacity_validate(&p)==0);return p;
}
static QrxAuraModelCapabilityProfile profile(const char *id,const char *family,QrxAuraModelTier tier,uint32_t q,uint32_t reasoning,int moe,uint32_t minpods,uint64_t mem,uint64_t tps){
    QrxAuraModelCapabilityProfile m;memset(&m,0,sizeof(m));m.version=QRX_AURA_FABRIC_VERSION;snprintf(m.model_id,sizeof(m.model_id),"%s",id);snprintf(m.model_version,sizeof(m.model_version),"1");snprintf(m.family,sizeof(m.family),"%s",family);snprintf(m.runtime_id,sizeof(m.runtime_id),"qrx-llm-runtime-v1");snprintf(m.quantization,sizeof(m.quantization),"registry-defined");m.tier=tier;m.min_memory_bytes=mem;m.recommended_memory_bytes=mem;m.min_storage_bytes=1*MIB;m.min_aggregate_milli_tokens_per_second=tps;m.min_pods=minpods;m.min_network_mbps=moe?100:0;m.max_latency_ms=100;m.min_reliability_bps=9000;m.min_cache_hit_bps=moe?4000:0;m.min_expert_locality_bps=moe?5000:0;m.max_context_tokens=32768;m.quality_bps=q;m.coding_bps=q;m.reasoning_bps=reasoning;m.rag_bps=q;m.tool_use_bps=q;m.is_moe=(uint8_t)moe;m.supports_single_device=(uint8_t)!moe;assert(qrx_aura_model_profile_validate(&m)==0);return m;
}
static QrxAuraPodAnnouncement pann(QrxAuraPodCapacity p,uint64_t seq){QrxAuraPodAnnouncement a;memset(&a,0,sizeof(a));a.version=QRX_AURA_GOSSIP_VERSION;a.sequence=seq;a.valid_from_height=H-10;a.valid_until_height=H+100;snprintf(a.service_endpoint,sizeof(a.service_endpoint),"qrxp2p://%s:39100",p.pod_id);a.capacity=p;return a;}
static QrxAuraModelProfileAnnouncement mann(QrxAuraModelCapabilityProfile m,uint64_t seq,char fill){QrxAuraModelProfileAnnouncement a;memset(&a,0,sizeof(a));a.version=QRX_AURA_GOSSIP_VERSION;snprintf(a.publisher_id,sizeof(a.publisher_id),"model-root");a.sequence=seq;a.valid_from_height=H-10;a.valid_until_height=H+100;hexfill(a.model_registry_commitment,fill);a.profile=m;return a;}
static void ingest_pod(QrxAuraPodGossipTable*t,QrxAuraPodAnnouncement*a,EVP_PKEY*k,KeyRing*r){uint8_t*s=NULL;size_t n=0;assert(qrx_aura_pod_announcement_sign(k,a,&s,&n)==0);assert(qrx_aura_pod_gossip_ingest(t,a,s,n,H,key_lookup,r)==0);free(s);}
static void ingest_model(QrxAuraModelGossipTable*t,QrxAuraModelProfileAnnouncement*a,EVP_PKEY*k,KeyRing*r){uint8_t*s=NULL;size_t n=0;assert(qrx_aura_model_announcement_sign(k,a,&s,&n)==0);assert(qrx_aura_model_gossip_ingest(t,a,s,n,H,key_lookup,r,authorize_model,"model-root")==0);free(s);}
static void test_anti_entropy_and_reasoning_route(void){
    EVP_PKEY *ka=new_key("Ed25519"),*kb=new_key("Ed25519"),*km=new_key("Ed25519");KeyRing pr={0},mr={0};ring_add(&pr,"provider-a",ka);ring_add(&pr,"provider-b",kb);ring_add(&mr,"model-root",km);
    QrxAuraPodGossipTable a_p,b_p;qrx_aura_pod_gossip_init(&a_p);qrx_aura_pod_gossip_init(&b_p);QrxAuraModelGossipTable a_m,b_m;qrx_aura_model_gossip_init(&a_m);qrx_aura_model_gossip_init(&b_m);
    QrxAuraPodAnnouncement pa=pann(pod("provider-a","pod-a"),1),pb=pann(pod("provider-b","pod-b"),1);ingest_pod(&a_p,&pa,ka,&pr);ingest_pod(&b_p,&pb,kb,&pr);
    QrxAuraModelCapabilityProfile qwen=profile("qrx/qwen-edge-compatible","Qwen-compatible",QRX_AURA_TIER_EDGE,6500,7000,0,1,2*GIB,1000);QrxAuraModelCapabilityProfile deep=profile("qrx/deepseek-reasoning-compatible","DeepSeek-compatible reasoning",QRX_AURA_TIER_CLUSTER,7200,9400,1,2,32*GIB,12000);QrxAuraModelProfileAnnouncement qa=mann(qwen,1,'2'),da=mann(deep,1,'3');ingest_model(&a_m,&qa,km,&mr);ingest_model(&b_m,&da,km,&mr);
    QrxAuraGossipDigest d1,d2;assert(qrx_aura_gossip_digest(&a_p,&a_m,H,&d1)==0);assert(qrx_aura_gossip_digest(&b_p,&b_m,H,&d2)==0);assert(strcmp(d1.combined_root,d2.combined_root));size_t pu=0,mu=0;assert(qrx_aura_gossip_anti_entropy_merge(&a_p,&a_m,&b_p,&b_m,H,key_lookup,&pr,key_lookup,&mr,authorize_model,"model-root",&pu,&mu)==0&&pu==1&&mu==1);assert(qrx_aura_gossip_anti_entropy_merge(&b_p,&b_m,&a_p,&a_m,H,key_lookup,&pr,key_lookup,&mr,authorize_model,"model-root",&pu,&mu)==0&&pu==1&&mu==1);assert(qrx_aura_gossip_digest(&a_p,&a_m,H,&d1)==0&&qrx_aura_gossip_digest(&b_p,&b_m,H,&d2)==0&&!strcmp(d1.combined_root,d2.combined_root));
    QrxAuraRouteRequest rr;memset(&rr,0,sizeof(rr));rr.version=QRX_AURA_FABRIC_VERSION;rr.mode=QRX_AURA_ROUTE_AUTO;rr.task_class=QRX_AURA_TASK_CHAT;rr.complexity_bps=1000;QrxAuraRouteDecision dec;assert(qrx_aura_gossip_live_route(&a_p,&a_m,H,&rr,&dec)==0&&!strcmp(dec.model_id,"qrx/qwen-edge-compatible"));rr.task_class=QRX_AURA_TASK_REASONING;rr.complexity_bps=9000;assert(qrx_aura_gossip_live_route(&a_p,&a_m,H,&rr,&dec)==0&&!strcmp(dec.model_id,"qrx/deepseek-reasoning-compatible")&&dec.selected_pods>=2);
    /* Equal sequence with different content is an equivocation, not a merge. */
    QrxAuraPodGossipTable evil_p;qrx_aura_pod_gossip_init(&evil_p);QrxAuraModelGossipTable empty_m;qrx_aura_model_gossip_init(&empty_m);QrxAuraPodAnnouncement conflict=pa;conflict.capacity.latency_ms=21;ingest_pod(&evil_p,&conflict,ka,&pr);assert(qrx_aura_gossip_anti_entropy_merge(&a_p,&a_m,&evil_p,&empty_m,H,key_lookup,&pr,key_lookup,&mr,authorize_model,"model-root",NULL,NULL)==-2);
    qrx_aura_pod_gossip_free(&evil_p);qrx_aura_model_gossip_free(&empty_m);
    /* Stale source entries are ignored rather than poisoning convergence. */
    QrxAuraPodGossipTable stale_p;qrx_aura_pod_gossip_init(&stale_p);QrxAuraModelGossipTable stale_m;qrx_aura_model_gossip_init(&stale_m);QrxAuraPodAnnouncement stale=pa;stale.sequence=2;stale.valid_until_height=H-1;uint8_t*ss=NULL;size_t ssn=0;assert(qrx_aura_pod_announcement_sign(ka,&stale,&ss,&ssn)==0);stale_p.entries=calloc(1,sizeof(*stale_p.entries));assert(stale_p.entries);stale_p.capacity=1;stale_p.count=1;stale_p.entries[0].announcement=stale;stale_p.entries[0].signature=ss;stale_p.entries[0].signature_len=ssn;assert(qrx_aura_gossip_anti_entropy_merge(&a_p,&a_m,&stale_p,&stale_m,H,key_lookup,&pr,key_lookup,&mr,authorize_model,"model-root",&pu,&mu)==0&&pu==0&&mu==0);qrx_aura_pod_gossip_free(&stale_p);qrx_aura_model_gossip_free(&stale_m);
    qrx_aura_pod_gossip_free(&a_p);qrx_aura_pod_gossip_free(&b_p);qrx_aura_model_gossip_free(&a_m);qrx_aura_model_gossip_free(&b_m);EVP_PKEY_free(ka);EVP_PKEY_free(kb);EVP_PKEY_free(km);
}

int main(void){
    test_frame_crypto();
    test_leases();
#ifndef _WIN32
    test_real_remote_dispatch();
#endif
    test_drive_model_fetch();
    test_anti_entropy_and_reasoning_route();
    return 0;
}
