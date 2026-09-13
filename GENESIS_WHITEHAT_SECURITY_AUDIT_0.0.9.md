# GENESIS WHITEHAT SECURITY AUDIT 0.0.9

**Scope:** QRX Core 0.0.9 Genesis, security / Mainnet hardening only.
**Baseline archive:** `qrx-core-0.0.9-genesis-protocol-readiness-source.zip`
(MANIFEST.sha256 verified: 1262 entries, 0 mismatches).
**Video / upscaling work:** explicitly out of scope, not touched.

> **This is an internal whitehat / red-team audit.**
> It is **not** an independent external security audit and must never be
> presented as one. No external cryptography review has taken place.

---

## 1. Verdict

| Gate | Status |
|------|--------|
| **MAINNET CODE SECURITY** | **NO-GO (pending build + Phase 185 run)** |
| **GENESIS CEREMONY** | **NO-GO** |

**Why code security is not GO yet.** The remote malformed-transaction DoS is
fixed in source and statically proven unreachable, but the fixed tree has
**not been compiled and Phase 185 has not been executed**. The audit
environment has OpenSSL 3.0.13 (ML-DSA requires 3.5+), no CMake and no
network access, so the Core cannot be built here. The project's own rule
applies: no Mainnet GO until the die()/exit() DoS is disproven **with a
running-node attack test**. That test is written and registered; it must be
run on a real build host.

**Why the ceremony is NO-GO.** Independent of code state, the source still
contains `DEV_GOV_1..5` governance placeholders and placeholder bootstrap
validator addresses. Real keys and real validator addresses must be
installed before the final Genesis hash is computed. This is expected and
does not block code-security sign-off.

---

## 2. Findings

| ID | Severity | Finding | Attack | Fix | Regression test | Status |
|----|----------|---------|--------|-----|-----------------|--------|
| WH-01 | **CRITICAL** | `verify_tx_text()` and its whole validation subtree terminate the process via `die()` → `exit(1)` | Any unauthenticated peer completes a self-signed HELLO, sends one malformed TX, and the validator exits. `node_handle_client()` runs **inline in the daemon process** (no fork, no thread), so the entire node dies, not a worker | Thread-local untrusted-input fault barrier in `die()`; layered ingress `verify_tx_text_untrusted()` (size cap → leak-free stateless crypto gate → stateful validation under barrier) | `genesis_phase184`, `genesis_phase185`, `genesis_phase186` | **FIXED IN SOURCE, UNVERIFIED AT RUNTIME** |
| WH-02 | **HIGH** | 59 distinct process-fatal call paths reachable from network ingress, not just the known one | e.g. `node_handle_client → verify_tx_text → parse_nonnegative_ll_strict → parse_ll_strict → die`; `… → validate_trade_fields_common → native_order_lock_requirements → die`; `generals_relay_store → hash_primary_hex → sha3_512_hex → die` | Covered by the WH-01 barrier; proven by static reachability audit (59 → 0) | `genesis_phase186` | **FIXED IN SOURCE** |
| WH-03 | **HIGH** | `build_hello_message()` calls `die()` on purely local conditions but is **remote-triggerable** | Peer sends AURA gossip → `node_handle_client → aura_gossip_fanout → aura_gossip_push_to_peer → build_hello_message`. On a headless validator `get_passphrase()` fails → daemon exits | Rewritten to return `-1` with single cleanup path, all pointers NULL-initialised; `*out_msg` left NULL | `genesis_phase186` | **FIXED IN SOURCE** |
| WH-04 | **MEDIUM** | All 6 callers of `build_hello_message()` ignored the return value and would pass `NULL` to `send_framed()` | NULL dereference once WH-03 returns an error instead of exiting | 4 call sites hardened to check result and NULL; 2 already checked `!hello` | `genesis_phase186` | **FIXED IN SOURCE** |
| WH-05 | **MEDIUM** | `verify_hello_msg()` leaked ~12–18 heap allocations on every early rejection | Peer repeats malformed HELLO handshakes to grow daemon RSS without ever authenticating | Restructured to a single `done:` cleanup path, all pointers NULL-initialised | *(needs ASan run)* | **FIXED IN SOURCE** |
| WH-06 | **MEDIUM** | `validate_privacy_consensus_tx()` returns `int` but calls `parse_nonnegative_ll_strict()`, which dies | A `PRIVACY_*` transaction with a non-numeric `amount` terminates the node | Covered by the WH-01 barrier | `genesis_phase185` | **FIXED IN SOURCE** |
| WH-07 | **LOW** | Root `CMakeLists.txt` lacked `enable_testing()`; root `ctest` reported *No tests were found* and exited 0 (Finding 8) | CI reports green while the entire Core suite never runs | `enable_testing()` added at root behind `QRX_BUILD_TESTS` | — | **FIXED** |
| WH-08 | **INFO / RESIDUAL** | Fault barrier unwinds with `longjmp`, leaking heap allocated inside the stateful path (~10–20 KB per rejected *signed* transaction) | Attacker with any keypair signs garbage repeatedly to grow RSS. Bounded by rate limiting (12/60 s) and peer ban (score 100, +30 per bad TX) | Not fully fixed. Mitigated by the stateless pre-gate: unsigned garbage never reaches the barrier | *(needs ASan/soak)* | **OPEN — see §5** |

