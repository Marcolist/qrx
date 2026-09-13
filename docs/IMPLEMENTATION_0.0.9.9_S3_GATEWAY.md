# QRX Drive S3 Compatibility Gateway — 0.0.9.9

Status: FOUNDATION / ROADMAP INTEGRATION

Goal: expose QRX Drive through a practical S3-compatible facade while preserving
QRX-native storage, PRIVATE_PQ encryption, erasure coding, PoStor, repair and
provider economics underneath.

## Compatibility V1
- AWS Signature Version 4 authentication
- ListBuckets / CreateBucket / DeleteBucket
- PutObject / GetObject / HeadObject / DeleteObject
- ListObjectsV2
- Range GET
- user metadata
- multipart create/upload-part/complete/abort
- presigned GET / PUT
- object version identifiers
- scoped, revocable S3 credentials

## Security invariants
- Never use a QRX wallet seed/private key as an S3 secret.
- S3 credentials are derived/issued capabilities with bucket/prefix/action scope,
  expiry and revocation.
- Gateway authorization cannot grant rights beyond the QRX Drive owner policy.
- PRIVATE_PQ encryption remains below the compatibility layer.
- Request signatures and canonical requests are verified before storage mutation.
- Multipart completion verifies all referenced parts before publishing an object.

## Namespace mapping
S3 bucket -> QRX Drive namespace
S3 object key -> object path/key inside that namespace
S3 version id -> immutable QRX Drive object/manifest version
ETag -> compatibility metadata; it MUST NOT replace QRX content/Merkle commitments.

## Explicit non-goals for V1
Full AWS IAM emulation, S3 Select, Lambda integrations, Glacier semantics,
AWS billing APIs and AWS-specific replication control planes.

## Native extensions
Provider policy, proof inspection, resilience policy, .qrx hosting integration,
PUBLIC_SIGNED publishing and other QRX-native features remain available through
the native QRX Drive API.

## Migration
The target is compatibility with standard S3 SDKs/tools through a custom endpoint,
including migration/synchronization workflows. Compatibility claims must be
validated against real clients before release.
