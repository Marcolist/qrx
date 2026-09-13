#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "QRX Mainnet Genesis Dress Rehearsal / Byzantine Multi-Node Simulation"
echo "Scheduled Genesis: 2026-09-15 16:00:00 UTC / 18:00:00 CEST"
python3 "$ROOT/qrx-core/tests/test_phase7215_consensus_redteam.py" "$ROOT"
python3 "$ROOT/qrx-core/tests/test_phase7216_byzantine_multinode.py" "$ROOT"
echo "Dress rehearsal PASS"
