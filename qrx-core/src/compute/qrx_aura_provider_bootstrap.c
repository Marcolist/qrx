#include "compute/qrx_aura_provider_bootstrap.h"
#include <stdio.h>
#include <string.h>

static int nz(const char *s,size_t cap){ return s&&memchr(s,'\0',cap)&&s[0]; }

static int bootstrap_plan(const QrxAuraProviderBootstrapConfig *c,QrxResourceProviderPlan *p){
    if(!c||!p||!c->compute_threads||!c->model_cache_bytes) return -1;
    memset(p,0,sizeof(*p));
    p->version=QRX_RESOURCE_PROVIDER_VERSION;
    snprintf(p->region,sizeof(p->region),"%s",c->region);
    p->enabled_mask=QRX_PROVIDER_ENABLE_COMPUTE|QRX_PROVIDER_ENABLE_AI|QRX_PROVIDER_ENABLE_MODEL_CACHE|QRX_PROVIDER_ENABLE_NETWORK;
    p->model_cache_bytes=c->model_cache_bytes;p->compute_threads=c->compute_threads;
    p->network_egress_mbps=c->network_egress_mbps?c->network_egress_mbps:c->calibration.transport_bandwidth_mbps;
    if(!p->network_egress_mbps) p->network_egress_mbps=1;
    p->accelerator_enabled=c->calibration.device.backend!=QRX_MOE_BACKEND_CPU;
    p->cuda_enabled=c->calibration.device.backend==QRX_MOE_BACKEND_CUDA;
    p->metal_mlx_enabled=c->calibration.device.backend==QRX_MOE_BACKEND_MLX_METAL;
    p->requires_wallet_approval=c->require_wallet_approval?1:0;
    p->primary_action=QRX_OPP_ACTION_AI_ACCELERATOR;
    p->expected_utilization_bps=5000; /* bootstrap scheduling target, not measured telemetry */
    char commitment[65];
    return qrx_resource_provider_plan_commitment(p,commitment);
}

static void bootstrap_cleanup(QrxAuraProviderBootstrap *b){
    if(!b) return;
    if(b->service){qrx_aura_provider_service_stop(b->service);qrx_aura_provider_service_free(b->service);b->service=NULL;}
    if(b->plugin_open){qrx_aura_runtime_plugin_close(&b->plugin);b->plugin_open=0;}
    if(b->runtime.version==QRX_PROVIDER_RUNTIME_VERSION&&b->runtime.status==QRX_PROVIDER_RUNTIME_SERVING){
        qrx_resource_provider_runtime_begin_drain(&b->runtime);
        if(!b->runtime.active_jobs) qrx_resource_provider_runtime_shutdown(&b->runtime);
    }
    memset(&b->binding,0,sizeof(b->binding));
}

