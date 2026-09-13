# QRX 0.0.9.11 – AURA Files, Projects & Artifact Creation

Implemented foundation:

- immutable AURA Artifact metadata records
- QRX Drive object references
- PRIVATE_PQ flag carried with artifact/project metadata
- type metadata for text, documents, spreadsheets, presentations, code,
  archives, images, data and other artifacts
- MIME type and extension metadata
- source job reference
- content commitment field
- domain-separated SHA3-256 artifact commitments
- AURA Project manifests
- safe relative project paths
- duplicate path rejection
- traversal / absolute path rejection
- artifact-to-project binding through artifact commitments
- domain-separated SHA3-256 project commitments
- write-artifact Tool API preparation
- AURA Chat artifact attachment

Security invariants:

- artifact contents remain outside consensus metadata
- QRX Drive references are used rather than embedding file content
- project paths may not escape the project namespace
- immutable artifact records are required by V1
- Wallet seeds/private keys are not artifact metadata
- PRIVATE_PQ remains available for generated user files
- AURA file creation still goes through scoped Tool API / ordinary jobs

Validation:

- compute_phase136_aura_artifacts: PASS
- compute_phase135_aura_chat: PASS
- compute_phase134_aura_runtime: PASS

The focused validation does not claim a fresh full-suite pass.
