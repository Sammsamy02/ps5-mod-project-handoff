# Native thread and resolver prerequisites — 2026-09-30

## Finding

The cached SDK exposes the POSIX thread calls needed for a one-worker checkpoint, and the existing curl ELF links a POSIX threaded resolver path. This is declaration/export and link evidence only: the cached exports are SDK stubs, and no worker, TLS destructor, or resolver was executed on the console. The current native-core probe exercises one scalar `thread_local` on its calling thread only.

## What the pinned SDK exposes

The SDK v0.43 `pthread.h` declarations and `libkernel_web.so` export stubs cover these exact calls:

| Call | SDK prototype |
| --- | --- |
| create | `int pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *)` |
| join | `int pthread_join(pthread_t, void **)` |
| once | `int pthread_once(pthread_once_t *, void (*)(void))` |
| key lifecycle | `int pthread_key_create(pthread_key_t *, void (*)(void *))`; `int pthread_key_delete(pthread_key_t)` |
| key access | `void *pthread_getspecific(pthread_key_t)`; `int pthread_setspecific(pthread_key_t, const void *)` |
| timed join extension | `int pthread_timedjoin_np(pthread_t, void **, const struct timespec *)` in `pthread_np.h` |

The five relevant SDK files—both headers, `libkernel_web.so`, `libc.a`, and `libc++abi.a`—match the SDK ZIP pinned by SHA-256 `a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb`.

These matches establish compile/link availability, not the console implementations or their behavior. The `pthread_timedjoin_np` declaration and export are real, but the curl ELF does not import it and there is no runtime evidence for its timeout semantics. If a timed join returns on a wait timeout, that only bounds the wait; without cancellation evidence it does not establish that the worker stopped.

## What the curl spike actually links and does

The corrected curl config defines `HAVE_GETADDRINFO`, `HAVE_PTHREAD_H`, and `USE_THREADS_POSIX`; curl’s `curl_setup.h` derives `CURLRES_THREADED` and `CURLRES_ASYNCH` from that configuration. `asyn-thrdd.c` starts `getaddrinfo_thread` through `Curl_thread_create`. `curl_threads.c` maps that wrapper to `pthread_create`, `pthread_join`, `pthread_detach`, and pthread mutex calls.

The linked HTTP/HTTPS-only smoke ELF imports `pthread_create`, `pthread_detach`, `pthread_join`, `pthread_mutex_{init,destroy,lock,unlock}`, and `sceNetResolverCreate`, `sceNetResolverDestroy`, `sceNetResolverStartAton`, and `sceNetResolverStartNtoa`, along with socket and polling calls. Its `DT_NEEDED` set is `libkernel_web.sprx`, `libSceLibcInternal.sprx`, and `libSceNet.sprx`. The fixture never calls `curl_easy_perform`; it made no DNS or network request.

Curl’s resolver has material lifetime and hang implications. It retains two references to resolver context while a worker runs. Cleanup joins if the worker is marked done and detaches it otherwise. The source says an earlier `pthread_cancel` attempt caused hangs and resource leaks; cancellation is not used. Other await/cleanup paths call plain `pthread_join`, so a resolver call that does not return can hold up cleanup. A detached resolver can continue after its caller starts cleanup while the worker-held context reference remains. This code provides no hard resolver deadline or cancellation guarantee. The exported `pthread_timedjoin_np` does not change that evidence.

## Current core TLS evidence and unresolved source behavior

The core probe’s `thread_local volatile int tls_value=41` is read, changed to 42, and reset by `tls_check()` on the same thread. The seven owned core checks include `thread_local_single_thread`; they do not create a worker or compare TLS between threads. The `core_bridge.o` object references `__emutls_get_address`. The retained final core ELF contains `__emutls_get_address`, `emutls_init`, and `emutls_key_destructor`, and imports `pthread_once`, `pthread_key_create`, `pthread_getspecific`, `pthread_setspecific`, and mutex operations. It has no `pthread_create`/`pthread_join` import and does not import `__cxa_thread_atexit`.

The SDK’s retained `libc.a:emutls.o` has the same emutls and pthread-key symbol surface. `libc++abi.a:cxa_thread_atexit.cpp.o` defines `__cxa_thread_atexit` and contains `DtorsManager`/`run_dtors` symbols; its object references `pthread_key_create`, `pthread_setspecific`, and `__emutls_get_address`. The SDK C++ ABI header declares `__cxa_thread_atexit`. However, the SDK package retains these as compiled objects, not the `emutls.c` or `cxa_thread_atexit.cpp` source. Therefore the exact worker-local initialization, destructor order, cleanup-on-exit behavior, and failure paths remain unresolved. The current trivial scalar probe does not pull in or exercise the C++ thread-local destructor path.

## Recommended next checkpoint

Before any curl or resolver runtime test, use one owned, joinable worker in a separate diagnostic fixture. Keep it free of curl, sockets, DNS, accounts, and external hosts. The fixture should:

- explicitly create one joinable worker, run a short routine, join exactly once, and verify the worker result;
- call `pthread_once` and exercise one SDK emulated `thread_local` scalar from the worker, showing the worker’s initial value and mutation are isolated from the main thread;
- create one pthread key with a destructor, set and read its worker value, then verify the destructor’s observable effect after the worker exits and join returns;
- keep the thread handle, key, worker context, and result/destructor counters alive until join completes; do not detach the worker or free shared context early.

Treat a stalled create/join or missing teardown observation as unresolved. There is no in-process hard timeout or cancellation claim for this checkpoint; a process-level watchdog can provide operational recovery, but does not establish pthread cancellation. Keep non-trivial C++ `thread_local` destructor behavior as a separate gate until its retained implementation semantics can be source-reviewed and observed.

Evidence inspected: [curl compile/link report](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-09-30-native-curl-compile-spike.md), [SDK pthread declarations](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/pthread.h), [non-POSIX timed-join declaration](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/pthread_np.h), [curl threaded resolver source](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/curl-src/lib/asyn-thrdd.c), [curl pthread wrapper](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/curl-src/lib/curl_threads.c), and [current core TLS probe](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/core_bridge.cpp).

Review scope: static inspection and cached symbol/header comparison only; no builds, target execution, DNS, or network calls.
