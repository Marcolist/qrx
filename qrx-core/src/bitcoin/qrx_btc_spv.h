#ifndef QRX_BTC_SPV_H
#define QRX_BTC_SPV_H

#include <stddef.h>
#include <stdint.h>
#include "qrxdb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QRX_BTC_SPV_HASH_HEX 65
#define QRX_BTC_SPV_CHAINWORK_HEX 96

typedef struct {
    char hash[QRX_BTC_SPV_HASH_HEX];
    char prev_hash[QRX_BTC_SPV_HASH_HEX];
    char merkle_root[QRX_BTC_SPV_HASH_HEX];
    char header_hex[161];
    char chainwork_hex[QRX_BTC_SPV_CHAINWORK_HEX];
    uint64_t height;
    uint32_t version;
    uint32_t timestamp;
    uint32_t bits;
    uint32_t nonce;
    int active;
} QrxBtcSpvHeaderInfo;

typedef struct {
    char txid[QRX_BTC_SPV_HASH_HEX];
    char block_hash[QRX_BTC_SPV_HASH_HEX];
    uint64_t block_height;
    uint64_t confirmations;
    uint64_t merkle_index;
    int merkle_valid;
    int active_chain;
} QrxBtcSpvProofResult;

typedef struct {
    char txid[QRX_BTC_SPV_HASH_HEX];
    int vout;
    int64_t sats;
} QrxBtcTxOutputMatch;

int qrx_btc_spv_network_valid(const char *network);
/* Deterministic Bitcoin Core-style 2016-block retarget arithmetic.
 * Exposed so release tests can pin consensus vectors without duplicating the
 * compact-target implementation. actual_timespan is clamped to 1/4..4x. */
int qrx_btc_spv_retarget_bits(uint32_t previous_bits, uint32_t network_powlimit_bits,
                              int64_t actual_timespan, uint32_t *out_bits);
/* Pure cross-chain funding release gate used by consensus before expensive
 * SPV proof verification. Returns 1 only while funding is still allowed, the
 * first proof has not been locked, and the session is awaiting funding. */
int qrx_btc_spv_funding_policy_valid(int64_t current_qrx_height, int64_t funding_deadline_qrx_height,
                                     int proof_locked, int has_funding_txid, int status_awaiting_funding);
const char *qrx_btc_spv_genesis_header_hex(const char *network);
int qrx_btc_spv_init(QrxDB *db, const char *network, char *err, size_t err_sz);
int qrx_btc_spv_stage_header(QrxDB *db, QrxDBBatch *batch, const char *network,
                             const char *header_hex, QrxBtcSpvHeaderInfo *out,
                             int *became_best, char *err, size_t err_sz);
int qrx_btc_spv_get_header(QrxDB *db, const char *network, const char *hash_or_height,
                           QrxBtcSpvHeaderInfo *out, char *err, size_t err_sz);
int qrx_btc_spv_get_best(QrxDB *db, const char *network, QrxBtcSpvHeaderInfo *out,
                         char *err, size_t err_sz);
int qrx_btc_spv_confirmations(QrxDB *db, const char *network, const char *block_hash,
                              uint64_t block_height, uint64_t *confirmations,
                              int *active, char *err, size_t err_sz);
int qrx_btc_spv_verify_merkle(QrxDB *db, const char *network, const char *txid,
                              const char *block_hash, uint64_t tx_index,
                              const char *branch_csv, QrxBtcSpvProofResult *out,
                              char *err, size_t err_sz);
int qrx_btc_tx_find_output(const char *rawtx_hex, int64_t expected_sats,
                           const char *expected_scriptpubkey_hex,
                           QrxBtcTxOutputMatch *out, char *err, size_t err_sz);
int qrx_btc_spv_stage_proof(QrxDBBatch *batch, const char *network,
                            const QrxBtcSpvProofResult *proof,
                            int vout, int64_t sats, const char *scriptpubkey_hex,
                            char *err, size_t err_sz);
int qrx_btc_spv_get_stored_proof(QrxDB *db, const char *network, const char *txid,
                                 QrxBtcSpvProofResult *out, int *vout, int64_t *sats,
                                 char *scriptpubkey_hex, size_t script_sz,
                                 char *err, size_t err_sz);

#ifdef __cplusplus
}
#endif

#endif
