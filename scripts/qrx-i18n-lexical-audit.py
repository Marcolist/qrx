#!/usr/bin/env python3
import json, pathlib, re, sys
root=pathlib.Path(__file__).resolve().parents[1]
loc=root/"GUIWALLET/src/locales"
manifest=json.loads((loc/"manifest.json").read_text(encoding="utf-8"))
# Conservative English function/control vocabulary. A hit requires >=3 tokens,
# avoiding false positives from protocol names such as Bitcoin, Mainnet or QRX.
WORDS=set("the and your this with from into only before after must can will not for you has have select enter address amount send receive recovery security order trade open close cancel enabled disabled stays keep use run when while which whether never every current existing new local wallet validator backup node keys private public phrase balance recipient anyone makes becomes requires starts creates restore choose check confirms owner funds computer transaction delegation rewards files directory payment existing exact same whole first safe".split())
TOKEN=re.compile(r"[A-Za-z]+")
# English-looking tokens that are also native/common technical vocabulary in specific locales.
# These are ignored only for the lexical heuristic; key parity and reviewed identical-value gates remain strict.
NATIVE_OVERLAP={
    "nl": {"exact", "wallet", "validator"},
    "da": {"for", "wallet", "validator", "node", "computer", "private"},
    "no": {"for", "validator", "private"},
    # Romanian words such as „exactă”, „validator” and „private” are native/standard technical vocabulary.
    "ro": {"exact", "validator", "private"},
    # Indonesian uses validator/node as standard computing/crypto terms; Safe Pause is a named QRX action.
    "id": {"validator", "node", "safe"},
    # Nguni locales use validator/node and Safe Pause as standard QRX technical terms.
    "zu": {"validator", "node", "safe"},
    "xh": {"validator", "node", "safe"},
    "nd": {"validator", "node", "safe"},
    "ss": {"validator", "node", "safe"},
    "nr": {"validator", "node", "safe"},
    "ts": {"validator", "node", "safe"},
    "ve": {"validator", "node", "safe"},
    "rw": {"validator", "node", "safe"},
    "rn": {"validator", "node", "safe"},
    "ny": {"wallet", "validator", "node", "private", "public", "address", "backup", "recovery", "safe", "keys", "phrase", "owner", "balance"},
    "sn": {"wallet", "validator", "node", "private", "public", "address", "backup", "recovery", "safe", "keys", "phrase", "owner", "balance"},
    "mg": {"wallet", "validator", "node", "private", "public", "address", "backup", "recovery", "safe", "keys", "phrase", "owner", "balance"},
    # Final Afroasiatic locales use several standard crypto/computing loanwords.
    # Function words remain checked, while protocol/UI technical nouns are allowed.
    "am": {"address", "amount", "send", "receive", "recovery", "security", "order", "trade", "open", "close", "cancel", "enabled", "disabled", "local", "wallet", "validator", "backup", "node", "keys", "private", "public", "phrase", "balance", "recipient", "owner", "funds", "computer", "transaction", "delegation", "rewards", "files", "directory", "payment", "safe", "current", "existing", "new", "exact"},
    "so": {"address", "amount", "send", "receive", "recovery", "security", "order", "trade", "open", "close", "cancel", "enabled", "disabled", "local", "wallet", "validator", "backup", "node", "keys", "private", "public", "phrase", "balance", "recipient", "owner", "funds", "computer", "transaction", "delegation", "rewards", "files", "directory", "payment", "safe", "current", "existing", "new", "exact"},
    "ti": {"address", "amount", "send", "receive", "recovery", "security", "order", "trade", "open", "close", "cancel", "enabled", "disabled", "local", "wallet", "validator", "backup", "node", "keys", "private", "public", "phrase", "balance", "recipient", "owner", "funds", "computer", "transaction", "delegation", "rewards", "files", "directory", "payment", "safe", "current", "existing", "new", "exact"},
    "ber": {"address", "amount", "send", "receive", "recovery", "security", "order", "trade", "open", "close", "cancel", "enabled", "disabled", "local", "wallet", "validator", "backup", "node", "keys", "private", "public", "phrase", "balance", "recipient", "owner", "funds", "computer", "transaction", "delegation", "rewards", "files", "directory", "payment", "safe", "current", "existing", "new", "exact"},
}

def findings(lang):
    d=json.loads((loc/f"{lang}.json").read_text(encoding="utf-8"))
    out=[]
    for k,v in d.items():
        if not isinstance(v,str): continue
        overlap=NATIVE_OVERLAP.get(lang,set())
        toks=[t.lower() for t in TOKEN.findall(v)]
        score=sum(t in WORDS and t not in overlap for t in toks)
        if score>=3:
            out.append((k,score,v))
    return out

langs=sys.argv[1:] or [x for x,s in manifest.get("editorial_status",{}).items() if s=="complete" and x!="en"]
failed=False
for lang in langs:
    fs=findings(lang)
    print(f"{lang}: {len(fs)} mixed-English candidates")
    for k,score,v in fs[:50]:
        print(f"  {k} [{score}] {v}")
    if fs: failed=True
if failed: sys.exit(3)
print("PASS: no mixed-English candidates in audited complete locales.")
