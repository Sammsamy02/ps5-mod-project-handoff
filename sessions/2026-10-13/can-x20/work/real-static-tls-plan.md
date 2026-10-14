# Unchanged DLL static TLS preparation check

**Goal:** Measure whether the staged platform-neutral static TLS registration code accepts five unchanged Windows x64 DLLs after genuine PE section mapping and base relocation, without executing them.

**Stage:** `cg-real-tls-evidence-20261013-10`, copied from the isolated static TLS core stage. Add a bounded C++ host harness and a Python runner. The harness reads each source DLL, validates PE/header/section/relocation bounds, copies sections to private memory, applies DIR64/HIGHLOW base relocations, then calls `register_mapped_static_tls`. It verifies the assigned index, copied template and zero-fill on two threads, and callback targets inside executable sections. The runner pins SHA-256 before and after each case and reports expected callback counts. No console calls, imports, callbacks, entry points, or source binary writes.

**Acceptance:** All five DLLs produce a successful TLS preparation result with original hashes unchanged, or a precise failure that identifies a generic missing requirement. The result cannot be described as successful DLL loading or gameplay.