### 2.1 Second audit pass

| ID | Severity | Finding | Attack | Fix | Verified | Status |
|----|----------|---------|--------|-----|----------|--------|
| WH-09 | **HIGH (local)** | Shell injection in `mempool-status` and `reindex-state` (original Finding 7) | Command lines were built as `find '<node_dir>/mempool' ...` and `ls -1 '<chain_dir>/blocks'/*.block` and run through `popen()`. A data-directory path containing a single quote escapes the quoting. **Proven with a working PoC against the built binary**: a node dir named `x'; touch /tmp/PWNED; echo '` created the file | Replaced both with `qrx_dirlist_regular_files()`: native `opendir`/`readdir` + `lstat`/`S_ISREG` (symlinks ignored), `FILE_ATTRIBUTE_REPARSE_POINT` skipped on Windows, deterministic sort. `popen_qrx`/`pclose_qrx` macros removed entirely | **Exploit re-run fails after fix**; output equivalence checked (3 regular files, symlink + subdir excluded) | **FIXED & VERIFIED** |
| WH-10 | **CRITICAL** | Complete RPC authentication bypass (original Finding 2) | `qrxd.c` RPC loop: any input not starting with `POST`/`GET`/`OPTIONS` fell through to `handle_command()` with **no token, no HTTP auth, no host/origin check**. With `--allow-remote-rpc` a remote attacker sends plaintext `stop\n` and bypasses the whole HTTP auth stack | Legacy plaintext restricted to loopback peers and refused on Mainnet; `accept()` now captures the peer address to make that check possible | compiles clean; **runtime test outstanding** | **FIXED IN SOURCE** |
| WH-11 | **HIGH** | Mainnet allowed non-loopback RPC bind | `--allow-remote-rpc` exposed wallet RPC directly on Mainnet | `validate_rpc_exposure()` now hard-refuses any non-loopback bind when the network is Mainnet; remote admin must use SSH tunnel/VPN | compiles clean | **FIXED IN SOURCE** |
| WH-12 | **HIGH** | RPC Slowloris (original Finding 3) | No timeout was set on the accepted RPC socket, and the RPC accept loop is **single-threaded**. One peer sending a single byte and stalling pins the entire RPC interface indefinitely | `qrx_rpc_set_socket_timeouts()` sets `SO_RCVTIMEO`/`SO_SNDTIMEO` (10 s) on every accepted socket | compiles clean; **runtime test outstanding** | **FIXED IN SOURCE** |
| WH-13 | **MEDIUM** | Oversized request body silently truncated | A `Content-Length` larger than the buffer set `expected_total = sizeof(cmd)` and the truncated request was still parsed | Now answered with HTTP 413 and the connection closed | compiles clean | **FIXED IN SOURCE** |
| WH-14 | **HIGH (local)** | Free of indeterminate stack pointers in the privacy spend path (original Finding 6) | `privacy_build_spend_payload()` declared `char *ranges[2]={0}` **after** two `goto fail` statements in the same statement sequence. Reaching `fail:` from either jump executed `for(i<2) free(ranges[i])` on never-initialised stack pointers — an arbitrary-pointer free | All declarations hoisted above every jump; `outs`/`ow` zeroed as well | compiles clean; **needs ASan** | **FIXED IN SOURCE** |

Swept for the same declaration-after-goto pattern across `privacy/` and
`velocity/`. One further candidate (`buyer` in `qrx_btc_spv.inc`) was checked
by hand and is **not** a defect: those pointers are freed locally before each
jump and the `fail:` label does not touch them.

