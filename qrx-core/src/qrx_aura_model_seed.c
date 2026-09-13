#include "compute/qrx_aura_model_origin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0){
    fprintf(stderr,
        "Usage: %s --catalog FILE --model MODEL_ID [--cas DIR] [--work DIR] [--resolve-only]\n"
        "          [--hf-token-env NAME] [--allow-gated]\n\n"
        "Imports one governed upstream model revision into the local QRX content-addressed seed cache.\n"
        "The upstream revision is resolved to an immutable SHA and license/gating policy is checked.\n"
        "Large models can require hundreds of GB or more; use --resolve-only to inspect metadata first.\n",
        argv0);
}

static void progress(void *ctx,const char *phase,const char *path,uint64_t done,uint64_t total){
    (void)ctx;
    fprintf(stderr,"[aura-seed] %s %s %llu/%llu\n",phase?phase:"",path?path:"",
            (unsigned long long)done,(unsigned long long)total);
}

int main(int argc,char **argv){
    const char *catalog=NULL,*model_id=NULL,*cas_dir="./qrx-aura-model-cas",*work_dir=NULL,*token_env=NULL;
    int resolve_only=0,allow_gated=0;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--catalog")&&i+1<argc)catalog=argv[++i];
        else if(!strcmp(argv[i],"--model")&&i+1<argc)model_id=argv[++i];
        else if(!strcmp(argv[i],"--cas")&&i+1<argc)cas_dir=argv[++i];
        else if(!strcmp(argv[i],"--work")&&i+1<argc)work_dir=argv[++i];
        else if(!strcmp(argv[i],"--hf-token-env")&&i+1<argc)token_env=argv[++i];
        else if(!strcmp(argv[i],"--resolve-only"))resolve_only=1;
        else if(!strcmp(argv[i],"--allow-gated"))allow_gated=1;
        else {usage(argv[0]);return 2;}
    }
    if(!catalog||!model_id){usage(argv[0]);return 2;}

    QrxAuraOriginModelSpec specs[64];size_t count=0;
    if(qrx_aura_origin_catalog_load(catalog,specs,sizeof(specs)/sizeof(specs[0]),&count)){
        fprintf(stderr,"Could not load origin catalog: %s\n",catalog);return 3;
    }
    const QrxAuraOriginModelSpec *spec=NULL;
    for(size_t i=0;i<count;i++)if(!strcmp(specs[i].model_id,model_id)){spec=&specs[i];break;}
    if(!spec){fprintf(stderr,"Model not found in catalog: %s\n",model_id);return 4;}

    QrxAuraOriginConfig cfg;
    if(qrx_aura_origin_config_defaults(&cfg))return 5;
    cfg.allow_gated=(uint8_t)(allow_gated?1:0);
    if(work_dir)snprintf(cfg.work_dir,sizeof(cfg.work_dir),"%s",work_dir);
    if(token_env){const char *v=getenv(token_env);if(v&&v[0])snprintf(cfg.hf_token,sizeof(cfg.hf_token),"%s",v);}

    QrxAuraOriginRepoMetadata meta;
    int rc=qrx_aura_hf_repo_resolve(&cfg,spec,&meta);
    if(rc){fprintf(stderr,"Origin resolve rejected (rc=%d). Check network, license/gating policy and revision.\n",rc);return 6;}
    printf("model_id=%s\nrepo=%s\nrevision=%s\nlicense=%s\ngated=%u\nselected_files=%u\n",
           spec->model_id,meta.repo_id,meta.resolved_revision,meta.license_id,(unsigned)meta.gated,meta.file_count);
    if(resolve_only)return 0;

    QrxStorageFs *fs=NULL;
    if(qrx_storage_fs_open(cas_dir,UINT64_MAX,0,&fs)){fprintf(stderr,"Could not open QRX CAS: %s\n",cas_dir);return 7;}
    QrxAuraOriginSeedResult *seed=calloc(1,sizeof(*seed));
    if(!seed){qrx_storage_fs_close(fs);return 8;}
    rc=qrx_aura_hf_seed_to_qrx_drive(&cfg,spec,fs,progress,NULL,seed);
    if(rc){fprintf(stderr,"Model seed failed (rc=%d).\n",rc);free(seed);qrx_storage_fs_close(fs);return 9;}
    printf("manifest_root=%s\nmodel_bundle_root=%s\nsource_map_root=%s\nassets=%u\ntotal_bytes=%llu\nmoe=%u\n",
           seed->distribution_manifest_root,seed->drive_bundle_root,seed->source_map_root,
           seed->distribution_manifest.asset_count,(unsigned long long)seed->distribution_manifest.total_bytes,
           (unsigned)seed->registry_record.is_moe);
    if(seed->registry_record.is_moe)printf("expert_manifest_root=%s\nexperts=%u\nexperts_per_token=%u\n",
           seed->registry_record.expert_manifest_root,seed->expert_manifest.expert_count,seed->expert_manifest.experts_per_token);
    free(seed);qrx_storage_fs_close(fs);return 0;
}
