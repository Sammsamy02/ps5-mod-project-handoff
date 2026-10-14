# cap01-kread-probe-v1 — LSTAR classifier fix, build receipt

2026-10-13, root session (ZCode). Implements the fix prescribed by
`LSTAR-DIAGNOSIS.md` §"Recommended fix" (agent KG, read-only scope there;
implementation was deliberately left to a later pass). Mac-only: built,
disassembled, hashed. No console contact — loading remains a user-authorized
step per TEST-CARD.md. PS5 is unavailable until the user says otherwise.

## 1. What changed (v0 → v1), and nothing else

| Aspect | v0 (KernelReadProbe.c, PRESERVED byte-exact) | v1 (KernelReadProbe-v1.c, THIS) |
|---|---|---|
| kekcall return | `int64_t`, CF encoded by negating RAX (`is_error ? -(int64_t)ret : ret`) | raw `uint64_t` RAX + separate `int* cf_error` out-param (CF captured via the existing `"=@ccc"` constraint) |
| classify_msr_result | `(int64_t)rc < 0` tested FIRST → every canonical kernel address (bit 63 set) misread as errno | `cf_error` tested FIRST; value only range-checked `0 < rc < 0x1000`; bit 63 set is a valid success |
| classify-failure logs | no raw rc (made v0's LSTAR FAIL ambiguous) | every failure branch prints `rc=0x…` + `cf=` |
| CHECK errno branch | detected via negative int64 encoding | detected via CF directly; same gate semantics (PASS ⇔ rc==0, unchanged) |
| Probe ID / log path | `cap01-kread-probe-v0` / `/data/cap01-kread-probe-v0.log` | `cap01-kread-probe-v1` / `/data/cap01-kread-probe-v1.log` (distinct evidence, v0 log never clobbered) |
| Checks, order, shape gates, exit codes | 4 checks, same | identical |

New-revision law honored: distinct probe ID, distinct console log path, v0
source/ELF/console evidence all preserved untouched.

## 2. Build (verbatim)

```
make   # in cap01-03/probe/ — prospero-clang via sdk-v0.43 prospero.mk,
       # CFLAGS -Wall -Werror -O2 -g, PATH prepends /opt/homebrew/opt/llvm@18/bin
```

Result: zero errors, zero warnings.

| Artifact | Size | SHA256 |
|---|---:|---|
| `cap01-kread-probe-v1.elf` | 121,352 B | `2b89f99a9cd3f72f96dea8149854dbf47dec026d135df1b21a8f5238c88380c4` |
| `cap01-kread-probe-v0.elf` (unchanged, provenance) | 119,432 B | `12ae76a08fd7afb2b6c0da06d6c51ccc34f9d9fc8726bd2fc7847a85c3192dcb` |

v1 ELF format: ELF 64-bit LSB PIE executable, x86-64, dynamically linked,
with debug_info, not stripped — same format class as v0 and the
console-proven ploader.elf.

## 3. Compiled-code verification (llvm-objdump, llvm@18)

`classify_msr_result` is inlined at -O2; verified at each syscall site in
`main`. LSTAR site (offset 0xcb):

```
cb:  syscall
cd:  setb  %cl                ; cl = RFLAGS.CF
d0:  jb    0x159              ; CF set → classify-failure (logs rc + cf)
d6:  leaq  -0x1(%rax), %rdx
da:  cmpq  $0xffe, %rdx        ; 1 <= rc <= 0xFFF → errno-class failure
e1:  jbe   0x159
e3:  movq  %rax, %rcx          ; RAW value — no sign test anywhere
e6:  shrq  $0x2f, %rcx         ; value >> 47
ea:  cmpl  $0x1fffe, %ecx      ; == 0x1FFFF canonical kernel half
f0:  jbe   0x185               ; shape fail (logs value)
f6:  … PASS print path (value, top32)
```

A bit-63-set LSTAR (e.g. `0xffffffff82…`) now reaches the canonical check
and prints `canonical_kernel=yes verdict=PASS`; in v0 the same bits were
consumed by the sign test before any value logic. CHECK site (0x66) keeps
gate semantics identical (`testq %rbx,%rbx; je PASS`), with CF now only
selecting the errno-vs-parent-pid log line. TSC sites (0x255/0x28f) capture
`setb` + range-check both reads before the delta compare. READ_DR gate
(rc == 0) unchanged, logs gain `cf=`.

## 4. Expected console output (NEXT authorized run — do not self-run)

Per LSTAR-DIAGNOSIS.md item 3, a healthy run should now print:

```
check=CHECK rc=0 verdict=PASS meaning=kstuff_resident_and_eligible
check=RDMSR_LSTAR value=0xffff<…52 hex…> top32=0xffffffff canonical_kernel=yes verdict=PASS
evidence=RDMSR_EFER value=0x0000000000011d01 verdict=LOGGED_ONLY
check=RDMSR_TSC t1=0x… t2=0x… delta=… verdict=PASS
check=READ_DR rc=0 verdict=PASS
summary checks=4 failures=0
overall=PASS exit=0
```

If LSTAR still FAILs, the new failure line now carries `rc=`/`cf=` and is
self-diagnosing (CF set + small rc = delivery-path errno; CF clear + rc in
errno range = errno-in-RAX convention; large non-canonical rc = shape
question). Note the 3-restart law: v0 has 1 clean boot so far; the law
needs 3 passes across restarts for the console-proven claim — v1 runs
count toward it fresh.

## 5. Console procedure delta vs TEST-CARD.md

Same single-run procedure as TEST-CARD.md, with the v1 filename:
upload `cap01-kread-probe-v1.elf` (readback-verify sha above), load via
`pldmgr /loadpayload`, read `/data/cap01-kread-probe-v1.log`. One run per
boot, user OK first. v0's log/evidence stay as-is for the record.
