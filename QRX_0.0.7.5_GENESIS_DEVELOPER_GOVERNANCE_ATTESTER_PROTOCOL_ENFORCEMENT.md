# QRX 0.0.7.5 – Genesis Developer Governance + Consensus Attester Registry + Protocol Upgrade Enforcement

## Ziel

QRX hardcodiert **nicht CURA als Privacy Provider**. Genesis verankert nur eine Developer-Governance-Root aus öffentlichen Ed25519-Schlüsseln. CURA oder jeder weitere KYC-/Privacy-Provider wird anschließend über denselben threshold-signierten Governance-Pfad zugelassen.

Private Governance- oder Attester-Schlüssel werden niemals in Genesis, Chain-State oder Repository geschrieben.

## 1. Fünf Developer-Governance-Keys erzeugen

Je Key möglichst auf einem getrennten Offline-System/Datenträger:

```bash
export QRX_GOV_PASSPHRASE='DEINE-SEHR-LANGE-PASSPHRASE'
qrx governance-keygen ./DEV_GOV_1 DEV_GOV_1
qrx governance-keygen ./DEV_GOV_2 DEV_GOV_2
qrx governance-keygen ./DEV_GOV_3 DEV_GOV_3
qrx governance-keygen ./DEV_GOV_4 DEV_GOV_4
qrx governance-keygen ./DEV_GOV_5 DEV_GOV_5
unset QRX_GOV_PASSPHRASE
```

Pro Verzeichnis entstehen:

- `governance.key` – AES-256-CBC verschlüsselter PKCS#8 Private Key, Unix-Dateirechte 0600
- `governance.pub.pem` – öffentlicher Ed25519-Key
- `governance.pub` – QRX Public Descriptor mit Key-ID, Public Key und Fingerprint

Die `governance.key` Dateien bleiben offline. Für Genesis werden ausschließlich die `.pub` Descriptors verwendet.

## 2. 3-of-5 Governance-Root initialisieren

```bash
qrx governance-genesis-init ~/.qrx/mainnet 3 \
  ./DEV_GOV_1/governance.pub \
  ./DEV_GOV_2/governance.pub \
  ./DEV_GOV_3/governance.pub \
  ./DEV_GOV_4/governance.pub \
  ./DEV_GOV_5/governance.pub
```

Chain-State:

```text
governance/governance.conf
governance/governance_roots.db
```

Darin stehen nur Public Keys/Fingerprints und `threshold=3`.

## 3. CURA als normalen Privacy-Attester vorbereiten

CURA erzeugt einen **eigenen**, von Governance getrennten Attester-Key:

```bash
export QRX_ATTESTER_PASSPHRASE='ANDERE-SEHR-LANGE-PASSPHRASE'
qrx privacy-attester-keygen ./cura-attester cura
unset QRX_ATTESTER_PASSPHRASE
```

Es entstehen:

```text
cura-attester/attester.key
cura-attester/attester.pub.pem
cura-attester/attester.pub
```

`attester.key` darf niemals an die Developer-Governance weitergegeben werden.

## 4. CURA on-chain vorschlagen

```bash
qrx governance-attester-propose \
  cura-add.proposal \
  ATTESTER_ADD \
  cura \
  ./cura-attester/attester.pub \
  verified-privacy,hidden-balance \
  0
```

Das Proposal enthält Public Key, Capabilities, Activation Height und einen zufälligen Replay-Nonce.

## 5. Drei unabhängige Governance-Signaturen

Auf mindestens drei Governance-Key-Systemen separat:

```bash
qrx governance-sign ./DEV_GOV_1 cura-add.proposal cura.sig1
qrx governance-sign ./DEV_GOV_2 cura-add.proposal cura.sig2
qrx governance-sign ./DEV_GOV_3 cura-add.proposal cura.sig3
```

Jede Signatur bindet exakt den Proposal-Inhalt. Nachträgliche Änderungen invalidieren alle Signaturen.

## 6. Proposal anwenden

```bash
qrx governance-apply ~/.qrx/mainnet cura-add.proposal \
  cura.sig1 cura.sig2 cura.sig3
```

QRX prüft:

1. Governance Genesis vorhanden
2. alle Signaturen gehören zu ACTIVE Genesis Roots
3. mindestens drei **verschiedene** Key-IDs
4. Proposal Hash passt
5. Activation Height ist erreicht
6. Proposal wurde noch nicht angewendet

Danach erscheint CURA in `privacy_attesters.db` als `ACTIVE`.

Doppelte Signaturen zählen nicht mehrfach. Proposal-Replays und manipulierte Proposals werden fail-closed verworfen.

## 7. Direkte Provider-Manipulation wird nach Governance Genesis blockiert

Die alten Bootstrap-Befehle `privacy-attester-register` und `privacy-attester-disable` bleiben für Pre-Governance-/Regression-Szenarien verfügbar. Sobald `governance/governance.conf` existiert, werden direkte Registry-Mutationen abgelehnt. Änderungen müssen dann threshold-signiert erfolgen.

## 8. Provider deaktivieren / Key rotieren

Derselbe Ablauf wird für folgende Actions verwendet:

```text
ATTESTER_ADD
ATTESTER_DISABLE
ATTESTER_ROTATE_KEY
```

Damit ist CURA nicht privilegiert. Jeder akzeptierte Provider folgt demselben Registry-Modell.

## 9. Mandatory Protocol Upgrade

Upgrade-Proposal:

```bash
qrx governance-protocol-propose \
  protocol-v8.proposal \
  8 \
  2500000 \
  3 \
  4 \
  VERIFIED_PRIVACY_V2,SHIELDED_PROOF_V4
```

Bedeutung:

- Protocol Version 8
- Aktivierung ab Block 2.500.000
- Minimum Transaction Version 3
- Minimum Privacy Version 4
- neue Feature Flags

Danach wieder drei Governance-Signaturen und `governance-apply`.

`protocol-info` bzw. RPC/CLI `getprotocolinfo` liefert unter anderem:

```text
active_protocol
minimum_tx_version
minimum_privacy_version
wallet_supported_protocol
wallet_supported_privacy
update_required
next_activation_height
```

Transaktionen unterhalb der aktivierten Minimum-TX-Version werden im Core fail-closed als obsolete abgewiesen.

## 10. Wallet UX

Die Tauri-Wallet liest `getprotocolinfo` beim Refresh. Bei `update_required=true` wird eine Mandatory-Update-Warnung angezeigt. QUB Send, Staking und Delegation werden im GUI gesperrt; Recovery und Receive bleiben zugänglich. Der Core selbst blockiert obsolete Transaktionsversionen unabhängig von der GUI.

## Sicherheitsgrenzen

- Governance-Key != Development-Fund-Key.
- Governance-Key != Privacy-Attester-Key.
- Keine privaten Governance-Schlüssel im Genesis/Chain-State.
- Keine KYC-PII im Attester-Registry-State.
- 3-of-5 verhindert Single-Key-Provider-Injection.
- Replay-, Duplicate-Signer- und Proposal-Tamper-Schutz vorhanden.
- Die aktuelle Governance-State-Transition ist threshold-authentifiziert. Für vollständig dezentrale Mainnet-Governance kann später Validator-/On-chain-Voting über denselben Proposal-Typ gelegt werden.

## Validation 0.0.7.5

Der native Final Release Audit umfasst 13 Gates. Neu geprüft werden insbesondere:

- 5 Developer Governance Public Roots / threshold 3
- keine Private Keys im Chain-State
- Provider-Keygen
- CURA Admission über 3 unabhängige Signaturen
- Duplicate-Signer rejection
- Direct-Mutation rejection nach Governance Genesis
- Replay rejection
- Proposal tamper rejection
- Protocol Upgrade Schedule
- Minimum-TX-Version Enforcement
- `getprotocolinfo` in Core/CLI/RPC
- GUI Mandatory-Update-Surface

`RESULT: QRX 0.0.7.5 FINAL RELEASE AUDIT PASSED ON THIS NATIVE HOST`

Native macOS-/Windows-/Linux-ARM Installer werden weiterhin von den jeweiligen nativen CI-Runnern gebaut; sie wurden in dieser Linux-Umgebung nicht erzeugt.
