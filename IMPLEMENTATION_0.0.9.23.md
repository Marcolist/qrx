# QRX 0.0.9.23 — QRX OS Compute Integration

Status: DONE — integration protocol foundation (2026-09-09)

This milestone binds the four QRX service planes used by QRX OS applications:

- QRX Wallet: user identity and payment authority
- QRX Drive: private data, artifacts and project/model references
- QRX Compute: deterministic job graphs and execution
- QRX Chain: escrow, proof/settlement references and fee ceilings

Implemented in `qrx-core/src/compute/qrx_os_compute.[ch]`:

- versioned QRX OS compute workspace descriptor
- application classes: Chat, Coding, Documents, Research, RAG, Build Tasks, AI Agents, Batch Compute
- deterministic required-service masks per application
- owner binding: job graph owner must equal wallet identity
- fee and expiry ceilings inherited from the workspace
- Drive namespace requirement for artifact-bearing applications
- chain escrow + settlement references are mandatory
- Build Tasks require both COMPILE and TEST nodes
- Chat V1 rejects write-artifact capability, keeping chat-only work non-mutating by default
- run lifecycle READY -> SUBMITTED -> RUNNING -> COMPLETED / FAILED, plus pre-run cancellation
- immutable result artifact binding through the existing AURA artifact commitment
- domain-separated SHA3-256 commitments:
  - `QRX/OS/COMPUTE-WORKSPACE/V1`
  - `QRX/OS/COMPUTE-RUN/V1`

Security/integration boundary:

This milestone is a Core integration contract. It does not claim that QRX OS GUI applications, wallet signing prompts, live Drive upload/download transport, compute provider execution, or on-chain settlement RPCs are all wired end-to-end in a production QRX OS build yet. Those components use the already-defined subsystem APIs and are wired in later UI/runtime phases.

Validation:

- new test: `compute_phase148_qrx_os_compute`
- full registered CTest suite: 81/81 PASS
- developer build used `-DQRX_REQUIRE_PQC=OFF` only because the CI/container OpenSSL is not the production QRX PQC toolchain; production/release policy remains OpenSSL >=3.5 with required PQ support.
