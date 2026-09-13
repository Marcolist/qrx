# QRX 0.0.9 Genesis Freeze — AURA Nano/Edge Zero-Touch Completion

Date: 2026-09-11

This freeze patch closes the small-host usability gap without changing the consensus schedule.

## Delivered

- Model-origin catalog V2 with memory, throughput and task-quality metadata.
- Real zero-touch GGUF entries for Qwen 0.6B, Qwen 1.7B, Qwen 4B, DeepSeek R1 Distill Qwen 1.5B and Ministral 3B.
- Task-aware automatic model selection for Nano/Edge hosts.
- A separate Advanced/gated catalog for Meta Llama; normal AUTO cannot select it.
- Deterministic single-quantization import using a catalog file filter.
- GGUF-aware manifest/source-map/origin fallback.
- llama.cpp Metal adapter kind for Apple Silicon; Metal is no longer conflated with MLX.
- Existing CPU and CUDA llama.cpp adapter paths remain intact.
- Phase176 regression coverage for 3–8 GiB classes and pinned GGUF import.

## User experience

The beginner-facing choice remains:

`AURA Compute: Automatic` + `Eco | Balanced | Performance` + cache budget.

No model name, Hugging Face URL, quantization, CUDA/Metal/MLX choice or manual model directory is required in the intended packaged product.

## Packaging boundary

The model weights are fetched on demand and then replicated through QRX; they are not embedded in the source archive. Release installers/CI must publish or bundle verified runtime packages for the supported CPU/CUDA/Metal targets. The signed runtime package manager already performs hardware matching, content verification and one-click activation; native platform build/signing remains a release-host job.
