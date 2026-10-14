# Bounded Mbed TLS entropy feasibility — 2026-09-30

## Result

The bounded custom-source design is buildable and linkable for the PS5 SDK target with all `mbedtls_entropy_init()` defaults disabled. In the isolated probe, the application explicitly adds one strong source; that source performs one `sysctl(KERN_ARND)` attempt for 64 bytes per callback and returns an error on a failed, empty, short, or oversized response. A deterministic host fake-source run passed the no-default-source/no-callback-before-registration assertions, callback error handling, and both failed and successful default CTR_DRBG seed checks.

This establishes source compilation, static archive creation, SDK ELF linking, and local callback/API control flow. It does not establish that PS5 firmware resolves or permits `sysctl(KERN_ARND)`, returns 64 bytes, or supplies cryptographically strong entropy. The SDK ELF was not executed or dispatched.

## Pinned inputs and isolation

- Mbed TLS tag `mbedtls-3.6.7`, commit `068ff080b369adfac81509f9b57b2afabaf82dc5`; local codeload archive SHA-256 `ca6bd316bbec49ef20088f39b8755fcaec0b7e45781506de55b17cd191ae6937`.
- Framework submodule commit `dde0c4a0e448a0552f18817dcea633bb851fd288`; local archive SHA-256 `3b0d4864fa877dc9165b90ff3d5b68505c6176c0b22d317d99fe8c89a993e9fd`.
- Both source archives were extracted into the new ignored subtree `.superpowers/native-dependency-spike/bounded-entropy-feasibility/{target,host}/source/`. In each copy, the only Mbed TLS configuration change is uncommenting `MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES` in `include/mbedtls/mbedtls_config.h`. The SDK, shared tool cache, original dependency spike, and product sources/configuration were not changed by this probe.
- SDK compilation used cached SDK v0.43 `prospero-clang`, cached LLVM 21.1.8 `llvm-config`, and the already isolated LLVM 21.1.8 resource directory. The host behavior fixture used the macOS `cc` compiler and the separate host-built copy.

## Configuration behavior

In pinned `library/entropy.c`, `mbedtls_entropy_init()` first clears `source_count`, clears the source array, and initializes the accumulator. Every built-in registration is then inside the outer `#if !defined(MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES)` block (lines 27–59). Within that block, the platform source has an additional `!MBEDTLS_NO_PLATFORM_ENTROPY` guard; the optional hardware and NV-seed registrations have their own guards. Therefore:

- `MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES` disables all built-in registrations, including platform, hardware-alt, and NV-seed sources.
- `MBEDTLS_NO_PLATFORM_ENTROPY` only suppresses the platform poll function/source. By itself it does not suppress enabled hardware-alt or NV-seed sources.
- `MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES` does not compile out the platform poll implementation. `library/entropy_poll.c` has its own `!MBEDTLS_NO_PLATFORM_ENTROPY` guard.

The target build defines `MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES` and leaves `MBEDTLS_NO_PLATFORM_ENTROPY` undefined. Its preprocessed `mbedtls_entropy_init()` contains no source-registration calls. The default `entropy_poll.o` still exists in the static archive and has an unresolved `sysctl`, as expected with platform polling compiled in. A separate SDK compile with `-DMBEDTLS_NO_PLATFORM_ENTROPY` produced an empty `entropy_poll_no_platform.o` with no defined poll function and no undefined imports. The linked probe map does not extract upstream `entropy_poll.o`; it extracts `entropy.o` and the owned source objects instead.

The host fixture resets both fake-reader and source-callback counters before calling `mbedtls_entropy_func()` immediately after initialization. It asserts `MBEDTLS_ERR_ENTROPY_NO_SOURCES_DEFINED` and that both counters remain zero, so the no-default-source result is not inferred from a stale counter. After one explicit `mbedtls_entropy_add_source(..., MBEDTLS_ENTROPY_SOURCE_STRONG)` registration, the entropy call succeeds.

