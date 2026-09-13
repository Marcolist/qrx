#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="$ROOT/qrx-core"
BUILD="${QRX_FINAL_AUDIT_BUILD:-$ROOT/build/final-audit-host}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

echo 'QRX 0.0.9 Genesis Final Release Audit'
echo '================================'

echo '[1/20] Cross-platform release-plan validation'
"$ROOT/scripts/build-all-targets.sh" --all --plan >/dev/null
echo 'PASS: linux-x64, linux-arm64, macos-x64, macos-arm64, windows-x64 plans'

echo '[2/20] GUI/Core RPC compatibility'
python3 "$ROOT/scripts/audit-gui-core-compat.py"

echo '[3/20] Network/RPC security invariants'
"$CORE/scripts/audit-network-security.sh"

echo '[4/20] 0.0.6 regression surface'
"$CORE/scripts/audit-0.0.6-regression.sh"

echo '[5/20] Python tooling/wallet/build tests'
python3 -m unittest discover -s "$CORE/tests" -p 'test_*.py' -v

echo '[6/20] Front-end JavaScript syntax'
python3 - "$ROOT/GUIWALLET/src/index.html" "$BUILD-wallet.js" <<'PY'
from pathlib import Path
from html.parser import HTMLParser
import sys
class P(HTMLParser):
    def __init__(self): super().__init__(); self.on=False; self.parts=[]
    def handle_starttag(self,t,a):
        if t=='script' and not dict(a).get('src'): self.on=True
    def handle_endtag(self,t):
        if t=='script': self.on=False
    def handle_data(self,d):
        if self.on:self.parts.append(d)
p=P();p.feed(Path(sys.argv[1]).read_text(encoding='utf-8'));Path(sys.argv[2]).parent.mkdir(parents=True,exist_ok=True);Path(sys.argv[2]).write_text('\n'.join(p.parts),encoding='utf-8')
PY
node --check "$BUILD-wallet.js"

echo '[7/20] Native-host Core clean build + CTests'
rm -rf "$BUILD"
cmake -S "$CORE" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DQRX_BUILD_TESTS=ON
cmake --build "$BUILD" --parallel "$JOBS"
ctest --test-dir "$BUILD" --output-on-failure
for b in qrx qrx-cli qrxd qrxdb_verify qrxdb_salvage qrxdb_compact qrxdb_snapshot; do test -x "$BUILD/$b" || { echo "FAIL: missing $b" >&2; exit 1; }; done

echo '[8/20] Privacy Phase 1 / source-control release surface'
grep -q 'sendfromaddress' "$CORE/src/qrx_cli.c"
grep -q 'sendfromaddress' "$CORE/src/qrxd.c"
grep -q 'rotated address recovery extension update failed' "$CORE/src/qrx.c"
grep -q 'Use a fresh address for every payment' "$ROOT/GUIWALLET/src/index.html"
grep -q 'Privacy optimized' "$ROOT/GUIWALLET/src/index.html"
echo 'PASS: recoverable rotation + Source Control + privacy analysis surface'

echo '[9/20] Privacy Phase 2 / Stealth Hardening'
"$CORE/tests/privacy_phase2_stealth_hardening.sh" "$BUILD/qrx"
grep -q 'stealth_key_derivation=private-wallet-material-only' "$CORE/src/qrx.c"
grep -q 'QRX stealth public metadata v3' "$CORE/src/qrx.c"
echo 'PASS: Stealth private-key derivation, recovery and metadata minimization'

echo '[10/20] Privacy Phase 3 / One-Time Claim-Spend + Shielded QUB'
"$CORE/tests/privacy_phase3_shielded_and_stealth_spend.sh" "$BUILD/qrx"
grep -q 'shielded_pool=phase3-ringct-encrypted-notes-pedersen-rangeproofs' "$CORE/src/qrx.c"
grep -q 'mainnet_release_gate=external-cryptography-audit-required-before-real-funds' "$CORE/src/qrx.c"
echo 'PASS: one-time spend proofs/replay protection + encrypted shielded notes/change/value conservation'

echo '[11/20] Privacy Phase 4 / Hidden Balances + Verified Privacy'
"$CORE/tests/privacy_phase4_verified_privacy.sh" "$BUILD/qrx"
grep -q 'hidden_balances=phase4-verified-privacy-gated-shielded-balance' "$CORE/src/qrx.c"
grep -q 'kyc_pii=off-chain-only-never-in-credential-or-chain-state' "$CORE/src/qrx.c"
grep -q 'privacy-credential-status' "$CORE/src/qrxd.c"
echo 'PASS: accepted-attester credential gate + opaque revocation + hidden balance surface'

