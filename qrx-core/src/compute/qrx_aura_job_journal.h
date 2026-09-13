#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_AURA_JOB_JOURNAL_VERSION 1u
#define QRX_AURA_JOB_JOURNAL_MAX 256u
#define QRX_AURA_JOB_JOURNAL_MAX_RESULT (16u*1024u*1024u)
#define QRX_AURA_JOB_ID_MAX 129u

typedef enum { QRX_AURA_JOB_PREPARED=1, QRX_AURA_JOB_COMMITTED=2 } QrxAuraJobState;
typedef struct {
    uint32_t version;
    QrxAuraJobState state;
    char requester_id[QRX_AURA_JOB_ID_MAX];
    char lease_id[65];
    uint64_t sequence;
    char request_commitment[65];
    uint32_t result_status;
    uint8_t *result;
    size_t result_len;
    char result_commitment[65];
} QrxAuraJobRecord;
typedef struct {
    QrxAuraJobRecord records[QRX_AURA_JOB_JOURNAL_MAX];
    uint32_t count;
    uint64_t revision;
} QrxAuraJobJournal;

void qrx_aura_job_journal_init(QrxAuraJobJournal *j);
void qrx_aura_job_journal_free(QrxAuraJobJournal *j);
QrxAuraJobRecord *qrx_aura_job_journal_find(QrxAuraJobJournal *j,const char *requester_id,const char *lease_id,uint64_t sequence);
const QrxAuraJobRecord *qrx_aura_job_journal_find_const(const QrxAuraJobJournal *j,const char *requester_id,const char *lease_id,uint64_t sequence);
int qrx_aura_job_journal_prepare(QrxAuraJobJournal *j,const char *requester_id,const char *lease_id,uint64_t sequence,const char request_commitment[65]);
int qrx_aura_job_journal_commit(QrxAuraJobJournal *j,const char *requester_id,const char *lease_id,uint64_t sequence,const char request_commitment[65],uint32_t result_status,const void *result,size_t result_len);
int qrx_aura_job_journal_save(const char *path,const QrxAuraJobJournal *j);
int qrx_aura_job_journal_load(const char *path,QrxAuraJobJournal *j);
size_t qrx_aura_job_journal_compact(QrxAuraJobJournal *j,uint32_t keep_committed,uint32_t keep_prepared);
#ifdef __cplusplus
}
#endif
