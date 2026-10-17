# Callable HTTPS transport

`mb_curl_runtime_start_v1` initializes curl once, `mb_curl_download_v1` runs
sequential HTTPS downloads, and `mb_curl_runtime_close_v1` releases that process
owner. The caller must exclusively own curl/PSA, serialize every call and
provide a checked real platform entropy source. There is no reset or rebind.
Each operation has fresh handles, a deadline and a bounded entropy window;
lifetime counters and clock correlation persist. Actual entropy or invalid
clock faults are terminal. Rejected transfers and ordinary timeouts allow the
next independent operation.

The producer accepts an already authorized HTTPS resource and explicit trust
bundle, authenticates its hostname and streams into the existing borrowed,
fresh unpublished file sink. It sends no Nexus account headers or cookies and
does not follow redirects or use a proxy. The producer does not establish Nexus
entitlement. The native account-bound resolver validates provider-issued
URIs using the existing Nexus client rules; a native job dispatcher must bind that result before calling this producer.

The native sink ceiling is 1 GiB, matching the companion's default. Per-job
caps can be lower. A 64 KiB carry coalesces smaller curl callbacks; the sink has
its separate 64 KiB readback scratch. Neither allocates a whole archive body.
Curl and Mbed TLS still have their own allocations. Completion requires a real
successful transfer, accepted final status/framing, sync and exact sink
readback. Failure receipts contain no completed identity; any written prefix
remains private and caller-owned. The producer never publishes or installs it.

The matching retained profile is curl 8.18 plus its initialization-failure
backport and Mbed TLS 3.6.7. It uses TLS 1.2 and the retained cipher/certificate
policy. The optional resolver entry preserves URL hostname authentication.
With the synchronous resolver and positional I/O callbacks, deadlines are
checked around library calls; they cannot interrupt a blocked platform call.
The caller must keep params and callback contexts valid and immutable during
each operation. The fixed attempt uses approximately 128 KiB of stack before
library calls; a native stack/runtime check remains necessary.

## Local verification

Run from the managed worktree with trusted retained host dependencies:

```sh
python3 outputs/ps5-mod-bridge/native-host/download/verify.py \
  --profile .superpowers/native-dependency-spike/native-curl-profile-experiment-1 \
  --curl-source .superpowers/native-dependency-spike/curl-release \
  --patched-curl .superpowers/native-dependency-spike/curl-8.18-seed-refusal-host-1/check-4/patched-consumer/libcurl.a \
  --output .superpowers/native-dependency-spike/curl-stream-download-1/fresh-run \
  --archive .superpowers/native-observations/nexus-real-mods-2026-10-01/leon-2k-v1.1.zip \
  --archive .superpowers/native-observations/nexus-real-mods-2026-10-01/mod-menu-v6.zip
```

The output directory must be new and outside the retained dependency trees.
The verifier trusts their compiler wrapper, headers and matching Mbed archives;
it is not a clean dependency build. It checks the selected patched curl
archive, builds ordinary-host callers and runs owned loopback HTTPS servers.
The normal TLS caller uses real clocks and `getentropy`. A separate private
owner copy uses synthetic clock/source boundaries; it never changes the OS
clock or seeds the real TLS caller.

Root-observed `full-1/summary.json` passed 19 sequential jobs: a generated
195,035,136-byte stream, the unchanged 105,647,244-byte Leon ZIP and 26,903-byte
mod-menu ZIP, fragmented chunked and fixed-length bodies, rejected certificate
roots/hostnames, truncation, redirects, encoding, size caps, cancellation,
timeout and sink/readback errors. A later job succeeded after those failures.
The large stream used 2,976 sink feeds/writes/reads. Raw TCP EOF without HTTP
framing was rejected; authenticated TLS closure completed the corresponding
close-delimited case. Four private clock/source controls and terminal initial
entropy refusal also passed. The first fixture incorrectly expected raw EOF
to complete a close-delimited TLS response; that failed receipt is preserved.

The measured Leon and mod-menu files also passed the existing ordinary-host
default archive analysis with their exact download identities. That analysis
retains unverified compatibility and unavailable install authority.

These transfer results establish the producer/sink seam on the development
host. They do not establish live Nexus access, installation or engine execution.

## Application build and startup integration

