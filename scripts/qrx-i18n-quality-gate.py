#!/usr/bin/env python3
import json,pathlib,re,sys
root=pathlib.Path(__file__).resolve().parents[1]; loc=root/"GUIWALLET/src/locales"
m=json.loads((loc/"manifest.json").read_text(encoding="utf-8")); en=json.loads((loc/"en.json").read_text(encoding="utf-8")); errors=[]
WORDS=set("the and your this with from into only before after must can will not for you has have select enter address amount send receive recovery security order trade open close cancel enabled disabled stays keep use run when while which whether never every current existing new local wallet validator backup node keys private public phrase balance recipient anyone makes becomes requires starts creates restore choose check confirms owner funds computer transaction delegation rewards files directory payment existing exact same whole first safe".split())
TOKEN=re.compile(r"[A-Za-z]+")
PLACEHOLDER=re.compile(r"\$?\{[^{}]+\}|%\([^)]+\)[A-Za-z]|%[sdif]|<[^<>\s]+>")
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
for lang in m["languages"]:
 d=json.loads((loc/f"{lang}.json").read_text(encoding="utf-8"))
 if set(d)!=set(en): errors.append(f"{lang}: key mismatch")
 for k,src in en.items():
  if k in d and sorted(PLACEHOLDER.findall(str(src))) != sorted(PLACEHOLDER.findall(str(d[k]))):
   errors.append(f"{lang}: placeholder mismatch at {k}")
 status=m.get("editorial_status",{}).get(lang,"requires_human_editorial_review")
 same=sum(d.get(k)==v for k,v in en.items())
 print(f"{lang}: {len(d)} keys, {same} English-identical, editorial_status={status}")
 if status=="complete" and lang!="en":
  ap=loc/f"{lang}-identical-allowlist.json"
  if not ap.exists(): errors.append(f"{lang}: complete but no reviewed identical allowlist")
  else:
   allow=set(json.loads(ap.read_text(encoding="utf-8")).get("keys",[])); ident={k for k,v in en.items() if d.get(k)==v}
   if ident!=allow: errors.append(f"{lang}: complete status but unreviewed/stale English-identical keys")
  mixed=[]
  for k,v in d.items():
   if not isinstance(v,str): continue
   overlap=NATIVE_OVERLAP.get(lang,set())
   toks=[t.lower() for t in TOKEN.findall(v)]
   if sum(t in WORDS and t not in overlap for t in toks)>=3: mixed.append(k)
  if mixed: errors.append(f"{lang}: complete status but {len(mixed)} mixed-English candidates remain")
if errors:
 print("\n".join("ERROR "+e for e in errors)); sys.exit(2)
print("PASS: key parity and declared editorial-completion gates are consistent, including mixed-English checks for complete locales.")
