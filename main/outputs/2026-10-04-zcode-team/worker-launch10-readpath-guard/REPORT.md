# worker-launch10-readpath-guard — controller hands-on build

Verdict: the ARMED initializer path performs NO data-mode reads of exec-only
pages. Slot 2 (FName ctor 0x63d590) and slot 4 (SCO 0x890aa0) are assigned and
called (instruction fetch, X-permitted); detour-verification reads are skipped
while seed-bound. All real reads (pool chain, GMalloc variable, instance vptr)
target RW data. This staging turns that audit into an enforced invariant:
- slots 0/1/3 refuse before any read unless the seed address classifies as
  DataReadable against the verified phdr table;
- slots 2/4 refuse the BIND unless the address classifies as CodeCallOnly;
- gaps and LOAD8 classify Refused for both actions.

Compile-gated with the exact recovered unit argv: exit 0, 6 pre-existing
warnings, 0 new; object +4,600 B carrying all 7 refusal strings (baseline has
none). Inline helpers fully inlined.

Integration: this staging COMPOSES the gmalloc V1/V2 validator (copied base)
— the launch-10 initializer unit is this file. Arming is therefore justified
by audit + guard, pending the geometry fix landing and the full one-pass gate.
