#include "compute/qrx_aura_fabric_gossip.h"
#include "resource/qrx_resource_globe.h"
#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GIB (1024ULL*1024ULL*1024ULL)

typedef struct {
    const char *id[16];
    EVP_PKEY *key[16];
    size_t count;
} KeyRing;

static void hexfill(char out[65], char c) { for (int i=0;i<64;i++) out[i]=c; out[64]=0; }
static void tohex(const uint8_t *p,size_t n,char *out){static const char*h="0123456789abcdef";for(size_t i=0;i<n;i++){out[i*2]=h[p[i]>>4];out[i*2+1]=h[p[i]&15];}out[n*2]=0;}

static EVP_PKEY *new_ed25519(void) {
    EVP_PKEY_CTX *c=EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519,NULL); EVP_PKEY *k=NULL;
    assert(c && EVP_PKEY_keygen_init(c)==1 && EVP_PKEY_keygen(c,&k)==1 && k);
    EVP_PKEY_CTX_free(c); return k;
}
static void ring_add(KeyRing*r,const char*id,EVP_PKEY*k){assert(r&&id&&k&&r->count<16);r->id[r->count]=id;r->key[r->count]=k;r->count++;}
static int key_lookup(void *ctx,const char *id,EVP_PKEY **out){KeyRing*r=ctx;if(!r||!id||!out)return -1;for(size_t i=0;i<r->count;i++)if(!strcmp(r->id[i],id)){assert(EVP_PKEY_up_ref(r->key[i])==1);*out=r->key[i];return 0;}return -1;}
static int model_authorize(void *ctx,const char *id){const char*allowed=ctx;return (allowed&&id&&!strcmp(allowed,id))?0:-1;}

static QrxAuraPodCapacity pod(const char*provider,const char*id,const char*region,uint64_t free_gib,uint64_t mtps,uint32_t net,uint32_t lat,uint32_t rel,uint32_t hit,uint32_t locality,QrxMoeRuntimeBackend backend,uint32_t accel,uint32_t roles){
    QrxAuraPodCapacity p;memset(&p,0,sizeof(p));p.version=QRX_AURA_FABRIC_VERSION;
    snprintf(p.provider_id,sizeof(p.provider_id),"%s",provider);snprintf(p.pod_id,sizeof(p.pod_id),"%s",id);snprintf(p.region,sizeof(p.region),"%s",region);
    p.backend=backend;p.accelerator_features=accel;p.role_mask=roles;p.usable_memory_bytes=(free_gib+2)*GIB;p.free_memory_bytes=free_gib*GIB;p.model_cache_free_bytes=(free_gib/2)*GIB;
    p.measured_milli_tokens_per_second=mtps;p.network_egress_mbps=net;p.latency_ms=lat;p.utilization_bps=2200;p.reliability_bps=rel;p.cache_hit_bps=hit;p.expert_locality_bps=locality;
    p.max_context_tokens=32768;p.node_count=1;p.available=1;hexfill(p.calibration_commitment,'a');assert(qrx_aura_pod_capacity_validate(&p)==0);return p;
}
static QrxAuraModelCapabilityProfile model(const char*id,QrxAuraModelTier tier,uint64_t min_gib,uint64_t rec_gib,uint64_t min_mtps,uint32_t min_pods,uint32_t quality,int moe,int single){
    QrxAuraModelCapabilityProfile m;memset(&m,0,sizeof(m));m.version=QRX_AURA_FABRIC_VERSION;snprintf(m.model_id,sizeof(m.model_id),"%s",id);snprintf(m.model_version,sizeof(m.model_version),"1");
    snprintf(m.family,sizeof(m.family),"%s",tier>=QRX_AURA_TIER_K2_CLASS?"Kimi-class":"Qwen-class");snprintf(m.runtime_id,sizeof(m.runtime_id),"qrx-llm-runtime-v1");snprintf(m.quantization,sizeof(m.quantization),"Q4");
    m.tier=tier;m.min_memory_bytes=min_gib*GIB;m.recommended_memory_bytes=rec_gib*GIB;m.min_storage_bytes=2*GIB;m.min_aggregate_milli_tokens_per_second=min_mtps;m.min_pods=min_pods;m.min_network_mbps=moe?100:0;m.max_latency_ms=moe?150:500;
    m.min_reliability_bps=8000;m.min_cache_hit_bps=moe?4000:0;m.min_expert_locality_bps=moe?5000:0;m.max_context_tokens=8192;m.quality_bps=quality;m.coding_bps=quality;m.reasoning_bps=quality;m.rag_bps=quality;m.tool_use_bps=quality;
    m.is_moe=(uint8_t)moe;m.supports_single_device=(uint8_t)single;assert(qrx_aura_model_profile_validate(&m)==0);return m;
}
static QrxAuraPodAnnouncement pod_ann(QrxAuraPodCapacity p,uint64_t seq,uint64_t from,uint64_t until){QrxAuraPodAnnouncement a;memset(&a,0,sizeof(a));a.version=QRX_AURA_GOSSIP_VERSION;a.sequence=seq;a.valid_from_height=from;a.valid_until_height=until;snprintf(a.service_endpoint,sizeof(a.service_endpoint),"qrxp2p://%s:39100",p.pod_id);a.capacity=p;return a;}
static QrxAuraModelProfileAnnouncement model_ann(QrxAuraModelCapabilityProfile m,const char*pub,uint64_t seq,uint64_t from,uint64_t until,char commit){QrxAuraModelProfileAnnouncement a;memset(&a,0,sizeof(a));a.version=QRX_AURA_GOSSIP_VERSION;snprintf(a.publisher_id,sizeof(a.publisher_id),"%s",pub);a.sequence=seq;a.valid_from_height=from;a.valid_until_height=until;hexfill(a.model_registry_commitment,commit);a.profile=m;return a;}

