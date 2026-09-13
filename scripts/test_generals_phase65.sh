#!/usr/bin/env bash
set -euo pipefail
Q="$(cd "$(dirname "$0")/../qrx-core" && pwd)/src/qrx.c"
grep -q 'type=GENERALS_RELAY' "$Q"
grep -q 'generals_relay_store' "$Q"
grep -q 'generals-relay-publish' "$Q"
grep -q 'generals_autonomous_world_resolve' "$Q"
grep -q 'CONSENSUS_AUTO' "$Q"
grep -q 'generals_auto_resolved' "$Q"
echo 'PASS phase6.5 decentralized relay + autonomous world wiring'
