#include "storage/qrx_storage_fs.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define QRX_MKDIR(path) _mkdir(path)
#define QRX_UNLINK(path) _unlink(path)
#define QRX_FILENO(fp) _fileno(fp)
#define QRX_FSYNC(fd) _commit(fd)
#define QRX_FSEEK(fp,off) _fseeki64((fp),(__int64)(off),SEEK_SET)
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <unistd.h>
#define QRX_MKDIR(path) mkdir((path), 0700)
#define QRX_UNLINK(path) unlink(path)
#define QRX_FILENO(fp) fileno(fp)
#define QRX_FSYNC(fd) fsync(fd)
#define QRX_FSEEK(fp,off) fseeko((fp),(off_t)(off),SEEK_SET)
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct QrxStorageFs {
    char root[PATH_MAX];
    char objects[PATH_MAX];
    char tmp[PATH_MAX];
    char usage_file[PATH_MAX];
    uint64_t max_usage_bytes;
    uint64_t min_free_space_bytes;
    uint64_t used_bytes;
    uint64_t reserved_bytes;
    uint64_t object_count;
    CRYPTO_RWLOCK *lock;
};

static int is_sep(char c) { return c == '/' || c == '\\'; }

static int mkdir_one(const char *path) {
    if (QRX_MKDIR(path) == 0) return 0;
    return errno == EEXIST ? 0 : -1;
}

static int mkdir_p_local(const char *path) {
    if (!path || !*path) return -1;
    char buf[PATH_MAX];
    size_t n = strlen(path);
    if (n >= sizeof(buf)) return -1;
    memcpy(buf, path, n + 1);
    for (size_t i = 1; i < n; ++i) {
        if (!is_sep(buf[i])) continue;
#ifdef _WIN32
        if (i == 2 && buf[1] == ':') continue;
#endif
        char save = buf[i];
        buf[i] = 0;
        if (*buf && mkdir_one(buf) != 0) return -1;
        buf[i] = save;
    }
    return mkdir_one(buf);
}

static int file_flush_sync(FILE *f) {
    if (fflush(f) != 0) return -1;
    int fd = QRX_FILENO(f);
    if (fd < 0) return -1;
    return QRX_FSYNC(fd) == 0 ? 0 : -1;
}

static int atomic_replace(const char *src, const char *dst) {
#ifdef _WIN32
    return MoveFileExA(src, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
#else
    return rename(src, dst);
#endif
}

static int path_is_regular(const char *path, uint64_t *size_out) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
#ifdef _WIN32
    if ((st.st_mode & _S_IFREG) == 0) return 0;
#else
    if (!S_ISREG(st.st_mode)) return 0;
#endif
    if (size_out) *size_out = (uint64_t)st.st_size;
    return 1;
}

static int valid_object_id(const char *id) {
    if (!id || strlen(id) != 64) return 0;
    for (int i = 0; i < 64; ++i)
        if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) return 0;
    return 1;
}

static void bytes_to_hex32(const unsigned char in[32], char out[65]) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        out[i * 2] = hex[in[i] >> 4];
        out[i * 2 + 1] = hex[in[i] & 15];
    }
    out[64] = 0;
}

static int digest_init(EVP_MD_CTX **ctx_out) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return -1;
    if (EVP_DigestInit_ex(ctx, EVP_sha3_256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }
    *ctx_out = ctx;
    return 0;
}

static int digest_final(EVP_MD_CTX *ctx, char out[65]) {
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int n = 0;
    if (EVP_DigestFinal_ex(ctx, md, &n) != 1 || n != 32) return -1;
    bytes_to_hex32(md, out);
    return 0;
}

static int object_path(QrxStorageFs *fs, const char *id, char out[PATH_MAX], int create_dirs) {
    if (!fs || !valid_object_id(id)) return -1;
    char d1[PATH_MAX], d2[PATH_MAX];
    if (snprintf(d1, sizeof(d1), "%s/%.2s", fs->objects, id) >= (int)sizeof(d1)) return -1;
    if (snprintf(d2, sizeof(d2), "%s/%.2s", d1, id + 2) >= (int)sizeof(d2)) return -1;
    if (create_dirs && (mkdir_p_local(d1) != 0 || mkdir_p_local(d2) != 0)) return -1;
    if (snprintf(out, PATH_MAX, "%s/%s.blob", d2, id) >= PATH_MAX) return -1;
    return 0;
}

