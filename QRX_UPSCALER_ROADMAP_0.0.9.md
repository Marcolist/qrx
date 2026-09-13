# QRX Upscaler — Roadmap entry (0.0.9 branch, post-Genesis delivery)

**Status: LOCAL FOUNDATION IMPLEMENTED / AI + DISTRIBUTED MODE PREPARED.**
The 0.0.9 branch now ships the deterministic local upscaler core, image/batch
processing, video frame orchestration, host capability detection and the wallet
Apps entry. AI inference and distributed provider execution remain separately
gated work; capability detection must never imply that a model/runtime is
already installed or validated.

---

## 1. What it is

**QRX Upscaler** is an application in the wallet's **Apps** area. It enlarges
images with AI models and processes video by splitting it into individual
frames, upscaling those frames as a batch, and reassembling them into a video.

It can run in two modes:

| Mode | Where the work happens | Requires |
|------|------------------------|----------|
| **Local** | The user's own machine, through the existing verified AURA runtime | Nothing beyond a compatible signed runtime |
| **Distributed** | Parallel frame batches across QRX providers, with QRX Drive for frame/asset transport and QRX-Net for discovery | `COMPUTE_POUC_V1`, plus `DRIVE_V1` and `QRX_NET_V1` for transport/discovery |

Local mode is the default and must stay useful on a single modest machine, in
the same spirit as AURA Nano/Edge models. Distributed mode is an accelerator,
never a requirement.

---

## 2. Local 0.0.9 foundation vs. post-Genesis activation

The distributed mode is economically and cryptographically meaningful work
handed to third-party providers. That is exactly what Proof of Useful Compute
governs. Shipping an app that pays providers for frame batches before
`COMPUTE_POUC_V1` is active would either bypass the activation model or create
an unmetered side channel for compute rewards.

The activation architecture is therefore **unchanged**:

```
LOCKED -> NOT_READY -> SOAKING -> WAITING_TARGET_DATE
       -> READY_FOR_GOVERNANCE -> SCHEDULED -> ACTIVE
```

* Local classical upscaling has no consensus or economic effect and ships in
  0.0.9. AI execution is permitted only when a compatible runtime/model is
  locally available and validated; otherwise the app falls back to classical.
* Distributed upscaling is **fail-closed** until `COMPUTE_POUC_V1` reaches a
  governance-committed activation height. Before that the app may register
  interest and show readiness, but must not create compute escrows, jobs or
  rewards.
* No new emission. Upscaling jobs are paid from the user's own authorized
  compute escrow under the existing PoUC rules. Tokenomics are untouched:
  0.25 QUB / 10 s, 25,000,000 atoms, service layers mint nothing.

Target: **not before COMPUTE_POUC_V1 (31 Jan 2027+)** for distributed mode.
The `+` matters: a date never activates anything by itself.

---

## 3. Pipeline

```
input (image or video)
   |
   +-- image  --> single upscale job
   |
   +-- video  --> demux to frames + audio track
                     |
                     v
                  frame batches (deterministic, content-addressed)
                     |
              +------+------+
              |             |
           local         distributed
           runtime       provider fan-out
              |             |
              +------+------+
                     v
              upscaled frames
                     |
                     v
              reassemble + remux original audio
                     |
                     v
                   output
```

Frames are content-addressed, so a batch that fails or is returned by a
dishonest provider can be re-issued without redoing the whole video, and an
identical retry can return the committed result through the existing durable
job/result journal.

---

## 4. Security requirements (binding on any future implementation)

These are written now so the implementation cannot quietly skip them.

**Model and runtime trust.** Upscaling models are ordinary AURA models: signed
catalog entry, content-addressed manifest, SHA3 verification, publisher trust
root bundled with the application, license and provenance policy, governance
decision. No "download the newest file named upscaler.pth".

**Untrusted input is untrusted.** Video and image containers are attacker-
controlled data. The decoder is the largest attack surface in this whole
feature. Demuxing/decoding must run in the existing sandbox, must enforce hard
limits on resolution, frame count, duration and total decoded bytes, and must
never be able to terminate the wallet or node process. The same rule as the
P2P transaction path applies: untrusted input never reaches `exit()`,
`abort()` or `die()`.

**Path safety.** Frame filenames, container metadata and provider-returned
asset names are untrusted strings. No `../`, no absolute paths, no symlink
escape, no archive traversal. Output paths are derived locally, never taken
from a remote response.

**No shell command strings.** Frame extraction and reassembly must never use
`system()`/`popen()` or concatenate untrusted input into a shell command. The
current local video path may invoke an operator-selected external media tool as
a separate process through `execv`/`_spawnv` with an argument vector; it is not
linked into QRX Core.

**Provider honesty.** A distributed frame batch is a PoUC job and inherits
verification, challenge and slashing. A provider returning wrong or
recycled frames must be detectable: per-frame content roots bound to the job,
spot verification, and no settlement above the escrowed budget.

**Privacy.** Frames of a user's private video are user data. Distributed mode
must be explicit opt-in per job, must state plainly that frame content leaves
the machine, and must default to local for anything the user has not opted in.
QRX Drive transport uses the normal encrypted key-envelope path.

**Youth safety and content policy** follow the existing QRX-Net layers; an
upscaler must not become a bypass around them.

---

## 5. Acceptance criteria for the remaining AI/distributed work

1. Local classical image upscale works on a machine with no GPU. **Met.**
2. Host profiles use one `apple-silicon` compatibility profile for every Apple
   M generation, while retaining generation/variant only as tuning metadata;
   Raspberry Pi 5 is supported and ODROID-N2+ is explicitly experimental for
   Mali-G52/PanVK with fallback when Vulkan/render-device probing fails. **Met.**
3. Video round-trip preserves duration, frame rate and audio.
4. Malformed/hostile media corpus: decoder survives, process stays alive,
   documented resource limits hold.
5. Path-traversal corpus over frame names and provider responses: no write
   outside the designated output directory.
6. Distributed mode proven fail-closed before `COMPUTE_POUC_V1` activation
   height, with a regression test in the same style as the staged-activation
   tests.
7. Reward conservation: settlement never exceeds escrow; no new QUB minted.
8. No `system()`/`popen()` anywhere in the feature.

---

## 6. Explicit boundary for 0.0.9 Genesis

The local classical Upscaler and hardware capability probe are shipped
application functionality. AI model/runtime delivery and QRX Compute fan-out
remain non-consensus app/runtime work, while paid distributed execution stays
fail-closed behind `COMPUTE_POUC_V1`.
