# Native format dispatch v1

This synchronous, read-only C module chooses a trusted built-in format operation from bounded captured bytes. It has no game, title or mod identity input. The first adapter wraps the existing classic-v11 PAK candidate validator; it does not implement an engine runtime or an installer.

`Supported` describes only the requested format operation and its explicit scope. A matching footer is `Unverified` until the actual candidate finishes. Successful metadata validation yields `classic_v11_metadata`; successful validation of one requested payload adds `selected_payload`. Engine/build and asset/platform states stay `Unverified`, installation authority stays `Unsupported`, and runtime, installation, authenticity and game-acceptance flags stay false. Declared archive SHA1 checks detect mismatches against those declarations; they do not establish authenticity. Path-hash semantics and other payloads remain unchecked.

The descriptor is `format.unreal.pak.classic_v11` / `1.0.0`, format `unreal_pak`, profile `classic_v11_unencrypted_stored_sha1_v1`. The existing profile accepts unencrypted classic-v11 metadata and selected None/Zlib payloads. Encrypted/other known versions are unavailable; unknown bytes, including unrecognized IoStore inputs, have no implemented route. A valid archive containing PC assets receives no platform, game or install approval.

## API and ownership

Include `dispatch.h`. All POD headers require exact `struct_size` and ABI version1; reserved fields must be zero. Registration strings are fixed NUL-terminated, zero-padded ASCII. IDs use `[a-z0-9_.-]+`; versions contain numeric dot-separated components.

1. Build trusted registrations with `mb_pak_adapter_registration_v1()` and seal them using `mb_registry_seal_v1()`. The registry copies metadata/function pointers, sorts unique IDs and accepts at most16 entries. Use a nonzero generation and the actual trusted execution context. The registry must remain immutable and alive for all operations. Its seal marker is structural validation, not authentication. No downloaded registration, plugin loading or arbitrary callback address is supported.
2. Capture immutable prefix/tail windows of at most4096 bytes each. Prefix starts at0, tail ends at the declared logical size, and a nonzero source token binds the caller-asserted immutable snapshot. No pointer is retained. `mb_dispatch_probe_v1()` calls each eligible trusted callback at most once. Callbacks charge before inspecting bytes with `mb_probe_charge_v1()` and share one unreplenished budget of at most131072 steps. The classic-v11 footer probe charges9 inspected bytes for a candidate.
3. Use exactly one operation: `MB_FORMAT_OP_METADATA` or `MB_FORMAT_OP_SELECTED_PAYLOAD`. Ambiguous, incomplete, exhausted or callback-error probes cannot select a conclusive positive route. No validation failure falls through to another adapter.
4. Pass a candidate to `mb_pak_adapter_validate_v1()` with the same registry generation/context/descriptor/slot and source size/token. Metadata requires `want_selected=0,index=0`; payload validation requires `want_selected=1` and a valid directory-row index. The wrapper calls `pak_validate_v11_candidate()` exactly once. Caller supplies the existing disjoint candidate buffers, its limits, one unreplenished codec budget and trusted provider/cancellation backing. No heap allocation, source write, saved pointer or asynchronous operation is introduced.
5. Serialize an owning result with `mb_core_format_report_json()`. Its required length excludes NUL; a size query or short buffer returns `MB_CORE_BUFFER_TOO_SMALL`, and short output is empty. An output capacity up to8192 is accepted. Serialization does not re-probe, read the source or consume work. It uses the existing core status convention and reports four separate scoped states, constant causes and consumed counters.

Known aliases or invalid backing/ABI fail preflight without writes. A stale source/registry binding invokes no provider and publishes an Unverified binding-mismatch receipt; child buffers remain untouched and unpublished. Once the candidate enters its operation, its existing whole-buffer cleanup contract applies: every operational failure clears the sink/rows/names/scratch, including a child `BAD_ARGUMENT` raised after reads. No caller may reuse an earlier positive receipt or sink following a failed call. Arbitrary C backing, provider snapshot immutability and stateless built-in callback behavior are trusted caller obligations.

Probes perform no I/O. Integration must put the capture reads and candidate reads under one outer charged provider ledger, reserving full requests and failed attempts before callbacks without reset/refund. The candidate's inner read limits and the codec budget are additional bounds, not an outer aggregate ledger. Cancellation is cooperative at existing candidate boundaries; the fixture observed16 charged reads before an after-read cancellation check. This module does not supply a native transfer/quarantine owner, target observation or HTTP endpoint.

`host_fixture` records describe host fixture execution. `ps5_native` is the context supplied by an actual native owner; selecting that constant or compiling a target caller establishes no PS5 runtime proof. Format facts belong in operation details/inventory. They must not become planner translation capabilities or a Supported title/mod badge.

## Focused verification

Run from the repository root with the read-only pinned Mbed3.6.7 and Zlib1.3.2 source roots:

```sh
python3 outputs/ps5-mod-bridge/native-host/format/verify.py \
  --mbedtls-source .superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5 \
  --zlib-source .superpowers/native-dependency-spike/native-zlib-codec-experiment-1/upstream/zlib-1.3.2 \
  --output-dir outputs/ps5-mod-bridge/native-host/format/build/green
```

The focused driver freshly compiles the current PAK closure and matching dependencies, checks pinned source correspondence, executes five host fixture groups and parses actual probe/validated JSON. Tests cover both retained real archives, metadata and selected2048-byte payloads, empty metadata, registry validation, ambiguity/incomplete captures, exact/one-short budgets, stale bindings, cancellation, early/late source failures, declared-digest failure, no-write aliases and report sizing. `--red` uses the retained missing-implementation stub and requires an actual compiled behavioral failure. Build products/logs/receipts stay in the supplied output directory.

For PS5 compile/link only, add the pinned paths:

```sh
python3 outputs/ps5-mod-bridge/native-host/format/verify.py \
  --mbedtls-source .superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5 \
  --zlib-source .superpowers/native-dependency-spike/native-zlib-codec-experiment-1/upstream/zlib-1.3.2 \
  --output-dir outputs/ps5-mod-bridge/native-host/format/build/native \
  --ps5-sdk outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk \
  --llvm-config outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config \
  --resource-dir .superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21
```

This mode never runs its ELF. It verifies x86_64 FreeBSD ELF, BIND_NOW and the exact observed libc imports. `free` is retained by Mbed's unused `mbedtls_zeroize_and_free` helper in `platform_util.c`; the invoked SHA1 and caller-arena Zlib paths do not use that helper. There is no claim of target runtime, native provider behavior or successful deployment. Development compilers and host fixtures do not change the finished application's PS5-only runtime requirement.
