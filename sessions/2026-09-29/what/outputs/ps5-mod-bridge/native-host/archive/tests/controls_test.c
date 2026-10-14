#define _DARWIN_C_SOURCE 1
#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "../intake.h"
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int failures,checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);++failures;}}while(0)
typedef struct env {
 mb_archive_source_v1 s;mb_archive_limits_v1 l;mb_archive_budget_v1 b;
 mb_archive_buffers_v1 v,replay;mb_archive_inventory_v1 inv;
 mb_archive_materialize_request_v1 q;mb_archive_materialized_v1 out;
 mb_archive_owned_file_v1 journal[131];uint32_t selected[128];int qfd;
 const char *base;char fixture[8192];
} env;
static int unhex(const char *s,uint8_t out[32]){unsigned i,x;if(strlen(s)!=64)return 0;for(i=0;i<32;i++){if(sscanf(s+2*i,"%2x",&x)!=1)return 0;out[i]=(uint8_t)x;}return 1;}
static mb_archive_buffers_v1 buffers(void){mb_archive_buffers_v1 b={0};b.row_capacity=128;b.name_capacity=65536;b.sort_capacity=128;b.chunk_capacity=MB_ARCHIVE_CHUNK_BYTES;
 b.rows=calloc(128,sizeof(*b.rows));b.names=calloc(1,65536);b.sort_indices=calloc(128,sizeof(*b.sort_indices));b.chunk=calloc(1,MB_ARCHIVE_CHUNK_BYTES);
 if(!b.rows||!b.names||!b.sort_indices||!b.chunk)exit(3);return b;}