int qrx_aura_provider_bootstrap_start(const QrxAuraProviderBootstrapConfig *c,QrxAuraProviderBootstrap *out){
    if(!c||!out||c->version!=QRX_AURA_PROVIDER_BOOTSTRAP_VERSION||!c->user_enabled||
       !nz(c->provider_id,sizeof(c->provider_id))||!nz(c->pod_id,sizeof(c->pod_id))||
       !nz(c->network,sizeof(c->network))||!nz(c->region,sizeof(c->region))||
       qrx_moe_calibration_profile_validate(&c->calibration)||!c->provider_private_key||
       !c->requester_key_lookup||!c->height_fn||!c->model_registry||!c->lease_journal_path[0]) return -1;
    memset(out,0,sizeof(*out));
    QrxResourceProviderPlan plan;
    if(bootstrap_plan(c,&plan)) return -2;
    if(qrx_resource_provider_runtime_init(&out->runtime,c->provider_id,c->network,&plan)||
       qrx_resource_provider_runtime_mark_ready(&out->runtime)) return -3;
    if(qrx_resource_provider_market_register(&out->runtime,c->require_wallet_approval?c->wallet_approval:NULL,
                                              c->height_fn(c->height_ctx),&out->registration)||
       qrx_resource_provider_runtime_start_serving(&out->runtime)){
        bootstrap_cleanup(out);return -4;
    }
    out->binding.version=QRX_AURA_LIVE_DISPATCH_VERSION;
    snprintf(out->binding.provider_id,sizeof(out->binding.provider_id),"%s",c->provider_id);
    snprintf(out->binding.pod_id,sizeof(out->binding.pod_id),"%s",c->pod_id);
    out->binding.provider_runtime=&out->runtime;out->binding.device=c->calibration.device;out->binding.local=1;
    qrx_aura_model_cache_catalog_init(&out->cache_catalog);
    if(c->runtime_plugin_path&&c->runtime_plugin_path[0]){
        if(qrx_aura_runtime_plugin_open(c->runtime_plugin_path,&out->binding.device,c->model_cache_fs,&out->plugin)){
            bootstrap_cleanup(out);return -5;
        }
        out->plugin_open=1;
    }else{
        if(qrx_moe_worker_adapter_validate(&c->fallback_adapter)){bootstrap_cleanup(out);return -5;}
        out->binding.adapter=c->fallback_adapter;
    }
    uint64_t free_mem=c->free_memory_bytes?c->free_memory_bytes:c->calibration.device.device_memory_bytes;
    if(free_mem>c->calibration.device.device_memory_bytes) free_mem=c->calibration.device.device_memory_bytes;
    uint32_t roles=QRX_AURA_POD_ROLE_INFERENCE|QRX_AURA_POD_ROLE_ROUTER|QRX_AURA_POD_ROLE_PREPOST|
                   QRX_AURA_POD_ROLE_VERIFY|QRX_AURA_POD_ROLE_MODEL_CACHE;
    uint32_t latency_ms=c->calibration.transport_latency_us/1000u;
    if(qrx_aura_pod_from_calibration(c->provider_id,c->pod_id,c->region,&c->calibration,roles,free_mem,
                                     c->model_cache_bytes,plan.network_egress_mbps,latency_ms,0,
                                     9900,7000,7000,32768,1,&out->advertised_capacity)){
        bootstrap_cleanup(out);return -6;
    }
    QrxAuraProviderServiceConfig sc;memset(&sc,0,sizeof(sc));
    sc.version=QRX_AURA_PROVIDER_SERVICE_VERSION;sc.binding=&out->binding;sc.model_cache=&out->cache_catalog;
    sc.model_registry=c->model_registry;sc.requester_key_lookup=c->requester_key_lookup;sc.requester_key_ctx=c->requester_key_ctx;
    sc.provider_private_key=c->provider_private_key;sc.height_fn=c->height_fn;sc.height_ctx=c->height_ctx;
    sc.model_fetch=c->model_fetch;sc.model_fetch_ctx=c->model_fetch_ctx;
    if(out->plugin_open){sc.model_execute=qrx_aura_runtime_plugin_model_execute_adapter;sc.model_execute_ctx=&out->plugin;}
    snprintf(sc.listen_host,sizeof(sc.listen_host),"%s",c->listen_host[0]?c->listen_host:"127.0.0.1");
    sc.listen_port=c->listen_port;snprintf(sc.lease_journal_path,sizeof(sc.lease_journal_path),"%s",c->lease_journal_path);
    sc.require_secure_dispatch=c->require_secure_dispatch;sc.enable_pq_hybrid_sessions=c->enable_pq_hybrid_sessions;
    sc.pq_session_ttl_blocks=c->pq_session_ttl_blocks;sc.auto_advertise=c->advertise?1:0;
    sc.advertised_capacity=out->advertised_capacity;sc.advertisement_ttl_blocks=120;
    sc.advertise=c->advertise;sc.advertise_ctx=c->advertise_ctx;
    if(qrx_aura_provider_service_start(&sc,&out->service)){bootstrap_cleanup(out);return -7;}
    return 0;
}

int qrx_aura_provider_bootstrap_stop(QrxAuraProviderBootstrap *b){
    if(!b) return -1;
    int rc=0;
    if(b->service){rc=qrx_aura_provider_service_stop(b->service);qrx_aura_provider_service_free(b->service);b->service=NULL;}
    if(b->plugin_open){qrx_aura_runtime_plugin_close(&b->plugin);b->plugin_open=0;}
    if(b->runtime.version==QRX_PROVIDER_RUNTIME_VERSION&&b->runtime.status==QRX_PROVIDER_RUNTIME_SERVING){
        if(qrx_resource_provider_runtime_begin_drain(&b->runtime)) rc=-2;
    }
    if(b->runtime.version==QRX_PROVIDER_RUNTIME_VERSION&&b->runtime.status==QRX_PROVIDER_RUNTIME_DRAINING&&!b->runtime.active_jobs){
        if(qrx_resource_provider_runtime_shutdown(&b->runtime)) rc=-3;
    }
    return rc;
}
const char *qrx_aura_provider_bootstrap_endpoint(const QrxAuraProviderBootstrap *b){return b&&b->service?qrx_aura_provider_service_endpoint(b->service):NULL;}
const QrxResourceProviderRuntime *qrx_aura_provider_bootstrap_runtime(const QrxAuraProviderBootstrap *b){return b?&b->runtime:NULL;}
const QrxAuraPodCapacity *qrx_aura_provider_bootstrap_capacity(const QrxAuraProviderBootstrap *b){return b?&b->advertised_capacity:NULL;}
