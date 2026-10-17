# Acquisition queue actual app fixture and native audit

DONE. This agent changed only the new native-host/tests/nexus_acquisition_queue_verify.py and ignored phase evidence/audit files. No product implementation, build driver, Git, paused file or credential input was edited. No remaining concrete finding.

Historical behavioral RED uses the retained compiled GREEN2 test-shim binary from 19e007515e276a70cc9b32ff443c63cd262578a3, SHA256 3b7f34798265ca133a77acd68be1f76c7b99e9c720c3e17f0f6a56d7697c2045. Its host profile artifact is a56486fd9b861f47b49ace574f8ddf30041c100bfaf54cac959226cab31146eb. All 136 manifest source hashes were checked by git show against that commit and saved under red-1/source-snapshot; manifest/config snapshots are also preserved. Actual owned PKCE succeeds, but a valid strict POST /api/acquisitions returns 400 instead of 202 before any discovery or archive request. Receipt retains intended_atomic_acquisition_available=false and expected_red_observed=true. Product barrier was released immediately after that observation.

Actual TLS GREEN passes 51/51 checks, exit 0. It runs one frozen composed app binary in separate premium and fresh free-account processes. The only curl change is the existing test-only socket call-site shim, which redirects approved loopback :443 connections into the owned TLS listener while retaining real curl/CA/SNI/hostname verification. The synthetic prerequisite graph maps Movement fixture mod 7/file 21 to Leon fixture mod 6/file 20. This is owned metadata, not a claim about the real mods' dependency relationship.

Premium admission returns exactly two prerequisite-first child mappings. One POST is followed by a provider-condition wait with no app GET/poll/request: both original archives complete, driver request count stays 1→1, observed interval 2.586 seconds. Both workers independently reread live userinfo, visibility and authorized download links. Exactly two archive requests carry no account headers. The real file sink, original ZIP reader and analyzer complete; private source files and reports match unchanged Leon (105647244 bytes, SHA256 661aabf0e6a1cb91e9164501108b5d19db13c2ca21cee651f676e0706f3feef9) and Movement (2805 bytes, SHA256 3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad). No single-source preparation or automatic generation occurs.

After fixture primary metadata changes, exact group replay returns the same receipt/IDs with no provider or transfer request. Changed intent conflicts, client-supplied plan data is refused, wrong CSRF and GET are refused, and another cookie cannot read a child. Exact accepted receipt IDs are passed to one existing explicit generation. Its real private outputs, complete physical readback, bytes and SHA256 match all five retained pair-oracle artifacts. The sidecar is 187714978 bytes; four metadata artifacts also match. There are no extra archive downloads.

A fresh free-account/no-grant process accepts the metadata-ready group, then both workers progress without HTTP polling (driver count 1→1) and end needs_nexus_authorization. There are zero download-link/archive requests or retained original files, and the explicit generation attempt is refused without a subset/output. Thus plan/queue admission confers no entitlement or acquisition authority. Premium TLS has 16 requests; free TLS has 10 requests. Public privacy/bounds, original/capture identity preservation, private scratch identity and cleanup pass for both runs.

The host profile artifact is 15b29a802473207c2f7ed1e7a066d8c1d1273990f3f39a55c01c1bc5a76b14e0 (2330872 bytes). Native static audit passes for cd3518d329ae8da6691031be1eef6656218120db01fb5da570ea084e72dbd1bb (2844848 bytes). All 136 sources, 53 objects, 16524 header records and 243 retained archive inputs are current, with no host headers. Atomic acquisition submission and prior submit/worker/plan/transport/catalog/preparation/generation bodies are retained. One selected crypto provider supplies SHA256/zeroize and the existing format-only SHA1 remains scoped separately. Imported-symbol membership is unchanged at 137. PT_DYNAMIC=SHT_DYNAMIC is 336 bytes/21×16 entries, aligned 16384, with a sole terminal DT_NULL.

This is ordinary-host execution plus native compile/static composition evidence. The native ELF was not executed. There was no production Nexus/account contact, console contact, game/system publication, dependency upgrade or stopped-action retry. Runtime/installation/game-load-order/version satisfaction authority remains false or unresolved. Persistent installation and free-account website grant/resume remain outside this completed slice.

Commands:

```
python3 outputs/ps5-mod-bridge/native-host/tests/nexus_acquisition_queue_verify.py --phase red --output .superpowers/native-dependency-spike/native-nexus-acquisition-queue-1/red-1
# expected_red_observed=true; intended availability=false
python3 outputs/ps5-mod-bridge/native-host/tests/nexus_acquisition_queue_verify.py --phase green --app-profile .superpowers/native-dependency-spike/native-nexus-acquisition-queue-1/configured-host-1/desktop_fixture-download --output .superpowers/native-dependency-spike/native-nexus-acquisition-queue-1/actual-tls-green-1
# exit 0; checks=51; all_passed=true
python3 .superpowers/native-dependency-spike/native-nexus-acquisition-queue-1/native_artifact_audit.py
# exit 0; passed=true; failures=[]
```

Identity hashes:

- `outputs/ps5-mod-bridge/native-host/tests/nexus_acquisition_queue_verify.py` SHA256 `62164243713aa3c33dd0b248b4d193bd629b1f9a80ffdf5b83a2d87e05d44c34`
- `.superpowers/native-dependency-spike/native-nexus-acquisition-queue-1/red-1/receipt.json` SHA256 `45d3ce8c6b64f3829b58966dced8fa323380c3ff5c424435b2ae76de195eeea6`
- `.superpowers/native-dependency-spike/native-nexus-acquisition-queue-1/actual-tls-green-1/receipt.json` SHA256 `c4483f16a623103c050a658878824d8c7a7fe2de1bec00cf1d15e0b2e4e8647a`
- `.superpowers/native-dependency-spike/native-nexus-acquisition-queue-1/actual-tls-green-1/premium/receipt.json` SHA256 `5eeea911567ff046672c59e51dd0dd7dc4e7d693b8c1080fdad9dc7efaeaca6a`
- `.superpowers/native-dependency-spike/native-nexus-acquisition-queue-1/actual-tls-green-1/free/receipt.json` SHA256 `1cc5ac8971ce1ebc60a4d677cb2cd9bc32662d4398c4f1ed2573b8a029a5d08f`
- `.superpowers/native-dependency-spike/native-nexus-acquisition-queue-1/native-artifact-audit-1/receipt.json` SHA256 `fec17c6af4e91c12d5eb39cb33b85070ec3a401b5566c88506e90d4dc547b9cb`
