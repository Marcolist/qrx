#include "resource/qrx_capacity_proof.h"

#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>

static void put_u64be(uint8_t out[8], uint64_t v) {
    for (int i=7;i>=0;--i) { out[i]=(uint8_t)(v&0xffu); v>>=8; }
}

static int challenge_digest(const QrxCapacityCommitment *c,
                            const uint8_t finalized_block_hash[64],
                            uint8_t out[64]) {
    static const char domain[] = "QRX-CAPACITY-CHALLENGE-V1";
    EVP_MD_CTX *ctx = NULL; unsigned int n = 0; uint8_t eb[8], sb[8]; int rc = -1;
    if (!c || !finalized_block_hash || !out || !c->provider_id[0] || !c->slot_count) return -1;
    put_u64be(eb, c->epoch); put_u64be(sb, c->slot_count);
    ctx = EVP_MD_CTX_new(); if (!ctx) return -1;
    if (EVP_DigestInit_ex(ctx, EVP_sha3_512(), NULL) != 1) goto done;
    if (EVP_DigestUpdate(ctx, domain, sizeof(domain)-1) != 1 ||
        EVP_DigestUpdate(ctx, c->provider_id, strlen(c->provider_id)) != 1 ||
        EVP_DigestUpdate(ctx, eb, sizeof(eb)) != 1 ||
        EVP_DigestUpdate(ctx, sb, sizeof(sb)) != 1 ||
        EVP_DigestUpdate(ctx, c->merkle_root, sizeof(c->merkle_root)) != 1 ||
        EVP_DigestUpdate(ctx, finalized_block_hash, 64) != 1 ||
        EVP_DigestFinal_ex(ctx, out, &n) != 1 || n != 64) goto done;
    rc = 0;
done: EVP_MD_CTX_free(ctx); return rc;
}

int qrx_capacity_commit(const char *provider_id,
                        uint64_t epoch,
                        const uint8_t *slots,
                        size_t slot_count,
                        size_t slot_bytes,
                        QrxCapacityCommitment *out) {
    if (!out || !provider_id || !*provider_id || strlen(provider_id) >= sizeof(out->provider_id) ||
        !slots || !slot_count || !slot_bytes || slot_bytes > UINT32_MAX ||
        slot_count > SIZE_MAX / 64) return -1;
    uint8_t *leaves = (uint8_t*)malloc(slot_count * 64); if (!leaves) return -1;
    for (size_t i=0;i<slot_count;++i) {
        if (qrx_merkle_leaf_hash((uint64_t)i, slots + i*slot_bytes, slot_bytes, leaves+i*64) != 0) { free(leaves); return -1; }
    }
    memset(out,0,sizeof(*out)); out->version=QRX_CAPACITY_PROOF_VERSION;
    strcpy(out->provider_id,provider_id); out->epoch=epoch; out->slot_count=(uint64_t)slot_count; out->slot_bytes=(uint32_t)slot_bytes;
    int rc=qrx_merkle_root_from_leaves(leaves,slot_count,out->merkle_root); free(leaves); return rc;
}

uint64_t qrx_capacity_challenge_index(const QrxCapacityCommitment *c,
                                      const uint8_t finalized_block_hash[64]) {
    uint8_t h[64]; if (challenge_digest(c,finalized_block_hash,h)!=0 || !c->slot_count) return UINT64_MAX;
    uint64_t v=0; for(int i=0;i<8;++i) v=(v<<8)|h[i]; return v % c->slot_count;
}

int qrx_capacity_build_challenge_proof(const QrxCapacityCommitment *c,
                                       const uint8_t *slots,
                                       size_t slot_count,
                                       size_t slot_bytes,
                                       const uint8_t finalized_block_hash[64],
                                       QrxCapacityChallengeProof *out) {
    if (!c || !slots || !out || slot_count != c->slot_count || slot_bytes != c->slot_bytes || slot_count > SIZE_MAX/64) return -1;
    uint64_t idx=qrx_capacity_challenge_index(c,finalized_block_hash); if(idx==UINT64_MAX || idx>=slot_count) return -1;
    uint8_t *leaves=(uint8_t*)malloc(slot_count*64); if(!leaves)return -1;
    for(size_t i=0;i<slot_count;++i) if(qrx_merkle_leaf_hash((uint64_t)i,slots+i*slot_bytes,slot_bytes,leaves+i*64)!=0){free(leaves);return -1;}
    uint8_t root[64]; if(qrx_merkle_root_from_leaves(leaves,slot_count,root)!=0 || CRYPTO_memcmp(root,c->merkle_root,64)!=0){free(leaves);return -1;}
    memset(out,0,sizeof(*out)); out->slot_index=idx;
    int rc=qrx_merkle_build_proof_from_leaves(leaves,slot_count,(size_t)idx,&out->merkle_proof); free(leaves); return rc;
}

int qrx_capacity_verify_challenge(const QrxCapacityCommitment *c,
                                  const uint8_t finalized_block_hash[64],
                                  const uint8_t *revealed_slot,
                                  size_t revealed_slot_len,
                                  const QrxCapacityChallengeProof *proof) {
    if(!c||!finalized_block_hash||!revealed_slot||!proof||revealed_slot_len!=c->slot_bytes) return -1;
    uint64_t want=qrx_capacity_challenge_index(c,finalized_block_hash); if(want==UINT64_MAX||proof->slot_index!=want||proof->merkle_proof.leaf_index!=want||proof->merkle_proof.leaf_count!=c->slot_count)return -1;
    uint8_t leaf[64]; if(qrx_merkle_leaf_hash(want,revealed_slot,revealed_slot_len,leaf)!=0)return -1;
    return qrx_merkle_verify_proof(leaf,&proof->merkle_proof,c->merkle_root);
}
