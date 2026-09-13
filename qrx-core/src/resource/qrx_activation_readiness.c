#include "resource/qrx_activation_readiness.h"
#include "resource/qrx_resource.h"
#include "resource/qrx_resource_live.h"
#include "resource/qrx_storage_consensus.h"
#include "resource/qrx_storage_market.h"
#include "compute/qrx_compute_provider_identity.h"
#include "qrxdb.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void reason(QrxProtocolActivationReadiness *o,const char *s){if(o)snprintf(o->blocking_reason,sizeof(o->blocking_reason),"%s",s?s:"");}
static void score(QrxProtocolActivationReadiness *o){if(!o)return;o->readiness_bps=o->criteria_required?(uint32_t)(((uint64_t)o->criteria_passed*10000ULL)/o->criteria_required):0;if(o->readiness_bps>10000)o->readiness_bps=10000;}

typedef struct {
    QrxDB *db; uint64_t height; uint64_t mature; uint64_t youngest_age;
    char operators[256][129]; size_t operator_count;
} MatureCtx;
static int operator_seen_or_add(MatureCtx *c,const char *operator_id){
    if(!c||!operator_id||!*operator_id)return 1;
    for(size_t i=0;i<c->operator_count;i++)if(!strcmp(c->operators[i],operator_id))return 1;
    if(c->operator_count>=256)return 1;
    snprintf(c->operators[c->operator_count],sizeof(c->operators[c->operator_count]),"%s",operator_id);c->operator_count++;return 0;
}
static int mature_cb(const char *key,const char *value,uint32_t value_len,void *ctxp){
    (void)value;(void)value_len;const char *pre="storage/provider/";if(strncmp(key,pre,strlen(pre)))return 0;
    const char *id=key+strlen(pre);if(!*id||strchr(id,'/'))return 0;MatureCtx *c=(MatureCtx*)ctxp;QrxStorageProviderState p;
    if(qrx_storage_provider_get(c->db,id,&p)||p.status!=QRX_PROVIDER_ACTIVE||!p.proven_capacity_bytes||!p.attested_asn[0]||!p.attested_region[0])return 0;
    operator_seen_or_add(c,p.operator_id[0]?p.operator_id:id);
    char hk[320],hv[64];snprintf(hk,sizeof(hk),"storage/provider-activation-height/%s",id);if(qrxdb_get(c->db,hk,hv,sizeof(hv)))return 0;
    uint64_t ah=strtoull(hv,NULL,10),age=c->height>=ah?c->height-ah:0;if(age>=QRX_DRIVE_READINESS_SOAK_BLOCKS)c->mature++;
    if(c->youngest_age==UINT64_MAX||age<c->youngest_age)c->youngest_age=age;
    return 0;
}
static int count_cb(const char *key,const char *value,uint32_t value_len,void *ctx){(void)key;(void)value;(void)value_len;(*(uint64_t*)ctx)++;return 0;}

static uint32_t compute_safety_flags(QrxDB *db){
    static const char*names[]={"verification_stable","rewards_simulated","fasttrack_fair","standard_tasks_protected","fasttrack_dev_share_transparent","model_integrity_verified","provider_fraud_controlled","resource_globe_privacy_checked","server_independence_verified"};
    uint32_t n=0;char k[180],v[32];for(size_t i=0;i<sizeof(names)/sizeof(names[0]);i++){snprintf(k,sizeof(k),"consensus:compute:readiness:%s",names[i]);if(qrxdb_get(db,k,v,sizeof(v))==0&&!strcmp(v,"1"))n++;}return n;
}

const char *qrx_protocol_readiness_status_name(QrxProtocolReadinessStatus s){switch(s){case QRX_PROTOCOL_READINESS_LOCKED:return "LOCKED";case QRX_PROTOCOL_READINESS_NOT_READY:return "NOT_READY";case QRX_PROTOCOL_READINESS_SOAKING:return "SOAKING";case QRX_PROTOCOL_READINESS_WAITING_TARGET_DATE:return "WAITING_TARGET_DATE";case QRX_PROTOCOL_READINESS_READY_FOR_GOVERNANCE:return "READY_FOR_GOVERNANCE";case QRX_PROTOCOL_READINESS_SCHEDULED:return "SCHEDULED";case QRX_PROTOCOL_READINESS_ACTIVE:return "ACTIVE";default:return "UNKNOWN";}}