The actual `native-host/build.py` now accepts an explicit matching download
profile. It compiles the producer, serialized owner and file sink into the
existing application, excluding the incompatible legacy `entropy.c/drbg.c`
checkpoint objects. Its default legacy build is unchanged. Each preparation
copies the curl archive and two translation units into a new output directory,
then applies the existing 8.18 initialization-failure backport. The pinned TLS
configuration, vendor inputs, retained archives, wrapper path bindings and
native tool/header closure are checked; retained inputs stay unchanged. This
is artifact reuse with two rebuilt units, rather than a clean dependency build.

For example, from the managed worktree, with a new output root:

```sh
python3 outputs/ps5-mod-bridge/native-host/build.py \
  --profile loopback \
  --download-profile .superpowers/native-dependency-spike/native-curl-profile-experiment-1 \
  --curl-source .superpowers/native-dependency-spike/curl-release \
  --output-root outputs/ps5-mod-bridge/native-host/build/download/fresh-native
```

`desktop_fixture` selects the corresponding real host composition. The
builder retains compile/link diagnostics even on failure, includes dependency
identity in both modes, and copies the curl/Mbed/framework notices. Native
linking retains `mb_curl_download_v1` and `mb_nexus_client_request_v1`
explicitly; this creates no HTTP endpoint.

The existing application entry point starts the runtime before opening its
listener and closes it on returned exits. The native callback performs exactly
one `sysctl({CTL_KERN,KERN_ARND})`, accepts only return zero, errno zero and
length 32, and wipes the full output on refusal. The owner supplies its budget
and terminal failure policy. The desktop callback uses real `getentropy`.
`/report` includes cached scalar startup metadata and marks the legacy
checkpoint `not_selected`. It does not perform more entropy work per report.

The actual app has local PS5 compile/link evidence, with native headers and
the checked callback present. Owned host controls verify runtime startup,
normal/lifetime shutdown, bind failure and entropy refusal before listening;
private wrappers observe one host start and one host close attempt per path.
After failed initialization the runtime already owns its internal cleanup, so
that final host close attempt returns `BAD_STATE`. No second global cleanup
or reset follows.

