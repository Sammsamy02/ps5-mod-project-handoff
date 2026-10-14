# curl 8.18 initialization failure backport

This is an ordinary-host dependency experiment, not an integrated PS5 download
transport. It retains curl 8.18.0 and the existing Mbed TLS 3.6.7 profile.

The unchanged consumer crashed with SIGSEGV when its host-only entropy policy
refused curl's CTR_DRBG seed. `mbedtls_init()` called `failf(NULL, ...)`, although
the logger requires an easy handle. Removing that call alone exposed another
failure: `Curl_ssl_init()` marked itself initialized before its backend
succeeded, so a second public initialization incorrectly returned success.

The patch releases initialized backend state on seed failure and records the
SSL initialization latch only after success. Failure still returns
`CURLE_FAILED_INIT`; it does not reset the terminal entropy owner, relax TLS
verification, or add a production retry.

The patch is bound to curl source commit
`2eebc58c4b8d68c98c8344381a9f6df4cca838fd` and Mbed TLS commit
`068ff080b369adfac81509f9b57b2afabaf82dc5`. The verifier checks both original
translation-unit hashes before copying them. Its compilation uses the retained
matching host configuration and libraries; it is not a clean dependency build.
Supply the trusted retained source and profile only: the source headers,
compiler wrapper and fixture script are not independently authenticated by this
verifier. Preservation checks cover the two source files, archive and consumer
binary, rather than every file in the retained trees.

Run from the managed worktree with the retained experiment inputs:

```sh
python3 outputs/ps5-mod-bridge/native-host/dependency-patches/curl_init_failure_verify.py \
  --curl-source .superpowers/native-dependency-spike/curl-release \
  --profile .superpowers/native-dependency-spike/native-curl-profile-experiment-1 \
  --output .superpowers/native-dependency-spike/curl-8.18-seed-refusal-host-1/check-new \
  --loopback
```

The output directory must be new and outside both input trees. The verifier
disables bytecode writes when importing the optional loopback fixture. It
copies two source files and the
static archive, recompiles those two units, and links private host consumers. It
leaves the original sources, archive and binary unchanged. The observed variant
wraps only backend cleanup calls and invokes the real Mbed TLS free functions.
The ordinary consumer has no cleanup instrumentation.

Root-observed `check-4/summary.json` passed all nine checks:

- Untouched consumer: seed refusal reproduces SIGSEGV.
- Logger-only intermediate: public initialization returns `2`, then incorrectly
  `0`, reproducing the false latch independently of the crash.
- Patched refusal: both public calls return `2`; the source remains terminal.
  Actual backend teardown releases the DRBG and entropy contexts and calls PSA
  teardown once. The observer checks cleared context bytes after accounting for
  the pinned free routines' documented reset fields.
- Healthy initialization: both calls return `0`; the second adds no entropy
  callbacks or OS random-source calls. Final balanced cleanup releases state.
- Real existing consumer: curl-seed and PSA-seed refusals return ordinary error
  receipts, with no transfer or committed bytes and a cleared caller buffer.
- Local TLS controls: exact fixture download succeeds; wrong root and hostname
  fail before an HTTP request. Listeners close and no redirects occur.
- Retained source/archive/binary hashes are unchanged.

These results do not establish complete PSA global-memory erasure. The existing
consumer's `failed_init_backend_erasure: not_confirmed` field stays unchanged;
separate context observers provide the narrower cleanup evidence.

Ownership is a prerequisite for using this patch. `mbedtls_cleanup()` frees the
process-wide PSA key store. PSA initialization is idempotent, not reference
counted; teardown is unsuitable while another component has active keys. This
experiment is serialized, uses the retained AES-backed DRBG profile, and owns
PSA exclusively. It does not establish correctness for shared PSA consumers,
PSA-backed DRBG configurations, other TLS backends, or later global-init failure
paths. Production integration requires an explicit ownership decision.

No dependency upgrade, native build, console contact, game-file change, Nexus
account request, payload publication, or application integration occurred.
