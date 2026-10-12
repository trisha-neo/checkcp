# Vendored BLAKE3 (official C implementation)

These files are copied verbatim from the upstream BLAKE3 reference repository,
tag **1.8.5**, from its `c/` directory (plus the root license files):

    https://github.com/BLAKE3-team/BLAKE3/tree/1.8.5/c

Included sources:

- `blake3.h`, `blake3_impl.h`
- `blake3.c`, `blake3_dispatch.c`, `blake3_portable.c`
- `blake3_neon.c` — ARM NEON path (compiled on arm64/aarch64)
- `blake3_{sse2,sse41,avx2,avx512}_x86-64_unix.S` — x86-64 SIMD assembly

The build selects the right SIMD sources by architecture (see the Makefile /
CMakeLists.txt): NEON on arm64, the x86-64 `.S` files on x86_64, and the pure
portable path everywhere else. Nothing here is modified from upstream.

License: dual-licensed CC0-1.0 and Apache-2.0 (with the LLVM exception).
See `LICENSE_CC0`, `LICENSE_A2`, `LICENSE_A2LLVM`.

To update: re-download the same file list from a newer tag and bump the version
noted above.
