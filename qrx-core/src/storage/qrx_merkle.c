#include "storage/qrx_merkle.h"

#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>

static void put_u64be(uint8_t out[8], uint64_t v) {
    for (int i = 7; i >= 0; --i) { out[i] = (uint8_t)(v & 0xffu); v >>= 8; }
}

static int sha3_512_parts(const void *a, size_t alen,
                          const void *b, size_t blen,
                          const void *c, size_t clen,
                          uint8_t out[64]) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int n = 0;
    int ok = 0;
    if (!ctx) return -1;
    if (EVP_DigestInit_ex(ctx, EVP_sha3_512(), NULL) != 1) goto done;
    if (alen && EVP_DigestUpdate(ctx, a, alen) != 1) goto done;
    if (blen && EVP_DigestUpdate(ctx, b, blen) != 1) goto done;
    if (clen && EVP_DigestUpdate(ctx, c, clen) != 1) goto done;
    if (EVP_DigestFinal_ex(ctx, out, &n) != 1 || n != 64) goto done;
    ok = 1;
done:
    EVP_MD_CTX_free(ctx);
    return ok ? 0 : -1;
}

int qrx_merkle_leaf_hash(uint64_t index,
                         const uint8_t *data,
                         size_t data_len,
                         uint8_t out[64]) {
    static const char domain[] = "QRX-MERKLE-LEAF-V1";
    uint8_t ib[8];
    if ((!data && data_len) || !out) return -1;
    put_u64be(ib, index);
    return sha3_512_parts(domain, sizeof(domain)-1, ib, sizeof(ib), data, data_len, out);
}

static int node_hash(const uint8_t left[64], const uint8_t right[64], uint8_t out[64]) {
    static const char domain[] = "QRX-MERKLE-NODE-V1";
    uint8_t pair[128];
    memcpy(pair, left, 64); memcpy(pair + 64, right, 64);
    return sha3_512_parts(domain, sizeof(domain)-1, pair, sizeof(pair), NULL, 0, out);
}

static size_t next_pow2(size_t n) {
    size_t p = 1;
    if (!n) return 0;
    while (p < n) {
        if (p > SIZE_MAX / 2) return 0;
        p <<= 1;
    }
    return p;
}

int qrx_merkle_root_from_leaves(const uint8_t *leaf_hashes,
                                size_t leaf_count,
                                uint8_t out[64]) {
    if (!leaf_hashes || !leaf_count || !out) return -1;
    size_t width = next_pow2(leaf_count);
    if (!width || width > SIZE_MAX / 64) return -1;
    uint8_t *level = (uint8_t*)malloc(width * 64);
    if (!level) return -1;
    for (size_t i = 0; i < width; ++i) {
        size_t src = i < leaf_count ? i : leaf_count - 1;
        memcpy(level + i*64, leaf_hashes + src*64, 64);
    }
    while (width > 1) {
        for (size_t i = 0; i < width; i += 2) {
            uint8_t h[64];
            if (node_hash(level + i*64, level + (i+1)*64, h) != 0) { free(level); return -1; }
            memcpy(level + (i/2)*64, h, 64);
        }
        width >>= 1;
    }
    memcpy(out, level, 64);
    free(level);
    return 0;
}

int qrx_merkle_root_from_chunks(const uint8_t *data,
                                size_t data_len,
                                size_t chunk_size,
                                uint8_t out[64],
                                size_t *leaf_count_out) {
    if ((!data && data_len) || !chunk_size || !out) return -1;
    size_t leaves = data_len ? (data_len + chunk_size - 1) / chunk_size : 1;
    if (leaves > SIZE_MAX / 64) return -1;
    uint8_t *hashes = (uint8_t*)malloc(leaves * 64);
    if (!hashes) return -1;
    for (size_t i = 0; i < leaves; ++i) {
        size_t off = i * chunk_size;
        size_t len = off < data_len ? data_len - off : 0;
        if (len > chunk_size) len = chunk_size;
        const uint8_t *p = len ? data + off : NULL;
        if (qrx_merkle_leaf_hash((uint64_t)i, p, len, hashes + i*64) != 0) { free(hashes); return -1; }
    }
    int rc = qrx_merkle_root_from_leaves(hashes, leaves, out);
    free(hashes);
    if (rc == 0 && leaf_count_out) *leaf_count_out = leaves;
    return rc;
}

int qrx_merkle_build_proof_from_leaves(const uint8_t *leaf_hashes,
                                       size_t leaf_count,
                                       size_t leaf_index,
                                       QrxMerkleProof *out) {
    if (!leaf_hashes || !leaf_count || leaf_index >= leaf_count || !out) return -1;
    size_t width = next_pow2(leaf_count);
    if (!width || width > SIZE_MAX / 64) return -1;
    uint8_t *level = (uint8_t*)malloc(width * 64);
    if (!level) return -1;
    for (size_t i = 0; i < width; ++i) {
        size_t src = i < leaf_count ? i : leaf_count - 1;
        memcpy(level + i*64, leaf_hashes + src*64, 64);
    }
    memset(out, 0, sizeof(*out));
    out->leaf_index = (uint64_t)leaf_index;
    out->leaf_count = (uint64_t)leaf_count;
    size_t idx = leaf_index;
    while (width > 1) {
        if (out->depth >= QRX_MERKLE_MAX_DEPTH) { free(level); return -1; }
        size_t sibling = idx ^ 1u;
        memcpy(out->siblings[out->depth], level + sibling*64, 64);
        out->depth++;
        for (size_t i = 0; i < width; i += 2) {
            uint8_t h[64];
            if (node_hash(level + i*64, level + (i+1)*64, h) != 0) { free(level); return -1; }
            memcpy(level + (i/2)*64, h, 64);
        }
        idx >>= 1;
        width >>= 1;
    }
    free(level);
    return 0;
}

int qrx_merkle_verify_proof(const uint8_t leaf_hash[64],
                            const QrxMerkleProof *proof,
                            const uint8_t expected_root[64]) {
    if (!leaf_hash || !proof || !expected_root || !proof->leaf_count ||
        proof->leaf_index >= proof->leaf_count || proof->depth > QRX_MERKLE_MAX_DEPTH) return -1;
    uint8_t cur[64]; memcpy(cur, leaf_hash, 64);
    uint64_t idx = proof->leaf_index;
    for (uint32_t d = 0; d < proof->depth; ++d) {
        uint8_t h[64];
        int rc = (idx & 1u) ? node_hash(proof->siblings[d], cur, h)
                            : node_hash(cur, proof->siblings[d], h);
        if (rc != 0) return -1;
        memcpy(cur, h, 64);
        idx >>= 1;
    }
    return CRYPTO_memcmp(cur, expected_root, 64) == 0 ? 0 : -1;
}