static int live_common(const char *chain_dir,QrxProtocolActivationReadiness *o){
    QrxResourceDashboardSnapshot d;QrxStorageAtlasCell *cells=NULL;size_t cn=0;if(qrx_resource_live_snapshot(chain_dir,3,&d,&cells,&cn))return -1;qrx_storage_atlas_free(cells);
    o->serving_providers=d.serving_providers;o->independent_asns=d.independent_asns;o->visible_regions=d.visible_regions;o->proven_bytes=d.storage.proven_bytes;o->active_contracts=d.active_contracts;o->active_domains=d.active_domains;o->avg_availability_bps=d.avg_availability_bps;o->avg_proof_success_bps=d.avg_proof_success_bps;o->health_score=d.health_score;return 0;
}
static void set_schedule(QrxProtocolActivationReadiness *o,const char *chain,const char *feature){
    if(!strcmp(feature,QRX_DRIVE_V1_FEATURE_FLAG)){o->target_time=qrx_resource_target_time(chain);o->activation_height=qrx_resource_activation_height(chain);}
    else if(!strcmp(feature,QRX_NET_V1_FEATURE_FLAG)){o->target_time=qrx_net_target_time(chain);o->activation_height=qrx_net_activation_height(chain);}
    else if(!strcmp(feature,QRX_ADVERTISING_V1_FEATURE_FLAG)){o->target_time=qrx_advertising_target_time(chain);o->activation_height=qrx_advertising_activation_height(chain);}
    else if(!strcmp(feature,QRX_COMPUTE_POUC_V1_FEATURE_FLAG)){o->target_time=qrx_compute_pouc_target_time(chain);o->activation_height=qrx_compute_pouc_activation_height(chain);}
}
static int active_at(const char *chain,const char *feature,uint64_t h){if(!strcmp(feature,QRX_DRIVE_V1_FEATURE_FLAG))return qrx_storage_protocol_enabled_at_height(chain,(long long)h);if(!strcmp(feature,QRX_NET_V1_FEATURE_FLAG))return qrx_net_protocol_enabled_at_height(chain,(long long)h);if(!strcmp(feature,QRX_ADVERTISING_V1_FEATURE_FLAG))return qrx_advertising_protocol_enabled_at_height(chain,(long long)h);if(!strcmp(feature,QRX_COMPUTE_POUC_V1_FEATURE_FLAG))return qrx_compute_pouc_protocol_enabled_at_height(chain,(long long)h);return 0;}

