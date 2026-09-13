#include "storage/qrx_storage_p2p.h"
#include "resource/qrx_storage_consensus.h"
#include "resource/qrx_storage_repair.h"
#include <string.h>
int qrx_storage_p2p_read_authorized(QrxDB *db,QrxStorageFs *fs,const char *provider,
                                    const char *cid,uint32_t shard,const char *oid,
                                    uint64_t offset,size_t length,unsigned char **out,size_t *out_len){
    if(!db||!fs||!provider||!*provider||!cid||!*cid||!oid||strlen(oid)!=64||!out||!out_len||length>QRX_STORAGE_P2P_MAX_RANGE)return -1;
    QrxStorageAssignmentRecord a;
    if(qrx_storage_assignment_get(db,cid,shard,&a)!=0)return -1;
    if(a.state!=QRX_ASSIGN_ACTIVE||strcmp(a.provider_id,provider)||strcmp(a.object_id,oid))return -1;
    if(!qrx_storage_fs_has(fs,oid))return -1;
    return qrx_storage_fs_read_range(fs,oid,offset,length,out,out_len);
}
int qrx_storage_p2p_write_authorized_file(QrxDB *db,QrxStorageFs *fs,const char *provider,const char *cid,uint32_t shard,const char *oid,const char *source_path){
    if(!db||!fs||!provider||!*provider||!cid||!*cid||!oid||strlen(oid)!=64||!source_path)return -1;
    QrxStorageAssignmentRecord a;if(qrx_storage_assignment_get(db,cid,shard,&a)!=0)return -1;
    int authorized=(a.state==QRX_ASSIGN_PENDING||a.state==QRX_ASSIGN_ACTIVE)&&!strcmp(a.provider_id,provider)&&!strcmp(a.object_id,oid);
    if(!authorized&&a.state==QRX_ASSIGN_REPAIRING){QrxStorageRepairRecord rr;if(!qrx_storage_repair_get(db,cid,shard,&rr)&&rr.state==QRX_ASSIGN_REPAIRING&&!strcmp(rr.replacement_provider_id,provider)&&!strcmp(rr.object_id,oid))authorized=1;}
    if(!authorized)return -1;
    char got[65];if(qrx_storage_fs_put_file(fs,source_path,got)!=0)return -1;return strcmp(got,oid)?-1:0;
}