int main(void){
    const uint64_t H=1000;
    KeyRing providers={0}, publishers={0};
    EVP_PKEY *ka=new_ed25519(),*kb=new_ed25519(),*kc=new_ed25519(),*kd=new_ed25519(),*evil=new_ed25519(),*root=new_ed25519();
    ring_add(&providers,"prov-a",ka);ring_add(&providers,"prov-b",kb);ring_add(&providers,"prov-c",kc);ring_add(&providers,"prov-d",kd);ring_add(&publishers,"model-root",root);ring_add(&publishers,"evil-root",evil);

    QrxAuraPodCapacity pc[4];
    pc[0]=pod("prov-a","pod-a","EU-WEST",8,5000,800,20,9900,7000,6500,QRX_MOE_BACKEND_CPU,QRX_MOE_ACCEL_FEAT_FP32,QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_ROUTER|QRX_AURA_POD_ROLE_VERIFY);
    pc[1]=pod("prov-b","pod-b","EU-WEST",16,11000,1000,13,9950,8500,8000,QRX_MOE_BACKEND_CUDA,QRX_MOE_ACCEL_FEAT_FP16|QRX_MOE_ACCEL_FEAT_FP32|QRX_MOE_ACCEL_FEAT_CUDA,QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_MODEL_CACHE);
    pc[2]=pod("prov-c","pod-c","EU-WEST",24,16000,1500,9,9970,9000,9000,QRX_MOE_BACKEND_MLX_METAL,QRX_MOE_ACCEL_FEAT_FP16|QRX_MOE_ACCEL_FEAT_FP32|QRX_MOE_ACCEL_FEAT_METAL|QRX_MOE_ACCEL_FEAT_MLX|QRX_MOE_ACCEL_FEAT_UNIFIED_MEM,QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_MODEL_CACHE);
    pc[3]=pod("prov-d","pod-d","EU-CENTRAL",4,1200,300,40,9500,2500,2000,QRX_MOE_BACKEND_CPU,QRX_MOE_ACCEL_FEAT_FP32,QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_PREPOST);

    QrxAuraPodAnnouncement pa[4];uint8_t *psig[4]={0};size_t psl[4]={0};EVP_PKEY *pkeys[4]={ka,kb,kc,kd};
    for(int i=0;i<4;i++){pa[i]=pod_ann(pc[i],1,H-5,H+100+(uint64_t)i);assert(qrx_aura_pod_announcement_validate(&pa[i],H)==0);assert(qrx_aura_pod_announcement_sign(pkeys[i],&pa[i],&psig[i],&psl[i])==0);}

    /* Canonical BE wire/hash format is deterministic independent of signing key. */
    uint8_t ph[32];char phx[65];assert(qrx_aura_pod_announcement_hash(&pa[0],ph)==0);tohex(ph,32,phx);assert(!strcmp(phx,"dc1ea8b0df34da4620fd91e19c4b134bb92031254192dc32fb452bb41973e35a"));
    uint8_t *wire=NULL,*roundsig=NULL;size_t wn=0,rsl=0;QrxAuraPodAnnouncement round;
    assert(qrx_aura_pod_wire_encode(&pa[0],psig[0],psl[0],&wire,&wn)==0);assert(qrx_aura_pod_wire_decode(wire,wn,&round,&roundsig,&rsl)==0);assert(!memcmp(&round,&pa[0],sizeof(round)));assert(rsl==psl[0]&&!memcmp(roundsig,psig[0],rsl));free(roundsig);free(wire);

    QrxAuraPodGossipTable pt; qrx_aura_pod_gossip_init(&pt);
    for(int i=0;i<4;i++)assert(qrx_aura_pod_gossip_ingest(&pt,&pa[i],psig[i],psl[i],H,key_lookup,&providers)==0);
    assert(pt.count==4&&pt.revision==4&&qrx_aura_pod_gossip_find(&pt,"pod-a"));
    assert(qrx_aura_pod_gossip_ingest(&pt,&pa[0],psig[0],psl[0],H,key_lookup,&providers)==-4); /* replay */

    /* A forged signature and a provider takeover of an existing pod ID fail closed. */
    uint8_t *bad=NULL;size_t badn=0;assert(qrx_aura_pod_announcement_sign(evil,&pa[0],&bad,&badn)==0);assert(qrx_aura_pod_gossip_ingest(&pt,&pa[0],bad,badn,H,key_lookup,&providers)==-3);free(bad);
    QrxAuraPodAnnouncement takeover=pa[0];takeover.sequence=2;snprintf(takeover.capacity.provider_id,sizeof(takeover.capacity.provider_id),"prov-b");uint8_t *ts=NULL;size_t tsl=0;assert(qrx_aura_pod_announcement_sign(kb,&takeover,&ts,&tsl)==0);assert(qrx_aura_pod_gossip_ingest(&pt,&takeover,ts,tsl,H,key_lookup,&providers)==-4);free(ts);

    QrxAuraModelCapabilityProfile mm[3];mm[0]=model("qrx/aura-nano",QRX_AURA_TIER_NANO,2,3,300,1,4500,0,1);mm[1]=model("qrx/aura-edge",QRX_AURA_TIER_EDGE,6,8,2000,1,6800,0,1);mm[2]=model("qrx/aura-k2-class",QRX_AURA_TIER_K2_CLASS,32,48,25000,3,9200,1,0);
    QrxAuraModelGossipTable mt;qrx_aura_model_gossip_init(&mt);QrxAuraModelProfileAnnouncement ma[3];uint8_t *ms[3]={0};size_t msl[3]={0};
    for(int i=0;i<3;i++){ma[i]=model_ann(mm[i],"model-root",(uint64_t)i+1,H-10,H+200,(char)('b'+i));assert(qrx_aura_model_announcement_sign(root,&ma[i],&ms[i],&msl[i])==0);assert(qrx_aura_model_gossip_ingest(&mt,&ma[i],ms[i],msl[i],H,key_lookup,&publishers,model_authorize,"model-root")==0);}
    uint8_t mh[32];char mhx[65];assert(qrx_aura_model_announcement_hash(&ma[0],mh)==0);tohex(mh,32,mhx);assert(!strcmp(mhx,"64accf4efaec89880a25864dba43501a3fc970a9f39f13b682ad8e24c894ecc1"));
    assert(mt.count==3&&qrx_aura_model_gossip_find(&mt,"qrx/aura-edge","1"));
    assert(qrx_aura_model_gossip_ingest(&mt,&ma[0],ms[0],msl[0],H,key_lookup,&publishers,model_authorize,"model-root")==-5);
    QrxAuraModelProfileAnnouncement evilma=ma[0];snprintf(evilma.publisher_id,sizeof(evilma.publisher_id),"evil-root");evilma.sequence=99;uint8_t *es=NULL;size_t esl=0;assert(qrx_aura_model_announcement_sign(evil,&evilma,&es,&esl)==0);assert(qrx_aura_model_gossip_ingest(&mt,&evilma,es,esl,H,key_lookup,&publishers,model_authorize,"model-root")==-2);free(es);

    /* Live fabric uses only current, signed announcements. Region visibility is
       provider-count privacy, not a public list of endpoints or node IDs. */
    QrxAuraFabricSnapshot snap;QrxAuraGlobeCell *cells=NULL;size_t cn=0;assert(qrx_aura_gossip_live_snapshot(&pt,&mt,H,3,&snap,&cells,&cn)==0);
    assert(snap.total_pods==4&&snap.provider_count==4&&cn==2&&snap.visible_regions==1&&snap.hidden_regions==1);
    int west=0,central=0;for(size_t i=0;i<cn;i++){if(!strcmp(cells[i].region,"EU-WEST")){west=1;assert(cells[i].publicly_visible&&cells[i].provider_count==3);}if(!strcmp(cells[i].region,"EU-CENTRAL")){central=1;assert(!cells[i].publicly_visible&&cells[i].provider_count==1);}}assert(west&&central);qrx_aura_globe_free(cells);
    QrxResourceGlobeCell *gc=NULL;size_t gn=0;assert(qrx_aura_gossip_resource_globe(&pt,H,3,&gc,&gn)==0&&gn==2);int gvis=0,ghid=0;for(size_t i=0;i<gn;i++){gvis+=gc[i].publicly_visible?1:0;ghid+=gc[i].publicly_visible?0:1;}assert(gvis==1&&ghid==1);qrx_resource_globe_free(gc);

    QrxAuraRouteRequest rr;memset(&rr,0,sizeof(rr));rr.version=QRX_AURA_FABRIC_VERSION;rr.mode=QRX_AURA_ROUTE_AUTO;rr.task_class=QRX_AURA_TASK_CHAT;rr.complexity_bps=1000;rr.allow_degradation=1;QrxAuraRouteDecision d;
    assert(qrx_aura_gossip_live_route(&pt,&mt,H,&rr,&d)==0&&d.tier==QRX_AURA_TIER_NANO);
    rr.task_class=QRX_AURA_TASK_REASONING;rr.complexity_bps=9000;assert(qrx_aura_gossip_live_route(&pt,&mt,H,&rr,&d)==0&&d.tier==QRX_AURA_TIER_K2_CLASS);

    /* Cache is only a signed transport cache: loading re-verifies every entry. */
    char dir[]="/tmp/qrx-aura-gossip-XXXXXX";assert(mkdtemp(dir));char pp[512],mp[512];snprintf(pp,sizeof(pp),"%s/pods.cache",dir);snprintf(mp,sizeof(mp),"%s/models.cache",dir);assert(qrx_aura_pod_gossip_cache_save(&pt,pp)==0);assert(qrx_aura_model_gossip_cache_save(&mt,mp)==0);
    QrxAuraPodGossipTable pt2;qrx_aura_pod_gossip_init(&pt2);QrxAuraModelGossipTable mt2;qrx_aura_model_gossip_init(&mt2);assert(qrx_aura_pod_gossip_cache_load(&pt2,pp,H,key_lookup,&providers)==4);assert(qrx_aura_model_gossip_cache_load(&mt2,mp,H,key_lookup,&publishers,model_authorize,"model-root")==3);
    QrxAuraFabricSnapshot snap2;QrxAuraGlobeCell *c2=NULL;size_t c2n=0;assert(qrx_aura_gossip_live_snapshot(&pt2,&mt2,H,3,&snap2,&c2,&c2n)==0);assert(snap2.total_pods==snap.total_pods&&snap2.provider_count==snap.provider_count&&c2n==2);qrx_aura_globe_free(c2);

    /* At H+101 pod-a has expired while the other three are still live. The
       snapshot drops it without trusting stale cached capacity. */
    assert(qrx_aura_pod_gossip_prune(&pt2,H+101)==1);assert(pt2.count==3);assert(qrx_aura_gossip_live_snapshot(&pt2,&mt2,H+101,3,&snap2,&c2,&c2n)==0);assert(snap2.total_pods==3);qrx_aura_globe_free(c2);

    qrx_aura_pod_gossip_free(&pt2);qrx_aura_model_gossip_free(&mt2);qrx_aura_pod_gossip_free(&pt);qrx_aura_model_gossip_free(&mt);unlink(pp);unlink(mp);rmdir(dir);
    for(int i=0;i<4;i++)free(psig[i]);for(int i=0;i<3;i++)free(ms[i]);EVP_PKEY_free(ka);EVP_PKEY_free(kb);EVP_PKEY_free(kc);EVP_PKEY_free(kd);EVP_PKEY_free(evil);EVP_PKEY_free(root);
    return 0;
}