Static primitive sweep of `qrx-core/src`: `system(` 0, `popen(` 0 (was 2),
`mktemp(`/`tmpnam(`/`gets(`/`alloca(` 0. The 23 apparent `gets(` hits are all
`fgets(`.

### 2.2 Third audit pass — AURA supply chain and privacy gate

| ID | Severity | Finding | Attack | Fix | Verified | Status |
|----|----------|---------|--------|-----|----------|--------|
| WH-15 | **CRITICAL** | Hugging Face bearer token leaked across redirects (original Finding 4) | `qrx_aura_model_origin.c` attached the token with `CURLOPT_HTTPHEADER` while `CURLOPT_FOLLOWLOCATION` was on. libcurl forwards **custom headers to any host** across redirects, so a hostile or compromised model origin redirecting to an attacker host receives the gated-repository credential | Token now supplied through libcurl's own bearer auth (`CURLOPT_HTTPAUTH`/`CURLOPT_XOAUTH2_BEARER`) with `CURLOPT_UNRESTRICTED_AUTH=0`, so libcurl drops it on a host change. Builds with libcurl too old for `CURLAUTH_BEARER` refuse the gated fetch instead of leaking | syntax-checked with a libcurl stub; **not compiled against real libcurl** | **FIXED IN SOURCE** |
| WH-16 | **HIGH** | HTTPS→HTTP redirect downgrade in model origin and runtime delivery (original Finding 4) | `CURLOPT_FOLLOWLOCATION` was set without `CURLOPT_REDIR_PROTOCOLS`, so a redirect could drop a signed model/runtime download to plain HTTP. Redirects were unbounded and TLS peer/host verification relied on libcurl defaults | HTTPS-only for request **and** every redirect hop, `MAXREDIRS 5`, explicit `SSL_VERIFYPEER=1`, `SSL_VERIFYHOST=2`, minimum TLS 1.2, origin URL must start with `https://` | syntax-checked with stub | **FIXED IN SOURCE** |
| WH-17 | **CRITICAL** | Privacy Mainnet gate was documentation only (original Finding 5) | `external-cryptography-audit-required-before-real-funds` existed **solely as a `printf` string**. Nothing in consensus prevented `PRIVACY_SHIELD` / `PRIVACY_TRANSFER` / `PRIVACY_UNSHIELD` from validating on Mainnet: unaudited shielded-value cryptography would have governed real funds at Genesis | Enforced gate: new `PRIVACY_V1` feature flag, `qrx_privacy_activation_height()` and `qrx_privacy_protocol_enabled_at_height()` mirroring the existing staged-activation framework. Shielded value movement is fail-closed on Mainnet until a governance-committed activation height. `PRIVACY_GOVERNANCE` and attester registration/rotation/revocation stay available as preflight. Alpha/testnet/regtest unchanged | compiles clean (all 114 TUs) | **FIXED IN SOURCE** |

WH-17 is the most consequential finding of the whole audit for Genesis: it is
the difference between "privacy is disabled on Mainnet" being a promise and
being a consensus rule.

### 2.3 Fourth pass — build/CI integrity

