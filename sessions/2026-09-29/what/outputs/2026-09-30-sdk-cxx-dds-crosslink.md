# SDK C++ and shared DDS cross-link checkpoint

The exact official SDK v0.43 ZIP already supplies a C++ payload route: 1,017 libc++ header members including span/vector/filesystem/exception, plus libc++.a/libc++abi.a/libunwind.a/libc++experimental.a. Its [C++ wrapper](https://github.com/ps5-payload-dev/sdk/blob/d9c9519116944a7f1c22d262012c53b80b3520b7/host/bin/prospero-clang%2B%2B) selects target headers, RTTI/exceptions/emulated TLS and the matching static runtime. [SDK usage/samples](https://github.com/ps5-payload-dev/sdk/tree/d9c9519116944a7f1c22d262012c53b80b3520b7/samples/hello_cxx23) provide a primary community precedent, not target evidence for our code.

Independent read-only agent verified these members directly in the pinned 8,814,217-byte archive (SHA-256 a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb), then matched extracted copies:

| Runtime | Bytes | SHA256 |
|---|---:|---|
| libc++.a |2,304,096|b1480e6edcb7cffba5ea999cb13da0d40b0def4c215ead442649bba71d32414f|
| libc++abi.a |692,520|52f58964c3bf5e283d05e0f58aa7c8fc43a3dbb16302f3a254589cdc1500aa42|
| libunwind.a |130,658|e57385a4093a064b10cb53b5295c9e51205821ecd8dbd2edad2d43275f67261b|

Configuration reports libc++ 180100, ABI 1/namespace __1, filesystem/threads enabled and timezone database disabled. No STL rebuild is needed merely to obtain these components. A prior root rg inventory omitted hidden/ignored scratch; absence from that output was not absence from the SDK.

Root then compiled unchanged production `src/dds.cpp` and `src/hash.cpp` with the SDK wrapper, C++20, -Os and warnings enabled. The initial tool-selection failure was a missing clang++ driver name in the earlier C-only selective LLVM extraction. A development-only relative clang++→clang alias to the already hash-verified LLVM 21.1.8 binary resolved it; no SDK source/global installation changed.

Both objects compiled successfully without diagnostics. A throwaway owned-fixture C++ main using the same parser/neutral round trip and a throw/catch path cross-linked successfully with the bundled static runtime. It was **never dispatched or executed**, on either Mac or PS5.

Cross-linked ELF: 1,463,656 bytes, SHA-256 `8db67552476bd526e5dcaa95ea9603234736dafaa5d8703240ad1693bea1cd88`, ELF64 x86-64 ET_DYN, entry 0. Loads RWE/RW/RW, aligned 16 KiB; largest load file end 1,032,744. Header end 64/program table 288/section table 1,463,656 all fit the section-derived loader prefix 1,463,656. Retained scratch layout/import/source receipt identifies production DDS SHA-256 d9acfee4ff4fb1fcbbe7abfdc2cec07a4cc867a13477c291d5c824214cab60b4 and hash.cpp SHA-256 97414506d67824e7b6de42f58a3fb08bde32f201b60d31efd9da838f4de517f3.

Selected dynamic imports include allocation/locale/stdio/filesystem/pthread and compiler helpers, in addition to normal SDK dependencies. Successful linking does not prove their firmware 13.60 runtime resolution, exception unwinding, emulated TLS, allocator or filesystem behavior. Full core additionally needs a target libarchive/codec port; desktop -larchive cannot be reused on PS5.

Next step: a C ABI seam into the reviewed native status host, keeping C++ buffers/handles/exception containment inside one artifact, with on-console parser/round-trip/exception tests. This is a practical native path, not a Windows translation layer or a verified PS5 texture writer.
