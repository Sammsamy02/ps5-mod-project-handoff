# Native Nexus requirements integration freeze

Scope: typed requirements POST, authenticated catalog wrapper, and local GET route. No live Nexus call, game/system write, credential read, console action, or installation behavior was performed. Root's pinned historical HTTP RED remains at `red-1/receipt.json` (7/7 assertions): the prior application returned 400 for the requirements path and issued no GraphQL POST.

## Frozen behavior and boundaries

- `mb_nexus_client_request_v1` now accepts the appended `MB_NEXUS_REQUIREMENTS` kind only with Bearer auth, a safe domain, and a positive source ID through `INT32_MAX`. It builds the canonical body with `nexus_requirements_body`, targets the fixed GraphQL URL, and calls the additive POST transfer ABI. Existing metadata GETs use their existing URL/GET transport path. Caller credential and response buffers keep the existing borrow/wipe/final-consumer behavior.
- Auth catalog kind value 5 first fetches the source mod using the existing visibility and adult-preference gates. Only a visible source reaches the requirements POST, using the same `Operation`, session, active unexpired token, trust configuration, cancellation, and total deadline. The decoded wrapper/object is checked for the exact source game/mod, completed discovery, unresolved row state, field bounds, unique Nexus relation IDs, and false version/install capability flags. The public response is an owning `{"requirements": ...}` object; no relation is marked satisfied and no file is selected.
- `GET /api/games/{domain}/mods/{id}/requirements` is exact-path only, accepts canonical positive IDs through `INT32_MAX`, uses the existing account admission and bounded owning HTTP response, and rejects larger IDs before a provider call. No `http.h` layout change was required.
- `tests/nexus_auth_catalog_driver.cpp` adds a fail-closed fake POST symbol solely so the existing GET-only regression can link against the additive client ABI. It marks its existing transport-contract assertion false if invoked; that fixture does not simulate requirements success. Root's owned-TLS app fixture covers actual requirements requests.

## Verification performed

From `outputs/ps5-mod-bridge/native-host/tests`:

```sh
python3 -m unittest test_nexus_auth_catalog
```

Result: `Ran 1 test ... OK` (7.557s). This compiles and exercises existing auth/catalog behavior with the actual typed client and decoder and the fixture's fail-closed POST stub.

From the repository root:

```sh
cc -std=c11 -Wall -Wextra -Werror -DMB_HOST_DOWNLOAD=1 -DMB_HOST_JOBS=1 \
  -I outputs/ps5-mod-bridge/native-host -I outputs/ps5-mod-bridge/include \
  -c outputs/ps5-mod-bridge/native-host/http.c \
  -o /tmp/modbridge-http-requirements.o

git diff --check -- \
  outputs/ps5-mod-bridge/native-host/auth.cpp \
  outputs/ps5-mod-bridge/native-host/auth.h \
  outputs/ps5-mod-bridge/native-host/http.c \
  outputs/ps5-mod-bridge/native-host/nexus_client.cpp \
  outputs/ps5-mod-bridge/native-host/nexus_client.h \
  outputs/ps5-mod-bridge/native-host/tests/nexus_auth_catalog_driver.cpp
```

Both returned exit status 0. The C compile used strict warnings-as-errors. These checks do not establish the owned-TLS requirements GREEN, which remains with the root's fresh app build and fixture.

## Source identities at freeze

Owned implementation and focused unit-fixture files:

| File | SHA-256 |
|---|---|
| `outputs/ps5-mod-bridge/native-host/nexus_client.cpp` | `8ac2f1880c0992593a44b7aba559b01ec482bb1830f95f603e13b08a5505e078` |
| `outputs/ps5-mod-bridge/native-host/nexus_client.h` | `6568403b8f2eade6d55357cc040a83fc46272ed6a05d3e59d619c5e7b46c444b` |
| `outputs/ps5-mod-bridge/native-host/auth.cpp` | `3fc388f3ab357adfa7659e3270129249b2bb1fac83aad4275983eb41f12552f6` |
| `outputs/ps5-mod-bridge/native-host/auth.h` | `15a55b3d67266645a3050a651b47b424e080458c9c15e2cbc129bb18a4121fd5` |
| `outputs/ps5-mod-bridge/native-host/http.c` | `3bee04a04287006cf2c55e861e04833dbdcc9b17ed37c6b8726a8231c2dfc6ac` |
| `outputs/ps5-mod-bridge/native-host/tests/nexus_auth_catalog_driver.cpp` | `06430eddb3aadae7d47c34bc3ca21e312d32d41c4a3ee86a0814feb33ef5f9e6` |

Dependencies checked at this handoff:

| File | SHA-256 |
|---|---|
| `outputs/ps5-mod-bridge/native-host/download/provider_transfer.h` | `da2d491c60753f120fa09147e48529142d0a108ff84c889fd50143ffa55eef9a` |
| `outputs/ps5-mod-bridge/src/nexus.cpp` | `f68371255f36aef3a39751635dd1e76cdb35e07a2eff9d11e9980aef318db0de` |
| `outputs/ps5-mod-bridge/include/modbridge/nexus.hpp` | `ea6e8dcf335ff70e2b27edb86795173440324322853531b8ef50e645f1cd4a97` |
| `outputs/ps5-mod-bridge/include/modbridge/nexus_requirements_query.h` | `0cefed3073fefc0829cc0b36e82c1b3df86da1e2c05ee4b6104077d226360eaf` |
| Root-owned app fixture `outputs/ps5-mod-bridge/native-host/tests/nexus_requirements_verify.py` | `e20fe03b392188a40c5f1b470b0b0fec0276aaebcdaa8e1c45b14ae97ffcf70b` |

