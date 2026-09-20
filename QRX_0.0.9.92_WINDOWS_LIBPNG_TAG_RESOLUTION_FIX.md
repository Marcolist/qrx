# QRX 0.0.9.92 — Windows libpng tag resolution fix

- Keeps the strict maintainer-published archive SHA-256 check.
- If the archive bytes do not match, clones the exact upstream `v1.6.58` tag with `--branch ... --single-branch --depth 1`.
- Removes the redundant hand-built `refs/tags/...` fetch that produced `v/tags/v1.6.58` on the failing Windows path.
- Verifies both pinned commit `3061454d980de7d53608f594194cfac722721d2a` and exact tag name before CMake is allowed to build libpng.
- No unverified source bytes are accepted.
