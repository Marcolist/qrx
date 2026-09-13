#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"
Q="$R/qrx-core/src/qrx.c"
P="$R/qrx-core/src/privacy/qrx_privacy_consensus.inc"
B="$R/qrx-core/src/velocity/qrx_btc_spv.inc"
X="$R/qrx-core/src/velocity/qrx_crosschain.inc"
H="$R/qrx-core/src/bitcoin/qrx_btc_spv.h"
G="$R/GUIWALLET/src/index.html"
T="$R/GUIWALLET/src-tauri/src/main.rs"
# Consensus privacy, chain binding, governance and atomic state.
grep -q 'PRIVACY_SHIELD' "$Q"
grep -q 'PRIVACY_TRANSFER' "$Q"
grep -q 'PRIVACY_UNSHIELD' "$Q"
grep -q 'PRIVACY_GOVERNANCE' "$Q"
grep -q 'chain_id' "$P"; grep -q 'genesis_hash' "$P"; grep -q 'protocol_version' "$P"
grep -q 'atomic_stage_privacy' "$P"
# BTC/QUB funding safety and immutable first proof.
grep -q 'btc_funding_deadline_qrx_height' "$X"
grep -q 'btc_funding_safety_qrx_blocks' "$X"
grep -q 'btc_funding_proof_locked' "$B"
grep -q 'qrx_btc_spv_funding_policy_valid' "$H"
# Production GUI uses consensus privacy + VELOCITY cross-chain, never legacy HTLC invokes.
grep -q 'crosschain_place_buy' "$T"; grep -q 'crosschain_submit_funding_proof' "$T"
grep -q 'PRIVACY_SHIELD' "$G"; grep -q 'VELOCITY Cross-Chain' "$G"
! grep -q 'draft-waiting-for-real-swap-engine' "$T"
! grep -q 'no-custody-in-GUI-placeholder' "$T"
! grep -q 'core_create_swap' "$T"
! grep -q 'core_redeem_swap' "$T"
! grep -q 'core_refund_swap' "$T"
# No Mainnet opt-out escape hatch is passed by the wallet.
grep -q 'env_remove("QRX_ENABLE_MAINNET_HTLC")' "$T"
echo 'PASS: Phase 7.1 privacy/HTLC consensus + production GUI wiring'