static int filesystem_free(const char *path, uint64_t *out) {
#ifdef _WIN32
    ULARGE_INTEGER avail;
    if (!GetDiskFreeSpaceExA(path, &avail, NULL, NULL)) return -1;
    *out = (uint64_t)avail.QuadPart;
    return 0;
#else
    struct statvfs v;
    if (statvfs(path, &v) != 0) return -1;
    *out = (uint64_t)v.f_bavail * (uint64_t)v.f_frsize;
    return 0;
#endif
}

static int write_usage_locked(QrxStorageFs *fs) {
    char tmp[PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", fs->usage_file) >= (int)sizeof(tmp)) return -1;
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    int ok = fprintf(f, "format=qrx-storage-usage-v1\nused_bytes=%llu\nobject_count=%llu\n",
                     (unsigned long long)fs->used_bytes,
                     (unsigned long long)fs->object_count) > 0;
    if (!ok || file_flush_sync(f) != 0) { fclose(f); QRX_UNLINK(tmp); return -1; }
    if (fclose(f) != 0) { QRX_UNLINK(tmp); return -1; }
    if (atomic_replace(tmp, fs->usage_file) != 0) { QRX_UNLINK(tmp); return -1; }
    return 0;
}

static int reserve_bytes(QrxStorageFs *fs, uint64_t n) {
    if (!CRYPTO_THREAD_write_lock(fs->lock)) return -1;
    int rc = 0;
    if (fs->max_usage_bytes && (n > fs->max_usage_bytes || fs->used_bytes > fs->max_usage_bytes - n || fs->reserved_bytes > fs->max_usage_bytes - fs->used_bytes - n)) {
        rc = -2;
    } else {
        uint64_t free_bytes = 0;
        if (filesystem_free(fs->root, &free_bytes) != 0 || free_bytes < fs->min_free_space_bytes || n > free_bytes - fs->min_free_space_bytes) rc = -3;
        else fs->reserved_bytes += n;
    }
    CRYPTO_THREAD_unlock(fs->lock);
    return rc;
}

static void release_reservation(QrxStorageFs *fs, uint64_t n) {
    if (!CRYPTO_THREAD_write_lock(fs->lock)) return;
    if (fs->reserved_bytes >= n) fs->reserved_bytes -= n;
    else fs->reserved_bytes = 0;
    CRYPTO_THREAD_unlock(fs->lock);
}

static int commit_new_object(QrxStorageFs *fs, uint64_t reserved, uint64_t actual) {
    if (!CRYPTO_THREAD_write_lock(fs->lock)) return -1;
    if (fs->reserved_bytes >= reserved) fs->reserved_bytes -= reserved;
    else fs->reserved_bytes = 0;
    if (UINT64_MAX - fs->used_bytes < actual) { CRYPTO_THREAD_unlock(fs->lock); return -1; }
    fs->used_bytes += actual;
    fs->object_count++;
    int rc = write_usage_locked(fs);
    CRYPTO_THREAD_unlock(fs->lock);
    return rc;
}

static int random_temp_path(QrxStorageFs *fs, char out[PATH_MAX]) {
    unsigned char r[12];
    if (RAND_bytes(r, sizeof(r)) != 1) return -1;
    char hex[25];
    static const char hc[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(r); ++i) { hex[i*2] = hc[r[i] >> 4]; hex[i*2+1] = hc[r[i] & 15]; }
    hex[24] = 0;
    return snprintf(out, PATH_MAX, "%s/%s.part", fs->tmp, hex) < PATH_MAX ? 0 : -1;
}

#ifdef _WIN32
static int scan_dir_bytes(const char *path, uint64_t *bytes, uint64_t *files) {
    char pattern[PATH_MAX];
    if (snprintf(pattern, sizeof(pattern), "%s/*", path) >= (int)sizeof(pattern)) return -1;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND ? 0 : -1;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        char child[PATH_MAX];
        if (snprintf(child, sizeof(child), "%s/%s", path, fd.cFileName) >= (int)sizeof(child)) { FindClose(h); return -1; }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (scan_dir_bytes(child, bytes, files) != 0) { FindClose(h); return -1; }
        } else {
            ULARGE_INTEGER s; s.HighPart = fd.nFileSizeHigh; s.LowPart = fd.nFileSizeLow;
            *bytes += (uint64_t)s.QuadPart; (*files)++;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return 0;
}
static int clean_tmp_dir(const char *path) {
    char pattern[PATH_MAX];
    if (snprintf(pattern, sizeof(pattern), "%s/*", path) >= (int)sizeof(pattern)) return -1;
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        char child[PATH_MAX];
        if (snprintf(child, sizeof(child), "%s/%s", path, fd.cFileName) < (int)sizeof(child)) QRX_UNLINK(child);
    } while (FindNextFileA(h, &fd));
    FindClose(h); return 0;
}
#else
static int scan_dir_bytes(const char *path, uint64_t *bytes, uint64_t *files) {
    DIR *d = opendir(path); if (!d) return errno == ENOENT ? 0 : -1;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char child[PATH_MAX];
        if (snprintf(child, sizeof(child), "%s/%s", path, de->d_name) >= (int)sizeof(child)) { closedir(d); return -1; }
        struct stat st;
        if (lstat(child, &st) != 0) continue;
        if (S_ISLNK(st.st_mode)) continue;
        if (S_ISDIR(st.st_mode)) {
            if (scan_dir_bytes(child, bytes, files) != 0) { closedir(d); return -1; }
        } else if (S_ISREG(st.st_mode)) {
            *bytes += (uint64_t)st.st_size; (*files)++;
        }
    }
    closedir(d); return 0;
}
static int clean_tmp_dir(const char *path) {
    DIR *d = opendir(path); if (!d) return errno == ENOENT ? 0 : -1;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char child[PATH_MAX];
        if (snprintf(child, sizeof(child), "%s/%s", path, de->d_name) >= (int)sizeof(child)) continue;
        struct stat st; if (lstat(child, &st) != 0) continue;
        if (S_ISREG(st.st_mode)) QRX_UNLINK(child);
    }
    closedir(d); return 0;
}
#endif