int qrx_protocol_activation_readiness(const char *chain_dir,const char *feature,uint64_t h,int64_t now,QrxProtocolActivationReadiness *o){
    if(!chain_dir||!feature||!o)return -1;
    if(strcmp(feature,QRX_DRIVE_V1_FEATURE_FLAG)&&strcmp(feature,QRX_NET_V1_FEATURE_FLAG)&&strcmp(feature,QRX_ADVERTISING_V1_FEATURE_FLAG)&&strcmp(feature,QRX_COMPUTE_POUC_V1_FEATURE_FLAG))return -2;
    memset(o,0,sizeof(*o));snprintf(o->feature_flag,sizeof(o->feature_flag),"%s",feature);o->current_height=h;o->activation_height=-1;set_schedule(o,chain_dir,feature);
    if(active_at(chain_dir,feature,h)){o->status=QRX_PROTOCOL_READINESS_ACTIVE;o->readiness_bps=10000;reason(o,"feature is active at current height");return 0;}
    if(o->activation_height>=0){o->status=QRX_PROTOCOL_READINESS_SCHEDULED;o->readiness_bps=10000;reason(o,"threshold-signed on-chain activation is scheduled");return 0;}
    if(live_common(chain_dir,o))return -3;

    if(!strcmp(feature,QRX_DRIVE_V1_FEATURE_FLAG)){
        o->criteria_required=8;o->required_soak_blocks=QRX_DRIVE_READINESS_SOAK_BLOCKS;
        QrxDB db;if(qrxdb_init(&db,chain_dir))return -4;MatureCtx mc={0};mc.db=&db;mc.height=h;mc.youngest_age=UINT64_MAX;qrxdb_scan_prefix(&db,"storage/provider/",mature_cb,&mc);qrxdb_close(&db);o->mature_attested_providers=mc.mature;o->independent_operators=mc.operator_count;o->soak_blocks=mc.youngest_age==UINT64_MAX?0:mc.youngest_age;
        o->criteria_passed+=(o->serving_providers>=QRX_DRIVE_READINESS_MIN_PROVIDERS);o->criteria_passed+=(o->independent_operators>=QRX_DRIVE_READINESS_MIN_OPERATORS);o->criteria_passed+=(o->independent_asns>=QRX_DRIVE_READINESS_MIN_ASNS);o->criteria_passed+=(o->visible_regions>=QRX_DRIVE_READINESS_MIN_REGIONS);o->criteria_passed+=(o->proven_bytes>=QRX_DRIVE_READINESS_MIN_PROVEN_BYTES);o->criteria_passed+=(o->avg_availability_bps>=QRX_DRIVE_READINESS_MIN_AVAILABILITY_BPS);o->criteria_passed+=(o->avg_proof_success_bps>=QRX_DRIVE_READINESS_MIN_PROOF_BPS);o->criteria_passed+=(o->health_score>=QRX_DRIVE_READINESS_MIN_HEALTH_SCORE);score(o);
        if(o->criteria_passed<o->criteria_required){o->status=QRX_PROTOCOL_READINESS_NOT_READY;reason(o,"storage capacity/diversity/health criteria are not yet satisfied");return 0;}if(o->mature_attested_providers<QRX_DRIVE_READINESS_MIN_PROVIDERS){o->status=QRX_PROTOCOL_READINESS_SOAKING;reason(o,"storage providers have not completed the 7-day maturity soak");return 0;}
    }else if(!strcmp(feature,QRX_NET_V1_FEATURE_FLAG)){
        o->dependencies_required=1;o->dependencies_active=qrx_storage_protocol_enabled_at_height(chain_dir,(long long)h)?1:0;o->required_soak_blocks=QRX_NET_READINESS_SOAK_BLOCKS;long long dep=qrx_resource_activation_height(chain_dir);if(!o->dependencies_active||dep<0){o->status=QRX_PROTOCOL_READINESS_LOCKED;reason(o,"DRIVE_V1 must be active first");return 0;}o->dependency_soak_blocks=o->soak_blocks=h>=(uint64_t)dep?h-(uint64_t)dep:0;
        o->criteria_required=6;o->criteria_passed+=(o->serving_providers>=QRX_NET_READINESS_MIN_PROVIDERS);o->criteria_passed+=(o->independent_asns>=QRX_NET_READINESS_MIN_ASNS);o->criteria_passed+=(o->visible_regions>=QRX_NET_READINESS_MIN_REGIONS);o->criteria_passed+=(o->active_contracts>=QRX_NET_READINESS_MIN_ACTIVE_CONTRACTS);o->criteria_passed+=(o->avg_availability_bps>=QRX_NET_READINESS_MIN_AVAILABILITY_BPS);o->criteria_passed+=(o->health_score>=QRX_NET_READINESS_MIN_HEALTH_SCORE);score(o);if(o->criteria_passed<o->criteria_required){o->status=QRX_PROTOCOL_READINESS_NOT_READY;reason(o,"Drive is active but QRX-Net hosting prerequisites are not yet stable");return 0;}if(o->soak_blocks<o->required_soak_blocks){o->status=QRX_PROTOCOL_READINESS_SOAKING;reason(o,"DRIVE_V1 has not completed the QRX-Net dependency soak");return 0;}
    }else if(!strcmp(feature,QRX_ADVERTISING_V1_FEATURE_FLAG)){
        o->dependencies_required=1;o->dependencies_active=qrx_net_protocol_enabled_at_height(chain_dir,(long long)h)?1:0;o->required_soak_blocks=QRX_AD_READINESS_SOAK_BLOCKS;long long dep=qrx_net_activation_height(chain_dir);if(!o->dependencies_active||dep<0){o->status=QRX_PROTOCOL_READINESS_LOCKED;reason(o,"QRX_NET_V1 must be active first");return 0;}o->dependency_soak_blocks=o->soak_blocks=h>=(uint64_t)dep?h-(uint64_t)dep:0;
        o->criteria_required=6;o->criteria_passed+=(o->serving_providers>=QRX_AD_READINESS_MIN_PROVIDERS);o->criteria_passed+=(o->independent_asns>=QRX_AD_READINESS_MIN_ASNS);o->criteria_passed+=(o->visible_regions>=QRX_AD_READINESS_MIN_REGIONS);o->criteria_passed+=(o->active_domains>=QRX_AD_READINESS_MIN_ACTIVE_DOMAINS);o->criteria_passed+=(o->avg_availability_bps>=QRX_AD_READINESS_MIN_AVAILABILITY_BPS);o->criteria_passed+=(o->health_score>=QRX_AD_READINESS_MIN_HEALTH_SCORE);score(o);if(o->criteria_passed<o->criteria_required){o->status=QRX_PROTOCOL_READINESS_NOT_READY;reason(o,"QRX-Net is active but advertising delivery prerequisites are not yet mature");return 0;}if(o->soak_blocks<o->required_soak_blocks){o->status=QRX_PROTOCOL_READINESS_SOAKING;reason(o,"QRX_NET_V1 has not completed the advertising dependency soak");return 0;}
    }else{
        o->dependencies_required=2;o->dependencies_active=(qrx_storage_protocol_enabled_at_height(chain_dir,(long long)h)?1:0)+(qrx_net_protocol_enabled_at_height(chain_dir,(long long)h)?1:0);o->required_soak_blocks=QRX_COMPUTE_READINESS_SOAK_BLOCKS;long long d1=qrx_resource_activation_height(chain_dir),d2=qrx_net_activation_height(chain_dir),dep=d1>d2?d1:d2;if(o->dependencies_active<2||dep<0){o->status=QRX_PROTOCOL_READINESS_LOCKED;reason(o,"DRIVE_V1 and QRX_NET_V1 must both be active first");return 0;}o->dependency_soak_blocks=o->soak_blocks=h>=(uint64_t)dep?h-(uint64_t)dep:0;
        QrxDB db;if(qrxdb_init(&db,chain_dir))return -4;qrxdb_scan_prefix(&db,QRX_COMPUTE_PROVIDER_IDENTITY_PREFIX,count_cb,&o->compute_providers);o->compute_safety_flags=compute_safety_flags(&db);qrxdb_close(&db);
        o->criteria_required=6;o->criteria_passed+=(o->compute_providers>=QRX_COMPUTE_READINESS_MIN_PROVIDERS);o->criteria_passed+=(o->compute_safety_flags>=QRX_COMPUTE_READINESS_REQUIRED_SAFETY_FLAGS);o->criteria_passed+=(o->serving_providers>=QRX_COMPUTE_READINESS_MIN_STORAGE_PROVIDERS);o->criteria_passed+=(o->independent_asns>=QRX_COMPUTE_READINESS_MIN_ASNS);o->criteria_passed+=(o->visible_regions>=QRX_COMPUTE_READINESS_MIN_REGIONS);o->criteria_passed+=(o->health_score>=QRX_COMPUTE_READINESS_MIN_HEALTH_SCORE);score(o);if(o->criteria_passed<o->criteria_required){o->status=QRX_PROTOCOL_READINESS_NOT_READY;reason(o,"compute provider or PoUC safety-readiness evidence is incomplete");return 0;}if(o->soak_blocks<o->required_soak_blocks){o->status=QRX_PROTOCOL_READINESS_SOAKING;reason(o,"compute dependencies have not completed the 14-day shadow/soak period");return 0;}
    }
    if(now<o->target_time){o->status=QRX_PROTOCOL_READINESS_WAITING_TARGET_DATE;reason(o,"technical readiness passes, but the policy not-before date has not been reached");return 0;}o->status=QRX_PROTOCOL_READINESS_READY_FOR_GOVERNANCE;o->readiness_bps=10000;reason(o,"technical criteria, dependency soak and target-date policy pass; governance may schedule activation");return 0;
}

