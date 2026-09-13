#include "storage/qrx_storage_fs.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define QRX_MKDIR(path) _mkdir(path)
#define QRX_RMDIR(path) _rmdir(path)
#define QRX_PID _getpid()
#else
#include <unistd.h>
#define QRX_MKDIR(path) mkdir((path), 0700)
#define QRX_RMDIR(path) rmdir(path)
#define QRX_PID getpid()
#include <sys/stat.h>
#endif

static void test_path(char *out, size_t n) {
#ifdef _WIN32
    const char *base = getenv("TEMP"); if (!base) base = ".";
#else
    const char *base = getenv("TMPDIR"); if (!base) base = "/tmp";
#endif
    snprintf(out, n, "%s/qrx-storage-test-%lld-%ld", base, (long long)time(NULL), (long)QRX_PID);
}

int main(void) {
    char root[1024]; test_path(root, sizeof(root));
    QRX_MKDIR(root);

    QrxStorageFs *fs = NULL;
    assert(qrx_storage_fs_open(root, 1024 * 1024, 0, &fs) == 0);
    assert(fs != NULL);
    assert(strcmp(qrx_storage_fs_backend_name(), "filesystem") == 0);

    const char payload[] = "QRX Drive 0.0.8.1 atomic content-addressed storage";
    char id1[65], id2[65];
    assert(qrx_storage_fs_put(fs, payload, sizeof(payload) - 1, id1) == 0);
    assert(strlen(id1) == 64);
    assert(qrx_storage_fs_has(fs, id1) == 1);

    QrxStorageFsStats st;
    assert(qrx_storage_fs_stats(fs, &st) == 0);
    assert(st.object_count == 1);
    assert(st.used_bytes == sizeof(payload) - 1);

    /* Idempotent duplicate: content ID and accounting remain stable. */
    assert(qrx_storage_fs_put(fs, payload, sizeof(payload) - 1, id2) == 0);
    assert(strcmp(id1, id2) == 0);
    assert(qrx_storage_fs_stats(fs, &st) == 0);
    assert(st.object_count == 1);
    assert(st.used_bytes == sizeof(payload) - 1);

    unsigned char *got = NULL; size_t got_len = 0;
    assert(qrx_storage_fs_read(fs, id1, &got, &got_len) == 0);
    assert(got_len == sizeof(payload) - 1);
    assert(memcmp(got, payload, got_len) == 0);
    free(got);

    /* Quota is fail-closed. */
    unsigned char *too_big = (unsigned char*)malloc(1024 * 1024);
    assert(too_big != NULL); memset(too_big, 0xA5, 1024 * 1024);
    char too_big_id[65];
    assert(qrx_storage_fs_put(fs, too_big, 1024 * 1024, too_big_id) == -2);
    free(too_big);

    qrx_storage_fs_close(fs);

    /* Reopen performs crash-style accounting reconciliation. */
    fs = NULL;
    assert(qrx_storage_fs_open(root, 1024 * 1024, 0, &fs) == 0);
    assert(qrx_storage_fs_stats(fs, &st) == 0);
    assert(st.object_count == 1);
    assert(st.used_bytes == sizeof(payload) - 1);
    assert(qrx_storage_fs_delete(fs, id1) == 0);
    assert(qrx_storage_fs_has(fs, id1) == 0);
    assert(qrx_storage_fs_stats(fs, &st) == 0);
    assert(st.object_count == 0 && st.used_bytes == 0);
    qrx_storage_fs_close(fs);

    /* Directory cleanup is intentionally left to the test runner/temp cleaner;
       nested CAS directories make portable recursive deletion noisy here. */
    puts("PASS: QRX 0.0.8.1 native filesystem backend");
    return 0;
}