int qrx_storage_fs_open(const char *root, uint64_t max_usage_bytes, uint64_t min_free_space_bytes, QrxStorageFs **out) {
    if (!root || !*root || !out) return -1;
    *out = NULL;
    QrxStorageFs *fs = (QrxStorageFs*)calloc(1, sizeof(*fs));
    if (!fs) return -1;
    if (strlen(root) >= sizeof(fs->root)) { free(fs); return -1; }
    snprintf(fs->root, sizeof(fs->root), "%s", root);
    if (snprintf(fs->objects, sizeof(fs->objects), "%s/objects", root) >= (int)sizeof(fs->objects) ||
        snprintf(fs->tmp, sizeof(fs->tmp), "%s/tmp", root) >= (int)sizeof(fs->tmp) ||
        snprintf(fs->usage_file, sizeof(fs->usage_file), "%s/usage.state", root) >= (int)sizeof(fs->usage_file)) { free(fs); return -1; }
    fs->max_usage_bytes = max_usage_bytes;
    fs->min_free_space_bytes = min_free_space_bytes;
    fs->lock = CRYPTO_THREAD_lock_new();
    if (!fs->lock) { free(fs); return -1; }
    if (mkdir_p_local(fs->root) != 0 || mkdir_p_local(fs->objects) != 0 || mkdir_p_local(fs->tmp) != 0) {
        qrx_storage_fs_close(fs); return -1;
    }
    *out = fs;
    if (qrx_storage_fs_recover(fs) != 0) { qrx_storage_fs_close(fs); *out = NULL; return -1; }
    return 0;
}

void qrx_storage_fs_close(QrxStorageFs *fs) {
    if (!fs) return;
    if (fs->lock) CRYPTO_THREAD_lock_free(fs->lock);
    OPENSSL_cleanse(fs, sizeof(*fs));
    free(fs);
}

int qrx_storage_fs_recover(QrxStorageFs *fs) {
    if (!fs || !CRYPTO_THREAD_write_lock(fs->lock)) return -1;
    int rc = clean_tmp_dir(fs->tmp);
    uint64_t bytes = 0, files = 0;
    if (rc == 0) rc = scan_dir_bytes(fs->objects, &bytes, &files);
    if (rc == 0 && fs->max_usage_bytes && bytes > fs->max_usage_bytes) rc = -2;
    if (rc == 0) {
        fs->used_bytes = bytes;
        fs->object_count = files;
        fs->reserved_bytes = 0;
        rc = write_usage_locked(fs);
    }
    CRYPTO_THREAD_unlock(fs->lock);
    return rc;
}

static int finalize_temp(QrxStorageFs *fs, const char *tmp_path, const char id[65], uint64_t reserved, uint64_t actual) {
    char final[PATH_MAX];
    if (object_path(fs, id, final, 1) != 0) { QRX_UNLINK(tmp_path); release_reservation(fs, reserved); return -1; }
    uint64_t existing = 0;
    if (path_is_regular(final, &existing)) {
        QRX_UNLINK(tmp_path);
        release_reservation(fs, reserved);
        return existing == actual ? 0 : -1;
    }
    if (atomic_replace(tmp_path, final) != 0) { QRX_UNLINK(tmp_path); release_reservation(fs, reserved); return -1; }
    if (commit_new_object(fs, reserved, actual) != 0) return -1;
    return 0;
}

