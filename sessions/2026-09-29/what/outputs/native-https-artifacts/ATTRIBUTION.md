# Native HTTPS checkpoint source and notices

This private candidate links the pinned ps5-payload-dev SDK v0.43, Mbed TLS3.6.7 and the unchanged DDS/hash core with its existing nlohmann JSON and PicoSHA2 dependencies. Exact compiled sources, headers, libraries and tools are identified in development_lan-manifest.json. Frozen application and TLS configuration sources are in source/; additional shared/vendor/SDK inputs remain at the recorded local paths.

The eight retained notices and their exact sources/hashes are in license-references.json. Mbed TLS selects the Apache-2.0 option; its pinned framework notice is retained separately. The SDK GPL notice and LLVM18.1.0 runtime reference notices are historical references preserved from the earlier checkpoint; the pinned SDK release lacks a standalone license file. Reference texts do not independently attest every bundled archive's provenance. Pinned target headers retain their own notices.

The fixture directory includes public certificates and server source. Its private signing key was not packaged. This artifact is a controlled development transport checkpoint, with installation_supported=false. The manifest's console_executed=false records the build-time fact; any later target observation will be recorded separately. No app or artifact was published or pushed.
