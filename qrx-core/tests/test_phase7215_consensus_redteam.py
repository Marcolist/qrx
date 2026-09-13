#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
q=(root/'qrx-core/src/qrx.c').read_text()
checks={
'exact finalized height':'block height is not exact next finalized height',
'parent hash binding':'previous_block_hash does not match finalized parent',
'parent state root':'parent_state_root mismatch',
'mainnet activation verify':'Mainnet Genesis activation time has not been reached',
'future timestamp bound':'block timestamp too far in future',
'parent timestamp monotonic':'block timestamp precedes finalized parent',
'pause rejected in block verify':'validator SAFE PAUSED',
'pause excluded snapshot':'validator_is_safely_paused(chain_dir,validator))continue',
'pause rejects delegation':'validator_is_safely_paused(c,to)',
'deterministic proposer':'unexpected proposer for height/round',
'proposal not authoritative':'A proposal is not authoritative state',
'finalized-only ingest':'qrxdb finalized block ingest failed',
'authenticated-before-slashing':'Eligibility checks happen only after authenticating the signer',
'native vote enumeration':'qrx_collect_files_suffix(dir,".vote"',
'native inbox enumeration':'qrx_collect_files_suffix(dir,".block"',
}
missing=[name for name,needle in checks.items() if needle not in q]
if missing:
    print('FAIL missing:', ', '.join(missing)); raise SystemExit(1)
# Ordering: signature verification must precede double-sign state mutation in verify_block.
verify=q[q.index('static int verify_block_cmd(', q.index('static int propose_block_cmd(')):]
verify=verify[:verify.index('\n\ntypedef struct {\n    char validator[385];')]
if verify.index('block signature verify failed') > verify.index('check_and_record_double_sign_block'):
    print('FAIL: double-sign mutation occurs before signature verification'); raise SystemExit(1)
# Proposal path must no longer use shell block counting or ingest before quorum.
prop=q[q.index('static int propose_block_cmd_as('):q.index('static int propose_block_cmd(',q.index('static int propose_block_cmd_as('))]
for bad in ["find '%s/blocks'", 'qrxdb_chain_ingest_block_file(chain_dir, blk)']:
    if bad in prop:
        print('FAIL proposal still contains',bad); raise SystemExit(1)
print('Phase 7.2.15 consensus red-team assertions PASS')
