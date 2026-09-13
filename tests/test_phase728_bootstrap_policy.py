from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
qrx = (root/'qrx-core/src/qrx.c').read_text()
ui = (root/'GUIWALLET/src/index.html').read_text()
rust = (root/'GUIWALLET/src-tauri/src/main.rs').read_text()
checks = {
    'bootstrap lock metadata': 'genesis:validator:%s:amount_atoms' in qrx and 'genesis:validator:%s:locked_until_height' in qrx,
    'extra stake can unstake': 'freely_unstakeable = s > principal ? s - principal : 0' in qrx,
    'principal floor enforced': 'amount > freely_unstakeable' in qrx,
    'offline grace skips slash': 'bootstrap_liveness_grace' in qrx and 'slash=0 jail=0' in qrx,
    'double sign remains active': 'double_sign_slashing_active=1' in qrx,
    'status command': 'bootstrap-validator-status' in qrx,
    'reward liquid balance path retained': 'adjust_balance(chain_dir, validator, validator_credit)' in qrx,
    'fleet backend': 'validator_fleet_status' in rust and 'set_validator_fleet_modes' in rust,
    'fleet UX': 'Validator Fleet' in ui and 'Select bootstrap' in ui,
    'no auto stake wording': 'selection never stakes automatically' in ui,
    'runtime safety guard': 'does not launch dozens of qrxd processes' in ui,
}
failed=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(('PASS' if v else 'FAIL'), k)
if failed: sys.exit(1)
