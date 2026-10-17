/* Native owner semantics. The typed Windows ABI and actual DLL execution are
 * tested by the separate bridge fixture. Backend pointers deliberately lack
 * alignment and have boundary canaries. */
#include "../mb_win_heap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#c); exit(1); } } while (0)
typedef struct allocation { unsigned char *raw; size_t size; } allocation;
typedef struct backend {
 allocation blocks[32]; size_t calls, frees, live; int fail;
 int32_t errors[2]; unsigned thread;
} backend;
static backend memory;
static mb_win_heap heap;
static void *allocate(void *context,size_t size){
 backend *b=context;CHECK(b==&memory);++b->calls;if(b->fail)return NULL;
 CHECK(size&&size<=4096);unsigned char *raw=malloc(size+2);CHECK(raw);memset(raw,0xa5,size+2);raw[0]=0xbd;raw[size+1]=0xee;
 for(unsigned i=0;i<32;i++)if(!b->blocks[i].raw){b->blocks[i]=(allocation){raw,size};++b->live;return raw+1;}
 CHECK(0);return NULL;
}
static void release(void *context,void *pointer){
 backend *b=context;CHECK(b==&memory);
 for(unsigned i=0;i<32;i++)if(b->blocks[i].raw&&b->blocks[i].raw+1==pointer){allocation a=b->blocks[i];CHECK(a.raw[0]==0xbd&&a.raw[a.size+1]==0xee);memset(a.raw,0xdd,a.size+2);free(a.raw);b->blocks[i]=(allocation){0};++b->frees;--b->live;return;}
 CHECK(0);
}
static int32_t *errno_slot(void *context){backend *b=context;CHECK(b==&memory&&b->thread<2);return &b->errors[b->thread];}
static mb_win_heap_ops ops={&memory,allocate,release,errno_slot};
static void reset(void){CHECK(!memory.live);memset(&memory,0,sizeof(memory));memset(&heap,0,sizeof(heap));memory.errors[0]=71;memory.errors[1]=93;CHECK(mb_win_heap_open(&heap,&ops)==MB_WIN_HEAP_OK);}
static void finish(void){CHECK(!heap.live_count&&!heap.live_bytes&&!memory.live);CHECK(mb_win_heap_close(&heap)==MB_WIN_HEAP_OK&&!heap.active);}
static void ownership(void){
 reset();CHECK(mb_win_heap_open(&heap,&ops)==MB_WIN_HEAP_BAD_STATE);mb_win_heap_pin a={0},b={0};
 CHECK(mb_win_heap_acquire(&heap,&a)==MB_WIN_HEAP_OK&&heap.pin_count==1&&a.owner==&heap);
 CHECK(mb_win_heap_acquire(&heap,&a)==MB_WIN_HEAP_BAD_STATE);CHECK(mb_win_heap_acquire(&heap,&b)==MB_WIN_HEAP_OK&&heap.pin_count==2);
 mb_win_heap_pin copied=a;CHECK(mb_win_heap_release(&copied)==MB_WIN_HEAP_BAD_STATE&&heap.pin_count==2);
 CHECK(mb_win_heap_close(&heap)==MB_WIN_HEAP_BUSY&&heap.active);
 CHECK(mb_win_heap_release(&a)==MB_WIN_HEAP_OK&&!a.owner&&!a.next&&heap.pin_count==1);
 CHECK(mb_win_heap_release(&b)==MB_WIN_HEAP_OK&&heap.pin_count==0);finish();
 CHECK(mb_win_heap_close(&heap)==MB_WIN_HEAP_OK);CHECK(mb_win_heap_errno(&heap)==NULL);
 CHECK(mb_win_heap_acquire(&heap,&a)==MB_WIN_HEAP_BAD_STATE);
 mb_win_heap_ops missing=ops;missing.errno_slot=NULL;CHECK(mb_win_heap_open(&heap,&missing)==MB_WIN_HEAP_INVALID_ARGUMENT);
}
static void aligned_allocations(void){
 reset();const size_t sizes[]={0,1,7,16,19,1024};void *blocks[6];size_t total=0;
 for(unsigned i=0;i<6;i++){blocks[i]=mb_win_heap_malloc(&heap,sizes[i]);CHECK(blocks[i]&&((uintptr_t)blocks[i]%16)==0);if(sizes[i])memset(blocks[i],0x40+i,sizes[i]);total+=sizes[i];CHECK(heap.live_count==i+1&&heap.live_bytes==total&&memory.errors[0]==71);}
 CHECK(mb_win_heap_close(&heap)==MB_WIN_HEAP_BUSY);size_t freed=memory.frees;mb_win_heap_free(&heap,NULL);CHECK(memory.frees==freed&&memory.errors[0]==71);
 for(unsigned i=0;i<6;i++)mb_win_heap_free(&heap,blocks[i]);CHECK(memory.calls==6&&memory.frees==6);finish();
}
static void calloc_rules(void){
 reset();const size_t pairs[][2]={{7,9},{1,31},{0,SIZE_MAX},{SIZE_MAX,0},{0,0}};
 for(unsigned i=0;i<5;i++){size_t count=pairs[i][0],size=pairs[i][1],n=count*size;unsigned char *p=mb_win_heap_calloc(&heap,count,size);CHECK(p&&((uintptr_t)p%16)==0&&heap.live_count==1&&heap.live_bytes==n);for(size_t j=0;j<n;j++)CHECK(p[j]==0);CHECK(memory.errors[0]==71);mb_win_heap_free(&heap,p);}
 size_t calls=memory.calls;CHECK(mb_win_heap_calloc(&heap,SIZE_MAX,2)==NULL&&memory.calls==calls&&memory.errors[0]==MB_WIN_HEAP_ENOMEM);
 CHECK(mb_win_heap_malloc(&heap,SIZE_MAX)==NULL&&memory.calls==calls);CHECK(mb_win_heap_calloc(&heap,1,SIZE_MAX)==NULL&&memory.calls==calls);finish();
}
static void resize_rules(void){
 reset();unsigned char *p=mb_win_heap_realloc(&heap,NULL,0);CHECK(p&&heap.live_count==1&&heap.live_bytes==0);mb_win_heap_free(&heap,p);
 p=mb_win_heap_realloc(&heap,NULL,17);CHECK(p);for(unsigned i=0;i<17;i++)p[i]=(unsigned char)(i+3);
 unsigned char *grown=mb_win_heap_realloc(&heap,p,73);CHECK(grown&&heap.live_count==1&&heap.live_bytes==73);for(unsigned i=0;i<17;i++)CHECK(grown[i]==i+3);memset(grown+17,0x6d,56);
 unsigned char *shrunk=mb_win_heap_realloc(&heap,grown,9);CHECK(shrunk&&heap.live_count==1&&heap.live_bytes==9);for(unsigned i=0;i<9;i++)CHECK(shrunk[i]==i+3);
 CHECK(mb_win_heap_realloc(&heap,shrunk,0)==NULL&&heap.live_count==0&&heap.live_bytes==0&&memory.errors[0]==71);finish();
}
static void failure_preserves_allocation(void){
 reset();unsigned char *p=mb_win_heap_malloc(&heap,41);CHECK(p);for(unsigned i=0;i<41;i++)p[i]=(unsigned char)(255-i);
 memory.fail=1;CHECK(mb_win_heap_realloc(&heap,p,95)==NULL&&heap.live_count==1&&heap.live_bytes==41&&memory.live==1&&memory.frees==0&&memory.errors[0]==MB_WIN_HEAP_ENOMEM);
 for(unsigned i=0;i<41;i++)CHECK(p[i]==255-i);
 size_t calls=memory.calls;CHECK(mb_win_heap_realloc(&heap,p,SIZE_MAX)==NULL&&memory.calls==calls&&heap.live_count==1&&heap.live_bytes==41);for(unsigned i=0;i<41;i++)CHECK(p[i]==255-i);
 CHECK(mb_win_heap_malloc(&heap,0)==NULL&&memory.errors[0]==MB_WIN_HEAP_ENOMEM);CHECK(mb_win_heap_calloc(&heap,3,8)==NULL&&memory.errors[0]==MB_WIN_HEAP_ENOMEM);
 memory.fail=0;mb_win_heap_free(&heap,p);CHECK(memory.errors[0]==MB_WIN_HEAP_ENOMEM);finish();
}
static void errno_identity(void){
 reset();CHECK(mb_win_heap_errno(&heap)==&memory.errors[0]);*mb_win_heap_errno(&heap)=54;memory.thread=1;CHECK(mb_win_heap_errno(&heap)==&memory.errors[1]&&*mb_win_heap_errno(&heap)==93);
 memory.fail=1;CHECK(mb_win_heap_malloc(&heap,8)==NULL&&memory.errors[1]==12&&memory.errors[0]==54);memory.thread=0;CHECK(*mb_win_heap_errno(&heap)==54);memory.fail=0;
 void *p=mb_win_heap_calloc(&heap,3,8);CHECK(p&&memory.errors[0]==54&&memory.errors[1]==12);mb_win_heap_free(&heap,p);CHECK(memory.errors[0]==54);finish();
}
int main(void){ownership();aligned_allocations();calloc_rules();resize_rules();failure_preserves_allocation();errno_identity();puts("6 heap-owner groups passed: pinned lifetime, alignment/accounting, calloc/overflow, realloc semantics, failure preservation, errno identity");return 0;}