| ID | Severity | Finding | Impact | Fix | Verified | Status |
|----|----------|---------|--------|-----|----------|--------|
| WH-18 | **MEDIUM** | Release packaging still on 0.0.7 (Finding 9) | CI released artifacts named `qrx-0.0.7.7-*` from tag `v0.0.7*`. Worse, `final-release-audit.sh` asserted `qrx_version":"0.0.7.6"` while the packaging script said `0.0.7.7` — **that gate could never pass**, so the final release audit was never run to completion | All release paths moved to `0.0.9-genesis`; the audit now also hard-fails on any residual `0.0.7` string in the packaging path | grep sweep clean | **FIXED** |
| WH-19 | **LOW** | Non-reproducible wallet dependency install (Finding 10) | `npm install` can resolve newer transitive versions than the audited tree | `npm ci` when a lock file exists; otherwise an explicit warning that the build is not reproducible. No lock file is committed today, so the warning fires | `bash -n` clean | **FIXED** |
| WH-20 | **MEDIUM** | Readiness RPC unreachable from qrx-cli (Finding 11) | `qrx-cli` dispatch is an explicit allowlist ending in `usage()`. Neither `getdriveactivationreadiness` nor `getprotocolreadiness` was in it. The Tauri command `protocol_activation_readiness` shells out to exactly that CLI command, so **the GUI's staged-activation readiness display was broken end to end** | Both commands added to dispatch and usage | compiles clean | **FIXED** |
| WH-21 | **MEDIUM** | Consensus red-team test was brittle *and already red* (Finding 12) | The test matched minified C such as `validator_is_safely_paused(chain_dir,validator))continue`, but the real code has spaces. **The shipped 0.0.9 archive fails this test**, so the consensus red-team gate the release audit claims to run was not passing | Rewritten to semantic checks: whitespace-normalised guard strings, function-scoped call assertions, brace-matched bodies, declaration-vs-definition aware | passes on hardened **and** original tree; **fails on both mutation tests** (safe-pause removed from snapshot, and from delegation) | **FIXED & MUTATION-TESTED** |
| WH-22 | **LOW** | 0.0.6 regression audit expected deleted build scripts (Finding 13) | Audit failed at `scripts/build-linux-x64-static.sh`, which was consolidated into `build-linux-static.sh` | Updated to the current build structure and strengthened: it now asserts each of the five 0.0.6 release targets is still known to the unified builder, and that a pinned static OpenSSL is still built from source. The 59-command legacy inventory check is untouched | audit runs green | **FIXED** |
| WH-23 | **MEDIUM** | Final release audit could not fail honestly (Finding 14) | 14 steps, 0.0.7.6-era, no sanitizers, no ctest-count floor, no placeholder gate | Rebuilt as 20 steps: root CTest must discover ≥117 tests (0 is a hard fail), full ASan and UBSan runs, abort-reachability + consensus red-team + no-shell-primitives + privacy-gate assertions, genesis economics/memo/readiness flags, and a hard NO-GO on `REPLACE_WITH_DEV_GOV_`, `qrx1bootstrap0...` and real PEM private keys. Ends with an explicit MAINNET CODE SECURITY / GENESIS CEREMONY verdict and exit code 2 on ceremony block | `bash -n` clean; blocker logic tested in isolation. **Caught a false positive in my own check**: scanning `$ROOT/scripts` matched the auditor's own grep pattern, which would have reported NO-GO on a clean tree. Narrowed to real PEM headers with self-exclusion | **FIXED** |

### 2.4 QRX Upscaler app

Added to the Apps launchpad as a real tile opening a dedicated `view-upscaler`
panel. It is deliberately **not** a fake working upscaler: the panel states
that no upscaling or video code ships in 0.0.9, describes local vs accelerated
mode, and wires the live `protocol_activation_readiness('COMPUTE_POUC_V1')`
call so the gating is visible rather than asserted. Documentation-gate mapping
added; the gate now passes with 21 GUI views. GUI JavaScript passes
`node --check`.

### 2.5 Findings from the original brief not yet addressed

All numbered findings from the original brief are now addressed. What remains
is runtime verification, not source work: the fixed tree must be built and the
full suite, the sanitizers and Phase 185 must be executed on a real host.

---

## 3. Evidence

### 3.1 Why WH-01 is critical rather than a worker crash

`qrx-core/src/qrx.c`, node accept loop:

```c
struct sockaddr_in cli; socklen_t clilen = sizeof(cli);
int fd = accept(s, (struct sockaddr*)&cli, &clilen);
...
if(!storage_conn){ node_handle_client(fd, node_dir); qrx_close_socket(fd); }
```

`node_handle_client()` is called directly in the daemon's main loop. There is
no `fork()` and no per-connection thread, so `exit(1)` inside validation ends
the whole validator process.

`die()` before the fix:

```c
static void die(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap);
    va_end(ap); fputc('\n', stderr); exit(1);
}
```

`verify_tx_text()` alone contained **45** `die()` calls; the sub-validators
`validate_agent_fields_common` (9), `validate_trade_fields_common` (21),
`validate_gateway_management_tx` (6), `validate_execution_report_tx` (13) are
declared `void` and can only signal failure by dying. The `.inc` translation
units add more: `qrx_privacy_consensus.inc` (35), `qrx_crosschain.inc` (30),
`qrx_btc_spv.inc` (25).

### 3.2 Authentication does not protect the path

`verify_hello_msg()` verifies the network binding and that the HELLO
signature matches the presented public key. There is **no allowlist**: any
self-generated Ed25519 keypair passes. The transaction path is therefore
reachable by an unauthenticated attacker.

### 3.3 Static reachability, before and after

