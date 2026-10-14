# Native transport experiments

September 30, 2026. These throwaway experiments follow the lighter process in [EXPERIMENTS.md](ps5-mod-bridge/EXPERIMENTS.md): focused checks and short result notes, with review deferred to promotion into console/game/system writes.

| Prerequisite | Focused host result | PS5 target result |
| --- | --- | --- |
| HTTPS client | Real TLS1.2 exchange with the owned local server passed; wrong root, wrong hostname, expired/future certificate and unavailable clock all failed before HTTP data | Fresh matching crypto/X509/TLS libraries compiled and linked; unexecuted |
| ZIP Deflate reader | Real libarchive+zlib decoded the owned memory ZIP; eight corruption/limit/path controls rejected | Fresh libraries compiled and linked; unexecuted |
| Sign-in challenge transform | Public RFC7636 vector and two additional valid fixtures matched the unchanged desktop implementation;11 invalid verifiers rejected | Compiled and linked with the existing approved crypto configuration; unexecuted |
| Callback query parser |27 checks passed,22 compared with the unchanged desktop callback logic, including cancellation/replay/expiry and bounded decoding | Compiled and linked; unexecuted |

The new TLS ELF uses deterministic development entropy and must **never be uploaded**. An actual console HTTPS checkpoint needs the reviewed real entropy source, checked target clock, matching TLS configuration, trust inputs and its own promotion review. Host trust/certificate fixtures do not establish Nexus server compatibility.

The initial ZIP proof handles a frozen fixture. The follow-on bounded preflight and combined intake below handle a restricted ZIP envelope. Decoding remains separate from filesystem extraction, game format conversion and installation.

Detailed scratch notes and commands remain in the managed worktree under `.superpowers/native-observations/` and `.superpowers/native-dependency-spike/`, with `transport-experiments-progress.json` recording active workers. No full review bundle, sanitizer sweep or checksum package was required for these throwaways. The later integrated native HTTPS candidate received one promotion review and one console dispatch, described below. No account login or game-file write occurred.

The preceding [DRBG console checkpoint](2026-09-30-native-drbg-checkpoint.md) passed on the PS5 and shut down its owned process while leaving FTP running. Nexus sign-in/download/install, exact game acceptance and engine runtime ports remain unfinished; these experiments enable none of them yet.

## Follow-on results

| Experiment | New evidence | Remaining boundary |
| --- | --- | --- |
| Actual Nexus certificate profile | Fresh Mbed TLS authenticated both public Nexus domains with zero verification flags and no HTTP data; explicit P384/SHA384/RSA support and official X1 root were needed | Strict overall result failed because peer close notification was unconfirmed; the separate local reproduction below does not change that result. No PS5 execution |
| Generic ZIP metadata preflight |48 focused cases passed; Store/Deflate metadata, paths, geometry and limits checked; target compile/link succeeded | ZIP64 and unsupported features remain rejected; target unexecuted |
| Combined ZIP intake |15 actual-library checks passed, including mixed Store/Deflate nested files; late corruption and limit failures expose an empty whole result | No extraction/writer or native runtime evidence |
| Real-source TLS seed seam |40 injected-boundary scenarios passed with real Mbed seeding and the reviewed collector; two polls, shared budget, failures and post-seed denial checked | Host injection is not new native runtime evidence |
| Native DNS interface | Standard API linked through SDK libc to SCE resolver/pool imports; two numeric-only host controls passed | Native resolution and timeout behavior remain unverified |
| Reusable HTTPS operation | Actual-library localhost success and wrong-hostname rejection passed; source closed after two polls, cached-result ownership and cleanup checked; actual native collector/clock compiled and linked | Fixed fixture contract, numeric address only; complete status-host integration described below |
| Connection closing and completeness | Five local controls distinguish TLS close notification, ordinary EOF and wait deadline; the same EOF accompanies either a complete 5/5-byte body or a truncated 3/5-byte body | A proposed outcome contract preserves authentication, explicit message completeness, local cleanup and peer closure separately; no general HTTP parser or native proof |
| Complete native HTTPS status host | Four actual-library local cases passed: valid, wrong hostname, truncated response and absent peer close. Matching corrected native libraries and complete host linked without compiler arithmetic helper imports | [Controlled PS5 HTTPS operation passed](2026-09-30-native-https-console-checkpoint.md), with owned shutdown and FTP preserved. This provides no Nexus account/download capability yet |
| Actual UE4SS Lua dependency | Six bounded host cases passed; the pinned Lua5.4.7 dependency and owned pthread glue compiled and linked for PS5 | Native unexecuted; no UE4SS Unreal bindings, hooks or game acceptance. [Scope and evidence](2026-09-30-ue4ss-lua-portability.md) |
| Generic PAK v11 primary metadata | Read-only C reader passed 79 host assertions and native compile/link. A procedural 1.27 GB logical archive required 54,863 metadata bytes | No entry inventory, secondary verification, payload extraction, writer or mod acceptance. [Results and current sample limitation](2026-09-30-pak-reader-results.md) |

The observed Nexus chain matches [Let's Encrypt's documented YE1 hierarchy](https://letsencrypt.org/certificates/). Host-system trust was not substituted for the new Mbed client's explicit root. These follow-on results preserve the same scratch-only process and do not enable installation. The Lua dependency experiment completed its host/native compile proof; the actual UE4SS binding graph is now being investigated.

The UI prototype and desktop flows are built, but the console account/catalog/download/install flow is unfinished. No mod has been verified running in Stellar Blade. Generic archive intake and the actual UE4SS Lua dependency are compatibility groundwork; Unreal reflection, ABI, hooks, asset conversion and game acceptance remain unresolved. Earlier roadmap entries labelled "Supported" are targets, not verified capabilities.