int qrx_storage_fs_put(QrxStorageFs *fs, const void *data, size_t len, char out_id[65]) {
    if (!fs || (!data && len) || !out_id) return -1;
    uint64_t n = (uint64_t)len;
    int rr = reserve_bytes(fs, n); if (rr != 0) return rr;
    char tmp[PATH_MAX];
    if (random_temp_path(fs, tmp) != 0) { release_reservation(fs, n); return -1; }
    FILE *f = fopen(tmp, "wb"); if (!f) { release_reservation(fs, n); return -1; }
    EVP_MD_CTX *ctx = NULL;
    if (digest_init(&ctx) != 0) { fclose(f); QRX_UNLINK(tmp); release_reservation(fs, n); return -1; }
    int rc = 0;
    if (len && fwrite(data, 1, len, f) != len) rc = -1;
    if (rc == 0 && len && EVP_DigestUpdate(ctx, data, len) != 1) rc = -1;
    if (rc == 0 && digest_final(ctx, out_id) != 0) rc = -1;
    EVP_MD_CTX_free(ctx);
    if (rc == 0 && file_flush_sync(f) != 0) rc = -1;
    if (fclose(f) != 0) rc = -1;
    if (rc != 0) { QRX_UNLINK(tmp); release_reservation(fs, n); return -1; }
    return finalize_temp(fs, tmp, out_id, n, n);
}

int qrx_storage_fs_put_file(QrxStorageFs *fs, const char *source_path, char out_id[65]) {
    if (!fs || !source_path || !out_id) return -1;
    uint64_t source_size = 0;
    if (!path_is_regular(source_path, &source_size)) return -1;
    int rr = reserve_bytes(fs, source_size); if (rr != 0) return rr;
    FILE *in = fopen(source_path, "rb"); if (!in) { release_reservation(fs, source_size); return -1; }
    char tmp[PATH_MAX];
    if (random_temp_path(fs, tmp) != 0) { fclose(in); release_reservation(fs, source_size); return -1; }
    FILE *out = fopen(tmp, "wb"); if (!out) { fclose(in); release_reservation(fs, source_size); return -1; }
    EVP_MD_CTX *ctx = NULL;
    if (digest_init(&ctx) != 0) { fclose(in); fclose(out); QRX_UNLINK(tmp); release_reservation(fs, source_size); return -1; }
    unsigned char buf[1024 * 1024];
    uint64_t written = 0; int rc = 0;
    for (;;) {
        size_t got = fread(buf, 1, sizeof(buf), in);
        if (got) {
            if (fwrite(buf, 1, got, out) != got || EVP_DigestUpdate(ctx, buf, got) != 1) { rc = -1; break; }
            written += (uint64_t)got;
        }
        if (got < sizeof(buf)) { if (ferror(in)) rc = -1; break; }
    }
    if (rc == 0 && written != source_size) rc = -1;
    if (rc == 0 && digest_final(ctx, out_id) != 0) rc = -1;
    EVP_MD_CTX_free(ctx);
    fclose(in);
    if (rc == 0 && file_flush_sync(out) != 0) rc = -1;
    if (fclose(out) != 0) rc = -1;
    OPENSSL_cleanse(buf, sizeof(buf));
    if (rc != 0) { QRX_UNLINK(tmp); release_reservation(fs, source_size); return -1; }
    return finalize_temp(fs, tmp, out_id, source_size, written);
}

int qrx_storage_fs_has(QrxStorageFs *fs, const char *id) {
    if (!fs || !valid_object_id(id)) return 0;
    char path[PATH_MAX]; if (object_path(fs, id, path, 0) != 0) return 0;
    return path_is_regular(path, NULL);
}

int qrx_storage_fs_read(QrxStorageFs *fs, const char *id, unsigned char **out, size_t *out_len) {
    if (!fs || !out || !out_len || !valid_object_id(id)) return -1;
    *out = NULL; *out_len = 0;
    char path[PATH_MAX]; uint64_t n = 0;
    if (object_path(fs, id, path, 0) != 0 || !path_is_regular(path, &n) || n > SIZE_MAX) return -1;
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    unsigned char *buf = (unsigned char*)malloc((size_t)n + (n == 0));
    if (!buf) { fclose(f); return -1; }
    if (n && fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return -1; }
    fclose(f); *out = buf; *out_len = (size_t)n; return 0;
}

