#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
QRXD_SRC="$ROOT/qrx-core/src/qrxd.c"
GUI_SRC="$ROOT/GUIWALLET/src-tauri/src/main.rs"
BTC_SRC="$ROOT/GUIWALLET/btc-wallet-service/src/main.rs"

fail(){ echo "SECURITY AUDIT: FAIL: $*" >&2; exit 1; }
pass(){ echo "SECURITY AUDIT: PASS: $*"; }

grep -q 'snprintf(g_rpc_bind, sizeof(g_rpc_bind), "127.0.0.1")' "$QRXD_SRC" || fail "qrxd default RPC bind is not loopback"
grep -q 'validate_rpc_exposure' "$QRXD_SRC" || fail "remote RPC guard missing"
grep -q -- '--allow-remote-rpc' "$QRXD_SRC" || fail "explicit remote RPC opt-in missing"
grep -q 'remote RPC requires BOTH --rpc-user and --rpc-password' "$QRXD_SRC" || fail "remote RPC credential guard missing"
grep -q '127.0.0.1:{}' "$GUI_SRC" || fail "GUI does not explicitly pin qrxd RPC to loopback"
if grep -Eq 'TcpListener|TcpListener::bind|hyper::Server|axum::serve|warp::serve' "$BTC_SRC"; then
  fail "BTC sidecar unexpectedly exposes a listener; inspect manually"
fi
pass "Core RPC defaults to IPv4 loopback"
pass "Non-loopback RPC requires explicit opt-in plus credentials"
pass "GUI-managed qrxd explicitly binds RPC to loopback"
pass "BTC wallet sidecar is stdin/stdout only; no TCP server listener"

if command -v ss >/dev/null 2>&1; then
  echo
  echo "Live socket snapshot (Linux):"
  ss -lntp 2>/dev/null | grep -E '(:37660|:37661|:37662|:37663|qrxd|qrx-btc)' || true
  if ss -lnt 2>/dev/null | grep -E '(^|[[:space:]])(0\.0\.0\.0|\*):3766[0-3][[:space:]]' >/dev/null; then
    fail "a QRX RPC port is currently listening on all IPv4 interfaces"
  fi
  pass "no live QRX RPC listener detected on 0.0.0.0"
elif command -v lsof >/dev/null 2>&1; then
  echo
  echo "Live socket snapshot (macOS/Unix):"
  lsof -nP -iTCP -sTCP:LISTEN 2>/dev/null | grep -E 'qrxd|qrx-btc|:3766[0-3]' || true
else
  echo "SECURITY AUDIT: NOTE: ss/lsof unavailable; source-level checks completed only."
fi

cat <<'MSG'

Expected design:
  Wallet/Core RPC: 127.0.0.1:37660-37663 only by default.
  BTC Light sidecar: local stdin/stdout process, no inbound TCP listener.
  QRX P2P: separate listener and may use 0.0.0.0 so peers can reach the node.

Remote RPC is an advanced/manual server feature only. It requires:
  --allow-remote-rpc --rpc-user <user> --rpc-password <strong-secret>
and should still be protected by a host firewall/VPN. Do not expose it directly to the Internet.
MSG
