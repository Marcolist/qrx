#include "compute/qrx_aura_provider_host.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
static int nonempty(const char*s,size_t n){return s&&memchr(s,0,n)&&s[0];}
static int endpoint_ok(const char*s){return !s||!s[0]||!strncmp(s,"qrxrelay://",11);}
void qrx_aura_provider_host_config_defaults(QrxAuraProviderHostConfig*c){if(!c)return;memset(c,0,sizeof(*c));c->version=QRX_AURA_PROVIDER_HOST_CONFIG_VERSION;c->require_wallet_approval=1;c->require_secure_dispatch=1;c->enable_pq_hybrid_sessions=1;c->runtime_adapter.version=QRX_AURA_RUNTIME_ADAPTER_VERSION;c->runtime_adapter.kind=QRX_AURA_ADAPTER_AUTO;snprintf(c->listen_host,sizeof(c->listen_host),"127.0.0.1");}
int qrx_aura_provider_host_config_validate(const QrxAuraProviderHostConfig*c){if(!c||c->version!=1||c->runtime_adapter.version!=1||c->runtime_adapter.kind>QRX_AURA_ADAPTER_LLAMA_CPP_METAL||c->relay_count>QRX_AURA_PROVIDER_RELAY_MAX)return-1;if(c->enabled&&(!nonempty(c->provider_id,sizeof(c->provider_id))||!nonempty(c->pod_id,sizeof(c->pod_id))||!nonempty(c->network,sizeof(c->network))||!nonempty(c->region,sizeof(c->region))||!nonempty(c->lease_journal_path,sizeof(c->lease_journal_path))||!nonempty(c->job_journal_path,sizeof(c->job_journal_path))))return-2;if(c->enabled&&(!c->require_wallet_approval||!c->require_secure_dispatch||!c->enable_pq_hybrid_sessions))return-3;for(uint32_t i=0;i<c->relay_count;i++)if(!endpoint_ok(c->relay_endpoints[i]))return-4;if(c->relay_required&&!c->relay_count)return-5;return 0;}
static int atomic_replace(const char*s,const char*d){
#ifdef _WIN32
return MoveFileExA(s,d,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?0:-1;
#else
return rename(s,d);
#endif
}
int qrx_aura_provider_host_config_save(const char*path,const QrxAuraProviderHostConfig*c){
    if(!path||!path[0]||qrx_aura_provider_host_config_validate(c))return-1;
    char tmp[1400];if(snprintf(tmp,sizeof(tmp),"%s.tmp",path)>=(int)sizeof(tmp))return-1;
    FILE*f=fopen(tmp,"wb");if(!f)return-2;int ok=1;
#define W(k,fmt,v) do{if(fprintf(f,k "=" fmt "\n",v)<0)ok=0;}while(0)
    W("format","%s",QRX_AURA_PROVIDER_HOST_FORMAT);
    W("enabled","%u",(unsigned)c->enabled);
    W("provider_id","%s",c->provider_id);
    W("pod_id","%s",c->pod_id);
    W("network","%s",c->network);
    W("region","%s",c->region);
    W("compute_threads","%u",c->compute_threads);
    W("model_cache_bytes","%llu",(unsigned long long)c->model_cache_bytes);
    W("free_memory_bytes","%llu",(unsigned long long)c->free_memory_bytes);
    W("network_egress_mbps","%llu",(unsigned long long)c->network_egress_mbps);
    W("require_wallet_approval","%u",(unsigned)c->require_wallet_approval);
    W("require_secure_dispatch","%u",(unsigned)c->require_secure_dispatch);
    W("enable_pq_hybrid_sessions","%u",(unsigned)c->enable_pq_hybrid_sessions);
    W("listen_host","%s",c->listen_host);
    W("listen_port","%u",(unsigned)c->listen_port);
    W("lease_journal_path","%s",c->lease_journal_path);
    W("job_journal_path","%s",c->job_journal_path);
    W("runtime_adapter","%s",qrx_aura_runtime_adapter_name(c->runtime_adapter.kind));
    W("runtime_plugin_path","%s",c->runtime_adapter.explicit_path);
    W("runtime_adapter_dir","%s",c->runtime_adapter.adapter_dir);
    W("relay_required","%u",(unsigned)c->relay_required);
    for(uint32_t i=0;i<c->relay_count;i++)if(fprintf(f,"relay=%s\n",c->relay_endpoints[i])<0)ok=0;
#undef W
    if (fflush(f)) ok = 0;
    if (fclose(f)) ok = 0;
    if (!ok) { remove(tmp); return -3; }
    if (atomic_replace(tmp,path)) { remove(tmp); return -4; }
    return 0;
}
static void trim(char*s){if(!s)return;size_t n=strlen(s);while(n&&(s[n-1]=='\r'||s[n-1]=='\n'||isspace((unsigned char)s[n-1])))s[--n]=0;char*p=s;while(*p&&isspace((unsigned char)*p))p++;if(p!=s)memmove(s,p,strlen(p)+1);}
static int u64v(const char*s,uint64_t*out){if(!s||!s[0])return-1;errno=0;char*e=NULL;unsigned long long v=strtoull(s,&e,10);if(errno||!e||*e)return-1;*out=(uint64_t)v;return 0;}
static int u32v(const char*s,uint32_t*out){uint64_t v;if(u64v(s,&v)||v>0xffffffffu)return-1;*out=(uint32_t)v;return 0;}
static int cp(char*d,size_t n,const char*s){if(!d||!s||strlen(s)>=n)return-1;snprintf(d,n,"%s",s);return 0;}
int qrx_aura_provider_host_config_load(const char*path,QrxAuraProviderHostConfig*c){if(!path||!c)return-1;FILE*f=fopen(path,"rb");if(!f)return-2;qrx_aura_provider_host_config_defaults(c);char line[1600];int format=0;while(fgets(line,sizeof(line),f)){trim(line);if(!line[0]||line[0]=='#')continue;char*eq=strchr(line,'=');if(!eq){fclose(f);return-3;}*eq++=0;trim(line);trim(eq);uint32_t u32;if(!strcmp(line,"format")){if(strcmp(eq,QRX_AURA_PROVIDER_HOST_FORMAT)){fclose(f);return-3;}format=1;}else if(!strcmp(line,"enabled")){if(u32v(eq,&u32)||u32>1){fclose(f);return-3;}c->enabled=(uint8_t)u32;}else if(!strcmp(line,"provider_id")){if(cp(c->provider_id,sizeof(c->provider_id),eq)){fclose(f);return-3;}}else if(!strcmp(line,"pod_id")){if(cp(c->pod_id,sizeof(c->pod_id),eq)){fclose(f);return-3;}}else if(!strcmp(line,"network")){if(cp(c->network,sizeof(c->network),eq)){fclose(f);return-3;}}else if(!strcmp(line,"region")){if(cp(c->region,sizeof(c->region),eq)){fclose(f);return-3;}}else if(!strcmp(line,"compute_threads")){if(u32v(eq,&c->compute_threads)){fclose(f);return-3;}}else if(!strcmp(line,"model_cache_bytes")){if(u64v(eq,&c->model_cache_bytes)){fclose(f);return-3;}}else if(!strcmp(line,"free_memory_bytes")){if(u64v(eq,&c->free_memory_bytes)){fclose(f);return-3;}}else if(!strcmp(line,"network_egress_mbps")){if(u64v(eq,&c->network_egress_mbps)){fclose(f);return-3;}}else if(!strcmp(line,"require_wallet_approval")){if(u32v(eq,&u32)||u32>1){fclose(f);return-3;}c->require_wallet_approval=(uint8_t)u32;}else if(!strcmp(line,"require_secure_dispatch")){if(u32v(eq,&u32)||u32>1){fclose(f);return-3;}c->require_secure_dispatch=(uint8_t)u32;}else if(!strcmp(line,"enable_pq_hybrid_sessions")){if(u32v(eq,&u32)||u32>1){fclose(f);return-3;}c->enable_pq_hybrid_sessions=(uint8_t)u32;}else if(!strcmp(line,"listen_host")){if(cp(c->listen_host,sizeof(c->listen_host),eq)){fclose(f);return-3;}}else if(!strcmp(line,"listen_port")){if(u32v(eq,&u32)||u32>65535){fclose(f);return-3;}c->listen_port=(uint16_t)u32;}else if(!strcmp(line,"lease_journal_path")){if(cp(c->lease_journal_path,sizeof(c->lease_journal_path),eq)){fclose(f);return-3;}}else if(!strcmp(line,"job_journal_path")){if(cp(c->job_journal_path,sizeof(c->job_journal_path),eq)){fclose(f);return-3;}}else if(!strcmp(line,"runtime_adapter")){if(qrx_aura_runtime_adapter_parse(eq,&c->runtime_adapter.kind)){fclose(f);return-3;}}else if(!strcmp(line,"runtime_plugin_path")){if(cp(c->runtime_adapter.explicit_path,sizeof(c->runtime_adapter.explicit_path),eq)){fclose(f);return-3;}}else if(!strcmp(line,"runtime_adapter_dir")){if(cp(c->runtime_adapter.adapter_dir,sizeof(c->runtime_adapter.adapter_dir),eq)){fclose(f);return-3;}}else if(!strcmp(line,"relay_required")){if(u32v(eq,&u32)||u32>1){fclose(f);return-3;}c->relay_required=(uint8_t)u32;}else if(!strcmp(line,"relay")){if(c->relay_count>=QRX_AURA_PROVIDER_RELAY_MAX||cp(c->relay_endpoints[c->relay_count],sizeof(c->relay_endpoints[0]),eq)){fclose(f);return-3;}c->relay_count++;}}
fclose(f);if(!format)return-3;return qrx_aura_provider_host_config_validate(c);}
