#include "../mb_win_threads.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#c); abort(); } } while (0)
typedef struct gate { pthread_mutex_t mutex; pthread_cond_t cond; unsigned ready,phase,checked; } gate;
typedef struct worker { mb_win_thread_domain *domain; gate *gate; uint32_t index,number; int leave; uintptr_t error_address; int marker; } worker;
static void *worker_main(void *opaque){
 worker *w=opaque;int rc;CHECK(mb_win_threads_enter(w->domain,&rc)==MB_WIN_THREADS_OK);
 uint32_t *last=mb_win_threads_last_error(w->domain);int32_t *crt=mb_win_threads_errno(w->domain);
 CHECK(last&&*last==0&&crt&&*crt==0);w->error_address=(uintptr_t)last;
 *last=UINT32_C(0x80000000)+w->number;*crt=(int32_t)(90+w->number);errno=EDOM;
 CHECK(mb_win_threads_tls_get(w->domain,w->index)==NULL&&*last==0);
 *last=UINT32_C(0x80000000)+w->number;
 CHECK(mb_win_threads_tls_set(w->domain,w->index,&w->marker)==1&&*last==UINT32_C(0x80000000)+w->number);
 CHECK(pthread_mutex_lock(&w->gate->mutex)==0);++w->gate->ready;CHECK(pthread_cond_broadcast(&w->gate->cond)==0);
 while(w->gate->phase<1)CHECK(pthread_cond_wait(&w->gate->cond,&w->gate->mutex)==0);
 CHECK(pthread_mutex_unlock(&w->gate->mutex)==0);
 CHECK(*last==UINT32_C(0x80000000)+w->number&&*crt==(int32_t)(90+w->number)&&errno==EDOM);
 CHECK(mb_win_threads_tls_get(w->domain,w->index)==&w->marker&&*last==0);*last=700+w->number;
 CHECK(pthread_mutex_lock(&w->gate->mutex)==0);++w->gate->checked;CHECK(pthread_cond_broadcast(&w->gate->cond)==0);
 while(w->gate->phase<2)CHECK(pthread_cond_wait(&w->gate->cond,&w->gate->mutex)==0);
 CHECK(pthread_mutex_unlock(&w->gate->mutex)==0);
 CHECK(*last==700+w->number);CHECK(mb_win_threads_tls_get(w->domain,w->index)==NULL&&*last==0);
 CHECK(*crt==(int32_t)(90+w->number)&&errno==EDOM);
 if(w->leave){CHECK(mb_win_threads_leave(w->domain,&rc)==MB_WIN_THREADS_OK);CHECK(mb_win_threads_errno(w->domain)==NULL);}
 return NULL;
}
int main(void){
 mb_win_thread_domain domain={0};mb_win_thread_pin pin={0};int rc=-1,marker=123;
 CHECK(mb_win_threads_open(&domain,&rc)==MB_WIN_THREADS_OK);
 CHECK(mb_win_threads_acquire(&domain,&pin)==MB_WIN_THREADS_OK);
 CHECK(mb_win_threads_close(&domain,&rc)==MB_WIN_THREADS_BUSY); /* pin alone */
 mb_win_thread_pin copy=pin;CHECK(mb_win_threads_release_pin(&copy)==MB_WIN_THREADS_BAD_STATE);
 CHECK(mb_win_threads_enter(&domain,&rc)==MB_WIN_THREADS_OK);
 uint32_t *last=mb_win_threads_last_error(&domain);int32_t *crt=mb_win_threads_errno(&domain);
 CHECK(last&&*last==0&&crt&&*crt==0&&(void*)last!=(void*)crt);*last=UINT32_MAX;*crt=-19;errno=ERANGE;
 mb_win_threads_lock(&domain);CHECK(mb_win_threads_last_error(&domain)==last&&mb_win_threads_errno(&domain)==crt);mb_win_threads_unlock(&domain);
 CHECK(mb_win_threads_tls_set(&domain,63,&marker)==1&&*last==UINT32_MAX);CHECK(mb_win_threads_tls_get(&domain,63)==&marker&&*last==0);
 CHECK(mb_win_threads_tls_free(&domain,63)==0&&*last==87);CHECK(mb_win_threads_tls_get(&domain,64)==NULL&&*last==87);
 CHECK(mb_win_threads_tls_set(&domain,MB_WIN_TLS_CAPACITY,&marker)==0&&*last==87);CHECK(mb_win_threads_tls_free(&domain,UINT32_MAX)==0&&*last==87);
 *last=456;uint32_t index=mb_win_threads_tls_alloc(&domain);CHECK(index==0&&*last==456);CHECK(mb_win_threads_tls_get(&domain,index)==NULL&&*last==0);
 CHECK(mb_win_threads_tls_set(&domain,index,&marker)==1);CHECK(*crt==-19&&errno==ERANGE);
 gate g={PTHREAD_MUTEX_INITIALIZER,PTHREAD_COND_INITIALIZER,0,0,0};worker a={&domain,&g,index,1,1,0,11},b={&domain,&g,index,2,0,0,22};pthread_t ta,tb;
 CHECK(pthread_create(&ta,NULL,worker_main,&a)==0&&pthread_create(&tb,NULL,worker_main,&b)==0);
 CHECK(pthread_mutex_lock(&g.mutex)==0);while(g.ready!=2)CHECK(pthread_cond_wait(&g.cond,&g.mutex)==0);
 CHECK(a.error_address!=b.error_address&&a.error_address!=(uintptr_t)last&&b.error_address!=(uintptr_t)last);
 CHECK(mb_win_threads_tls_get(&domain,index)==&marker);g.phase=1;CHECK(pthread_cond_broadcast(&g.cond)==0);
 while(g.checked!=2)CHECK(pthread_cond_wait(&g.cond,&g.mutex)==0);
 *last=321;CHECK(mb_win_threads_tls_free(&domain,index)==1&&*last==321&&marker==123&&a.marker==11&&b.marker==22);
 CHECK(mb_win_threads_tls_alloc(&domain)==index&&*last==321);CHECK(mb_win_threads_tls_get(&domain,index)==NULL&&*last==0);
 g.phase=2;CHECK(pthread_cond_broadcast(&g.cond)==0);CHECK(pthread_mutex_unlock(&g.mutex)==0);
 CHECK(pthread_join(ta,NULL)==0&&pthread_join(tb,NULL)==0);
 /* First worker unlinked explicitly, second by native thread-exit destructor. */
 CHECK(mb_win_threads_tls_free(&domain,index)==1);uint32_t indices[MB_WIN_TLS_CAPACITY];
 for(uint32_t i=0;i<MB_WIN_TLS_CAPACITY;i++){*last=55;indices[i]=mb_win_threads_tls_alloc(&domain);CHECK(indices[i]==i&&*last==55);CHECK(mb_win_threads_tls_get(&domain,i)==NULL&&*last==0);}
 CHECK(mb_win_threads_tls_alloc(&domain)==MB_WIN_TLS_OUT_OF_INDEXES&&*last==8&&*crt==-19);
 *last=812;CHECK(mb_win_threads_tls_set(&domain,1087,&marker)==1&&*last==812);CHECK(mb_win_threads_tls_get(&domain,1087)==&marker&&*last==0);
 CHECK(mb_win_threads_tls_free(&domain,1087)==1);CHECK(mb_win_threads_tls_get(&domain,1087)==NULL&&*last==87);
 CHECK(mb_win_threads_tls_alloc(&domain)==1087);CHECK(mb_win_threads_tls_get(&domain,1087)==NULL);
 for(uint32_t i=1;i<MB_WIN_TLS_CAPACITY;i++)CHECK(mb_win_threads_tls_free(&domain,indices[i])==1);
 CHECK(mb_win_threads_release_pin(&pin)==MB_WIN_THREADS_OK);CHECK(mb_win_threads_leave(&domain,&rc)==MB_WIN_THREADS_OK);
 CHECK(mb_win_threads_close(&domain,&rc)==MB_WIN_THREADS_BUSY); /* allocated slot alone */
 CHECK(mb_win_threads_enter(&domain,&rc)==MB_WIN_THREADS_OK&&*mb_win_threads_last_error(&domain)==0);CHECK(mb_win_threads_tls_get(&domain,0)==NULL);
 CHECK(mb_win_threads_tls_free(&domain,0)==1);CHECK(mb_win_threads_leave(&domain,&rc)==MB_WIN_THREADS_OK);
 CHECK(mb_win_threads_close(&domain,&rc)==MB_WIN_THREADS_OK&&domain.state==NULL);
 CHECK(pthread_cond_destroy(&g.cond)==0&&pthread_mutex_destroy(&g.mutex)==0);
 puts("3 actual threads: dynamic TLS isolation/reuse, 1088-slot exhaustion, last-error/errno separation, pin and registration lifetime passed");return 0;
}
