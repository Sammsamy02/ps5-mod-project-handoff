# ModBridge

Development foundation for a universal Nexus-to-native-PS5 mod translator. Shared format, runtime and engine backends are the product direction; no individual mod ID selects a private patch. The first hardware validation title will be selected at test time.

The finished private app must browse, download, convert and install entirely on the PS5, without a PC, phone or AI runtime dependency. The computer is permitted for development/build work. Missing or unverified PS5 backends block installation with a reason; the roadmap's coverage rows are targets, not implemented Supported labels. Online/co-op mods are excluded by user choice. See `../2026-09-30-console-only-roadmap.md` for the current requirements.

Console target identity is separate from the Nexus catalog domain. The trusted snapshot module supplies opaque target references and optional exact-byte build hashes; browser labels and Nexus names cannot supply platform evidence. The pure native [captured metadata decoder](native-host/title/README.md) is implemented and compile/link checked for PS5, while the default console provider remains unavailable pending full enumeration and runtime observation. A separate read-only source API captures one candidate under fixed cache roots; it is host-tested and PS5 compile/link checked, with no provider enablement. Metadata names, versions and file digests supply no build/runtime/install acceptance. The companion host now serves immutable bounded inventory pages and current-generation confirmation. The UI commits complete captures atomically, including 128 generic fixture titles; the old small-snapshot route remains separate. Paging transport availability does not establish a native source provider. Target-bearing analysis jobs reject stale references and preserve their original target through retries and durable receipts.

The local development API can look up any validated Nexus domain/mod ID through the same account client, including mods absent from the latest list. Analysis publishes bounded planner blocker codes without private paths. Successful archive inventory supports only that read-only operation; engine compatibility remains Unverified, structurally identified Windows/CLR runtime requirements are Unsupported under the current implementation, and installation remains unavailable.

The desktop development host connects account lifecycle, Nexus catalog/files, quarantined downloads and nonexecuting archive analysis to a controller interface. The ordinary-SDK C/C++ host has now run twice on the user's PS5 with the integrated production DDS/hash module, matching build reports and seven passing owned checks. Shutdown closed its endpoint and a new PID reused the port. See [native core checkpoint](../2026-09-30-native-core-console-checkpoint.md) and [native-host source](native-host/README.md). The earlier freestanding diagnostic remains an unexecuted separate candidate. The complete Nexus manager, UE4SS/other runtime ports, asset conversion and game installation remain incomplete.

Shared DDS/BC1–BC7 inspection and immutable lossless neutral blocks are implemented and tested. They preserve format/topology/mips/cube/volume/trailer evidence and distinguish capped prefixes from truncated files. Parsing never supplies PS5 writer or game acceptance permission. The same production DDS/hash sources now execute through the SDK's bundled C++ runtime in the native host, with owned allocator/exception/single-thread TLS and DDS fixture evidence. The historical standalone cross-link was unexecuted; it is separate from the integrated host. See [DDS verification](../2026-09-30-dds-module-implementation.md) and [C++ cross-link](../2026-09-30-sdk-cxx-dds-crosslink.md).

## Build and test

Requires a C++20 compiler, Make, Python 3 and a linkable libarchive library. The development Mac uses Clang 17 and system libarchive 3.7.4. Vendored dependency versions, URLs, licenses and hashes are in `third_party/manifest.json`. Ordinary builds verify those bytes and do not fetch dependencies.

```sh
make
make test
make test-ui
make test-all
make sanitize-test
```

Individual checks: `make test-hash`, `make test-dds`, `make test-inventory`, `make test-planner`, `make test-journal`. Use `make archive-version` to see the actual library linked on your machine. The full preparation core still requires native archive/codec dependencies and runtime validation; desktop success does not establish them.

`make test` runs C++ and Python tests, including the account/provider/host suites. `make test-ui` additionally needs Node, Playwright and an available Chromium browser. Set `NODE_PATH` for an existing nonlocal Playwright installation and `CHROMIUM_EXECUTABLE_PATH` if needed. Its actual-host integration uses owned provider/archive fixtures and the real C++ analyzer; it never contacts Nexus or a console. Sanitizers instrument the C++ build, not the Python interpreter or browser.

## Private local interface and Nexus account

Build the core, then run the companion on the computer:

```sh
python3 companion/host.py \
  --store .private/account.json \
  --quarantine .private/quarantine \
  --port 18123
```