## Bounded source behavior

The owned probe callback is in `.superpowers/native-dependency-spike/bounded-entropy-feasibility/bounded_entropy_callback.c`; the target adapter is in `target_sysctl_source.c`.

- Mbed TLS calls each registered source with a 128-byte output capacity (`MBEDTLS_ENTROPY_MAX_GATHER`). The owned callback requests only `MBEDTLS_ENTROPY_BLOCK_SIZE` bytes from its reader, which is 64 bytes in this SHA-512 accumulator build.
- The target reader makes one call to `sysctl()` using `{ CTL_KERN, KERN_ARND }`, with the requested length supplied through `oldlenp`. There is no polling loop, retry, `/dev/urandom` fallback, or call to Mbed TLS's platform poll function in the owned callback.
- The callback starts with `*output_length = 0`, accepts only `rc == 0` and exactly 64 returned bytes, records `errno` when `rc != 0`, and restores the caller's previous `errno`. It uses `rc` as the syscall status: if `rc == 0`, it ignores `errno` and decides from the returned length. The fixture covers `rc == -1, errno == EIO`, but does not cover `rc == 0` with nonzero `errno`. It maps a nonzero `rc` or invalid length to `MBEDTLS_ERR_ENTROPY_SOURCE_FAILED`, zeroizes the full supplied output capacity on failure, and never reports partial bytes as success. The source length is checked against both the callback capacity and `MBEDTLS_ENTROPY_MAX_GATHER` before the reader is called.
- `callback_calls` is incremented once immediately before the sole reader call. The target adapter contains one `sysctl()` call and no loop. A successful callback returns 64 bytes with a 64-byte strong-source threshold.

The Mbed TLS API describes `olen` as the actual number of bytes written and allows zero. Its gather implementation calls a source with a 128-byte buffer, updates the accumulator only for `olen > 0`, and trusts the callback-provided length. The owned callback's exact-length check is therefore important: it rejects zero/short responses instead of allowing repeated gathers and rejects oversize values before Mbed TLS could read past its 128-byte temporary buffer.

## Gather and DRBG call counts

This build selects the SHA-512 entropy accumulator: `MBEDTLS_ENTROPY_BLOCK_SIZE` is 64 bytes and `MBEDTLS_ENTROPY_MAX_GATHER` is 128 bytes. `mbedtls_entropy_func()` rejects requests over the 64-byte block size. It gathers before every successful return and requires both every registered source's threshold and at least 64 accumulated strong-source bytes. With the single source configured at a 64-byte threshold and returning exactly 64 bytes, one gather satisfies both conditions; the source is called once per `mbedtls_entropy_func()` call.

`ENTROPY_MAX_LOOP` is 256, and the loop checks `count++ > ENTROPY_MAX_LOOP` before gathering. If a custom callback repeatedly reports success without enough strong bytes, Mbed TLS can perform 257 gather passes and then fail on the next loop check. The owned callback instead returns an error on any incomplete read, so those cases stop after one read attempt.

With the default SHA-512 configuration, CTR_DRBG uses 48 bytes of entropy. The default AES key size is 32 bytes; 48 meets the implementation's 3/2-key-size condition, so the default nonce length is zero. `mbedtls_ctr_drbg_seed()` therefore calls its entropy function once for 48 bytes. The entropy module still gathers a full 64-byte strong block, so the custom source makes one 64-byte OS request for that seed. Default CTR_DRBG reseeding also requests 48 bytes once; the default reseed interval is 10,000 requests and prediction resistance is off. Explicit nonce lengths, prediction resistance, or other configuration can increase calls over a longer session, so the bound is per source callback and per entropy request, not a lifetime bound for arbitrary TLS use.