Native sign-in/account endpoints require the explicit compiled public OAuth
configuration in the [app README](../README.md#native-browser-sign-in-configuration).
Job, download and installation endpoints remain unavailable. Production
registration/trust, native runtime/stack checks and the job dispatcher remain
unfinished. No new console execution, live Nexus download, game-file
write, UE4SS binding or DLL execution was established by this integration.

## Typed Nexus metadata client

`mb_nexus_client_request_v1` is a synchronous, no-throw C ABI over the existing
fixed provider URL factory and owning C++ metadata decoder. It shares the
already-started curl/PSA runtime with archive downloads. Its public header
uses only C types; the private provider transfer callback runs after complete,
verified HTTP 200 delivery, inside the same owner window and busy gate through
decoder execution and transport cleanup. Archive requests retain their
separate credential-free policy.

Catalog requests use `https://api.nexusmods.com/v1/...` with an API key or
Bearer credential; API-key validation requires the API-key mode, and OAuth
user info requires Bearer at `https://users.nexusmods.com/oauth/userinfo`.
The caller cannot supply an arbitrary URL or headers. TLS peer/hostname
verification and explicit trust are mandatory; redirects, proxies, cookies,
netrc and provider credential forwarding are disabled. The fixed application
headers are `PS5ModBridge` and `0.1.0`. This implements the existing six metadata
kinds, including latest-added mods; it is not a complete Nexus search/catalog
or download-link/dependency endpoint.

Credentials are borrowed immutable bytes, 1..16384 in ASCII 33..126, with no
NUL required. Owned header copies are erased before release. There is no
account persistence, login browser, token exchange, refresh or retry in this
client. Production trust/session acquisition and account-authorized jobs
remain separate work.

Callers provide raw response storage of 1..8 MiB and report storage of
0..16 MiB. All specified spans must be disjoint; invalid layouts leave storage
untouched. Once layout is valid, the full raw capacity is erased on return,
and any supported failure erases the report capacity. No raw response, secret
or callback context is retained. A short output or zero-capacity query makes
one actual transfer and returns its required report length only if final
transport cleanup succeeds with the consumer's short-buffer refusal. A later
owner/clock/timeout failure revokes that length and any decoded report; a
second call makes a second request. This is not an automatic refetch.

Client `OK` means one complete authenticated response produced a whole report,
including owning malformed/resource-limit error envelopes. Only a normalized
`status:"decoded"` sets `metadata_decoded`. Neither fact grants account
visibility, download entitlement or installation support. Raw provider error
bodies are not returned; transport failures expose only scalar status facts.

The shared attempt still uses approximately 128 KiB of stack, and metadata
header construction adds a bounded temporary credential header. Native stack,
TLS and client execution remain unverified. The existing application HTTP
routes and unavailable service capabilities are unchanged.

### Local verification checkpoint

The current client passed 14 owned HTTPS request cases: all six request kinds,
both credential modes, a second unrelated game domain and maximum signed mod
ID, short report storage, a malformed final metadata record, HTTP 401, an
oversized declared body, and metadata requests around a credential-free opaque
archive download. The archive passed the actual file sink's byte comparison
and readback. Exact routes and authentication/application headers matched the
fixture ledger. One runtime completed 16 owner windows with 116 real host
entropy callbacks and no terminal failure. Private boundary controls separately
cover alias refusal, callback exceptions and late owner failure. An actual
HTTPS consumer that exceeds its deadline now returns timeout and revokes its
result, including when the consumer first refused the output.

The shared producer refactor also passed the existing 19-job archive regression
batch (29 checks), including unchanged Leon and menu ZIPs, framing, TLS name
and trust failures, redirects, limits, cancellation, sink/readback failures and
a healthy final transfer. The actual host entry point passed nine lifecycle
checks. Eleven focused builder tests passed. The final PS5 ELF has 13 objects
and 6,206 include entries with no host headers; this is compile/link evidence.

Replay the metadata suite from the managed worktree after preparing the retained
dependency profile and application host build:

```sh
python3 outputs/ps5-mod-bridge/native-host/tests/nexus_client_verify.py \
  --mode suite --source-mode current --expect green \
  --app-profile /absolute/path/to/fresh/desktop_fixture-download
```

Receipts are retained under
`.superpowers/native-dependency-spike/nexus-client-https-fixture-1/`; application
builds and archive/lifecycle receipts are under
`.superpowers/native-dependency-spike/native-nexus-provider-requests-1/`.
The verifier reuses three application C++ objects only after checking their
source, object and complete include-closure hashes. Curl/MbedTLS archives are
retained dependency artifacts identified by hash, rather than freshly rebuilt
for each replay. A header-only mutation control confirms stale objects are
rejected.

These are owned local servers with fake credentials. Since binding loopback
port 443 was denied, a private test-only socket shim routes only the expected
loopback address/port to an owned ephemeral listener. Production URLs, Host,
TLS SNI, hostname checks and CA validation remain in effect; this does not
verify production DNS, live Nexus or PS5 network/runtime behavior. Fixture EC
certificates use named curves supported by the pinned TLS profile. The pinned
curl profile compiles NETRC out; attempting its unsupported option exposed a
setup regression which was corrected without ignoring other setup failures.
No real account credentials, console connections or game/system writes were
used for this checkpoint.

## Account-bound download resolution

`mb_nexus_resolve_download_v1` composes the existing metadata client and private
provider transfer into one synchronous operation. Its C-only public header
accepts borrowed API-key or Bearer credentials, a selected domain/mod/file,
explicit TLS trust, optional exact provider address mappings, caller scratch
and a private URI output buffer. It starts no second runtime, stores no account,
performs no refresh/retry and writes no file. The URI can be passed to the
existing credential-free archive producer; success here covers resolution for
this live identity and source, rather than installation or game execution.

The operation fetches the current authenticated user. It requires either live
premium membership or a structured NXM website grant whose domain, mod, file,
user and future expiry all match. A supplied grant must validate even when the
user is premium. No caller premium flag grants access. It then checks the
selected mod's identity, availability, published status and explicit adult
marker. Unknown adult preference remains restrictive; raw user-info alone
does not implement the separate Nexus preference lookup. The parent-mod check
precedes link retrieval.

The only authenticated query exception is the canonical generated
`/v1/games/{domain}/mods/{mod}/files/{file}/download_link?key=...&expires=...`.
The borrowed key is percent encoded directly into bounded mutable storage.
Other authenticated queries, duplicate/extra fields, malformed encodings and
alternate route spellings remain refused. Query admission itself establishes
no entitlement. The caller does not supply an arbitrary provider URL.

A complete link response must contain 1..32 valid mirrors. Every URI, display
name and short name validates before the first URI is selected. URIs must fit
the existing producer's fewer-than-8192-byte HTTPS/443 contract, without
userinfo, fragments, backslashes or non-ASCII/control bytes. The helper's
short-buffer result never publishes a prefix; the public resolver's failures
also revoke the required length. Provider-issued URIs can contain signed
query credentials and remain private ephemeral output. They never belong in
public job receipts, logs or ordinary metadata responses. No account headers
are forwarded during the subsequent archive download.

The entire resolution uses one bounded monotonic budget; grants use checked
realtime expiry samples. Cancellation callback exceptions fail closed. All
specified spans, including nested grant inputs, must be disjoint before any
mutation. Valid raw/report scratch is erased across its full capacity, and
failed URI storage is erased in full. Final cleanup, deadline and grant checks
can revoke a URI selected earlier inside the authenticated body consumer.
Accessible owned URL/parsed URI values are erased before release; opaque JSON
parser and curl internals are not claimed securely erased.

Production trust/session acquisition, official login/callback handling,
native job admission and durability, prerequisite acquisition, archive
preparation/install dispatch and native runtime validation remain unfinished.
Current HTTP service capabilities remain unavailable. This resolver does not
execute UE4SS, Mod Engine, Windows DLLs or game hooks.

The local resolver checkpoint has passed 25 owned loopback TLS requests and
11 fixture assertions, including premium access, an encoded website grant,
wrong account/source, expired grants, hidden/unknown-adult mods, an invalid
final mirror, HTTP failure, a total timeout and healthy reuse afterward. The
selected URI was passed to the actual credential-free archive producer and
private file sink, and its bytes and readback matched the owned fixture.
Replay against a fresh download composition:

```sh
python3 outputs/ps5-mod-bridge/native-host/tests/nexus_download_verify.py \
  --app-profile /absolute/path/to/fresh/desktop_fixture-download
```

The driver reuses four application C++ objects only after matching their
source, object and complete include-closure hashes. The resolver closure must
contain its public header. C owner/transport/sink sources compile into the
driver, and the retained curl/Mbed TLS archive hashes are recorded. A private
socket hook admits only a requested loopback address on port 443 and connects
it to the owned ephemeral listener; original HTTPS origins, Host/SNI and
certificate checks remain in force. No live account or Nexus service is used.
Receipts contain scalar facts and boolean header/query comparisons, with no
credentials or signed URI values. Fake TLS keys and fixture artifacts remain
in ignored development evidence with private directory/key permissions.

The actual host and PS5 download compositions build with the resolver symbol
retained. All 14 native objects and 7,108 include entries match their manifest;
the native closure contains no host headers. The 11 builder checks, existing
14-request metadata HTTPS suite and nine application startup checks also pass.
Forty query-admission controls and 41 fake-boundary resolver controls cover
the narrow authenticated-query exception and late cleanup/clock revocation.
The PS5 artifact has not been executed for this checkpoint.

## Private authorization-code exchange

`oauth_code.h` and the private `oauth_code_transfer.h` add one fixed POST to
`https://users.nexusmods.com/oauth/token` through the same exclusive owner.
The canonical form contains authorization_code, redirect_uri, client_id, code
and code_verifier only. No caller URL, arbitrary header, client secret, retry
or refresh option exists. JSON token access/refresh values are bounded private
outputs; code/form/raw scratch and failed output blocks are wiped. The decoder
uses the existing resource gate and rejects duplicate token fields, invalid
Bearer type, invalid expiry and incomplete token pairs. Accessible parsed
strings are erased; opaque library internals have no blanket erasure claim.

Token success establishes no account identity. `auth.h` consumes one-use
PKCE state, verifies live userinfo, performs final deadline/expiry gates and
then commits the candidate with a bounded non-failing ownership replacement.
This is a final sampled deadline check, with the same blocked-syscall limits
as the existing transport. Expired tokens require reconnect in this version;
credentials are process-lifetime and there is no public token getter. Existing
metadata GET and credential-free archive behavior retain separate policies.
