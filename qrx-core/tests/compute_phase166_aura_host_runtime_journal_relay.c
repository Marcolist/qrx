#include "compute/qrx_aura_provider_host.h"
#include "compute/qrx_aura_job_journal.h"
#include "compute/qrx_aura_relay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#define getpid _getpid
#else
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static int sa(int fd,const void*p,size_t n){const char*b=p;while(n){int z=(int)send(fd,b,(int)n,0);if(z<=0)return-1;b+=z;n-=z;}return 0;}static int ra(int fd,void*p,size_t n){char*b=p;while(n){int z=(int)recv(fd,b,(int)n,0);if(z<=0)return-1;b+=z;n-=z;}return 0;}
typedef struct{char ep[256];int rc;} ProviderArg;
static void provider_echo(ProviderArg*a){a->rc=-1;int fd=qrx_aura_relay_provider_connect(a->ep);if(fd<0)return;uint8_t h[4],b[32];if(ra(fd,h,4))goto done;uint32_t n=((uint32_t)h[0]<<24)|((uint32_t)h[1]<<16)|((uint32_t)h[2]<<8)|h[3];if(n>sizeof(b)||n<16||ra(fd,b,n)||sa(fd,h,4)||sa(fd,b,n))goto done;a->rc=0;done:
#ifdef _WIN32
shutdown(fd,SD_BOTH);closesocket(fd);
#else
shutdown(fd,SHUT_RDWR);close(fd);
#endif
}
#ifdef _WIN32
static DWORD WINAPI provider_thread(LPVOID p){provider_echo((ProviderArg*)p);return 0;}
#else
static void*provider_thread(void*p){provider_echo((ProviderArg*)p);return NULL;}
#endif
int main(int argc,char**argv){CHECK(argc>=2);
    char cfgp[512],jp[512];snprintf(cfgp,sizeof(cfgp),"phase166-host-%ld.conf",(long)getpid());snprintf(jp,sizeof(jp),"phase166-jobs-%ld.dat",(long)getpid());remove(cfgp);remove(jp);
    QrxAuraProviderHostConfig c;qrx_aura_provider_host_config_defaults(&c);c.enabled=1;snprintf(c.provider_id,sizeof(c.provider_id),"provider-166");snprintf(c.pod_id,sizeof(c.pod_id),"pod-166");snprintf(c.network,sizeof(c.network),"alpha");snprintf(c.region,sizeof(c.region),"DE");c.compute_threads=4;c.model_cache_bytes=8ull<<30;c.free_memory_bytes=6ull<<30;c.network_egress_mbps=1000;snprintf(c.lease_journal_path,sizeof(c.lease_journal_path),"phase166-leases.dat");snprintf(c.job_journal_path,sizeof(c.job_journal_path),"%s",jp);c.runtime_adapter.kind=QRX_AURA_ADAPTER_LLAMA_CPP_CPU;snprintf(c.runtime_adapter.explicit_path,sizeof(c.runtime_adapter.explicit_path),"%s",argv[1]);CHECK(qrx_aura_provider_host_config_save(cfgp,&c)==0);QrxAuraProviderHostConfig d;CHECK(qrx_aura_provider_host_config_load(cfgp,&d)==0);CHECK(d.enabled&&d.compute_threads==4&&!strcmp(d.provider_id,c.provider_id)&&d.runtime_adapter.kind==QRX_AURA_ADAPTER_LLAMA_CPP_CPU);
    QrxMoeRuntimeDevice dev;memset(&dev,0,sizeof(dev));dev.version=QRX_MOE_RUNTIME_VERSION;dev.backend=QRX_MOE_BACKEND_CPU;snprintf(dev.device_name,sizeof(dev.device_name),"phase166 cpu");dev.accelerator_features=QRX_MOE_ACCEL_FEAT_FP32;dev.device_memory_bytes=8ull<<30;dev.max_batch_items=8;char resolved[1024];QrxAuraRuntimeAdapterKind rk;CHECK(qrx_aura_runtime_adapter_resolve(&d.runtime_adapter,&dev,resolved,&rk)==0&&rk==QRX_AURA_ADAPTER_LLAMA_CPP_CPU&&!strcmp(resolved,argv[1]));QrxAuraRuntimePluginHost host;CHECK(qrx_aura_runtime_adapter_open(&d.runtime_adapter,&dev,NULL,&host,resolved)==0);qrx_aura_runtime_plugin_close(&host);
    QrxAuraJobJournal j;qrx_aura_job_journal_init(&j);char lease[65],req[65];memset(lease,'a',64);lease[64]=0;memset(req,'b',64);req[64]=0;const char result[]="durable-result";CHECK(qrx_aura_job_journal_prepare(&j,"requester",lease,7,req)==0);CHECK(qrx_aura_job_journal_save(jp,&j)==0);CHECK(qrx_aura_job_journal_commit(&j,"requester",lease,7,req,0,result,sizeof(result))==0);CHECK(qrx_aura_job_journal_save(jp,&j)==0);QrxAuraJobJournal loaded;qrx_aura_job_journal_init(&loaded);CHECK(qrx_aura_job_journal_load(jp,&loaded)==0);const QrxAuraJobRecord*r=qrx_aura_job_journal_find_const(&loaded,"requester",lease,7);CHECK(r&&r->state==QRX_AURA_JOB_COMMITTED&&r->result_len==sizeof(result)&&!memcmp(r->result,result,sizeof(result)));char changed[65];memset(changed,'c',64);changed[64]=0;CHECK(qrx_aura_job_journal_prepare(&loaded,"requester",lease,7,changed)==-2);qrx_aura_job_journal_free(&loaded);qrx_aura_job_journal_free(&j);
    QrxAuraRelayServer*relay=NULL;CHECK(qrx_aura_relay_server_start("127.0.0.1",0,&relay)==0);ProviderArg pa;memset(&pa,0,sizeof(pa));snprintf(pa.ep,sizeof(pa.ep),"qrxrelay://127.0.0.1:%u/phase166",(unsigned)qrx_aura_relay_server_port(relay));
#ifdef _WIN32
    HANDLE th=CreateThread(NULL,0,provider_thread,&pa,0,NULL);CHECK(th!=NULL);Sleep(100);
#else
    pthread_t th;CHECK(pthread_create(&th,NULL,provider_thread,&pa)==0);usleep(100000);
#endif
    int fd=qrx_aura_relay_client_connect(pa.ep);CHECK(fd>=0);uint8_t hdr[4]={0,0,0,16},payload[16];for(int i=0;i<16;i++)payload[i]=(uint8_t)i;CHECK(sa(fd,hdr,4)==0&&sa(fd,payload,16)==0);uint8_t rh[4],rp[16];CHECK(ra(fd,rh,4)==0&&ra(fd,rp,16)==0&&!memcmp(hdr,rh,4)&&!memcmp(payload,rp,16));
#ifdef _WIN32
    shutdown(fd,SD_BOTH);closesocket(fd);WaitForSingleObject(th,5000);CloseHandle(th);
#else
    shutdown(fd,SHUT_RDWR);close(fd);pthread_join(th,NULL);
#endif
    CHECK(pa.rc==0);CHECK(qrx_aura_relay_server_stop(relay)==0);qrx_aura_relay_server_free(relay);remove(cfgp);remove(jp);printf("phase166 AURA host/runtime/journal/relay: PASS\n");return 0;}
