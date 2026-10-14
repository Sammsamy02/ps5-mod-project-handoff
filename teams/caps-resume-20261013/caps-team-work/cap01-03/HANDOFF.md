# #1/#3 — no admitted callable kernel provider

**Unfinished. No client or kernel operations staged.** The concrete blocker is absence of an exposed kernel-memory read/write service in the exact inspected public source, plus missing independent source↔resident-binary identity. A userspace wrapper cannot call the privileged internal copy helpers without adding a gate or bypassing their execution context. Neither is included here. Metadata, hardware-register reads and host-owned memory are not substitutes for kernel-memory proof.

SOURCE-AUDIT.json binds current source archive SHA256 b4fcf169a12aa184c812f4243c5f0cca40f1ad932d9a14530bbc667aef47ddf6 and7 relevant text members with before/after stable archive identity. No ELF binary was read. Key findings:

- ps5-kstuff/uelf/kekcall.c:20–108: dispatched client operations provide no generic kernel-VA copy service.
- ps5-kstuff/uelf/utils.h:7–29: copy helpers depend on the privileged internal context; their declarations do not export a userspace provider.
- ps5-kstuff/uelf/main.c:85–91 and syscall_fixes.c:63–66: debugging syscall routing/fixup does not define a bounded operation request, authorization, read/write byte-count or restoration contract.
- prosper0gdb/r0gdb.h:74–75: opaque pointer declaration alone is insufficient to stage the debugging client.
- No LICENSE/COPYING/NOTICE/COPYRIGHT-style member was present. Public redistribution permission was not established; no upstream source was copied or vendored. Only hashes, member names and source-analysis conclusions are retained.

The known release/catalog hash of the historical resident binary is not independent evidence that these source members/configuration produced it. There is no reviewed binary service inventory or current provider identity binding in this deliverable. The old “source archive missing” status is stale; the actual remaining issue is an admitted callable contract and binary identity, not archive acquisition.

## Exact integration prerequisite

Supply an already-existing legitimate provider whose source defines bounded owned-target read/write operations, error/partial-I/O semantics and owner lifetime/revocation, independently tied to the exact resident binary/configuration. Its existing privileges must suffice; no new exploit, security-hook modification, protection bypass or arbitrary network kernel-RW is acceptable. Then stage its real client and test that concrete contract. Until supplied, partial-I/O/restoration tests against a fabricated provider would not close #1/#3.

## Recovery and acceptance — not a launch authorization

Before an actual write trial: capture exact provider/client/build/firmware/boot identity; define an owned bounded target and independent known original/expected values; establish recovery access and preserved rollback through a controlled failure on that admitted interface. A clean exit and absence of logs are not recovery proof. Stop on unknown ownership, identity, partial results, unexpected bytes or restore failure; never repeat an ambiguous write. No arbitrary kernel address or game instruction target is proposed here.

#1 needs genuine provider reads with independent known answers and exact byte counts. #3 needs bounded compare-before-write, independently checked effect/readback, exact restoration and verification, plus observable revoke/drain behavior. Record failures without claiming success or silently treating partial I/O as complete. Each new admitted kernel rung requires3 clean passes across3 separate restarts, fresh logs/identities per boot, one console owner and one test per boot under the integrator's explicit console authorization. Existing pointer hooks/metadata and historical captures do not satisfy this acceptance.

An archive-binding utility and8 refusal tests were completed before the parent redirected effort away from generic tooling. They are retained as audit evidence only, not progress claims for kernel functionality; no further tooling work is planned. TDD logs show source-auditor absence then8 passing checks. No target build, console, SDK access, original/current runtime/frozen edits, privileged calls or payload output occurred. No actual tool rejection occurred in this run; the earlier queue's safety-filter stop lacks a retained exact rejection rationale.
