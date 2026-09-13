#pragma once
#include "net/qrx_net_name.h"
#include "qrxdb.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { QRX_RECORD_KEEP=0, QRX_RECORD_SET=1, QRX_RECORD_CLEAR=2 } QrxRecordUpdateMode;
typedef struct {
    QrxRecordUpdateMode qub_mode;
    const char *qub_address;
    QrxRecordUpdateMode web_mode;
    const uint8_t *web_manifest_root;
    QrxRecordUpdateMode publishing_mode;
    const uint8_t *publishing_key_commitment;
} QrxDomainUpdate;

int qrx_net_registry_get(QrxDB *db,const char *name,QrxDomainRecord *out);
typedef int (*QrxDomainRecordCallback)(const QrxDomainRecord *record,void *ctx);
int qrx_net_registry_list(QrxDB *db,const char *owner,QrxDomainRecordCallback cb,void *ctx);
int qrx_net_registry_history(QrxDB *db,const char *name,QrxDomainRecordCallback cb,void *ctx);


/* Stand-alone convenience operations. */
int qrx_net_registry_register(QrxDB *db,const char *name,const char *owner,
                              const char *qub_address,const uint8_t *web_root,
                              const uint8_t *publishing_commitment,
                              uint64_t current_height,uint64_t expiry_height,
                              QrxDomainRecord *out);
int qrx_net_registry_renew(QrxDB *db,const char *name,const char *owner,uint64_t expected_sequence,
                           uint64_t current_height,uint64_t new_expiry_height,QrxDomainRecord *out);
int qrx_net_registry_update(QrxDB *db,const char *name,const char *owner,uint64_t expected_sequence,
                            uint64_t current_height,const QrxDomainUpdate *update,QrxDomainRecord *out);
int qrx_net_registry_transfer(QrxDB *db,const char *name,const char *owner,uint64_t expected_sequence,
                              uint64_t current_height,const char *new_owner,QrxDomainRecord *out);

/* Atomic staging variants for the main applytx WAL batch. They never commit
 * independently and therefore cannot tear domain state from balances/nonces. */
int qrx_net_registry_stage_register(QrxDB *db,QrxDBBatch *batch,const char *name,const char *owner,
                                    const char *qub_address,const uint8_t *web_root,
                                    const uint8_t *publishing_commitment,
                                    uint64_t current_height,uint64_t expiry_height,
                                    QrxDomainRecord *out);
int qrx_net_registry_stage_renew(QrxDB *db,QrxDBBatch *batch,const char *name,const char *owner,uint64_t expected_sequence,
                                 uint64_t current_height,uint64_t new_expiry_height,QrxDomainRecord *out);
int qrx_net_registry_stage_update(QrxDB *db,QrxDBBatch *batch,const char *name,const char *owner,uint64_t expected_sequence,
                                  uint64_t current_height,const QrxDomainUpdate *update,QrxDomainRecord *out);
int qrx_net_registry_stage_transfer(QrxDB *db,QrxDBBatch *batch,const char *name,const char *owner,uint64_t expected_sequence,
                                    uint64_t current_height,const char *new_owner,QrxDomainRecord *out);
#ifdef __cplusplus
}
#endif
