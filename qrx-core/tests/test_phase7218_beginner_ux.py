from pathlib import Path
import sys
root=Path(sys.argv[1] if len(sys.argv)>1 else Path(__file__).parents[2])
s=(root/'GUIWALLET/src/index.html').read_text()
for marker in ['languageMenu','QRX_LANGUAGES','zh-CN','zh-TW','Bahasa Indonesia','ไทย','日本語','Safety Center','safetyScore','exportCurrentWalletBackup','generateFreshRecoveryBackup','Network Guard','data-advanced','toggleExperienceMode']:
    assert marker in s, marker
assert 'Plaintext private-key export is intentionally not the default' in s
assert 'textContent' in s
print('phase7218 beginner UX assertions PASS')