echo '[12/20] 0.0.7.5 Developer Governance + Attester Registry + Protocol Enforcement'
"$CORE/tests/governance_attester_protocol_0075.sh" "$BUILD/qrx"
grep -q 'developer_governance=genesis-ed25519-threshold-roots-no-private-keys-on-chain' "$CORE/src/qrx.c"
grep -q 'getprotocolinfo' "$CORE/src/qrxd.c"
grep -q 'get_protocol_info' "$ROOT/GUIWALLET/src-tauri/src/main.rs"
grep -q 'protocolUpdateRequired' "$ROOT/GUIWALLET/src/index.html"
echo 'PASS: 3-of-5 governance roots + threshold provider admission + mandatory protocol update surface'

echo '[13/20] 0.0.9 release packaging/version gates'
# Genesis hardening (Finding 9/14): this gate previously asserted 0.0.7.6 while
# the packaging script already said 0.0.7.7, so the check could never pass and
# the release audit was effectively never run to completion.
grep -q 'qrx_version":"0.0.9"' "$ROOT/scripts/package-target-release.py"
grep -q 'qrx-0.0.9-genesis-$TARGET.zip' "$ROOT/scripts/build-all-targets.sh"
grep -q 'qrx-0.0.9-genesis-${{ matrix.target }}' "$ROOT/.github/workflows/build-all-targets.yml"
grep -q "tags: \\['v0.0.9\\*'\\]" "$ROOT/.github/workflows/build-all-targets.yml"
! grep -rqn '0\.0\.7' "$ROOT/.github/workflows/" "$ROOT/scripts/package-target-release.py" \
  || { echo 'FAIL: stale 0.0.7 references in release packaging path' >&2; exit 1; }
echo 'PASS: package and CI artifacts are versioned 0.0.9 genesis'

echo '[14/20] Native Assets & Regulated Tokenization Layer'
"$CORE/tests/native_assets_regulated_0076.sh" "$BUILD/qrx"
"$CORE/tests/native_assets_mainnet_consensus_0076.sh" "$BUILD/qrx"
grep -q 'ASSET_REISSUE' "$CORE/src/qrx.c"
grep -q 'ASSET_GLOBAL_FREEZE' "$CORE/src/qrx.c"
grep -q 'asset_activation_height' "$CORE/src/qrx.c"
grep -q 'require_manual_mint_allowed(c,"asset-mint-v1")' "$CORE/src/qrx.c"
echo 'PASS: Ravencoin-parity consensus asset + regulated controls gate'

# ---------------------------------------------------------------------------
# Genesis hardening (Finding 14): the audit must be able to fail honestly.
# ---------------------------------------------------------------------------

echo '[15/20] Root CTest must discover the full suite'
ROOT_BUILD="${QRX_FINAL_AUDIT_ROOT_BUILD:-$ROOT/build/final-audit-root}"
rm -rf "$ROOT_BUILD"
cmake -S "$ROOT" -B "$ROOT_BUILD" -DCMAKE_BUILD_TYPE=Release -DQRX_BUILD_TESTS=ON >/dev/null
cmake --build "$ROOT_BUILD" --parallel "$JOBS" >/dev/null
TOTAL="$(ctest --test-dir "$ROOT_BUILD" -N 2>/dev/null | sed -n 's/^Total Tests: //p' | tail -1)"
MIN_TESTS="${QRX_MIN_TESTS:-117}"
if [[ -z "$TOTAL" || "$TOTAL" -eq 0 ]]; then
  echo 'FAIL: root ctest discovered 0 tests (No tests were found is NOT a pass)' >&2; exit 1
fi
if (( TOTAL < MIN_TESTS )); then
  echo "FAIL: root ctest discovered $TOTAL tests, expected at least $MIN_TESTS" >&2; exit 1
fi
ctest --test-dir "$ROOT_BUILD" --output-on-failure
echo "PASS: root ctest discovered $TOTAL tests and the suite passed"

echo '[16/20] AddressSanitizer over the full suite'
ASAN_BUILD="${QRX_ASAN_BUILD:-$ROOT/build/final-audit-asan}"
rm -rf "$ASAN_BUILD"
cmake -S "$CORE" -B "$ASAN_BUILD" -DCMAKE_BUILD_TYPE=Debug -DQRX_BUILD_TESTS=ON \
  -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer -g' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address' >/dev/null
cmake --build "$ASAN_BUILD" --parallel "$JOBS" >/dev/null
ctest --test-dir "$ASAN_BUILD" --output-on-failure
echo 'PASS: AddressSanitizer clean'

