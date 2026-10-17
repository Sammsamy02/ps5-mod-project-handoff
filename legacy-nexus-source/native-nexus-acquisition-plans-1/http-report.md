# Acquisition-plan HTTP and actual app fixture

DONE. Only http.c and the new tests/nexus_acquisition_plan_verify.py are product/test changes owned by this agent. http.h is unchanged. No remaining concrete concerns.

Exact POST /api/acquisition-plan shares the existing 4096-byte JSON framing and forwards cookie, CSRF, Origin and the exact intent span to the additive auth-owned API under its configured operation budget. A complete owning plan returns 200 even when unresolved; unsupported methods return 405 and unselected service 503. Auth failures retain the existing status map. Publication requires return/status/complete/length/NUL agreement. Failed storage is fully wiped and released, and the existing response owner disposes successful output after use. No new transport owner or public query route is added.

Genuine historical RED is pinned to a1c9543eda53a35239f9d7ea217970ccc87cf43f: original host artifact 7db757e346041b50d6ae868306d274f9e3f7b344bc8a341b449292358a7f226d and retained relinked fixture binary 82e9512101829ebf6f5b0bba7328f3ecc4607fc9ddb08eb1515ffc05da61c7da. All 136 historical source hashes were checked with git show against that commit, independently of concurrent edits. Owned real TLS PKCE login succeeds, but a valid new POST returns 400 rather than the intended 200. The failed availability check is preserved and expected_red_observed is explicitly true.

Focused HTTP component RED/GREEN is under http-controls-1. Immutable old HTTP bodies produce 7 failures in 17 checks; current C passes all 17. This covers split-body framing, 4096/4097 bounds, duplicate Content-Length, Transfer-Encoding, content type, Origin, trailing bytes, exact target, unselected service and the existing jobs route. A configured download-enabled C11/-Werror HTTP translation-unit compile passes with no diagnostics in enabled-syntax-1.

Actual app GREEN2 passes 55/55 checks, exit 0, with 121 owned TLS requests (1 token POST, 1 userinfo GET, 41 visibility GETs, 40 canonical requirements POSTs, 38 file-list GETs). It exercises the deduplicated diamond and prerequisite-first stable source order; retained inbound provenance; exact explicit and unique-current-primary file choices; historical signed-63-bit explicit membership; unknown, contradictory, ambiguous and archived hints; duplicate IDs; hidden child with no downstream read; cycles, foreign/external/DLC relations, large IDs and the eight-identity closure; strict intent/auth/framing; malformed/partial responses; one cumulative deadline with no ready prefix and successful reuse afterward. Source visibility precedes both downstream reads; their relative order is not pinned. Whole unresolved plans contain no selected-source prefix. Public privacy, clean shutdown, empty private scratch and unchanged scratch identity all pass.

GREEN1 is retained as a fixture-only failure: the contradiction case supplied unrecognized OPTIONAL, correctly producing file_choice_missing. The test alone changed to recognized OPTION with category_id=1 to exercise the contradictory mapping. Its original public result was still unresolved with acquisition_ready=false and selected_sources=[]; no product change or rebuild was needed. green-1/fixture-correction.json preserves the cause and old receipt identity.

The root's current host artifact is a56486fd9b861f47b49ace574f8ddf30041c100bfaf54cac959226cab31146eb (2309048 bytes). The GREEN relink binds current profile objects and include closure; only the test socket-shim call site redirects approved provider HTTPS to the owned TLS server. This does not claim a live account or provider interaction.

Assigned native static audit passes for artifact 966aaa2a1f75cb490656d3df99c98416f84576c99ea73561603e3d50a94289dd (2824312 bytes). All 136 sources, 53 objects, 16524 header records and 243 retained archive inputs match current hashes; no host headers occur. The new auth acquisition entry point and typed file selector are retained alongside prior callable transports/catalog/preparation/generation bodies. One selected TLS/PSA crypto owner supplies SHA256 and zeroize; the existing format-only SHA1 owner is separately scoped. Imports remain 137 with no membership change relative to a1c9543's retained requirements artifact. PT_DYNAMIC and SHT_DYNAMIC match at 336 bytes (21 aligned 16-byte entries), alignment 16384, with a sole terminal DT_NULL. Native artifact was inspected only, never executed.

The actual fixture and audit leave compatibility/version satisfaction unresolved and download, installation and game-load-order authority false. No production account/credentials, console, archive transfer, game/system write or stopped-action retry occurs. Paused/user files were untouched and this agent staged or committed nothing.

Verification commands:

```
python3 outputs/ps5-mod-bridge/native-host/tests/nexus_acquisition_plan_verify.py --phase green --app-profile .superpowers/native-dependency-spike/native-nexus-acquisition-plans-1/configured-host-1/desktop_fixture-download --output .superpowers/native-dependency-spike/native-nexus-acquisition-plans-1/green-2
# exit 0; 55 checks; all_passed=true
python3 .superpowers/native-dependency-spike/native-nexus-acquisition-plans-1/native_artifact_audit.py
# exit 0; passed=true; failures=[]
```

Frozen source hashes:

- `outputs/ps5-mod-bridge/native-host/http.c` SHA256 `83da8294a7ff74ecfaa69f76cbb384fd97a1d654246e5585d683e67c41adb6f0`
- `outputs/ps5-mod-bridge/native-host/tests/nexus_acquisition_plan_verify.py` SHA256 `1bb09a3fa309b00d5150e12b441dc942ddce3f9a3688c928bec72aeb82455a73`

Receipt hashes:

- `red-1/receipt.json` SHA256 `66b348b2e644dec0b5e003596cf272907a15741fbf11fe0fe58f9be79bd22656`
- `green-2/receipt.json` SHA256 `0dbfafef26d879fcdf5f3e2291e1dbbcdc8aec9fcdc709ad9576f431ba4014a3`
- `native-artifact-audit-1/receipt.json` SHA256 `5f97591f2d3ae7990fe1b8dc5586cbbc86b3950a4a0ab9c624f62c708195c08a`
