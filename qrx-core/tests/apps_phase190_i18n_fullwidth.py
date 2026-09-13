#!/usr/bin/env python3
import json, pathlib, re, sys
from bs4 import BeautifulSoup
root=pathlib.Path(sys.argv[1]).resolve()
wallet=root/'GUIWALLET'
ui_path=wallet/'src/index.html'
ui=ui_path.read_text(encoding='utf-8')
loc=wallet/'src/locales'
manifest=json.loads((loc/'manifest.json').read_text(encoding='utf-8'))
assert len(manifest['languages'])==55
english=json.loads((loc/'en.json').read_text(encoding='utf-8'))
new_keys={k for k in english if k.startswith('qrx.apps.') or k.startswith('qrx.upscaler.') or k.startswith('qrx.apphost.')}
assert len(new_keys)>=59, len(new_keys)
for lang in manifest['languages']:
    d=json.loads((loc/f'{lang}.json').read_text(encoding='utf-8'))
    assert set(d)==set(english), f'{lang} locale key mismatch'
    assert all(str(d[k]).strip() for k in new_keys), f'{lang} has blank new UX translation'
    if lang!='en':
        # Product/protocol identifiers may be language-neutral; the actual UX corpus must not be an English fallback wholesale.
        localized=sum(d[k]!=english[k] for k in new_keys)
        assert localized>=35, f'{lang}: only {localized} localized new UX entries'

soup=BeautifulSoup(ui,'html.parser')
for selector in ['#view-apps','#view-upscaler']:
    assert soup.select_one(selector), selector
# Static new UX must be wired to translation keys.
required=[
 ('#view-apps h3','wallet.auto.d9a36850b7f3'),
 ('#view-apps button.btn.primary','qrx.apps.install'),
 ('#qrxDeveloperPanel h4','qrx.apps.developer_mode'),
 ('#qrxDeveloperPanel button','qrx.apps.load_folder'),
 ('#view-upscaler .feature-card h4','qrx.upscaler.local'),
 ('#upscalerLocalStatus','qrx.upscaler.detecting'),
 ('#upscalerHardwareDetail','qrx.upscaler.checking'),
]
for sel,key in required:
    el=soup.select_one(sel); assert el is not None and el.get('data-i18n')==key,(sel,key,el)
# Dynamic paths must use the locale catalog rather than old hard-coded English UX.
scripts='\n'.join(x.get_text() for x in soup.find_all('script'))
for token in ["tr('qrx.apps.install')","qrxFmt('qrx.apps.payment_review'","tr('qrx.upscaler.detecting')","tr('qrx.upscaler.fallback')"]:
    assert token in scripts, token
for old in ['No wallet permissions requested.','requested a payment. Review it here; nothing has been signed or sent.','Capability probe unavailable · Classical mode remains available']:
    assert old not in scripts, old
# Apps/Upscaler are one-column top-level workspaces spanning the entire content width.
compact=re.sub(r'\s+','',ui)
assert '#view-apps:not(.hidden),#view-upscaler:not(.hidden){display:grid;grid-template-columns:minmax(0,1fr);gap:18px;width:100%;max-width:none}' in compact
assert '#view-apps>*,#view-upscaler>*{grid-column:1/-1;width:100%;min-width:0}' in compact
assert '#view-upscaler>.split{grid-template-columns:repeat(2,minmax(0,1fr));width:100%}' in compact
assert '@media(max-width:900px){#view-upscaler>.split{grid-template-columns:1fr}}' in compact
# App Host shares the same locale catalog and stays full-frame.
host=(wallet/'src/app-host/index.html').read_text(encoding='utf-8')
assert "../locales/en.json" in host and "qrx.apps.permissions" in host and "qrx.apphost.sandbox" in host
assert 'iframe{width:100%;height:100%' in host
print(f'PASS: {len(manifest["languages"])} locales, {len(english)} total keys, translated Apps/Upscaler/App Host UX and full-width workspaces')
