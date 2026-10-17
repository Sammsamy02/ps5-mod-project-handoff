# Owned Windows DLL execution confirmed on PS5

The user launched build `db1c40e6b35e4f16b4f78afcb38ed15c` from the uploaded `TEST_THIS_DLL.elf`. A bounded, read-only FTP retrieval of `/data/modbridge-owned-dll-candidate.jsonl` returned four complete JSON records with the exact build identity and consecutive sequence numbers 0–3.

All three PE export calls started and returned. Results were `86` and `93`, the consumer and helper resolved 10 and 6 imports, both image sources were acquired/released once, and final cleanup was retired with no pending ownership. Runtime and native-memory first failure fields were zero. This is actual standalone Windows AMD64 PE DLL execution on the PS5, rather than a loader notification or host fixture result.

File delivery succeeded. The launcher's stdout subsequently closed, producing separately recorded `EPIPE` (32); that can make the diagnostic driver return nonzero for reporting even though DLL execution and cleanup succeeded. The file evidence remains complete.

This test uses the project's owned no-entry consumer/helper DLLs. It does not establish arbitrary downloaded DLL compatibility, DllMain, static PE TLS, exception/unwind behavior, game hooks, UE4SS or an engine ABI. The separate entry + clock build is uploaded for the next user-run test and has no result yet.

Artifact: 185,608 bytes, SHA-256 `bec47619a5738e599bd80a7d2a252deb3220001e865d8fe651e2a28c8c1577f9`.

Result: 3,199 bytes, SHA-256 `95da1edac649a84c3bda6585a8fac7f261106f83e04bf0e54d1bc91aafe653f3`.

Read receipt and original result are retained under `.superpowers/native-observations/owned-dll-db1c40e6-result-20261004T034941Z/`. Independent review confirmed identity, parsed transitions, import counts, cleanup and separation of the stdout error. The previously frozen source/artifact receipts are unchanged. No game files or autoload settings changed, and the agent did not dispatch the payload.
