/*
 * QRX 0.0.9 Genesis hardening - fault-barrier semantics test.
 *
 * This test compiles the exact guard construct used in qrx.c (thread-local
 * jmp_buf, guarded die(), reentry refusal) in isolation, so the control flow
 * can be validated without the full OpenSSL/ML-DSA toolchain.
 *
 * It asserts the properties the P2P transaction ingress depends on:
 *   1. die() inside the guarded window returns a structured error, no exit().
 *   2. The failure reason reaches the caller.
 *   3. The guard is released again, so later valid input still works.
 *   4. Many consecutive malformed inputs never terminate the process.
 *   5. Guards do not nest (a nested jmp_buf would be corrupted).
 *   6. Concurrent worker threads keep independent guard state.
 *   7. Outside the guarded window die() keeps its fatal semantics.
 *
 * Build:  cc -O2 -pthread -o test_untrusted_guard_semantics \
 *            test_untrusted_guard_semantics.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

/* ---- construct mirrored from qrx.c ------------------------------------- */

#if defined(_MSC_VER)
#  define QRX_THREAD_LOCAL __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#  define QRX_THREAD_LOCAL _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
#  define QRX_THREAD_LOCAL __thread
#else
#  define QRX_THREAD_LOCAL
#endif

#define QRX_UNTRUSTED_REASON_MAX 256

static QRX_THREAD_LOCAL int     g_untrusted_guard_depth = 0;
static QRX_THREAD_LOCAL jmp_buf g_untrusted_guard_jmp;
static QRX_THREAD_LOCAL char    g_untrusted_guard_reason[QRX_UNTRUSTED_REASON_MAX];

static void die(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    if (g_untrusted_guard_depth > 0) {
        vsnprintf(g_untrusted_guard_reason, sizeof(g_untrusted_guard_reason), fmt, ap);
        va_end(ap);
        g_untrusted_guard_reason[sizeof(g_untrusted_guard_reason)-1] = 0;
        longjmp(g_untrusted_guard_jmp, 1);
    }
    vfprintf(stderr, fmt, ap); va_end(ap); fputc('\n', stderr); exit(1);
}

/* ---- stand-ins for the real validation tree ---------------------------- */

static int g_nested_attempted = 0;
static int g_nested_refused   = 0;

static int guarded_validate(const char *tx, char *err, size_t err_sz);

/* Deep call chain that dies, like verify_tx_text -> parse_ll_strict -> die. */
static void deep_level3(const char *tx) {
    if (!strcmp(tx, "bad_nonce"))  die("invalid nonce");
    if (!strcmp(tx, "bad_amount")) die("amount must be > 0");
    if (!strcmp(tx, "overflow"))   die("amount plus fee overflow");
    if (!strcmp(tx, "nested")) {
        char e[64];
        g_nested_attempted = 1;
        if (guarded_validate("bad_nonce", e, sizeof(e)) != 0 &&
            !strcmp(e, "validation reentry"))
            g_nested_refused = 1;
        die("nested path rejected");
    }
}
static void deep_level2(const char *tx) { deep_level3(tx); }
static void deep_level1(const char *tx) { deep_level2(tx); }

static int validate_stateful(const char *tx) {
    deep_level1(tx);
    return strcmp(tx, "valid") == 0 ? 0 : -1;
}

static int guarded_validate(const char *tx, char *err, size_t err_sz) {
    int rc;
    if (err && err_sz) err[0] = 0;
    if (g_untrusted_guard_depth > 0) {
        if (err && err_sz) snprintf(err, err_sz, "validation reentry");
        return -1;
    }
    g_untrusted_guard_reason[0] = 0;
    if (setjmp(g_untrusted_guard_jmp) != 0) {
        g_untrusted_guard_depth = 0;
        if (err && err_sz)
            snprintf(err, err_sz, "%s",
                     g_untrusted_guard_reason[0] ? g_untrusted_guard_reason : "invalid tx");
        return -1;
    }
    g_untrusted_guard_depth = 1;
    rc = validate_stateful(tx);
    g_untrusted_guard_depth = 0;
    if (rc != 0 && err && err_sz) snprintf(err, err_sz, "invalid tx");
    return rc;
}

/* ---- test harness ------------------------------------------------------ */

static int failures = 0;
static void check(int cond, const char *name) {
    printf("  [%s] %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

static void *worker(void *arg) {
    long id = (long)arg;
    char err[128];
    for (int i = 0; i < 2000; i++) {
        if (guarded_validate("bad_nonce", err, sizeof(err)) != 0 &&
            strcmp(err, "invalid nonce") == 0 &&
            g_untrusted_guard_depth == 0)
            continue;
        return (void*)1; /* guard state leaked across threads */
    }
    (void)id;
    return NULL;
}

int main(void) {
    char err[256];
    printf("QRX untrusted-input fault barrier semantics\n");
    printf("------------------------------------------\n");

    /* 1 + 2: guarded die() returns a structured error with its reason. */
    check(guarded_validate("bad_nonce", err, sizeof(err)) != 0, "malformed tx rejected, no exit()");
    check(strcmp(err, "invalid nonce") == 0, "failure reason propagated to caller");

    /* 3: guard released, valid input still accepted afterwards. */
    check(g_untrusted_guard_depth == 0, "guard released after failure");
    check(guarded_validate("valid", err, sizeof(err)) == 0, "valid tx still accepted after attack");

    /* 4: sustained malformed input keeps the process alive. */
    {
        const char *shapes[] = {"bad_nonce","bad_amount","overflow","garbage","", "valid"};
        int rejected = 0, accepted = 0;
        for (int i = 0; i < 100000; i++) {
            const char *tx = shapes[i % 6];
            if (guarded_validate(tx, err, sizeof(err)) == 0) accepted++; else rejected++;
        }
        check(rejected > 0 && accepted > 0, "100000 mixed inputs processed, process alive");
        check(g_untrusted_guard_depth == 0, "guard depth stable under load");
    }

    /* 5: nested guards refused. */
    guarded_validate("nested", err, sizeof(err));
    check(g_nested_attempted == 1, "nested guard attempt reached");
    check(g_nested_refused == 1, "nested guard refused (jmp_buf protected)");

    /* 6: per-thread guard isolation. */
    {
        pthread_t th[8]; long bad = 0;
        for (long i = 0; i < 8; i++) pthread_create(&th[i], NULL, worker, (void*)i);
        for (int i = 0; i < 8; i++) { void *r = NULL; pthread_join(th[i], &r); if (r) bad = 1; }
        check(bad == 0, "8 concurrent workers keep independent guard state");
    }

    /* 7: outside the guard die() must still be fatal (checked in a child). */
    {
        fflush(stdout); fflush(stderr);
        pid_t pid = fork();
        if (pid == 0) { die("fatal outside guard"); _exit(0); }
        int st = 0; waitpid(pid, &st, 0);
        check(WIFEXITED(st) && WEXITSTATUS(st) == 1, "die() still fatal outside guarded window");
    }

    printf("------------------------------------------\n");
    printf("%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASS", failures);
    return failures ? 1 : 0;
}
