# VERIFICATION.md — raw evidence, worker-8fam-console-prep, 2026-10-04
All commands run this session; paths absolute under /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team.

## 1. Re-hash of the 8-family SDK ELF (payload law: local re-hash == receipt sha256 EXACTLY)
```
792811911e773c9b07a47ba3b22bc5d290f9f3ab86056ddfc985cf25e6d804fb  /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-dll-8fam-combined/out-sdk-composition/c3aba79f968a492b849e5ece2f009e8e/owned-dll-candidate.elf
336552 bytes
/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-dll-8fam-combined/out-sdk-composition/c3aba79f968a492b849e5ece2f009e8e/owned-dll-candidate.elf: ELF 64-bit LSB pie executable, x86-64, version 1 (FreeBSD), dynamically linked, BuildID[md5/uuid]=0224ee94363709e5114bb133231aa4a7, not stripped
```
Receipt says: 792811911e773c9b07a47ba3b22bc5d290f9f3ab86056ddfc985cf25e6d804fb / 336552 — EXACT MATCH.

## 2. Undefined-import set diff (nm -u, sorted, uniq) — expect identical 55-set vs BOTH priors
```
counts: u6=      55 u7=      55 u8=      55
diff 6fam(133c5e9e) vs 8fam(c3aba79f): IDENTICAL
diff 7fam(e72f5318) vs 8fam(c3aba79f): IDENTICAL
```
The 55-symbol set (from u8):
```
__error
abort
calloc
clock_gettime
close
closedir
free
fstat
getpagesize
lstat
malloc
memcmp
memcpy
memmove
memset
mkdir
munmap
nanosleep
open
opendir
pread
pthread_cond_broadcast
pthread_cond_destroy
pthread_cond_init
pthread_cond_signal
pthread_cond_timedwait
pthread_cond_wait
pthread_condattr_destroy
pthread_condattr_init
pthread_condattr_setclock
pthread_create
pthread_equal
pthread_getspecific
pthread_join
pthread_key_create
pthread_key_delete
pthread_mutex_destroy
pthread_mutex_init
pthread_mutex_lock
pthread_mutex_unlock
pthread_self
pthread_setspecific
pwrite
readdir
sceKernelMprotect
sched_yield
send
sleep
strcmp
strdup
strlen
strrchr
sysconf
unlink
write
```

## 3. Host-oracle record comparison (host-normal.jsonl)

```
== 7fam host-normal (2cc39eef)
seq=0 phase=1 consumer_imports=0 results=[0, 0] memory.last.length=0 static_tls=None seh64=None
seq=1 phase=2 consumer_imports=0 results=[0, 0] memory.last.length=0 static_tls={'enabled': 1} seh64=None
seq=2 phase=3 consumer_imports=82 results=[86, 93] memory.last.length=98304 static_tls={'enabled': 1} seh64=None
seq=3 phase=5 consumer_imports=82 results=[86, 93] memory.last.length=147456 static_tls={'enabled': 1} seh64=None
== 8fam host-normal (256e9b3a)
seq=0 phase=1 consumer_imports=0 results=[0, 0] memory.last.length=0 static_tls=None seh64=None
seq=1 phase=2 consumer_imports=0 results=[0, 0] memory.last.length=0 static_tls={'enabled': 1} seh64={'enabled': 1}
seq=2 phase=3 consumer_imports=90 results=[86, 93] memory.last.length=114688 static_tls={'enabled': 1} seh64={'enabled': 1}
seq=3 phase=5 consumer_imports=90 results=[86, 93] memory.last.length=163840 static_tls={'enabled': 1} seh64={'enabled': 1}
```

## 4. The 8 consumer symbols (from tests/worker-seh64-kernel.def / worker-seh64-vcruntime.def)
```
LIBRARY KERNEL32.dll
EXPORTS
 RtlCaptureContext
 RtlLookupFunctionEntry
 RtlVirtualUnwind
 RtlAddFunctionTable
 RtlRemoveFunctionTable
 SetUnhandledExceptionFilter
 RaiseException
LIBRARY VCRUNTIME140.dll
EXPORTS
 __C_specific_handler
```

## 5. ELF sizes
```
6fam 133c5e9e: 300456
7fam e72f5318: 334656
8fam c3aba79f: 336552
```
