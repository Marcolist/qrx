# QRX Upscaler — third-party components and licence policy

**Policy: permissive licences only. No copyleft component is linked into or
shipped with QRX.**

Concretely that rules out GPL entirely, and it rules out LGPL as a *bundled*
dependency, because static linking an LGPL library imposes relinking
obligations on the QRX distribution that QRX does not want to carry.

## What the Upscaler is built from

| Component | Role | Licence | Class | Bundled? |
|-----------|------|---------|-------|----------|
| `qrx_upscaler.c` / `.h` | Resampling core, PPM/PGM I/O, batch orchestration | **MIT** | permissive | yes, QRX source |
| `qrx_upscaler_main.c` | CLI front end | **MIT** | permissive | yes, QRX source |
| libpng | PNG decode/encode | **libpng (PNG Reference Library licence)** | permissive, BSD-style | optional, linked if present |
| zlib | Deflate, pulled in by libpng | **zlib licence** | permissive | optional, via libpng |
| libm | `sin()` for kernel table construction | platform C library | — | platform |

If libpng is not found at configure time, the build simply reports
`PPM/PGM only` and everything else still works. There is no hard dependency on
anything outside the C standard library.

## Why there is no bundled video codec

Frame extraction and muxing need a container and codec stack. The realistic
options are FFmpeg (LGPL, and GPL in most distributed builds), GStreamer
(LGPL) and libav derivatives. All of them are copyleft, so under the policy
above none may be bundled.

QRX therefore ships **no** video codec. The `video` subcommand drives an
external media tool that the operator installs and names explicitly:

```
export QRX_UPSCALER_MEDIA_TOOL=/usr/bin/ffmpeg
qrx-upscaler video in.mp4 out.mp4 --scale 2
```

That tool runs as a **separate process**, invoked through `execv`/`_spawnv`
with an argument vector. It is never linked, never bundled, and never invoked
through a shell command string. Running a separate program does not make QRX a
derivative work of it, so the user's choice of media tool has no effect on
QRX's own licensing.

`image` and `batch` modes need no external tool at all.

### Permissive alternatives, if bundling is ever wanted

Should QRX later want self-contained video support without changing the
policy, these are the permissive building blocks:

| Purpose | Candidate | Licence |
|---------|-----------|---------|
| AV1 decode | dav1d | BSD-2-Clause |
| AV1 encode | libaom | BSD-2-Clause |
| VP8/VP9 | libvpx | BSD-3-Clause |
| H.264 | OpenH264 | BSD-2-Clause (patent terms apply separately) |
| MP4 container | minimp4 | public domain / MIT-style |

None of these is currently used; they are listed so the decision is a
deliberate one rather than an accident.

## AI model backend

The classical kernels below are implemented in QRX and carry no external
dependency. An AI upscaling backend is a separate, later step and is subject
to the same policy plus the existing AURA model rules:

| Candidate runtime | Licence | Class |
|-------------------|---------|-------|
| ONNX Runtime | MIT | permissive |
| ncnn | BSD-3-Clause | permissive |

Model **weights** carry their own licence/provenance obligations, independent of the runtime. Any
model reaching the governed QRX catalog must pass the existing provenance and
licence policy checks; a governance approval cannot override a licence deny
rule.

Starting with 0.0.9.59, the macOS Apple-Silicon release bundle stages two reviewed models from the official Real-ESRGAN `v0.2.5.0` release: `realesr-animevideov3-x2` and `realesrgan-x4plus`. Their exact `.param`/`.bin` SHA-256 values and the source archive SHA-256 are written into QRX model/provenance manifests at packaging time. Real-ESRGAN is distributed under BSD-3-Clause; the exact upstream license material from the pinned release bundle is copied into the wallet resources. GFPGAN weights are deliberately not bundled.

## Algorithms

The resampling kernels are standard, long-published signal-processing
constructions implemented directly from their definitions, not copied from any
existing codebase:

- **Lanczos-3** — windowed sinc, `a = 3`.
- **Mitchell–Netravali bicubic** — `B = C = 1/3`, from the 1988 SIGGRAPH
  paper describing the family.
- **Bilinear** and **nearest neighbour** — trivial.

All are evaluated once at table-build time and quantised to fixed-point
integers. The pixel loop is pure integer arithmetic, which is what makes the
output reproducible across hosts.

## Attribution requirements

libpng and zlib both require their copyright notices to be preserved in
distributed binaries. A release that links them must ship the corresponding
notice text alongside the QRX licence. Neither requires source disclosure of
QRX itself.


## 0.0.9.60 multi-platform packaging
The verified bundle now covers macOS ARM64/x64, Windows x64, Linux x64 and Linux ARM64. Portable binaries are taken only from the official Real-ESRGAN v0.2.5.0 release where an upstream artifact exists. Linux ARM64 has no official portable artifact in that release, so QRX builds the runtime from the pinned Real-ESRGAN-ncnn-vulkan v0.2.0 tag (commit prefix `37026f4`) with its pinned git submodules. All targets use the same reviewed x2/x4 model bytes from the official Ubuntu release asset and record model/runtime SHA-256 plus provenance in the bundle.