The host fixture also attempts CTR_DRBG seeding with a short fake read and asserts the pinned library returns `MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED` after exactly one read, then frees the failed DRBG as required by the API. It next seeds successfully and asserts exactly one source poll and one fake read for this pinned default configuration. The bytes are deterministic test data and are not evidence of entropy quality. The one-read result is a per-entropy-callback bound, not a total-lifetime budget. A product must account for every source use from one shared budget; for example, an additional raw 32-byte fetch plus this 64-byte DRBG seed draw would consume two reads. That combined product flow was not built or tested here, and successful 64-byte `sysctl(KERN_ARND)` behavior on the native target remains unproven.

## Error and zeroization notes

- A source callback failure propagates from `entropy_gather_internal()` and causes its 128-byte temporary buffer to be zeroized; `mbedtls_entropy_func()` zeroizes its 64-byte intermediate buffer on its exit path and copies output only after success.
- The owned callback also wipes the caller-provided capacity when its reader fails or returns an invalid length. Its fake-reader fixture confirmed zero, short, oversized, and `errno=EIO` outcomes each produce one read, zero output length, a zeroed output buffer, and `MBEDTLS_ERR_ENTROPY_SOURCE_FAILED`.
- Source-level caveat: `entropy_gather_internal()` directly returns if `entropy_update()` fails (line 230), bypassing its local-buffer cleanup label. That internal digest-error path was not injected. The normal callback-failure path goes through cleanup and is wiped.
- `mbedtls_ctr_drbg_seed()` documents that the caller must call `mbedtls_ctr_drbg_free()` after a failed seed. Its reseed helper zero-initializes the seed array, returns directly if its entropy callback fails, and reaches the final zeroizer only on the other paths. With `mbedtls_entropy_func()` as used here, an entropy failure does not copy partial output to the seed buffer; the target fixture still frees the DRBG after seed returns.
- `mbedtls_entropy_free()` clears Mbed TLS's internal source entries, but application-owned callback state is outside that context. The target fixture explicitly zeroizes its source-state object after freeing the entropy context.

## Build and link evidence

`make lib` succeeded for the isolated SDK-configured copy and built all three default-feature static archives. The target configuration differs only by the default-source switch.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `target/source/mbedtls-.../library/libmbedcrypto.a` | 1,177,638 | `302ff309b034bdc8d815347c58ba4fc55c128dc2bfba51434bb22bc6fa32aa3d` |
| `target/source/mbedtls-.../library/libmbedx509.a` | 129,682 | `e9f437c9e22d1df05ee8475ff3ab8ea9ec2b566c4594c9bb5f33fea64d9345f3` |
| `target/source/mbedtls-.../library/libmbedtls.a` | 592,520 | `c7e0c6fd536607a45bd5b2d399446bfa5816410d1bcbde79f53b8fd82285d7ea` |
| `target/bounded_entropy_probe.elf` | 577,792 | `d643a524def7b6c7ed0339f35421790a42256ece1ff066c06a455459e13d024a` |

The three owned SDK translation units compiled with `-std=c99 -Os -Wall -Wextra -Werror`, and the SDK linker produced an x86-64 FreeBSD-ABI ET_DYN ELF with `BIND_NOW`. The map contains `entropy.o`, `bounded_entropy_callback.o`, and `target_sysctl_source.o`, but no `entropy_poll.o`. The ELF's undefined imports include `sysctl`; the map and source object show this import comes from the owned one-call adapter. The normal SDK libraries are `libkernel_web.sprx`, `libSceLibcInternal.sprx`, and `libSceNet.sprx`. Successful linking does not show that these imports resolve on the target or that `sysctl(KERN_ARND)` succeeds there.

The separate host behavior fixture compiled with warnings-as-errors and ran to exit 0. It reported:

```text
no-default-sources=verified; zero-callbacks-before-add=verified; callback-failures=errno,zero,short,oversize; ctr-drbg-seed-failure=one-read; ctr-drbg-seed-polls=1; deterministic-fake-only
```

No console, account, or network was contacted. No SDK ELF was executed or dispatched, and this report makes no claim about target runtime entropy or cryptographic quality.