Open the printed loopback URL. The interface supports Games, Activity, Installed, keyboard navigation and standard-mapped gamepad input. Browser rendering and actual PS5 controller mappings remain unverified on the console. The host accepts only its exact loopback origin; it is not a LAN server for the PS5. Private credentials, signed links and analysis paths stay on the host. Public navigation IDs alone enter sessionStorage.

For the **Sign in** button, supply your own valid Nexus OAuth client identity and registered callback:

```sh
python3 companion/host.py \
  --store .private/account.json --quarantine .private/quarantine \
  --port 18123 --client-id YOUR_NEXUS_CLIENT_ID \
  --redirect-uri http://127.0.0.1:18123/auth/callback
```

This opens Nexus's authorization page, validates state/PKCE, verifies the returned identity, saves rotated credentials privately and returns to the UI. Failed/cancelled sign-in preserves the prior account. Disconnect invalidates outstanding sign-ins. No borrowed demo application identity is embedded. A valid client registration and live user authorization have not yet been tested.

Private personal API keys are also supported by the command-line companion. Keep the key in a private regular text file and connect with `python3 companion/nexus.py connect-key --store .private/account.json --key-file /path/to/private-key.txt`. The key is validated against Nexus before replacing the saved account. The UI host can then use that same store. The CLI also provides `status`, `login`, `games`, `mods`, `files`, `download` and `disconnect`; use each command's `--help` for arguments. Never paste secrets into source code or command arguments.

The catalog exposes Nexus games and latest mods, with file selection and direct official mod URL or domain/ID lookup. Console target selection is independent of that catalog selection. Full search/pagination and native title detection remain open. Premium direct links and free website-authorized NXM links follow the provider's rules; the host currently records website authorization as needed rather than claiming a completed download. The CLI accepts an authorized private `--nxm-file` for that handoff. No mod is installed by these flows. Jobs persist safe phase receipts and interruption records; a storage failure disables new submissions. Shutdown does not forcibly cancel an in-progress network/filesystem call. Private path checks assume cooperative sequential storage use; arbitrary concurrent filesystem mutation is not guaranteed.

## Package analysis and planning

```sh
./build/modbridge analyze /path/to/mod.zip --output inventory.json
./build/modbridge plan inventory.json \
  --target examples/target-unknown.json \
  --backends examples/backends-unavailable.json
```

Analysis is nonexecuting and reports structural facts, content hashes and missing requirements for mixed packages. Inspected plugins/installers are never loaded. The example target intentionally has no verified console capabilities; its plan stays unresolved. Backend declarations describe potential applicability, not completed translation or gameplay results.

Default limits: 10,000 entries, 1 GiB total expanded content, 256 MiB per entry, and 8 MiB in-memory inspection per entry. Override through `--max-entries`, `--max-bytes`, `--max-entry-bytes` and `--inspection-bytes`. Full hashes are streamed; capped inspection is reported as incomplete. Format/installer coverage must be read from the generated facts/issues, rather than inferred from extensions.

## Scratch installation development

Scratch commands require an explicitly initialized project-owned directory. They are intended to test file transactions, recovery and preservation of subsequent edits. They do not deploy into a console game.

```sh
./build/modbridge scratch-init /path/to/new-empty-scratch
./build/modbridge scratch-deploy /path/to/scratch /path/to/prepared manifest.json
./build/modbridge scratch-recover /path/to/scratch
./build/modbridge scratch-remove /path/to/scratch TRANSACTION_ID
```

Manifest shape:

```json
{
  "files": [
    {"source": "replacement.txt", "destination": "config.txt", "sha256": "CONTENT_SHA256"}
  ]
}
```

Source/destination paths are relative; content hashes are required. A changed installed file is preserved during removal. Hash checks/journals and descriptor-relative containment do not amount to a universal guarantee against arbitrary concurrent external filesystem mutation; actual implementation limits and test evidence belong in the progress report.

## Shared contracts and remaining work

`include/modbridge/core.hpp` defines portable module interfaces. `include/modbridge/backend.h` is a future C-compatible extension contract with explicit result ownership; dynamic loading is not implemented. `examples/console-capabilities-unknown.json` records unknown capabilities separately by preparation, standalone-host and in-game context.

The next work includes native TLS/account/cache/archive services, checked logical-file/container readers, automatic source-to-target mapping, engine/runtime implementations, platform asset writers, full Nexus navigation/download handoff and game validation. The user reports firmware 13.60 and Stellar Blade 01.014.001. Manager 0.5.2 and the narrow owned-SDK host route are observed; firmware identity, running loader identity, target game fingerprints and translation acceptance remain unverified. Existing examples intentionally retain unknown capabilities. No private signature or guess establishes game compatibility.
