# QRX 0.0.9.86 – Linux OpenSSL lib64 Layout Hardening

## Problem
On some Linux x86_64 distributions OpenSSL may default to installing static libraries below `lib64/`, while the QRX hermetic dependency ABI expects `$PREFIX/lib`. This caused the post-build guard to stop with `Dependency artifact missing: libcrypto.a`.

## Fix
The hermetic OpenSSL Configure invocation now explicitly uses `--libdir=lib`. This makes `libcrypto.a` and `libssl.a` land in the same canonical `$PREFIX/lib` directory consumed by QRX Core, CMake, curl and Rust/OpenSSL integration.

The existing artifact checks remain strict; the fix removes the layout ambiguity rather than weakening verification. On a prefix produced by 0.0.9.85 with OpenSSL in `lib64`, rerunning the build causes the OpenSSL reuse check to fail safely and rebuild OpenSSL into the canonical `lib` location.

## Scope
Linux/macOS hermetic dependency builder only. macOS already uses the same canonical layout; Windows is unchanged.
