# QRX 0.0.9.95 — Windows Dependency Artifact Resolver

Windows native dependency builds no longer assume that an upstream CMake project installed its artifacts into exactly the requested layout.

For zlib the builder now:
- repeats the hermetic QRX dependency prefix with `cmake --install --prefix`;
- requests static zlib explicitly;
- resolves artifacts from the QRX dependency prefix, the current zlib build tree, and that build's `install_manifest.txt`;
- prefers `zlibstatic.lib` over ambiguous `z.lib` outputs;
- adopts the matching `zlib.h` and generated `zconf.h` into the QRX dependency prefix;
- never performs an unbounded scan of unrelated system installations;
- only fails after the current build cannot provide a usable static artifact plus matching headers.

This specifically handles zlib/CMake layouts that install into a different prefix while preserving the hermetic downstream libpng/curl/Core build inputs.
