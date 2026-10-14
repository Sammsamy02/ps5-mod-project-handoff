#include <config.h>
#include <mono/metadata/w32process-unix-internals.h>
#include <pthread.h>
#if !defined(__PROSPERO__) || __FreeBSD__ != 9
#error genuine target required
#endif
#if !defined(TARGET_AMD64) || !defined(DISABLE_JIT) || !defined(ENABLE_COOP_SUSPEND)
#error target build policy mismatch
#endif
#if defined(DISABLE_AOT) || defined(MONO_KEYWORD_THREAD)
#error AOT must remain enabled and compiler TLS disabled
#endif
#if !defined(USE_DEFAULT_BACKEND) || defined(USE_BSD_BACKEND)
#error process diagnostic capability selection mismatch
#endif
#if defined(HAVE_DL_ITERATE_PHDR) || defined(HAVE_KINFO_GETFILE) || HAVE_DECL_PTHREAD_GETNAME_NP
#error current public SDK negative capability probes changed; review source port
#endif
_Static_assert(sizeof(void*)==8 && sizeof(long)==8, "LP64 required");
int final_target_configuration_verified;
