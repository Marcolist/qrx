# QRX 0.0.9.69 — Browser Address Bar & WebView Bounds Hardening

- Address input owns a draft state while typing; 700 ms browser-state polling cannot overwrite it.
- Native content WebView bounds are driven by the measured browser chrome height via ResizeObserver, with a 2 px safety gap.
- Validator Fleet card is full-width/min-width safe, wraps long explanatory text, and uses a horizontally scrollable sticky-header table.
- Genesis bootstrap validator capacity is 60 addresses. The mainnet material gate now requires all 60 real addresses; placeholders remain invalid by design.
- Fleet wording no longer implies a fixed daemon count: one node runtime serves the validator signer fleet.
