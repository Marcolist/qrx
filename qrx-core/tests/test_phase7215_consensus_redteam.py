#!/usr/bin/env python3
"""
Phase 7.2.15 consensus red-team assertions.

Genesis hardening (Finding 12): this test used to match exact minified C
substrings such as "validator_is_safely_paused(chain_dir,validator))continue".
That made it brittle: reformatting, one extra space or a renamed local turned a
green gate red with no behavioural change. The shipped 0.0.9 tree already
failed this check, because the real code reads
"validator_is_safely_paused(chain_dir, validator)) continue;".

The checks below are semantic instead: guard strings are matched with
whitespace normalised, and safe-pause enforcement is verified by locating the
relevant function and asserting it actually consults
validator_is_safely_paused(), not that it is spelled a particular way.
"""

from pathlib import Path
import re
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
src = (root / "qrx-core/src/qrx.c").read_text(encoding="utf-8", errors="replace")


def norm(text):
    return re.sub(r"\s+", "", text)


src_n = norm(src)
failures = []


def require_message(name, needle):
    if norm(needle) not in src_n:
        failures.append("missing guard: %s" % name)


def function_body(signature_prefix):
    """Body of the first *definition* matching signature_prefix. Forward
    declarations (terminated by ';') are skipped, otherwise brace matching
    would start at an unrelated later function."""
    i = -1
    while True:
        i = src.find(signature_prefix, i + 1)
        if i < 0:
            return None
        close = src.find(")", i)
        if close < 0:
            return None
        rest = src[close + 1:close + 40].lstrip()
        if rest.startswith("{"):
            break
    j = src.find("{", i)
    if j < 0:
        return None
    depth, k = 0, j
    while k < len(src):
        if src[k] == "{":
            depth += 1
        elif src[k] == "}":
            depth -= 1
            if depth == 0:
                return src[i:k + 1]
        k += 1
    return None


def require_calls(name, signature_prefix, callee):
    body = function_body(signature_prefix)
    if body is None:
        failures.append("function not found: %s" % signature_prefix)
        return
    if not re.search(r"\b%s\s*\(" % re.escape(callee), body):
        failures.append("%s: %s does not consult %s()" % (name, signature_prefix, callee))


require_message("exact finalized height", "block height is not exact next finalized height")
require_message("parent hash binding", "previous_block_hash does not match finalized parent")
require_message("parent state root", "parent_state_root mismatch")
require_message("mainnet activation verify", "Mainnet Genesis activation time has not been reached")
require_message("future timestamp bound", "block timestamp too far in future")
require_message("parent timestamp monotonic", "block timestamp precedes finalized parent")
require_message("pause rejected in block verify", "validator SAFE PAUSED")
require_message("deterministic proposer", "unexpected proposer for height/round")
require_message("proposal not authoritative", "A proposal is not authoritative state")
require_message("finalized-only ingest", "qrxdb finalized block ingest failed")
require_message("authenticated-before-slashing",
                "Eligibility checks happen only after authenticating the signer")

for suffix in (".vote", ".block"):
    if norm('qrx_collect_files_suffix(dir,"%s"' % suffix) not in src_n:
        failures.append("native enumeration missing for %s" % suffix)

# Safe pause must genuinely exclude a validator from the active snapshot and
# must be honoured when offline penalties are applied. Both checks are scoped
# to the owning function so that a match elsewhere cannot mask a removal.
require_calls("pause excluded from snapshot", "static int validator_snapshot_write(",
              "validator_is_safely_paused")
require_calls("pause honoured in offline penalties", "static int apply_offline_penalties(",
              "validator_is_safely_paused")

# Delegation staging must refuse a safely paused target validator. The check
# lives in the staking staging path, so assert on behaviour rather than on a
# particular command entry point.
require_calls("pause rejects delegation", "static int atomic_stage_staking(",
              "validator_is_safely_paused")

verify = function_body("static int verify_block_cmd(")
if verify is None:
    failures.append("verify_block_cmd not found")
else:
    sig = verify.find("block signature verify failed")
    dbl = verify.find("check_and_record_double_sign_block")
    if sig < 0 or dbl < 0:
        failures.append("verify_block_cmd: signature/double-sign markers not found")
    elif sig > dbl:
        failures.append("double-sign mutation occurs before signature verification")

prop = function_body("static int propose_block_cmd_as(")
if prop is None:
    failures.append("propose_block_cmd_as not found")
else:
    prop_n = norm(prop)
    if norm("find '%s/blocks'") in prop_n:
        failures.append("proposal still shells out to count blocks")
    if norm("qrxdb_chain_ingest_block_file(chain_dir, blk)") in prop_n:
        failures.append("proposal ingests a block before quorum")

if failures:
    print("FAIL:")
    for f in failures:
        print("  -", f)
    raise SystemExit(1)

print("Phase 7.2.15 consensus red-team assertions PASS")
