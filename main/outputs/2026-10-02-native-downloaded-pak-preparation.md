# Actual download job to shared PAK preparation

The native app now optionally feeds its exact completed, reopened read-only
download FD into the existing reusable PAK archive preparer. No individual mod
was edited and no expected output was supplied to the converter. Target details
are trusted captured-base profile data; the profile independently binds the
base's bytes and SHA256 before any job. Default analysis behavior remains.

The actual app ran against owned HTTPS login/account/download services with the
unchanged retained real 5,810-byte ZIP. Final proof passed all 25 checks, including
14 real TLS requests, duplicate suppression, kind conflicts, original archive
identity, retained analysis, whole-bundle extra-member refusal, explicit PAK
component preparation, three preserved regular members, wrong-game refusal,
credential-free archive requests and complete owned shutdown cleanup.

The component output was a 297,439,679-byte v11 None PAK, SHA256
`aee923b6ba55b8809332ed6401d3d1bc475302e5d54fcbcfd9fd01bc07b7176a`.
Independent repak decoding compared all 4,699 virtual-path/content entries:
zero mismatches, one replacement, 4,698 base files preserved, zero added paths.
The two accompanying regular files remain explicitly unhandled; the component
result does not claim the complete bundle, installation or gameplay support.

Three connected changes make this possible:

* Additive callback gates across archive intake, union preparation and writer,
  preserving existing v1 layouts/APIs. Eight focused cases/69 assertions passed,
  including actual monotonic expiry before writer I/O and completion revocation
  after scratch cleanup. Genuine gate-ignoring RED controls are retained.
* A separate pinned Mbed TLS 3.6.7 SHA1-only source unit supplies PAK hashing.
  Format and transport contexts stay separate; the transport remains the sole
  PSA/platform owner. Host digest vectors and native link-only checks passed.
* Private job ownership, deferred output acquisition, sync, read-only reopening,
  physical-order SHA256 observation and identity-bound cleanup. A reviewed
  callback-context/result backing alias was corrected with a disjoint staged
  result. Readback SHA256 identifies the artifact; the decoded-content proof
  comes from the independent development oracle.

The final PS5 SDK candidate compiled and linked without execution. Its SHA256 is
`a695720bc567b6f5bf7d792f4f5eb31a222f6696aeec353ffd46968d5969b5bc`
(2,468,504 bytes). Native compilation consumed 12,451 recorded header entries
with zero host headers. An SDK linker-layout defect was reproduced and fixed by
reusing the pinned caller-local script: PT_DYNAMIC and SHT_DYNAMIC now both have
336 bytes, aligned 16-byte entries and terminal DT_NULL; BIND_NOW is present.
Older experiment candidates with oversized PT_DYNAMIC remain unsuitable under
this layout contract. Native linking does not establish resolved PS5 imports,
filesystem behavior, console execution or in-game success.

Final evidence is private development material under
`.superpowers/native-dependency-spike/native-downloaded-pak-jobs-1/`:
`actual-http-tls-final-1/receipt.json`, `actual-http-tls-final-1/oracle.json`,
`configured-host-final-1/desktop_fixture-download/manifest.json`,
`configured-native-final-1/loopback-download/manifest.json` and
`native-layout-red.json`. Gate evidence is in `pak-prepare-operation-gates-1/`;
the dependency proof is in `native-format-sha1-provider-1/final-1/receipt.json`.
The API/build contract is documented in
`outputs/ps5-mod-bridge/native-host/format/PRIVATE_PREPARATION.md`.

This increment admits standalone unsigned/unencrypted v3 None mod PAKs against
captured v11 None bases up to 512 MiB. It does not join the other existing
standalone container/config adapters yet. Automatic target discovery, whole
archive/prerequisite orchestration, free-account download handoff, actual UE4SS
engine access, Windows DLL execution and the finished PS5 storefront/package
remain unfinished. No console connection, production account access, game/system
write or stopped native placement-caller retry occurred. The full goal stays
active; successful Townfall trials do not establish universal compatibility.
