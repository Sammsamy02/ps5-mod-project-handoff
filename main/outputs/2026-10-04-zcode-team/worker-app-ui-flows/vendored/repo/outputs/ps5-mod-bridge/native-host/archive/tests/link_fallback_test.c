#define _DARWIN_C_SOURCE 1
#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "../intake.h"
#include "../private/posix_compat.h"
#include <mbedtls/sha256.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef MB_ARCHIVE_POSIX_TEST_BACKEND
#error "host-only return/carry injection required"
#endif
static int checks,failures,mode,links,final_creates;
static uint8_t temporary_hash[32];static size_t temporary_size;static uint64_t temporary_inode;
static mb_archive_row_v1 rows[8],replay_rows[8];static uint32_t sorted[8],replay_sorted[8],selected[2];
static char names[1024],replay_names[1024];static uint8_t chunk[65536],replay_chunk[65536];
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);++failures;}}while(0)
static void snapshot(int directory,const char *name) {
 uint8_t bytes[4096];struct stat st;ssize_t n;int fd=openat(directory,name,O_RDONLY|O_NOFOLLOW);
 if(fd<0||fstat(fd,&st))exit(3);n=read(fd,bytes,sizeof(bytes));if(n<0||n!=(ssize_t)st.st_size||close(fd))exit(3);
 temporary_size=(size_t)n;temporary_inode=(uint64_t)st.st_ino;
 if(mbedtls_sha256(bytes,temporary_size,temporary_hash,0))exit(3);
}
struct mb_archive_posix_raw_result mb_archive_posix_test_call(enum mb_archive_posix_call call,
 long a,long b,long c,long d,long e) {
 long rc;int error;const char *name=(const char *)(intptr_t)b;(void)e;
 switch(call) {
 case MB_ARCHIVE_POSIX_OPENAT:
  if(!strcmp(name,"manifest-v1.bin")&&((int)c&O_CREAT)){++final_creates;CHECK(((int)c&(O_CREAT|O_EXCL|O_NOFOLLOW))==(O_CREAT|O_EXCL|O_NOFOLLOW));}
  rc=openat((int)a,name,(int)c,(mode_t)d);break;
 case MB_ARCHIVE_POSIX_FSTATAT:rc=fstatat((int)a,name,(struct stat *)(intptr_t)c,(int)d);break;
 case MB_ARCHIVE_POSIX_MKDIRAT:rc=mkdirat((int)a,name,(mode_t)c);break;
 case MB_ARCHIVE_POSIX_UNLINKAT:rc=unlinkat((int)a,name,(int)c);break;
 case MB_ARCHIVE_POSIX_LINKAT:
  ++links;snapshot((int)a,name);
  if(mode!=1){int fd=openat((int)c,(const char *)(intptr_t)d,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
   if(fd<0||write(fd,"foreign",7)!=7||close(fd))exit(3);}
  return (struct mb_archive_posix_raw_result){mode==2?EEXIST:EOPNOTSUPP,1};
 default:exit(3);
 }
 error=errno;return (struct mb_archive_posix_raw_result){rc<0?error:rc,(unsigned char)(rc<0)};
}
static int unhex(const char *text,uint8_t out[32]) {
 unsigned i,x;for(i=0;i<32;i++){if(sscanf(text+2*i,"%2x",&x)!=1)return 0;out[i]=(uint8_t)x;}return 1;
}
int main(int argc,char **argv) {
 mb_archive_source_v1 source={0};mb_archive_limits_v1 limits;mb_archive_budget_v1 budget;
 mb_archive_buffers_v1 primary={rows,8,names,sizeof(names),sorted,8,chunk,sizeof(chunk)};
 mb_archive_buffers_v1 replay={replay_rows,8,replay_names,sizeof(replay_names),replay_sorted,8,replay_chunk,sizeof(replay_chunk)};
 char path[8192],hex[65],case_name[32],text[8];FILE *identity;int parent,qfd,fd,status;struct stat st;uint8_t hash[32],bytes[4096];ssize_t got;
 if(argc!=3)return 3;
 snprintf(path,sizeof(path),"%s/source.zip.identity",argv[1]);identity=fopen(path,"r");
 if(!identity||fscanf(identity,"%64s %"SCNu64,hex,&source.size)!=2||!unhex(hex,source.sha256))return 3;fclose(identity);
 snprintf(path,sizeof(path),"%s/source.zip",argv[1]);source.fd=open(path,O_RDONLY|O_NOFOLLOW);if(source.fd<0)return 3;
 source.struct_size=sizeof(source);source.abi_version=1;memset(source.target_token,0x51,32);
 parent=open(argv[2],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(parent<0)return 3;
 for(mode=1;mode<=3;mode++) {
  mb_archive_inventory_v1 inventory={0};mb_archive_materialized_v1 out={0};mb_archive_materialize_request_v1 q={0};mb_archive_owned_file_v1 journal[5]={0};
  links=final_creates=0;memset(temporary_hash,0,32);temporary_size=0;temporary_inode=0;
  snprintf(case_name,sizeof(case_name),"case-%d",mode);if(mkdirat(parent,case_name,0700))return 3;
  qfd=openat(parent,case_name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(qfd<0||fstat(qfd,&st))return 3;
  mb_archive_limits_default_v1(&limits);limits.entries=8;limits.name_bytes=sizeof(names);
  mb_archive_budget_default_v1(&budget);budget.io_limit=128u*1024u;budget.operation_limit=512;
  CHECK(mb_archive_scan_v1(&source,&limits,&budget,&primary,&inventory)==0);CHECK(inventory.row_count==2);
  selected[0]=0;selected[1]=1;q.source=&source;q.inventory=&inventory;q.rows=rows;q.row_capacity=8;
  q.names=names;q.name_capacity=sizeof(names);q.selected=selected;q.selected_count=2;
  memcpy(q.target_token,source.target_token,32);q.quarantine_fd=qfd;q.quarantine_device=(uint64_t)st.st_dev;q.quarantine_inode=(uint64_t)st.st_ino;
  q.journal=journal;q.journal_capacity=5;q.replay=replay;
  status=mb_archive_materialize_v1(&q,&limits,&budget,&out);
  printf("case=%d expected=%d actual=%d final_state=%u cleanup=%u\n",mode,mode==1?0:MB_ARCHIVE_IO_ERROR,status,journal[4].state,out.cleanup_complete);
  CHECK(links==1);CHECK(budget.io_used<=budget.io_limit);CHECK(budget.operations_used<=budget.operation_limit);
  if(mode==1) {
   CHECK(status==0);CHECK(out.complete==1);CHECK(out.selected_count==2);CHECK(out.installation_supported==0);CHECK(final_creates==1);
   if(status==0) {
    CHECK(journal[4].state==MB_ARCHIVE_CREATED);CHECK(journal[3].state==MB_ARCHIVE_REMOVED);
    CHECK(journal[4].inode!=temporary_inode);CHECK(journal[4].expected_size==temporary_size);
    CHECK(journal[4].written_bytes==temporary_size);CHECK(memcmp(journal[4].sha256,temporary_hash,32)==0);
    fd=openat(qfd,"manifest-v1.bin",O_RDONLY|O_NOFOLLOW);CHECK(fd>=0);
    if(fd>=0){CHECK(fstat(fd,&st)==0);CHECK((uint64_t)st.st_ino==journal[4].inode);
     got=read(fd,bytes,sizeof(bytes));CHECK(got==(ssize_t)temporary_size);
     if(got>=0){CHECK(mbedtls_sha256(bytes,(size_t)got,hash,0)==0);CHECK(memcmp(hash,temporary_hash,32)==0);}close(fd);}
    CHECK(fstatat(qfd,"manifest-v1.part",&st,AT_SYMLINK_NOFOLLOW)==-1&&errno==ENOENT);
   }
  } else {
   CHECK(status==MB_ARCHIVE_IO_ERROR);CHECK(out.complete==0);CHECK(out.cleanup_complete==1);CHECK(out.selected_count==0&&out.decoded_bytes==0);
   CHECK(journal[4].state==MB_ARCHIVE_PLANNED);CHECK(final_creates==(mode==3?1:0));
   fd=openat(qfd,"manifest-v1.bin",O_RDONLY|O_NOFOLLOW);CHECK(fd>=0);
   if(fd>=0){memset(text,0,sizeof(text));CHECK(read(fd,text,7)==7);CHECK(strcmp(text,"foreign")==0);close(fd);}
   CHECK(fstatat(qfd,"intent-v1.bin",&st,AT_SYMLINK_NOFOLLOW)==-1&&errno==ENOENT);
   CHECK(fstatat(qfd,"manifest-v1.part",&st,AT_SYMLINK_NOFOLLOW)==-1&&errno==ENOENT);
   CHECK(fstatat(qfd,"member-00000000.bin",&st,AT_SYMLINK_NOFOLLOW)==-1&&errno==ENOENT);
   CHECK(fstatat(qfd,"member-00000001.bin",&st,AT_SYMLINK_NOFOLLOW)==-1&&errno==ENOENT);
  }
  close(qfd);
 }
 close(parent);close(source.fd);printf("link-fallback checks=%d failures=%d cases=3\n",checks,failures);return failures?1:0;
}
