# AURA Wallet AI Assistant

This build adds the AURA assistant UI to the wallet.

## UI

A new top-right chat icon opens AURA, styled like a compact ChatGPT-style conversation panel.

## Local mode

AURA can answer basic wallet/CLI questions locally without cloud tokens:

- get balance
- get new address
- send QUB
- staking
- delegation
- BTC Light basics
- Quantum Swaps / HTLC basics

Local chat history is stored only in browser localStorage.

## Cloud mode preview

Cloud mode is prepared but not connected yet.

Planned architecture:

```text
Wallet UI
  -> Cloudflare Worker
    -> package/payment check
    -> user/global rate limits
    -> ChatGPT API proxy
    -> response back to wallet
```

## Monetization model

- 30-day package
- payable with BTC or QUB
- backend calculates package price
- target margin: 50% after taxes
- rate limits per user and globally in Cloudflare

## Privacy model

- Chat history remains local.
- On continuation, the wallet can send the local conversation payload to the Cloudflare backend.
- The backend should not store chat history unless explicitly required.


## Validator status terminology

AURA Local Help now explains `tombstoned` and `jailed` directly.

- **Jailed**: temporary exclusion from active consensus.
- **Tombstoned**: permanent exclusion of that validator identity after a provable severe consensus violation, especially double-signing/equivocation.
- Current QRX default double-sign slash: **5000 bps = 50%** of slashable stake.
- Merely being offline does **not** by itself tombstone a validator.

Example questions: `What does tombstoned mean?`, `Was heißt tombstoned?`, `What is the difference between jailed and tombstoned?`.
