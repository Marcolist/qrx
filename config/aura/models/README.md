# QRX AURA Model Catalogs (Genesis 0.0.9)

## Beginner contract

A normal user selects only **AURA Automatic** (plus Eco/Balanced/Performance and an optional disk-cache limit). Model family, quantization, origin, runtime and provider placement are internal decisions.

`official-origin-catalog.qrx` is the zero-touch catalog. Its AUTO entries must be non-gated and carry explicit memory, measured-throughput and task-quality hints. Small GGUF entries provide graceful single-host operation before the network grows into larger Local/Cluster/MoE classes.

`optional-gated-origin-catalog.qrx` is Advanced-only. It may contain models such as Meta Llama whose official access/license flow can require explicit user/operator action. These entries have `auto_default=0` and may never block beginner first-run.

## Fetch order

Local cache -> LAN -> current MoE pod -> nearby QRX providers -> QRX Drive -> governed immutable external origin.

The external origin is bootstrap/fallback, not the long-term distribution layer. An authorized seed resolves `main` to an immutable upstream revision, imports only the governed file/quantization, content-addresses it, publishes QRX manifests and lets the replication controller distribute it.

## Why GGUF for Nano/Edge

The zero-touch small set uses GGUF variants that can share the same llama.cpp execution family across CPU, NVIDIA/CUDA and Apple Metal. This avoids forcing beginners to understand GGUF, MLX, CUDA or model conversion. MLX remains available as a separate runtime for models/packages that are intentionally published in an MLX-compatible form.
