QRX 0.0.8.60 – Interactive 3D Resource Globe + Live Region Layer
=================================================================

Implemented on top of 0.0.8.59.

Wallet:
- interactive Canvas-based 3D globe renderer (no external CDN/runtime dependency)
- live input exclusively from getresourceatlas
- drag rotation, wheel zoom, hover inspection and responsive redraw
- live layers: Proven Capacity, Utilization, Opportunity, Storage Health
- privacy threshold badge and hidden-cell accounting
- visible-region table from the exact same privacy-filtered atlas response
- no provider IP, endpoint, GPS or exact home-node coordinates enter the renderer
- known coarse region labels use coarse display centroids; unknown labels get a deterministic visual-only display position derived from the public region label
- hook for animated own-object shard routes: window.qrxSetOwnShardRoutes([...])
- shard route API accepts only coarse region labels and caps the display at 14 routes
- no fabricated shard routes are shown when the wallet has no authoritative route data

Privacy invariant:
The renderer consumes only daemon-side atlas cells already filtered by privacy_min_providers. It never derives or requests provider endpoints.

Next:
Wire authoritative own-file assignment/transfer events into qrxSetOwnShardRoutes, then add QRX Drive file browser/upload UX and live upload/download progress.
