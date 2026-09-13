#include "compute/qrx_aura_provider_service.h"
#include "compute/qrx_aura_relay.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define qrx_close_socket closesocket
typedef CRITICAL_SECTION qrx_service_mutex_t;
static void sm_init(qrx_service_mutex_t*m){InitializeCriticalSection(m);}
static void sm_lock(qrx_service_mutex_t*m){EnterCriticalSection(m);}
static void sm_unlock(qrx_service_mutex_t*m){LeaveCriticalSection(m);}
static void sm_free(qrx_service_mutex_t*m){DeleteCriticalSection(m);}
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define qrx_close_socket close
typedef pthread_mutex_t qrx_service_mutex_t;
static void sm_init(qrx_service_mutex_t*m){pthread_mutex_init(m,NULL);}
static void sm_lock(qrx_service_mutex_t*m){pthread_mutex_lock(m);}
static void sm_unlock(qrx_service_mutex_t*m){pthread_mutex_unlock(m);}
static void sm_free(qrx_service_mutex_t*m){pthread_mutex_destroy(m);}
#endif

static uint32_t be32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t be64(const uint8_t*p){uint64_t v=0;for(int i=0;i<8;i++)v=(v<<8)|p[i];return v;}
static void put32(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void put64(uint8_t*p,uint64_t v){for(int i=7;i>=0;i--){p[i]=(uint8_t)v;v>>=8;}}
static int hex64(const char*s){if(!s||strlen(s)!=64)return 0;for(int i=0;i<64;i++){char c=s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;}return 1;}
static void hex32(const uint8_t h[32],char out[65]){static const char x[]="0123456789abcdef";for(int i=0;i<32;i++){out[i*2]=x[h[i]>>4];out[i*2+1]=x[h[i]&15];}out[64]=0;}
typedef struct{uint8_t*p;size_t n,cap;} Buf;
static int grow(Buf*b,size_t add){if(!b||SIZE_MAX-b->n<add)return -1;size_t need=b->n+add;if(need<=b->cap)return 0;size_t c=b->cap?b->cap:256;while(c<need){if(c>SIZE_MAX/2){c=need;break;}c*=2;}uint8_t*q=realloc(b->p,c);if(!q)return -1;b->p=q;b->cap=c;return 0;}
static int bp(Buf*b,const void*p,size_t n){if(grow(b,n))return -1;if(n)memcpy(b->p+b->n,p,n);b->n+=n;return 0;}
static int b32(Buf*b,uint32_t v){uint8_t x[4];put32(x,v);return bp(b,x,4);}
static int b64(Buf*b,uint64_t v){uint8_t x[8];put64(x,v);return bp(b,x,8);}
static int bs(Buf*b,const char*s,size_t cap){if(!s||!memchr(s,'\0',cap))return -1;size_t n=strlen(s);if(n>UINT32_MAX)return -1;return b32(b,(uint32_t)n)||bp(b,s,n);}
typedef struct{const uint8_t*p;size_t n,o;} Rd;
static int rg(Rd*r,void*out,size_t n){if(!r||r->o>r->n||r->n-r->o<n)return -1;if(n)memcpy(out,r->p+r->o,n);r->o+=n;return 0;}
static int r32(Rd*r,uint32_t*out){uint8_t x[4];if(rg(r,x,4))return -1;*out=be32(x);return 0;}
static int r64(Rd*r,uint64_t*out){uint8_t x[8];if(rg(r,x,8))return -1;*out=be64(x);return 0;}
static int rs(Rd*r,char*out,size_t cap){uint32_t n=0;if(r32(r,&n)||n>=cap||r->o>r->n||r->n-r->o<n)return -1;memcpy(out,r->p+r->o,n);out[n]=0;r->o+=n;return 0;}
static int sha3_plain(const void*p,size_t n,char out[65]){EVP_MD_CTX*m=EVP_MD_CTX_new();uint8_t h[32];unsigned hn=0;if(!m||EVP_DigestInit_ex(m,EVP_sha3_256(),NULL)!=1||(n&&EVP_DigestUpdate(m,p,n)!=1)||EVP_DigestFinal_ex(m,h,&hn)!=1||hn!=32){if(m)EVP_MD_CTX_free(m);return -1;}EVP_MD_CTX_free(m);hex32(h,out);return 0;}
static int sha3_domain(const char*d,const void*p,size_t n,char out[65]){EVP_MD_CTX*m=EVP_MD_CTX_new();uint8_t h[32];unsigned hn=0;if(!m||EVP_DigestInit_ex(m,EVP_sha3_256(),NULL)!=1||EVP_DigestUpdate(m,d,strlen(d))!=1||(n&&EVP_DigestUpdate(m,p,n)!=1)||EVP_DigestFinal_ex(m,h,&hn)!=1||hn!=32){if(m)EVP_MD_CTX_free(m);return -1;}EVP_MD_CTX_free(m);hex32(h,out);return 0;}

static int expert_manifest_valid(const QrxAuraExpertManifest*m){return m&&m->version==QRX_AURA_EXPERT_MANIFEST_VERSION&&m->model_id[0]&&m->model_version[0]&&m->architecture[0]&&m->layer_count&&m->expert_count&&m->expert_count<=QRX_AURA_MOE_MAX_EXPERTS&&m->experts_per_token&&m->experts_per_token<=m->expert_count;}
int qrx_aura_expert_manifest_encode(const QrxAuraExpertManifest*m,uint8_t**out,size_t*outn){if(!expert_manifest_valid(m)||!out||!outn)return -1;Buf b={0};if(bp(&b,"QRXEM39\0",8)||b32(&b,m->version)||bs(&b,m->model_id,sizeof(m->model_id))||bs(&b,m->model_version,sizeof(m->model_version))||bs(&b,m->architecture,sizeof(m->architecture))||b32(&b,m->layer_count)||b32(&b,m->expert_count)||b32(&b,m->experts_per_token)){free(b.p);return -1;}*out=b.p;*outn=b.n;return 0;}
int qrx_aura_expert_manifest_decode(const uint8_t*in,size_t n,QrxAuraExpertManifest*m){if(!in||!m||n<8||memcmp(in,"QRXEM39\0",8))return -1;memset(m,0,sizeof(*m));Rd r={in+8,n-8,0};if(r32(&r,&m->version)||rs(&r,m->model_id,sizeof(m->model_id))||rs(&r,m->model_version,sizeof(m->model_version))||rs(&r,m->architecture,sizeof(m->architecture))||r32(&r,&m->layer_count)||r32(&r,&m->expert_count)||r32(&r,&m->experts_per_token)||r.o!=r.n||!expert_manifest_valid(m))return -1;return 0;}
int qrx_aura_expert_manifest_root(const QrxAuraExpertManifest*m,char out[65]){uint8_t*b=NULL;size_t n=0;if(qrx_aura_expert_manifest_encode(m,&b,&n))return -1;int rc=sha3_plain(b,n,out);free(b);return rc;}
int qrx_aura_expert_manifest_load(QrxStorageFs*fs,const QrxAiModelRecord*model,QrxAuraExpertManifest*out){if(!fs||!model||!out||!model->is_moe||!hex64(model->expert_manifest_root))return -1;uint8_t*b=NULL;size_t n=0;if(qrx_storage_fs_read(fs,model->expert_manifest_root,&b,&n))return -2;char root[65];int rc=sha3_plain(b,n,root);if(rc||strcmp(root,model->expert_manifest_root)||qrx_aura_expert_manifest_decode(b,n,out)||strcmp(out->model_id,model->model_id)||strcmp(out->model_version,model->model_version)||strcmp(out->architecture,model->architecture))rc=-3;free(b);return rc;}

int qrx_aura_moe_fragment_plan_commitment(const QrxAuraMoeFragmentPlan*p,char out[65]){if(!p||!out||p->version!=QRX_AURA_MOE_FRAGMENT_VERSION||p->strategy<QRX_AURA_FRAGMENT_SINGLE||p->strategy>QRX_AURA_FRAGMENT_EXPERT_RANGE||!hex64(p->model_commitment)||!p->fragment_count||p->fragment_count>QRX_AURA_MOE_MAX_FRAGMENTS)return -1;Buf b={0};if(b32(&b,p->version)||b32(&b,p->strategy)||bs(&b,p->model_id,sizeof(p->model_id))||bs(&b,p->model_version,sizeof(p->model_version))||bs(&b,p->model_commitment,65)||bs(&b,p->expert_manifest_root,65)||b32(&b,p->layer_count)||b32(&b,p->expert_count)||b32(&b,p->experts_per_token)||b32(&b,p->fragment_count)){free(b.p);return -1;}for(uint32_t i=0;i<p->fragment_count;i++){const QrxAuraMoeFragment*f=&p->fragments[i];if(f->fragment_index!=i||f->fragment_count!=p->fragment_count||!f->provider_id[0]||!f->pod_id[0]||b32(&b,f->fragment_index)||b32(&b,f->fragment_count)||b32(&b,f->expert_first)||b32(&b,f->expert_count)||b32(&b,f->layer_first)||b32(&b,f->layer_count)||bs(&b,f->provider_id,sizeof(f->provider_id))||bs(&b,f->pod_id,sizeof(f->pod_id))){free(b.p);return -1;}}int rc=sha3_domain("QRX/AURA/MOE-FRAGMENT-PLAN/V1",b.p,b.n,out);free(b.p);return rc;}
int qrx_aura_moe_fragment_plan_build(const QrxAiModelRecord*m,const QrxAuraExpertManifest*em,const QrxAuraAdmission*a,QrxAuraMoeFragmentPlan*out){if(!m||!a||!out||!a->pod_count||a->pod_count>QRX_AURA_MOE_MAX_FRAGMENTS||strcmp(m->model_id,a->route.model_id)||strcmp(m->model_version,a->route.model_version))return -1;char mc[65];if(qrx_ai_model_commitment(m,mc)||strcmp(mc,a->model_commitment))return -2;memset(out,0,sizeof(*out));out->version=QRX_AURA_MOE_FRAGMENT_VERSION;snprintf(out->model_id,sizeof(out->model_id),"%s",m->model_id);snprintf(out->model_version,sizeof(out->model_version),"%s",m->model_version);snprintf(out->model_commitment,65,"%s",mc);out->fragment_count=a->pod_count;
    if(m->is_moe){if(!em||!expert_manifest_valid(em)||strcmp(em->model_id,m->model_id)||strcmp(em->model_version,m->model_version)||strcmp(em->architecture,m->architecture))return -3;char er[65];if(qrx_aura_expert_manifest_root(em,er)||strcmp(er,m->expert_manifest_root)||em->expert_count<a->pod_count)return -3;out->strategy=QRX_AURA_FRAGMENT_EXPERT_RANGE;snprintf(out->expert_manifest_root,65,"%s",er);out->layer_count=em->layer_count;out->expert_count=em->expert_count;out->experts_per_token=em->experts_per_token;uint32_t base=em->expert_count/a->pod_count,rem=em->expert_count%a->pod_count,first=0;for(uint32_t i=0;i<a->pod_count;i++){QrxAuraMoeFragment*f=&out->fragments[i];uint32_t cnt=base+(i<rem?1u:0u);f->fragment_index=i;f->fragment_count=a->pod_count;f->expert_first=first;f->expert_count=cnt;f->layer_first=0;f->layer_count=em->layer_count;snprintf(f->provider_id,sizeof(f->provider_id),"%s",a->pods[i].provider_id);snprintf(f->pod_id,sizeof(f->pod_id),"%s",a->pods[i].pod_id);first+=cnt;}}
    else {out->strategy=a->pod_count==1?QRX_AURA_FRAGMENT_SINGLE:QRX_AURA_FRAGMENT_REPLICATED;memset(out->expert_manifest_root,'0',64);out->expert_manifest_root[64]=0;for(uint32_t i=0;i<a->pod_count;i++){QrxAuraMoeFragment*f=&out->fragments[i];f->fragment_index=i;f->fragment_count=a->pod_count;snprintf(f->provider_id,sizeof(f->provider_id),"%s",a->pods[i].provider_id);snprintf(f->pod_id,sizeof(f->pod_id),"%s",a->pods[i].pod_id);}}
    return qrx_aura_moe_fragment_plan_commitment(out,out->plan_commitment);
}
int qrx_aura_moe_fragment_payload_encode(const QrxAuraMoeFragmentPlan*p,uint32_t idx,const void*input,size_t in_n,uint8_t**out,size_t*outn){if(!p||!out||!outn||idx>=p->fragment_count||(in_n&&!input)||in_n>QRX_AURA_REMOTE_MAX_PAYLOAD-256)return -1;char pc[65];if(qrx_aura_moe_fragment_plan_commitment(p,pc)||strcmp(pc,p->plan_commitment))return -2;const QrxAuraMoeFragment*f=&p->fragments[idx];Buf b={0};if(bp(&b,QRX_AURA_MOE_FRAGMENT_MAGIC,8)||b32(&b,QRX_AURA_MOE_FRAGMENT_VERSION)||bs(&b,p->plan_commitment,65)||b32(&b,p->strategy)||b32(&b,f->fragment_index)||b32(&b,f->fragment_count)||b32(&b,f->expert_first)||b32(&b,f->expert_count)||b32(&b,f->layer_first)||b32(&b,f->layer_count)||b32(&b,p->experts_per_token)||b64(&b,(uint64_t)in_n)||bp(&b,input,in_n)){free(b.p);return -2;}*out=b.p;*outn=b.n;return 0;}
int qrx_aura_moe_fragment_payload_decode(const uint8_t*in,size_t n,QrxAuraMoeFragmentEnvelope*out){if(!in||!out||n<8||memcmp(in,QRX_AURA_MOE_FRAGMENT_MAGIC,8))return -1;memset(out,0,sizeof(*out));Rd r={in+8,n-8,0};uint64_t pn=0;if(r32(&r,&out->version)||rs(&r,out->plan_commitment,sizeof(out->plan_commitment))||r32(&r,&out->strategy)||r32(&r,&out->fragment_index)||r32(&r,&out->fragment_count)||r32(&r,&out->expert_first)||r32(&r,&out->expert_count)||r32(&r,&out->layer_first)||r32(&r,&out->layer_count)||r32(&r,&out->experts_per_token)||r64(&r,&pn)||out->version!=QRX_AURA_MOE_FRAGMENT_VERSION||!hex64(out->plan_commitment)||!out->fragment_count||out->fragment_index>=out->fragment_count||pn>SIZE_MAX||r.o>r.n||r.n-r.o!=(size_t)pn)return -2;out->input=malloc((size_t)pn?(size_t)pn:1);if(!out->input)return -3;if(pn)memcpy(out->input,r.p+r.o,(size_t)pn);out->input_len=(size_t)pn;return 0;}
void qrx_aura_moe_fragment_envelope_free(QrxAuraMoeFragmentEnvelope*e){if(!e)return;free(e->input);memset(e,0,sizeof(*e));}
int qrx_aura_moe_fragment_input_adapter(void*ctx,uint32_t idx,uint32_t count,const void*input,size_t in_n,uint8_t**out,size_t*outn){QrxAuraMoeFragmentPlan*p=ctx;if(!p||count!=p->fragment_count)return -1;return qrx_aura_moe_fragment_payload_encode(p,idx,input,in_n,out,outn);}

struct QrxAuraProviderService{
    QrxAuraProviderServiceConfig cfg;
    QrxAuraReservationLeaseTable leases;
    QrxAuraJobJournal jobs;
    char job_journal_path[1024];
    QrxAuraRemoteServerContext server;
    QrxAuraPqSessionCache pq_sessions;
    int listen_fd;
    int relay_fd;
    uint16_t port;
    char endpoint[QRX_AURA_GOSSIP_MAX_ENDPOINT];
    volatile int stop;
    volatile int running;
    volatile int relay_running;
    uint64_t last_advertised_until;
    qrx_service_mutex_t state_lock;
#ifdef _WIN32
    HANDLE thread;
    HANDLE relay_thread;
#else
    pthread_t thread;
    pthread_t relay_thread;
    int relay_thread_started;
#endif
};

static int net_init(void){
#ifdef _WIN32
    static LONG init=0;if(InterlockedCompareExchange(&init,1,0)==0){WSADATA w;if(WSAStartup(MAKEWORD(2,2),&w)!=0){InterlockedExchange(&init,0);return -1;}}
#endif
    return 0;
}
static int open_listener(const char*host,uint16_t port,uint16_t*out_port){if(net_init())return -1;int fd=(int)socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;int one=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(const char*)&one,sizeof(one));struct sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons(port);if(!host||!host[0]||!strcmp(host,"0.0.0.0"))a.sin_addr.s_addr=htonl(INADDR_ANY);else if(inet_pton(AF_INET,host,&a.sin_addr)!=1){qrx_close_socket(fd);return -1;}if(bind(fd,(struct sockaddr*)&a,sizeof(a))||listen(fd,32)){qrx_close_socket(fd);return -1;}socklen_t z=sizeof(a);if(getsockname(fd,(struct sockaddr*)&a,&z)){qrx_close_socket(fd);return -1;}*out_port=ntohs(a.sin_port);return fd;}
static uint32_t active_leases(const QrxAuraReservationLeaseTable*t){uint32_t n=0;if(t)for(uint32_t i=0;i<t->count;i++)if(t->entries[i].state==QRX_AURA_LEASE_ACTIVE)n++;return n;}
static int advertise_now(QrxAuraProviderService*s){if(!s||!s->cfg.auto_advertise)return 0;if(!s->cfg.advertise||qrx_aura_pod_capacity_validate(&s->cfg.advertised_capacity)||strcmp(s->cfg.advertised_capacity.provider_id,s->cfg.binding->provider_id)||strcmp(s->cfg.advertised_capacity.pod_id,s->cfg.binding->pod_id))return -1;uint64_t h=s->cfg.height_fn(s->cfg.height_ctx),ttl=s->cfg.advertisement_ttl_blocks?s->cfg.advertisement_ttl_blocks:120;if(UINT64_MAX-h<ttl)return -1;QrxAuraPodAnnouncement a;memset(&a,0,sizeof(a));a.version=QRX_AURA_GOSSIP_VERSION;a.sequence=++s->cfg.advertisement_sequence;a.valid_from_height=h;a.valid_until_height=h+ttl;snprintf(a.service_endpoint,sizeof(a.service_endpoint),"%s",s->endpoint);a.capacity=s->cfg.advertised_capacity;uint8_t*sig=NULL;size_t sn=0;if(qrx_aura_pod_announcement_sign(s->cfg.provider_private_key,&a,&sig,&sn))return -1;int rc=s->cfg.advertise(s->cfg.advertise_ctx,&a,sig,sn);free(sig);if(!rc)s->last_advertised_until=a.valid_until_height;return rc;}
static int should_refresh(QrxAuraProviderService*s){if(!s||!s->cfg.auto_advertise)return 0;uint64_t h=s->cfg.height_fn(s->cfg.height_ctx),ttl=s->cfg.advertisement_ttl_blocks?s->cfg.advertisement_ttl_blocks:120,margin=ttl/3?ttl/3:1;return !s->last_advertised_until||h+margin>=s->last_advertised_until;}
static void service_loop(QrxAuraProviderService*s){s->running=1;while(!s->stop){if(should_refresh(s))advertise_now(s);fd_set rf;FD_ZERO(&rf);FD_SET(s->listen_fd,&rf);struct timeval tv={1,0};int rc=select(s->listen_fd+1,&rf,NULL,NULL,&tv);if(rc<=0)continue;int fd=(int)accept(s->listen_fd,NULL,NULL);if(fd<0){if(s->stop)break;continue;}sm_lock(&s->state_lock);qrx_aura_remote_serve_fd(fd,&s->server);sm_unlock(&s->state_lock);qrx_close_socket(fd);}s->running=0;}
static void relay_loop(QrxAuraProviderService*s){
    s->relay_running=1;
    while(!s->stop&&s->cfg.relay_endpoint[0]){
        int fd=qrx_aura_relay_provider_connect(s->cfg.relay_endpoint);
        if(fd<0){
#ifdef _WIN32
            Sleep(250);
#else
            usleep(250000);
#endif
            continue;
        }
        s->relay_fd=fd;
        while(!s->stop){
            sm_lock(&s->state_lock);int rc=qrx_aura_remote_serve_fd(fd,&s->server);sm_unlock(&s->state_lock);
            if(rc)break;
        }
#ifdef _WIN32
        shutdown(fd,SD_BOTH);
#else
        shutdown(fd,SHUT_RDWR);
#endif
        qrx_close_socket(fd);s->relay_fd=-1;
    }
    s->relay_running=0;
}
#ifdef _WIN32
static DWORD WINAPI relay_service_thread(LPVOID p){relay_loop((QrxAuraProviderService*)p);return 0;}
static DWORD WINAPI service_thread(LPVOID p){service_loop((QrxAuraProviderService*)p);return 0;}
#else
static void*relay_service_thread(void*p){relay_loop((QrxAuraProviderService*)p);return NULL;}
static void*service_thread(void*p){service_loop((QrxAuraProviderService*)p);return NULL;}
#endif
int qrx_aura_provider_service_start(const QrxAuraProviderServiceConfig*c,QrxAuraProviderService**out){
    if(!c||!out||c->version!=QRX_AURA_PROVIDER_SERVICE_VERSION||!c->binding||!c->binding->provider_runtime||c->binding->provider_runtime->status!=QRX_PROVIDER_RUNTIME_SERVING||!c->model_cache||!c->requester_key_lookup||!c->provider_private_key||!c->height_fn||!c->lease_journal_path[0])return -1;
    *out=NULL;QrxAuraProviderService*s=calloc(1,sizeof(*s));if(!s)return -1;s->cfg=*c;s->listen_fd=-1;s->relay_fd=-1;sm_init(&s->state_lock);qrx_aura_job_journal_init(&s->jobs);
    if(EVP_PKEY_up_ref(c->provider_private_key)!=1){free(s);return -1;}s->cfg.provider_private_key=c->provider_private_key;
    if(c->enable_pq_hybrid_sessions){
        if(c->hybrid_kem_private_key){if(EVP_PKEY_is_a(c->hybrid_kem_private_key,QRX_AURA_PQ_HYBRID_KEM_NAME)!=1||EVP_PKEY_up_ref(c->hybrid_kem_private_key)!=1){EVP_PKEY_free(s->cfg.provider_private_key);free(s);return -1;}s->cfg.hybrid_kem_private_key=c->hybrid_kem_private_key;}
        else if(qrx_aura_pq_hybrid_kem_generate(&s->cfg.hybrid_kem_private_key)){EVP_PKEY_free(s->cfg.provider_private_key);free(s);return -1;}
        qrx_aura_pq_session_cache_init(&s->pq_sessions);
    } else s->cfg.hybrid_kem_private_key=NULL;
    size_t rec=0,exp=0;uint64_t h=c->height_fn(c->height_ctx);
    if(qrx_aura_reservation_leases_journal_recover(c->lease_journal_path,&s->leases,c->binding,1,h,&rec,&exp)){EVP_PKEY_free(s->cfg.hybrid_kem_private_key);EVP_PKEY_free(s->cfg.provider_private_key);sm_free(&s->state_lock);free(s);return -2;}
    if(c->job_journal_path[0])snprintf(s->job_journal_path,sizeof(s->job_journal_path),"%s",c->job_journal_path);
    else if(snprintf(s->job_journal_path,sizeof(s->job_journal_path),"%s.jobs",c->lease_journal_path)>=(int)sizeof(s->job_journal_path)){EVP_PKEY_free(s->cfg.hybrid_kem_private_key);EVP_PKEY_free(s->cfg.provider_private_key);sm_free(&s->state_lock);free(s);return -2;}
    int jrc=qrx_aura_job_journal_load(s->job_journal_path,&s->jobs);if(jrc!=0&&jrc!=1){EVP_PKEY_free(s->cfg.hybrid_kem_private_key);EVP_PKEY_free(s->cfg.provider_private_key);sm_free(&s->state_lock);free(s);return -2;}
    const char*host=c->listen_host[0]?c->listen_host:"127.0.0.1";s->listen_fd=open_listener(host,c->listen_port,&s->port);
    if(s->listen_fd<0){EVP_PKEY_free(s->cfg.hybrid_kem_private_key);EVP_PKEY_free(s->cfg.provider_private_key);free(s);return -3;}
    if(c->relay_endpoint[0])snprintf(s->endpoint,sizeof(s->endpoint),"%s",c->relay_endpoint);else snprintf(s->endpoint,sizeof(s->endpoint),"qrxp2p://%s:%u",host,(unsigned)s->port);
    s->server.binding=c->binding;s->server.leases=&s->leases;s->server.model_cache=c->model_cache;s->server.requester_key_lookup=c->requester_key_lookup;s->server.requester_key_ctx=c->requester_key_ctx;s->server.provider_private_key=s->cfg.provider_private_key;s->server.height_fn=c->height_fn;s->server.height_ctx=c->height_ctx;s->server.lease_journal_path=s->cfg.lease_journal_path;s->server.require_durable_leases=1;s->server.job_journal=&s->jobs;s->server.job_journal_path=s->job_journal_path;s->server.require_durable_jobs=1;s->server.secure_session_lookup=c->secure_session_lookup;s->server.secure_session_ctx=c->secure_session_ctx;s->server.pq_session_cache=c->enable_pq_hybrid_sessions?&s->pq_sessions:NULL;s->server.hybrid_kem_private_key=s->cfg.hybrid_kem_private_key;s->server.pq_session_ttl_blocks=c->pq_session_ttl_blocks;s->server.require_secure_dispatch=c->require_secure_dispatch;s->server.model_registry=c->model_registry;s->server.model_fetch=c->model_fetch;s->server.model_fetch_ctx=c->model_fetch_ctx;s->server.model_execute=c->model_execute;s->server.model_execute_ctx=c->model_execute_ctx;
    if(c->auto_advertise&&advertise_now(s)){qrx_close_socket(s->listen_fd);EVP_PKEY_free(s->cfg.hybrid_kem_private_key);EVP_PKEY_free(s->cfg.provider_private_key);free(s);return -4;}
#ifdef _WIN32
    s->thread=CreateThread(NULL,0,service_thread,s,0,NULL);if(!s->thread){qrx_close_socket(s->listen_fd);EVP_PKEY_free(s->cfg.hybrid_kem_private_key);EVP_PKEY_free(s->cfg.provider_private_key);free(s);return -5;}
#else
    if(pthread_create(&s->thread,NULL,service_thread,s)){qrx_close_socket(s->listen_fd);EVP_PKEY_free(s->cfg.hybrid_kem_private_key);EVP_PKEY_free(s->cfg.provider_private_key);free(s);return -5;}
#endif
    if(c->relay_endpoint[0]){
#ifdef _WIN32
        s->relay_thread=CreateThread(NULL,0,relay_service_thread,s,0,NULL);if(!s->relay_thread&&c->relay_required){s->stop=1;shutdown(s->listen_fd,SD_BOTH);qrx_close_socket(s->listen_fd);WaitForSingleObject(s->thread,5000);CloseHandle(s->thread);qrx_aura_job_journal_free(&s->jobs);EVP_PKEY_free(s->cfg.hybrid_kem_private_key);EVP_PKEY_free(s->cfg.provider_private_key);sm_free(&s->state_lock);free(s);return -6;}
#else
        if(!pthread_create(&s->relay_thread,NULL,relay_service_thread,s))s->relay_thread_started=1;else if(c->relay_required){s->stop=1;shutdown(s->listen_fd,SHUT_RDWR);qrx_close_socket(s->listen_fd);pthread_join(s->thread,NULL);qrx_aura_job_journal_free(&s->jobs);EVP_PKEY_free(s->cfg.hybrid_kem_private_key);EVP_PKEY_free(s->cfg.provider_private_key);sm_free(&s->state_lock);free(s);return -6;}
#endif
    }
    *out=s;return 0;
}
int qrx_aura_provider_service_stop(QrxAuraProviderService*s){
    if(!s)return -1;if(s->stop)return 0;s->stop=1;
    if(s->listen_fd>=0){
#ifdef _WIN32
        shutdown(s->listen_fd,SD_BOTH);
#else
        shutdown(s->listen_fd,SHUT_RDWR);
#endif
        qrx_close_socket(s->listen_fd);s->listen_fd=-1;
    }
    if(s->relay_fd>=0){
#ifdef _WIN32
        shutdown(s->relay_fd,SD_BOTH);
#else
        shutdown(s->relay_fd,SHUT_RDWR);
#endif
    }
#ifdef _WIN32
    if(s->thread){WaitForSingleObject(s->thread,5000);CloseHandle(s->thread);s->thread=NULL;}
    if(s->relay_thread){WaitForSingleObject(s->relay_thread,5000);CloseHandle(s->relay_thread);s->relay_thread=NULL;}
#else
    pthread_join(s->thread,NULL);if(s->relay_thread_started){pthread_join(s->relay_thread,NULL);s->relay_thread_started=0;}
#endif
    int a=qrx_aura_reservation_leases_journal_save(s->cfg.lease_journal_path,&s->leases);
    int b=qrx_aura_job_journal_save(s->job_journal_path,&s->jobs);
    return a?a:b;
}
void qrx_aura_provider_service_free(QrxAuraProviderService*s){if(!s)return;if(!s->stop)qrx_aura_provider_service_stop(s);qrx_aura_job_journal_free(&s->jobs);EVP_PKEY_free(s->cfg.hybrid_kem_private_key);EVP_PKEY_free(s->cfg.provider_private_key);OPENSSL_cleanse(&s->pq_sessions,sizeof(s->pq_sessions));sm_free(&s->state_lock);free(s);}
const char*qrx_aura_provider_service_endpoint(const QrxAuraProviderService*s){return s?s->endpoint:NULL;}
uint16_t qrx_aura_provider_service_port(const QrxAuraProviderService*s){return s?s->port:0;}
uint32_t qrx_aura_provider_service_active_leases(const QrxAuraProviderService*s){return s?active_leases(&s->leases):0;}
uint64_t qrx_aura_provider_service_announcement_sequence(const QrxAuraProviderService*s){return s?s->cfg.advertisement_sequence:0;}
uint32_t qrx_aura_provider_service_pq_session_count(const QrxAuraProviderService*s){return s?s->pq_sessions.count:0;}
const char*qrx_aura_provider_service_pq_kem_name(const QrxAuraProviderService*s){return s&&s->cfg.enable_pq_hybrid_sessions?QRX_AURA_PQ_HYBRID_KEM_NAME:NULL;}
