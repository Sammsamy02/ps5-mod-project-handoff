# Fixed native requirements POST transport

DONE: additive private `mb_curl_provider_post_transfer_v1` reuses the existing
serialized curl8.18/PSA owner and only admits the canonical read-only requirements
query at the exact fixed HTTPS/443 URL. Existing GET/OAuth/archive layouts and
symbols are unchanged. Entire outer params, borrowed spans and known first-byte
mutable context aliases are checked before mutation. Valid independent raw
storage is erased in full; unsafe aliases/layouts are untouched. JSON content
type is explicit; credential headers, TLS trust/name checks, no redirect/cookie
policy, consumer callback and final timeout/terminal cleanup use the shared path.
Full opaque context extents remain caller-owned; opaque curl internals are not
claimed securely erased. No credential or request/response body is logged.

Genuine RED: retained legacy transport was frozen before behavior edits. Actual
link failed for the missing POST symbol. An explicitly test-only compatibility
adapter ignoring POST body and invoking old GET returned BAD_ARGUMENT1 for the
canonical request, instead of admitted BAD_STATE2 with the owner not started.
`red-1/receipt.json` preserves all commands and baseline source hashes. No socket
or runtime started for this RED.

GREEN: `admission-green-1` and the final `tls-green-3` both pass29 real retained
transport admission controls. Actual owned TLS GREEN passes13 runtime checks
across9 requests: bearer/key POST bodies and exact headers; old GET control;
cancellation before socket and after consumer; consumer refusal; real local
expiry overriding consumer failure; reusable owner after timeout; no followed
redirect; explicit terminal owner fault after consumer and physical global
cleanup. Real OS `getentropy` supplied76 fresh32-byte callbacks (2432bytes).
The socket shim maps only approved resolved loopback:443 sockets to an ephemeral
owned port; real URL/Host/SNI/CA/name verification and pinned archives remain.
Returned status/complete and full raw wipe are asserted for every attempt.

Two preceding TLS attempts failed only before sockets: verifier assumed the
wrong manifest profile shape/source path, then the test driver lacked the host
`sys/random.h` declaration. Both failed receipts are retained; product behavior
was unchanged. There was no live account/provider request, native execution,
console access, game/system write, dependency upgrade or stopped-action retry.
Native compile and fullapp HTTP/typed decode proofs belong to the root phase.

Focused replay:

```
python3 outputs/ps5-mod-bridge/native-host/download/requirements_post_verify.py --output <fresh ignored directory>
```

Only the following assigned product/test files changed; they are frozen pending
concrete review findings. No staging or commit by this agent.

- `provider_transfer.h` SHA256 `da2d491c60753f120fa09147e48529142d0a108ff84c889fd50143ffa55eef9a`
- `curl_download.c` SHA256 `890f5df2882f8f86dd46b2bc9da01802a2a7f913f63e5c430a25e2f7eaa819a7`
- `requirements_post_verify.py` SHA256 `ded0d65fd8c19d5c66c61f75266567e8b6c8f39b0f5313b33435ca72547e8f8a`
- `tests/requirements_post_control.c` SHA256 `57629dc1ee176ee3f38aa112d63e2f38355e06ad328ed33be0326d507858de5f`
- `tests/requirements_post_tls.c` SHA256 `cdd4e60f932a9083d2a7c2b33a73233e8cf773c887b72c3cc6d31802c9e4e227`

Final TLS receipt SHA256 `906bf210f0307a429988634812d50cca406b82adf04dfba34966e00bcf2ec9f9`.