const char *qrx_drive_readiness_status_name(QrxDriveReadinessStatus s){switch(s){case QRX_DRIVE_READINESS_WAITING_SOAK:return "WAITING_SOAK";case QRX_DRIVE_READINESS_WAITING_TARGET_DATE:return "WAITING_TARGET_DATE";case QRX_DRIVE_READINESS_READY:return "READY";case QRX_DRIVE_READINESS_ALREADY_SCHEDULED:return "SCHEDULED";case QRX_DRIVE_READINESS_ACTIVE:return "ACTIVE";default:return "NOT_READY";}}
int qrx_drive_activation_readiness(const char *chain_dir,uint64_t h,int64_t now,QrxDriveActivationReadiness *o){if(!o)return -1;QrxProtocolActivationReadiness g;if(qrx_protocol_activation_readiness(chain_dir,QRX_DRIVE_V1_FEATURE_FLAG,h,now,&g))return -1;memset(o,0,sizeof(*o));o->current_height=g.current_height;o->target_time=g.target_time;o->activation_height=g.activation_height;o->serving_providers=g.serving_providers;o->mature_attested_providers=g.mature_attested_providers;o->independent_operators=g.independent_operators;o->independent_asns=g.independent_asns;o->visible_regions=g.visible_regions;o->proven_bytes=g.proven_bytes;o->avg_availability_bps=g.avg_availability_bps;o->avg_proof_success_bps=g.avg_proof_success_bps;o->health_score=g.health_score;o->soak_blocks=g.soak_blocks;o->soak_blocks_required=g.required_soak_blocks;o->criteria_passed=g.criteria_passed;o->criteria_required=g.criteria_required;switch(g.status){case QRX_PROTOCOL_READINESS_SOAKING:o->status=QRX_DRIVE_READINESS_WAITING_SOAK;break;case QRX_PROTOCOL_READINESS_WAITING_TARGET_DATE:o->status=QRX_DRIVE_READINESS_WAITING_TARGET_DATE;break;case QRX_PROTOCOL_READINESS_READY_FOR_GOVERNANCE:o->status=QRX_DRIVE_READINESS_READY;break;case QRX_PROTOCOL_READINESS_SCHEDULED:o->status=QRX_DRIVE_READINESS_ALREADY_SCHEDULED;break;case QRX_PROTOCOL_READINESS_ACTIVE:o->status=QRX_DRIVE_READINESS_ACTIVE;break;default:o->status=QRX_DRIVE_READINESS_NOT_READY;break;}return 0;}
