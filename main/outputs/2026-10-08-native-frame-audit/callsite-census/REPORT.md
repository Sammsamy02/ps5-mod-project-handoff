# Bounded Func+D8 call-pattern census

The executable PT_LOAD contains613 literal `call qword [base+0xD8]` byte patterns. All ModRM memory-call bases and SIB encodings are included. A local register-origin classifier finds566 zero-dereference/vtable-shaped sites,1 independently authenticated UFunction::Invoke site, and46 unresolved sites. **This does not close all UFunction.Func entry paths.** It does not justify an installation claim beyond the supported Invoke contract proved in the parent frame receipt.

The unique authenticated Invoke call is326B25 (`41ff97d8000000`). Its known function-self origin and save/set/restore frame90 are independent native dataflow, not pattern naming. It writes self to90 before dispatch and restores90 afterward. No second UFunction.Func invocation has been uniquely identified in this bounded pass.

The566 sites have a latest base origin shaped like `mov base,[object+0]`, often followed by a receiver argument and virtual call. They are **vtable-shaped diagnostic candidates**, not613 authenticated UFunction calls and not566 uniquely typed receiver-class proofs. A first-qword load can also be a data/array pointer; without receiver semantics this local shape alone cannot prove exclusion. Therefore none are silently counted as complete call-graph closure.

Unresolved classifications:23 spill/member/global loads;16 unavailable reliable local-prologue/alignment paths;7 other base definitions (mostly readonly-looking vtable LEAs). Their exact bytes, origins and bounded preceding instruction contexts are retained. Examples that do not resemble the Invoke UFunction receiver:

- 31F7F66: loads base from[receiver+8], then adjusts RDI+=8 before call; secondary-base-vtable shape.
- 5454FAD: loads base from[RBX+R14], while receiver is RBX+R14; same block calls88/98/A8/D8 virtual slots.
- 725D9DD/725DA54: base comes from[receiver+370], while RDI remains a different receiver; unresolved dispatch-table/member use.
- 7359DC6: base comes from[RBX+5A0], with RDI=RBX; unresolved dispatch-table/member use.
- Sixteen sites near6193B29..619CCF9 and632234 cannot be authoritatively aligned from this bounded prologue heuristic and remain raw patterns.

Limitations are explicit: a nearest `push rbp;mov rbp,rsp` linear decode is a local heuristic, not an authenticated function boundary/CFG. `mov fn,[function+D8];call fn`, computed LEA aliases, wrappers, thunk/register-call variants and tail jumps are outside the literal-memory-call census. Indirect calls may be reached through aliasing not visible in the preceding window. No completeness proof, runtime invocation, symbolic execution, receiver-class provenance or game activity is claimed.

The result favors keeping unsupported-entry handling separate. A known mapped Invoke entry can forward its captured original with the existing native frame. Unknown identity in a common wrapper cannot be forwarded safely using a guessed original or Code-8; either refuse installation before replacing Func for unproved entry profiles, or develop a per-registration identity/original thunk with a separately reviewed ABI and lifetime contract. This census does not choose or implement that design.

Reproduce with `python3 outputs/2026-10-08-native-frame-audit/callsite-census/scan_literal_calls.py` then `python3 .../classify_calls.py`. Both read only the authenticated saved image and write this owned diagnostic directory; neither executes engine code. The earlier frame receipt remains unchanged.
