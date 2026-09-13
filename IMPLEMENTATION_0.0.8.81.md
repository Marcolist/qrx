# QRX 0.0.8.81 — Youth Protection, Family Safety & Browser App

## Browser product shape
The QRX Browser is now a first-class QRX Wallet application, matching the QRX Generals launch model. It has its own Tauri window (`qrx-browser`), own HTML entry point and an Apps launchpad icon. The earlier inline QRX-Net browser remains a compatibility/admin surface, while the dedicated window is the normal browsing product surface.

## Family Safety
A new Core policy engine defines Adult, Teen and Child profiles with rating ceilings, category blocks, normalized `.qrx` allow/block lists and capability gates. Child and Teen profiles default to sponsored advertisements OFF and viewer-ad rewards OFF. Campaign payment is not considered an override signal.

## Local encrypted policy
Core policy persistence uses authenticated encryption and fails closed on wrong PIN or tampering. The desktop wallet uses its existing local-secret-vault pattern: Argon2id key derivation plus AES-256-GCM, scoped to the selected network/wallet and protected with private file permissions where available.

## Privacy
Family Safety remains local. Browsing history, selected profile and domain allow/block lists are not published to QRX consensus or a central moderation service.

## Tests
`net_phase123_youth_safety` adds profile, category, age, unrated, domain normalization, capability, encrypted vault, wrong-PIN and tamper tests. Full internal suite: 56/56 PASS.
