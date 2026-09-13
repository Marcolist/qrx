#!/usr/bin/env python3
from pathlib import Path
R=Path(__file__).resolve().parents[2]
q=(R/'qrx-core/src/qrx.c').read_text(); d=(R/'qrx-core/src/qrxd.c').read_text(); cli=(R/'qrx-core/src/qrx_cli.c').read_text(); gui=(R/'GUIWALLET/src-tauri/src/main.rs').read_text(); conf=(R/'GUIWALLET/src-tauri/tauri.conf.json').read_text()
assert 'legacy direct staking mutation disabled in Phase 7.2.12' in q
assert 'staking_claim_amount' in q and 'staking:claim_credit:' not in q
for x in ['STAKE_BOND','STAKE_UNBOND','STAKE_CLAIM','DELEGATE_BOND','DELEGATE_UNBOND','DELEGATE_CLAIM']: assert x in q
assert 'validator_is_tombstoned(c,to)' in q and 'validator_is_jailed_now(c,to)' in q
assert 'supply-invariant' in q
assert 'X-QRX-RPC-Token' in d and 'rpc_token_ok' in d and 'origin_allowed' in d and 'header_has_json_content_type' in d
assert 'X-QRX-RPC-Token' in cli and 'walletpassphrasehexfor' in cli
assert 'walletpassphrasehexfor' in gui and 'staking_consensus_broadcast' in gui
assert '&["delegate"' not in gui and '&["undelegate"' not in gui and '&["claim-undelegated"' not in gui
assert '"$HOME/**"' not in conf and '"csp": null' not in conf
print('Phase 7.2.12 static security assertions PASS')
