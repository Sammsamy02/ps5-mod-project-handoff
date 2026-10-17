#include <pthread.h>
#include <pthread_np.h>
#include <sys/thr.h>
#include <unistd.h>
#ifndef __PROSPERO__
#error genuine Prospero target required
#endif
#ifndef _POSIX_VERSION
#error no POSIX backend declaration
#endif
static int (*const attr_get)(pthread_t, pthread_attr_t*) = pthread_attr_get_np;
static int (*const stack_get)(const pthread_attr_t *restrict, void **restrict, size_t *restrict) = pthread_attr_getstack;
static int (*const os_id)(long*) = thr_self;
int api_probe(pthread_t thread, pthread_attr_t *attr, void **addr, size_t *size, long *tid, pthread_key_t *key) {
  return attr_get(thread,attr) + stack_get(attr,addr,size) + os_id(tid)
    + pthread_key_create(key,0) + pthread_setspecific(*key,addr)
    + (pthread_getspecific(*key) == 0) + pthread_key_delete(*key);
}