static void release(mb_archive_buffers_v1 *b){free(b->rows);free(b->names);free(b->sort_indices);free(b->chunk);}
static int zero(const void *p,size_t n){const unsigned char *s=p;size_t i;for(i=0;i<n;i++)if(s[i])return 0;return 1;}
static void start(env *e,const char *base,const char *name,int parent,unsigned id){char path[8192],hex[65],qname[32];FILE *f;struct stat st;
 memset(e,0,sizeof(*e));e->base=base;e->v=buffers();e->replay=buffers();e->s.struct_size=sizeof(e->s);e->s.abi_version=1;
 snprintf(e->fixture,sizeof(e->fixture),"%s/%s",base,name);snprintf(path,sizeof(path),"%s/%s.identity",base,name);
 f=fopen(path,"r");if(!f||fscanf(f,"%64s %"SCNu64,hex,&e->s.size)!=2||!unhex(hex,e->s.sha256))exit(3);fclose(f);
 e->s.fd=open(e->fixture,O_RDONLY|O_CLOEXEC);if(e->s.fd<0)exit(3);memset(e->s.target_token,0x51,32);
 mb_archive_limits_default_v1(&e->l);e->l.entries=128;e->l.name_bytes=65536;mb_archive_budget_default_v1(&e->b);
 snprintf(qname,sizeof(qname),"case-%03u",id);if(mkdirat(parent,qname,0700))exit(3);
 e->qfd=openat(parent,qname,O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(e->qfd<0||fstat(e->qfd,&st))exit(3);
 e->q.source=&e->s;e->q.inventory=&e->inv;e->q.rows=e->v.rows;e->q.row_capacity=e->v.row_capacity;
 e->q.names=e->v.names;e->q.name_capacity=e->v.name_capacity;e->q.selected=e->selected;e->q.selected_count=1;
 memcpy(e->q.target_token,e->s.target_token,32);e->q.quarantine_fd=e->qfd;e->q.quarantine_device=(uint64_t)st.st_dev;e->q.quarantine_inode=(uint64_t)st.st_ino;
 e->q.journal=e->journal;e->q.journal_capacity=131;e->q.replay=e->replay;
}
static void stop(env *e){close(e->s.fd);close(e->qfd);release(&e->v);release(&e->replay);}
static int scan(env *e){return mb_archive_scan_v1(&e->s,&e->l,&e->b,&e->v,&e->inv);}
static int materialize(env *e){return mb_archive_materialize_v1(&e->q,&e->l,&e->b,&e->out);}
static unsigned files(int qfd){DIR *d;struct dirent *p;unsigned n=0;int fd=openat(qfd,".",O_RDONLY|O_DIRECTORY);if(fd<0)return 999;d=fdopendir(fd);if(!d){close(fd);return 999;}
 while((p=readdir(d)))if(strcmp(p->d_name,".")&&strcmp(p->d_name,".."))++n;closedir(d);return n;}
static void failed_scan(env *e,int expected){int n=scan(e);CHECK(n==expected);CHECK(e->inv.complete==0);CHECK(e->inv.row_count==0);CHECK(e->inv.name_bytes==0);
 CHECK(zero(e->v.rows,e->v.row_capacity*sizeof(*e->v.rows)));CHECK(zero(e->v.names,e->v.name_capacity));CHECK(e->b.io_used<=e->b.io_limit);CHECK(files(e->qfd)==0);}
static void failed_materialize(env *e,int expected,unsigned remaining){int n=materialize(e);CHECK(n==expected);CHECK(e->out.complete==0);CHECK(e->out.decoded_bytes==0);CHECK(e->out.selected_count==0);
 CHECK(zero(e->out.inventory_sha256,32));CHECK(!e->out.manifest_name[0]);CHECK(zero(e->replay.rows,e->replay.row_capacity*sizeof(*e->replay.rows)));
 CHECK(e->b.io_used<=e->b.io_limit);CHECK(e->b.operations_used<=e->b.operation_limit);CHECK(files(e->qfd)==remaining);}
typedef struct observe{env *e;int mode,done;}observe;
static int late(void *context){observe *o=context;env *e=o->e;mb_archive_owned_file_v1 *p=e->journal+1;int fd;char c='X';
 if(o->done)return o->mode==1||o->mode==2||o->mode==6;
 if(p->written_bytes<65536)return 0;
 if(o->mode==1){o->done=1;return 1;}
 if(o->mode==2){if(fchmod(e->qfd,0500))exit(3);o->done=1;return 1;}
 if(o->mode==3){fd=openat(e->qfd,"manifest-v1.bin",O_CREAT|O_EXCL|O_WRONLY,0600);if(fd<0||write(fd,"foreign",7)!=7||close(fd))exit(3);o->done=1;return 0;}
 if(o->mode==4){fd=open(e->fixture,O_WRONLY);if(fd<0||pwrite(fd,&c,1,0)!=1||close(fd))exit(3);o->done=1;return 0;}
 if(o->mode==5&&p->written_bytes==p->expected_size){fd=openat(e->qfd,p->name,O_WRONLY|O_NOFOLLOW);if(fd<0||pwrite(fd,&c,1,0)!=1||close(fd))exit(3);o->done=1;return 0;}
 if(o->mode==6){if(unlinkat(e->qfd,p->name,0))exit(3);fd=openat(e->qfd,p->name,O_CREAT|O_EXCL|O_WRONLY,0600);
  if(fd<0||write(fd,"foreign",7)!=7||close(fd))exit(3);o->done=1;return 1;}
 return 0;
}
int main(int argc,char **argv){env e;unsigned id=0,i;int parent,fd;uint64_t baseline,start_io,owned_inode;observe o;uint8_t old;
 const char *bad[]={"traversal.zip","alias.zip","ancestor.zip","crc.zip","truncated.zip","link.tar"};
 const int reasons[]={MB_ARCHIVE_UNSAFE_PATH,MB_ARCHIVE_UNSAFE_PATH,MB_ARCHIVE_UNSAFE_PATH,MB_ARCHIVE_READER_REJECTED,MB_ARCHIVE_READER_REJECTED,MB_ARCHIVE_UNSUPPORTED_ENTRY};
 if(argc!=3)return 3;parent=open(argv[2],O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(parent<0)return 3;
 for(i=0;i<sizeof(bad)/sizeof(*bad);i++){start(&e,argv[1],bad[i],parent,id++);failed_scan(&e,reasons[i]);stop(&e);}
 start(&e,argv[1],"good.zip",parent,id++);e.s.sha256[0]^=1;failed_scan(&e,MB_ARCHIVE_IDENTITY_MISMATCH);stop(&e);
 start(&e,argv[1],"good.zip",parent,id++);--e.s.size;failed_scan(&e,MB_ARCHIVE_SOURCE_CHANGED);stop(&e);
 for(i=0;i<6;i++){start(&e,argv[1],"good.zip",parent,id++);
  if(i==0)e.l.raw_bytes=e.s.size-1;if(i==1)e.l.member_bytes=200002;if(i==2)e.l.expanded_bytes=200003;
  if(i==3)e.l.entries=1;if(i==4)e.l.name_bytes=50;if(i==5)e.b.io_limit=100;
  failed_scan(&e,MB_ARCHIVE_LIMIT);stop(&e);
 }
 start(&e,argv[1],"good.zip",parent,id++);CHECK(scan(&e)==0);start_io=e.b.io_used;CHECK(materialize(&e)==0);CHECK(e.out.complete==1);CHECK(e.out.installation_supported==0);
 CHECK(files(e.qfd)==3);baseline=e.b.io_used-start_io;CHECK(e.journal[1].written_bytes==200003);stop(&e);
 for(i=0;i<8;i++){start(&e,argv[1],"good.zip",parent,id++);CHECK(scan(&e)==0);
  if(i==0){e.q.selected_count=2;e.selected[1]=e.selected[0];}
  if(i==1)e.selected[0]=128;
  if(i==2)e.q.target_token[0]^=1;
  if(i==3)e.v.names[e.v.rows[0].original_offset]^=1;
  if(i==4)e.inv.inventory_sha256[0]^=1;
  if(i==5||i==7){fd=openat(e.qfd,"foreign",O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0||close(fd))return 3;}
  if(i==6)e.b.io_limit=e.b.io_used+baseline-1;
  if(i==7){DIR *d=fdopendir(dup(e.qfd));if(!d)return 3;while(readdir(d)){}if(closedir(d))return 3;}
  failed_materialize(&e,(i<=1||i==5||i==7)?MB_ARCHIVE_BAD_ARGUMENT:i==6?MB_ARCHIVE_LIMIT:MB_ARCHIVE_IDENTITY_MISMATCH,(i==5||i==7)?1:0);
  if(i==6){CHECK(e.journal[1].written_bytes==200003);CHECK(e.journal[1].state==MB_ARCHIVE_REMOVED);CHECK(e.out.cleanup_complete==1);}
  stop(&e);
 }
 for(i=1;i<=6;i++){start(&e,argv[1],i==4?"mutable.zip":"good.zip",parent,id++);CHECK(scan(&e)==0);o=(observe){&e,(int)i,0};e.b.cancelled=late;e.b.context=&o;
  failed_materialize(&e,i==1?MB_ARCHIVE_CANCELLED:(i==2||i==6)?MB_ARCHIVE_CLEANUP_INCOMPLETE:i==3?MB_ARCHIVE_IO_ERROR:MB_ARCHIVE_IDENTITY_MISMATCH,(i==2||i==6)?2:i==3?1:0);
  CHECK(o.done==1);CHECK(e.journal[1].written_bytes>=65536);CHECK(e.out.cleanup_complete==(i!=2&&i!=6));
  if(i==2){CHECK(e.journal[1].state==MB_ARCHIVE_RETAINED);CHECK(e.journal[1].inode!=0);CHECK(e.journal[0].state==MB_ARCHIVE_RETAINED);CHECK(fchmod(e.qfd,0700)==0);}
  if(i==6){char text[8]={0};CHECK(e.journal[0].state==MB_ARCHIVE_RETAINED);fd=openat(e.qfd,e.journal[1].name,O_RDONLY|O_NOFOLLOW);CHECK(fd>=0);
   if(fd>=0){CHECK(read(fd,text,7)==7);CHECK(strcmp(text,"foreign")==0);close(fd);}}
  stop(&e);
 }
 /* Preflight alias refuses without changing an existing caller result. */
 start(&e,argv[1],"good.zip",parent,id++);CHECK(scan(&e)==0);memset(&e.out,0x5a,sizeof(e.out));old=((uint8_t *)&e.out)[0];
 e.q.replay.rows=e.v.rows;CHECK(materialize(&e)==MB_ARCHIVE_BAD_ARGUMENT);CHECK(((uint8_t *)&e.out)[0]==old);CHECK(files(e.qfd)==0);stop(&e);
 /* A caller must not lose previously returned ownership by reusing its
  * nonempty journal, even if the next request cannot be admitted. */
 start(&e,argv[1],"good.zip",parent,id++);CHECK(scan(&e)==0);CHECK(materialize(&e)==0);
 owned_inode=e.journal[1].inode;CHECK(owned_inode!=0);CHECK(materialize(&e)==MB_ARCHIVE_BAD_ARGUMENT);
 CHECK(e.journal[1].inode==owned_inode);CHECK(e.journal[1].state==MB_ARCHIVE_CREATED);CHECK(e.out.complete==0);stop(&e);
 close(parent);printf("checks=%d failures=%d cases=%u\n",checks,failures,id);return failures?1:0;
}
