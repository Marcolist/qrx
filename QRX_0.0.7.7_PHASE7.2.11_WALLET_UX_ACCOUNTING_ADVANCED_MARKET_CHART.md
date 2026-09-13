# QRX 0.0.7.7 Phase 7.2.11 — Wallet UX, Accounting & Advanced Market Chart

## Goals
Make the desktop wallet understandable without protocol knowledge and remove unnecessary restart/password friction.

## Validator Mode
- Validator Mode ON/OFF never moves QUB.
- A normal address still requires the protocol minimum self-stake collateral before it can validate.
- A bootstrap address already has its 1,000 QUB Genesis self-stake.
- Validator Fleet changes are hot-reloaded into a running qrxd through `setvalidatorfleet`.
- If hot reload fails, the GUI explicitly offers `Restart now` or `Later`; no restart prompt appears on successful runtime apply.

## Legacy 0.0.6 wallet lock UX
- A cryptographically verified empty-passphrase legacy wallet remains session-ready.
- An unencrypted private-key wallet is shown as ready and does not ask for a password.
- Read-only refresh/inspection does not revoke a verified GUI signing session.
- Lock state is derived from actual key protection plus the verified in-memory session, not a stale cosmetic flag.

## Advanced Market Chart
Own QRX-native canvas chart; not an embedded TradingView product.
Range controls:
- `1M` = 1 minute
- `1h`
- `3h`
- `1d`
- `7d`
- `1m` = 1 month
- `1y`
- `ALL`
Includes OHLC candles, volume, high/low/last/change statistics, responsive desktop width, hover candle details, order book and recent trades.

## Accounting Ledger V4
The Complete Ledger remains State-Root-verified and gains explicit tax/accounting fields:
- tax_category
- tax_event
- tax_note
- fiat_currency
- fiat_unit_price
- fiat_gross_value
- fiat_fee_value
- price_source
- price_timestamp_utc

Validator rewards, delegation rewards, staking/delegation movements, trades/swaps, internal transfers, incoming receipts and outgoing movements are classified separately.

QRX deliberately does not invent historical EUR values. If no provable valuation source exists, fiat fields remain blank and the manifest states that accountant/user valuation is required. The export is structured for tax/accounting work, not automatic tax advice.

## Reward notifications
Separate toggles for:
- Validator rewards
- Delegation rewards
- Trading / settlement events
- Minimum QUB threshold for desktop alerts

The wallet keeps an in-app activity center and uses desktop notifications only after explicit permission. Repeated events are aggregated instead of producing one popup for each event.

## Layout
Primary views use the full desktop content width. Markets use a wide chart/workspace layout and collapse responsively on narrower windows rather than forcing a tablet-like central column.