int qrx_storage_fs_read_range(QrxStorageFs *fs, const char *id, uint64_t offset, size_t length, unsigned char **out, size_t *out_len) {
    if (!fs || !out || !out_len || !valid_object_id(id)) return -1;
    *out = NULL; *out_len = 0;
    char path[PATH_MAX]; uint64_t n = 0;
    if (object_path(fs, id, path, 0) != 0 || !path_is_regular(path, &n)) return -1;
    if (offset > n) return -1;
    if (length == 0) { unsigned char *z=(unsigned char*)malloc(1); if(!z)return -1; *out=z; return 0; }
    if (offset == n) return -1;
    uint64_t avail=n-offset; size_t want=length; if ((uint64_t)want>avail) want=(size_t)avail;
    FILE *f=fopen(path,"rb"); if(!f)return -1;
    if(QRX_FSEEK(f,offset)!=0){fclose(f);return -1;}
    unsigned char *buf=(unsigned char*)malloc(want?want:1); if(!buf){fclose(f);return -1;}
    if(want && fread(buf,1,want,f)!=want){free(buf);fclose(f);return -1;}
    fclose(f); *out=buf; *out_len=want; return 0;
}

int qrx_storage_fs_get_file(QrxStorageFs *fs, const char *id, const char *dst) {
    if (!fs || !dst || !valid_object_id(id)) return -1;
    char src[PATH_MAX];
    if (object_path(fs, id, src, 0) != 0 || !path_is_regular(src, NULL)) return -1;
    char tmp[PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s.qrxpart", dst) >= (int)sizeof(tmp)) return -1;
    FILE *in = fopen(src, "rb"), *out = NULL;
    if (!in) return -1;
    out = fopen(tmp, "wb"); if (!out) { fclose(in); return -1; }
    unsigned char buf[1024 * 1024]; int rc = 0;
    for (;;) {
        size_t got = fread(buf, 1, sizeof(buf), in);
        if (got && fwrite(buf, 1, got, out) != got) { rc = -1; break; }
        if (got < sizeof(buf)) { if (ferror(in)) rc = -1; break; }
    }
    fclose(in);
    if (rc == 0 && file_flush_sync(out) != 0) rc = -1;
    if (fclose(out) != 0) rc = -1;
    OPENSSL_cleanse(buf, sizeof(buf));
    if (rc == 0 && atomic_replace(tmp, dst) != 0) rc = -1;
    if (rc != 0) QRX_UNLINK(tmp);
    return rc;
}

int qrx_storage_fs_delete(QrxStorageFs *fs, const char *id) {
    if (!fs || !valid_object_id(id)) return -1;
    char path[PATH_MAX]; uint64_t n = 0;
    if (object_path(fs, id, path, 0) != 0 || !path_is_regular(path, &n)) return -1;
    if (!CRYPTO_THREAD_write_lock(fs->lock)) return -1;
    int rc = QRX_UNLINK(path);
    if (rc == 0) {
        fs->used_bytes = fs->used_bytes >= n ? fs->used_bytes - n : 0;
        if (fs->object_count) fs->object_count--;
        rc = write_usage_locked(fs);
    }
    CRYPTO_THREAD_unlock(fs->lock);
    return rc == 0 ? 0 : -1;
}

int qrx_storage_fs_stats(QrxStorageFs *fs, QrxStorageFsStats *out) {
    if (!fs || !out || !CRYPTO_THREAD_read_lock(fs->lock)) return -1;
    memset(out, 0, sizeof(*out));
    out->max_usage_bytes = fs->max_usage_bytes;
    out->min_free_space_bytes = fs->min_free_space_bytes;
    out->used_bytes = fs->used_bytes;
    out->reserved_bytes = fs->reserved_bytes;
    out->object_count = fs->object_count;
    if (fs->max_usage_bytes > fs->used_bytes + fs->reserved_bytes)
        out->quota_available_bytes = fs->max_usage_bytes - fs->used_bytes - fs->reserved_bytes;
    else if (fs->max_usage_bytes == 0)
        out->quota_available_bytes = UINT64_MAX;
    filesystem_free(fs->root, &out->filesystem_free_bytes);
    CRYPTO_THREAD_unlock(fs->lock);
    return 0;
}

const char *qrx_storage_fs_backend_name(void) { return "filesystem"; }
