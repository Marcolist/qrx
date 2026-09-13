#pragma once
#include "protocol/qrx_service_effects.h"
#include "qrxdb.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_DOMAIN_YEAR_BLOCKS 3153600ULL
#define QRX_DOMAIN_DEFAULT_BASE_ANNUAL_ATOMS 100000ULL

int qrx_net_consensus_prepare(const char *chain_dir,const char *tx_type,const char *from,const char *to,
                              uint64_t amount_atoms,const char *payload,const char *txid,uint64_t height,
                              QrxServiceEconomicEffect *effect);
int qrx_net_consensus_stage(QrxDB *db,QrxDBBatch *batch,const char *chain_dir,const char *tx_type,
                            const char *from,const char *to,uint64_t amount_atoms,const char *payload,
                            const char *txid,uint64_t height);
#ifdef __cplusplus
}
#endif
