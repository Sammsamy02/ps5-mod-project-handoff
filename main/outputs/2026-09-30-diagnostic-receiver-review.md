# Independent diagnostic receiver review — 2026-09-30

Result: no actionable implementation findings in the reviewed local checkpoint. This review does not approve console execution or establish a working native ABI.

## Scope and identity

Reviewed `outputs/ps5-mod-bridge/native-probe/diagnostic.py` and `native-probe/tests/test_diagnostic.py` against the diagnostic receiver design, implementation plan, console-only roadmap, native report formatter/collector, restricted ELF verifier, and saved WebKit Autoloader identification research. Base checkpoint: `810debf`; artifact/report gate commit: `7b87c76`; transport changes were uncommitted at review time.

Reviewed file SHA-256 values:

- `diagnostic.py`: `57ce801b048bc35619ca588a53656aefb8990a4ae33e86b4306e341262abf2d6`
- `tests/test_diagnostic.py`: `576cf5108361b3aa30751b545e143f854491a8e8687ce183c7aa9224d92a2d92`

## Evidence

Used `/WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3` from the managed worktree.

- `-m unittest discover -s outputs/ps5-mod-bridge/native-probe/tests -p 'test_diagnostic.py' -v`: 15 tests passed.
- `-m unittest discover -s outputs/ps5-mod-bridge/native-probe/tests -v`: 31 tests passed, including existing dependency, preparation, formatter and ELF verifier tests.
- Independent temporary, in-memory checks: 49 malformed parser inputs rejected, including boolean/float substitutions at every declared integer gate, wrong top-level types, BOM and duplicates in nested build/observation/capability objects.
- An independent hand-built ELF fixture moved its section-name table beyond the section-header table. Inspection chose the greater section end and excluded the remaining trailer, exercising the extent branch not reached by the saved candidate.
- A controlled socket/clock check advanced time during successful short sends until the common budget expired. The result was `send_failed`, 16,384 bytes sent, `send_complete=False`, and a closed socket. The successive operation budgets were approximately 0.10, 0.06 and 0.02 seconds. This specifically exercises progressing-send exhaustion rather than only a mocked socket timeout.

## Review observations

Artifact inspection uses one descriptor with no final-component symlink following, a regular-file check, bounded read, pinned size/hash, and immutable bytes before socket creation (`diagnostic.py:44–84`). The verifier operates on the same bytes later sent. The existing snapshot test and transport-time path mutation test assert actual byte identity against the pinned artifact, so they do not merely compare a mocked inspection result to itself.

The prefix is the maximum of the section table end and non-NOBITS section ends, with program-table and PT_LOAD coverage checks (`diagnostic.py:69–82`). Owned socket integration asserts exact received bytes, trailer exclusion and write-half closure. Its source interpretation agrees with the saved pinned downstream-fork research; this review did not independently fetch or authenticate the installed loader.

Strict object keys, duplicate rejection, numeric type/range checks, native origin/target restrictions, null environment facts and unknown/null capability evidence prevent the specified desktop, boolean and capability-spoofing cases (`diagnostic.py:103–155`). Parser acceptance still provides no authenticity, freshness or binding to the transmitted artifact.

One absolute monotonic deadline is shared by connect, short sends, shutdown and receives, and the remaining budget is recalculated before each operation (`diagnostic.py:194–224`). Real socket tests cover progressing responses and a complete report held open; parsing happens only after EOF. Response buffering uses at most 4,097 bytes and distinguishes empty, invalid, overflow, timeout and failed responses. Results retain byte counts and artifact/prefix metadata without returning raw error text (`diagnostic.py:185–241`). All sockets are closed in the normal and tested failure paths; no retry is exposed.

## Limits

The independently run suite covers local development contracts only. Real transport integration uses owned IPv4 loopback peers; IPv6 transport and actual OS send-buffer exhaustion were not independently exercised. Controlled clock/socket checks supplement those integration tests. `make test-all` was not rerun by this reviewer; wider core/provider/UI validation remains the implementer's separate checkpoint.

No native ELF was executed, no console or Nexus endpoint was contacted, and no implementation, index or commit was changed by this review. The native resolver/termination blockers remain open. The likely WebKit fork binds its listener on the console's own loopback; a successful PC loopback fixture cannot demonstrate LAN access to that loader. `report_received_unverified` is the maximum accepted evidence state and does not imply execution, firmware, loader identity, privileges, translation, installation or gameplay support. The module stays a development tool and does not fulfill the finished product's console-only runtime requirement.

## Root follow-up after the pinned review

The hashes above identify the independent review's scope. Root then replaced Python-3.10-only annotations with Optional, recognized Python 3.9's separate socket.timeout class, and added a failing late-EOF test plus a post-receive deadline check. Root verified 16 receiver / 32 native tests on Python 3.9.6 and 3.12.14 after these fixes. These changes are root-verified and do not extend the independent review's pinned-file endorsement. Exact reproduction and wider suite evidence are in `2026-09-30-diagnostic-receiver-report.md`.
