from pathlib import Path
root=Path(__file__).resolve().parents[1]
qrxd=(root/'qrx-core/src/qrxd.c').read_text()
front=(root/'qrx-core/src/core_frontend.c').read_text()
rust=(root/'GUIWALLET/src-tauri/src/main.rs').read_text()
html=(root/'GUIWALLET/src/index.html').read_text()
checks={
 'daemon default removed':'qrx_set_env("QRX_PASSPHRASE", "change-me"' not in qrxd,
 'creation default removed':'setenv_qrx("QRX_PASSPHRASE","change-me"' not in front,
 'legacy detector':'fn detect_legacy_default_passphrase' in rust and 'encrypted_pem_accepts_passphrase(&ed,"change-me")' in rust,
 'dedicated migration':'fn migrate_legacy_default_passphrase' in rust,
 'backup before mutation':'create_wallet_security_backup(&network,&wallet,"pre-passphrase-change")' in rust,
 'identity fingerprint':'public_key_to_der()' in rust and 'address_before' in rust and 'Identity verification failed after re-encryption' in rust,
 'gui panel':'legacyDefaultPassphrasePanel' in html and 'migrateLegacyDefaultPassphrase' in html,
 'minimum 12':'next.length<12' in html,
}
failed=[k for k,v in checks.items() if not v]
if failed:
 print('FAIL:',', '.join(failed)); raise SystemExit(1)
print('QRX 0.0.9.75 legacy wallet migration audit: PASS')
