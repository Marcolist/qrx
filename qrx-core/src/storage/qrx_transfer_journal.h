#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { char transfer_id[129]; char contract_id[129]; char direction[16]; char path[1024]; uint64_t total_bytes; uint64_t completed_bytes; uint32_t completed_shards; uint32_t required_shards; uint32_t data_shards; uint32_t parity_shards; uint64_t shard_size; char state[24]; } QrxTransferJournal;
int qrx_transfer_journal_save(const char *dir,const QrxTransferJournal *j);
int qrx_transfer_journal_load(const char *dir,const char *transfer_id,QrxTransferJournal *j);
#ifdef __cplusplus
}
#endif
