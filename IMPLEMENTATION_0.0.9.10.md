# QRX 0.0.9.10 – AURA Chat Protocol Foundation

Implemented in core compute layer:
- conversation/session binding
- encrypted context references (plaintext chat is not a consensus object)
- assistant messages backed by normal AURA MODEL_INFERENCE tool calls
- streaming sequence state
- hard per-conversation budget accounting
- cancel
- branch conversations
- job references and generated artifact references
- deterministic SHA3-256 chat-message commitments

Security / architecture:
AURA Chat does not bypass the QRX Compute Job Protocol, provider market,
escrow, sandbox or PoUC verification. Content is represented by QRX Drive
references; wallet private keys are never chat context.

Next:
0.0.9.11 Files, Projects & Artifact Creation, including typed artifact manifests
and QRX Drive/native + S3-compatible addressing.