`scripts/audit-untrusted-abort-reachability.py` walks the call graph of
`qrx.c` plus its `.inc` units from the ingress entry points and reports every
path to `die`/`exit`/`abort`/`assert`.

```
ORIGINAL TREE   RESULT: FAIL - 59 process-fatal path(s) reachable
HARDENED TREE   RESULT: PASS - none reachable
```

The control run against the unmodified archive is important: it demonstrates
the checker actually detects the known vulnerability rather than passing
vacuously.

---

## 4. Changes made

| File | Change |
|------|--------|
| `qrx-core/src/qrx.c` | `#include <setjmp.h>`; `QRX_THREAD_LOCAL` macro; thread-local guard state; guarded `die()` |
| `qrx-core/src/qrx.c` | New `verify_tx_text_untrusted()` with size cap, stateless gate, barrier |
| `qrx-core/src/qrx.c` | P2P TX ingress and `generals_relay_store()` rewired to the guarded entry point; oversized-TX rejection added |
| `qrx-core/src/qrx.c` | `build_hello_message()` rewritten (error return, single cleanup, NULL-init) |
| `qrx-core/src/qrx.c` | `verify_hello_msg()` rewritten (single cleanup path, no leaks) |
| `qrx-core/src/qrx.c` | 4 `build_hello_message()` call sites hardened |
| `qrx-core/CMakeLists.txt` | Registered phase 184/185/186 tests, 900 s timeout on 185 |
| `CMakeLists.txt` (root) | `enable_testing()` under `QRX_BUILD_TESTS` |
| `qrx-core/tests/test_untrusted_guard_semantics.c` | New |
| `qrx-core/tests/genesis_phase185_remote_malformed_tx_survival.py` | New |
| `scripts/audit-untrusted-abort-reachability.py` | New |

The guard is **thread-local by necessity**: `qrx_velocity_parallel_verify()`
dispatches validation callbacks to worker threads, and a process-global
`jmp_buf` would be corrupted by concurrent validation.

Outside the guarded window `die()` keeps its original fatal behaviour, so CLI
commands still exit non-zero on misuse.

---

## 5. Residual risk (WH-08), stated plainly

The barrier converts a crash into a structured error by unwinding with
`longjmp`. Allocations made inside the stateful validation path are not
freed during that unwind. Per rejected signed transaction this is roughly
10–20 KB.

This is a deliberate, bounded trade: an instant remote kill becomes a slow,
rate-limited leak. It is mitigated but not eliminated:

* the leak-free stateless gate rejects all unsigned/wrongly signed input
  before the barrier is entered, so the attacker must actually sign each
  transaction;
* per-IP rate limiting (`RATE_MAX_MSGS` 12 / `RATE_WINDOW_SECS` 60) and peer
  banning (`BAN_THRESHOLD` 100, +30 per invalid TX) cap the rate from any one
  source.

**Recommended follow-up before heavy Mainnet load:** convert
`verify_tx_text()` and its sub-validators to structured error returns
incrementally, function by function, each step compiled and ASan-tested. This
was deliberately **not** attempted blind here: an untested mass refactor of
~200 error paths in security-critical C risks introducing use-after-free or
double-free, which would be worse than the DoS it replaces.

---

## 6. What must be run before any GO

On a real build host with OpenSSL ≥ 3.5:

```bash
cmake -S . -B build-tests -DQRX_BUILD_TESTS=ON
cmake --build build-tests -j
ctest --test-dir build-tests --output-on-failure
```

Required outcomes:

1. Clean build with no new warnings in the changed functions.
2. `ctest` total **≥ 119** (116 baseline + phases 184/185/186) and **all PASS**.
   A total of 0 is a hard fail.
3. `genesis_phase185_remote_malformed_tx_survival` **PASS** — this is the
   Mainnet blocker.
4. ASan and UBSan builds over the full suite, specifically to quantify
   WH-08 and confirm WH-03/WH-04/WH-05 introduced no double-free.
5. `genesis_phase186` PASS in CI, so the reachability property cannot silently
   regress.

Only after 1–5 pass can **MAINNET CODE SECURITY: GO** be stated honestly.

---

## 7. Unchanged by design

* Activation architecture, status model and not-before target dates.
* 3-of-5 governance threshold and Governance Vault semantics.
* Tokenomics: 0.25 QUB / 10 s, 25,000,000 atoms; service layers remain
  escrow/fee funded and mint nothing.
* Privacy remains fail-closed on Mainnet.
* No video / upscaling code touched.
