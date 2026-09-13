# QRX 0.0.7.7 Phase 7 — Native Markets, On-Chain Order Book & Trading View

Phase 7 exposes the already-consensus-backed VELOCITY native order/trade state as a first-class wallet market surface.

## Delivered
- Wallet `Markets` view bound to the active QRX network and wallet.
- Verifiable native order book via Core `getorderbook`.
- Settled on-chain trade tape via Core `listtrades`.
- Chain-height OHLCV candle aggregation in the GUI; no redundant candle state is committed to consensus.
- Native LIMIT BUY/SELL creation uses existing `ORDER_CREATE`, VELOCITY matching, reserve and atomic settlement.
- Order cancellation uses existing `ORDER_CANCEL`.
- Mainnet acknowledgement before broadcast.
- External/Kraken liquidity is deliberately not blended into the native book.
- Chart/orderbook can be reconstructed by any node from canonical QRX state.

## Architecture
Canonical: ORDER_CREATE / ORDER_REPLACE / ORDER_CANCEL + settled trade records -> QRXDB / chain state.
Derived: book depth, tape and OHLCV -> wallet/explorer index/view.

## Release gate
Phase 7 is feature-complete wiring, not a security certification. Before Genesis/Mainnet release, run the Mainnet Release Candidate Audit of Core + Wallet + Generals + Markets as one system, including native Tauri builds and multi-node adversarial/restart/reorg tests.
