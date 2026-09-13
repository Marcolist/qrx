#include "storage/qrx_storage_activation.h"
#include "resource/qrx_storage_consensus.h"
#include "resource/qrx_storage_repair.h"
#include "storage/qrx_drive_prepare.h"
#include "storage/qrx_merkle.h"
#include <openssl/crypto.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int qrx_storage_assignment_local_ready(QrxDB *db,QrxStorageFs *fs,const char *provider,const char *cid,uint32_t shard){
    if(!db||!fs||!provider||!*provider||!cid||!*cid)return -1;
    QrxStorageAssignmentRecord a; if(qrx_storage_assignment_get(db,cid,shard,&a))return -1;
    if(a.state!=QRX_ASSIGN_PENDING||strcmp(a.provider_id,provider)||!a.leaf_count||!qrx_storage_fs_has(fs,a.object_id))return -1;
    uint64_t expect=(a.physical_bytes+QRX_DRIVE_POSTOR_LEAF_BYTES-1)/QRX_DRIVE_POSTOR_LEAF_BYTES;
    if(!expect||expect!=a.leaf_count||a.leaf_count>SIZE_MAX/64)return -1;
    uint8_t *leaves=malloc((size_t)a.leaf_count*64); if(!leaves)return -1;
    int rc=-1; uint64_t off=0;
    for(uint64_t i=0;i<a.leaf_count;i++){
        size_t want=(size_t)((a.physical_bytes-off)>QRX_DRIVE_POSTOR_LEAF_BYTES?QRX_DRIVE_POSTOR_LEAF_BYTES:(a.physical_bytes-off));
        unsigned char *buf=NULL; size_t got=0;
        if(!want||qrx_storage_fs_read_range(fs,a.object_id,off,want,&buf,&got)||got!=want||qrx_merkle_leaf_hash(i,buf,got,leaves+(size_t)i*64)){free(buf);goto done;}
        free(buf);off+=got;
    }
    if(off!=a.physical_bytes)goto done;
    uint8_t root[64]; if(qrx_merkle_root_from_leaves(leaves,(size_t)a.leaf_count,root))goto done;
    rc=CRYPTO_memcmp(root,a.merkle_root,64)?-1:0;
done: OPENSSL_cleanse(leaves,(size_t)a.leaf_count*64);free(leaves);return rc;
}

typedef struct {QrxDB*db;QrxStorageFs*fs;const char*provider;QrxStorageReadyAssignment*out;size_t cap,n;} Ctx;
static int scan_cb(const char *key,const char *value,uint32_t value_len,void *v){(void)value;(void)value_len;Ctx*c=v;if(c->n>=c->cap)return 1;const char*p="storage/assignment/";size_t pn=strlen(p);if(strncmp(key,p,pn))return 0;const char*s=key+pn,*slash=strrchr(s,'/');if(!slash||slash==s)return 0;size_t cn=(size_t)(slash-s);if(cn>=129)return 0;char cid[129];memcpy(cid,s,cn);cid[cn]=0;char*e=NULL;unsigned long sh=strtoul(slash+1,&e,10);if(!e||*e||sh>UINT32_MAX)return 0;QrxStorageAssignmentRecord a;if(qrx_storage_assignment_get(c->db,cid,(uint32_t)sh,&a)||a.state!=QRX_ASSIGN_PENDING||strcmp(a.provider_id,c->provider))return 0;if(qrx_storage_assignment_local_ready(c->db,c->fs,c->provider,cid,(uint32_t)sh))return 0;QrxStorageReadyAssignment*r=&c->out[c->n++];memset(r,0,sizeof(*r));snprintf(r->contract_id,sizeof(r->contract_id),"%s",cid);r->shard_index=(uint32_t)sh;snprintf(r->object_id,sizeof(r->object_id),"%s",a.object_id);return 0;}
int qrx_storage_collect_ready_assignments(QrxDB *db,QrxStorageFs *fs,const char *provider,QrxStorageReadyAssignment*out,size_t cap,size_t*count_out){if(!db||!fs||!provider||!*provider||!out||!cap||!count_out)return-1;Ctx c={db,fs,provider,out,cap,0};int rc=qrxdb_scan_prefix(db,"storage/assignment/",scan_cb,&c);*count_out=c.n;return rc<0?-1:0;}