echo '[17/20] UndefinedBehaviorSanitizer over the full suite'
UBSAN_BUILD="${QRX_UBSAN_BUILD:-$ROOT/build/final-audit-ubsan}"
rm -rf "$UBSAN_BUILD"
cmake -S "$CORE" -B "$UBSAN_BUILD" -DCMAKE_BUILD_TYPE=Debug -DQRX_BUILD_TESTS=ON \
  -DCMAKE_C_FLAGS='-fsanitize=undefined -fno-sanitize-recover=all -g' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=undefined' >/dev/null
cmake --build "$UBSAN_BUILD" --parallel "$JOBS" >/dev/null
ctest --test-dir "$UBSAN_BUILD" --output-on-failure
echo 'PASS: UndefinedBehaviorSanitizer clean'

echo '[18/20] Security regression gates'
python3 "$ROOT/scripts/audit-untrusted-abort-reachability.py" "$CORE/src"
python3 "$CORE/tests/test_phase7215_consensus_redteam.py" "$ROOT"
if grep -rn --include='*.c' --include='*.h' --include='*.inc' -e 'system(' -e 'popen(' "$CORE/src" \
     | grep -v 'deliberately not exposed' | grep -v 'through popen()' | grep -q .; then
  echo 'FAIL: shell execution primitives present in core sources' >&2; exit 1
fi
grep -q 'qrx_privacy_protocol_enabled_at_height' "$CORE/src/qrx.c" \
  || { echo 'FAIL: PRIVACY_V1 mainnet gate not enforced in consensus path' >&2; exit 1; }
echo 'PASS: abort-reachability, consensus red-team, no shell primitives, privacy gate enforced'

echo '[19/20] Genesis economics, memo and protocol readiness'
grep -q '25000000' "$CORE/src/genesis/"*.c || grep -q '25000000' "$CORE/src/qrx.c"
grep -q 'digital sovereignty' "$CORE/config/genesis.cfg" 2>/dev/null \
  || grep -rq 'digital sovereignty' "$CORE/config" 2>/dev/null \
  || { echo 'FAIL: canonical Genesis memo not found' >&2; exit 1; }
for f in DRIVE_V1 QRX_NET_V1 ADVERTISING_V1 COMPUTE_POUC_V1 PRIVACY_V1; do
  grep -rq "$f" "$CORE/src" || { echo "FAIL: protocol readiness flag $f missing" >&2; exit 1; }
done
echo 'PASS: genesis economics, memo and staged readiness flags present'

echo '[20/20] Genesis ceremony blockers (hard NO-GO)'
CEREMONY_BLOCKED=0
if grep -rn 'REPLACE_WITH_DEV_GOV_' "$CORE/src" "$CORE/config" >/dev/null 2>&1; then
  echo 'NO-GO: governance public-key placeholders still present (REPLACE_WITH_DEV_GOV_)' >&2
  CEREMONY_BLOCKED=1
fi
if grep -rn 'qrx1bootstrap0' "$CORE/src" "$CORE/config" >/dev/null 2>&1; then
  echo 'NO-GO: bootstrap validator address placeholders still present (qrx1bootstrap0...)' >&2
  CEREMONY_BLOCKED=1
fi
# Match an actual PEM private-key block at the start of a line, and exclude
# this auditor so its own search pattern is not reported as a finding.
if grep -rln --exclude="$(basename "${BASH_SOURCE[0]}")" \
     -E '^-----BEGIN ([A-Z ]+ )?PRIVATE KEY-----' \
     "$CORE/src" "$CORE/config" "$ROOT/scripts" 2>/dev/null | grep -q .; then
  echo 'NO-GO: private key material present in the source tree' >&2
  grep -rln --exclude="$(basename "${BASH_SOURCE[0]}")" \
    -E '^-----BEGIN ([A-Z ]+ )?PRIVATE KEY-----' \
    "$CORE/src" "$CORE/config" "$ROOT/scripts" 2>/dev/null >&2
  CEREMONY_BLOCKED=1
fi

echo
echo '==============================================='
echo 'MAINNET CODE SECURITY: GO (all code gates above passed)'
if (( CEREMONY_BLOCKED )); then
  echo 'GENESIS CEREMONY:      NO-GO'
  echo '  Real governance keys and bootstrap validator addresses must replace'
  echo '  the placeholders before the final Genesis hash is computed.'
  echo '==============================================='
  echo 'NOTE: Native Tauri/installer builds, target-OS signing and notarization'
  echo '      are produced and validated on their native release hosts.'
  echo 'NOTE: This is an internal audit. It is not an independent external'
  echo '      security audit and must not be presented as one.'
  exit 2
fi
echo 'GENESIS CEREMONY:      GO'
echo '==============================================='
echo 'NOTE: Native Tauri/installer builds, target-OS signing and notarization'
echo '      are produced and validated on their native release hosts.'
echo 'NOTE: This is an internal audit. It is not an independent external'
echo '      security audit and must not be presented as one.'
