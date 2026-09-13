#!/usr/bin/env python3
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
params=(ROOT/'qrx-core/src/chain_params.c').read_text()
qrxd=(ROOT/'qrx-core/src/qrxd.c').read_text()
qrx=(ROOT/'qrx-core/src/qrx.c').read_text()

required={
    'offline_penalty_bps':5,
    'offline_penalty_after_blocks':25920,
    'offline_penalty_interval_blocks':8640,
    'offline_max_slash_bps_per_outage':100,
    'offline_jail_after_blocks':60480,
    'offline_jail_seconds':3600,
    'validator_catchup_max_blocks':2,
    'validator_catchup_min_peers':1,
    'validator_catchup_stable_seconds':30,
}
for k,v in required.items():
    assert f'{k}={v}\\n' in params, (k,v)
    assert f'fork.0.{k}={v}\\n' in params, ('fork',k,v)

for token in ['validator_catchup_safe','waiting_for_peers','catching_up','sync_stabilizing',
              'validator_signing_paused','validator_auto_resume']:
    assert token in qrxd, token
assert 'if (missed < after + interval) continue;' in qrx
assert 'validator_offline_outage_bps.bin' in qrx
assert 'already_bps >= max_outage_bps' in qrx
assert 'missed >= jail_after' in qrx

# 10-second block profile: model completed liveness-penalty intervals after 72h grace.
BLOCKS_PER_DAY=8640
GRACE=25920
INTERVAL=8640
BPS=5
CAP=100

def penalty_bps(days):
    missed=int(days*BLOCKS_PER_DAY)
    if missed < GRACE+INTERVAL:
        return 0
    intervals=1 + (missed-(GRACE+INTERVAL))//INTERVAL
    return min(CAP, intervals*BPS)

assert penalty_bps(2)==0
assert penalty_bps(3)==0
assert penalty_bps(4)==5
assert penalty_bps(7)==20
assert penalty_bps(30)==100
# A 1000 QUB self-stake-only validator therefore risks at most ~10 QUB liveness slash
# for one uninterrupted outage under the home profile (rounding/current-power aside).
assert 1000*CAP/10000==10
print('Phase 7.2.13 home-validator safety assertions PASS')
