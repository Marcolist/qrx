#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="$ROOT/qrx-core"
BUILD="${QRX_FINAL_AUDIT_BUILD:-$ROOT/build/final-audit-host}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

echo 'QRX 0.0.7.6 Final Release Audit'
echo '================================'

echo '[1/14] Cross-platform release-plan validation'
"$ROOT/scripts/build-all-targets.sh" --all --plan >/dev/null
echo 'PASS: linux-x64, linux-arm64, macos-x64, macos-arm64, windows-x64 plans'

echo '[2/14] GUI/Core RPC compatibility'
python3 "$ROOT/scripts/audit-gui-core-compat.py"

echo '[3/14] Network/RPC security invariants'
"$CORE/scripts/audit-network-security.sh"

echo '[4/14] 0.0.6 regression surface'
"$CORE/scripts/audit-0.0.6-regression.sh"

echo '[5/14] Python tooling/wallet/build tests'
python3 -m unittest discover -s "$CORE/tests" -p 'test_*.py' -v

echo '[6/14] Front-end JavaScript syntax'
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

echo '[7/14] Native-host Core clean build + CTests'
rm -rf "$BUILD"
cmake -S "$CORE" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DQRX_BUILD_TESTS=ON
cmake --build "$BUILD" --parallel "$JOBS"
ctest --test-dir "$BUILD" --output-on-failure
for b in qrx qrx-cli qrxd qrxdb_verify qrxdb_salvage qrxdb_compact qrxdb_snapshot; do test -x "$BUILD/$b" || { echo "FAIL: missing $b" >&2; exit 1; }; done

echo '[8/14] Privacy Phase 1 / source-control release surface'
grep -q 'sendfromaddress' "$CORE/src/qrx_cli.c"
grep -q 'sendfromaddress' "$CORE/src/qrxd.c"
grep -q 'rotated address recovery extension update failed' "$CORE/src/qrx.c"
grep -q 'Use a fresh address for every payment' "$ROOT/GUIWALLET/src/index.html"
grep -q 'Privacy optimized' "$ROOT/GUIWALLET/src/index.html"
echo 'PASS: recoverable rotation + Source Control + privacy analysis surface'

echo '[9/14] Privacy Phase 2 / Stealth Hardening'
"$CORE/tests/privacy_phase2_stealth_hardening.sh" "$BUILD/qrx"
grep -q 'stealth_key_derivation=private-wallet-material-only' "$CORE/src/qrx.c"
grep -q 'QRX stealth public metadata v3' "$CORE/src/qrx.c"
echo 'PASS: Stealth private-key derivation, recovery and metadata minimization'

echo '[10/14] Privacy Phase 3 / One-Time Claim-Spend + Shielded QUB'
"$CORE/tests/privacy_phase3_shielded_and_stealth_spend.sh" "$BUILD/qrx"
grep -q 'shielded_pool=phase3-ringct-encrypted-notes-pedersen-rangeproofs' "$CORE/src/qrx.c"
grep -q 'mainnet_release_gate=external-cryptography-audit-required-before-real-funds' "$CORE/src/qrx.c"
echo 'PASS: one-time spend proofs/replay protection + encrypted shielded notes/change/value conservation'

echo '[11/14] Privacy Phase 4 / Hidden Balances + Verified Privacy'
"$CORE/tests/privacy_phase4_verified_privacy.sh" "$BUILD/qrx"
grep -q 'hidden_balances=phase4-verified-privacy-gated-shielded-balance' "$CORE/src/qrx.c"
grep -q 'kyc_pii=off-chain-only-never-in-credential-or-chain-state' "$CORE/src/qrx.c"
grep -q 'privacy-credential-status' "$CORE/src/qrxd.c"
echo 'PASS: accepted-attester credential gate + opaque revocation + hidden balance surface'

echo '[12/14] 0.0.7.5 Developer Governance + Attester Registry + Protocol Enforcement'
"$CORE/tests/governance_attester_protocol_0075.sh" "$BUILD/qrx"
grep -q 'developer_governance=genesis-ed25519-threshold-roots-no-private-keys-on-chain' "$CORE/src/qrx.c"
grep -q 'getprotocolinfo' "$CORE/src/qrxd.c"
grep -q 'get_protocol_info' "$ROOT/GUIWALLET/src-tauri/src/main.rs"
grep -q 'protocolUpdateRequired' "$ROOT/GUIWALLET/src/index.html"
echo 'PASS: 3-of-5 governance roots + threshold provider admission + mandatory protocol update surface'

echo '[13/14] 0.0.7.6 release packaging/version gates'
grep -q 'qrx_version":"0.0.7.6"' "$ROOT/scripts/package-target-release.py"
grep -q 'qrx-0.0.7.6-\$TARGET.zip' "$ROOT/scripts/build-all-targets.sh"
grep -q 'qrx-0.0.7.6-${{ matrix.target }}' "$ROOT/.github/workflows/build-all-targets.yml"
echo 'PASS: package and CI artifacts are versioned 0.0.7.6'

echo '[14/14] 0.0.7.6 Native Assets & Regulated Tokenization Layer'
"$CORE/tests/native_assets_regulated_0076.sh" "$BUILD/qrx"
"$CORE/tests/native_assets_mainnet_consensus_0076.sh" "$BUILD/qrx"
grep -q 'ASSET_REISSUE' "$CORE/src/qrx.c"
grep -q 'ASSET_GLOBAL_FREEZE' "$CORE/src/qrx.c"
grep -q 'asset_activation_height' "$CORE/src/qrx.c"
grep -q 'require_manual_mint_allowed(c,"asset-mint-v1")' "$CORE/src/qrx.c"
echo 'PASS: 0.0.7.6 Ravencoin-parity consensus asset + regulated controls gate'
echo 'RESULT: QRX 0.0.7.6 FINAL RELEASE AUDIT PASSED ON THIS NATIVE HOST'
echo 'NOTE: Full Tauri/installer builds for all five targets are executed by the native GitHub Actions matrix.'
