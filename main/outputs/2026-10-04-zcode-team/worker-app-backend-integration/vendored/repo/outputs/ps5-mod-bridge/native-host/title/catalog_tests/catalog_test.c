#if defined(__APPLE__)
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L
#include "catalog_test.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures, cases;
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);++failures;} } while(0)
#define CASE(name) do { ++cases; if(!(name)) { fprintf(stderr,"FAIL case %s\n",#name); ++failures; } } while(0)

typedef struct test_faults {
    char root[1024];
    char backup[1024];
    int readdir_error_after;
    int readdir_calls;
    int replace_on_eof;
    int duplicate_after_first;
    int duplicate_returned;
    int saved_entry_available;
    struct dirent saved_entry;
} test_faults;

static int t_lstat(void *v,const char *p,struct stat *s){(void)v;return lstat(p,s);}
static int t_open(void *v,const char *p,int f){(void)v;return open(p,f);}
static int t_atstat(void *v,int d,const char *p,struct stat *s,int f){(void)v;return fstatat(d,p,s,f);}
static int t_atopen(void *v,int d,const char *p,int f){(void)v;return openat(d,p,f);}
static int t_fstat(void *v,int d,struct stat *s){(void)v;return fstat(d,s);}
static int t_close(void *v,int d){(void)v;return close(d);}
static DIR *t_fdopendir(void *v,int d){(void)v;return fdopendir(d);}
static struct dirent *t_readdir(void *v,DIR *d){
    test_faults *f=(test_faults *)v;
    ++f->readdir_calls;
    if(f->readdir_error_after>0 && f->readdir_calls>=f->readdir_error_after){errno=EIO;return NULL;}
    if(f->duplicate_after_first&&f->saved_entry_available&&!f->duplicate_returned){
        f->duplicate_returned=1;return &f->saved_entry;
    }
    {
        struct dirent *entry=readdir(d);
        if(entry&&strcmp(entry->d_name,".")&&strcmp(entry->d_name,"..")&&!f->saved_entry_available){
            f->saved_entry= *entry;f->saved_entry_available=1;
        }
        if(!entry && f->replace_on_eof){
            f->replace_on_eof=0;
            if(rename(f->root,f->backup)==0 && mkdir(f->root,0700)==0) return NULL;
            errno=EIO;
        }
        return entry;
    }
}
static int t_closedir(void *v,DIR *d){(void)v;return closedir(d);}

static void fixture_ops(test_faults *faults,mb_title_catalog_test_ops_v1 *out){
    memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->abi_version=1;out->context=faults;
    out->lstat_path=t_lstat;out->open_path=t_open;out->stat_relative=t_atstat;out->open_relative=t_atopen;
    out->stat_fd=t_fstat;out->close_fd=t_close;out->fdopendir_stream=t_fdopendir;
    out->readdir_stream=t_readdir;out->closedir_stream=t_closedir;
}
static int root_new(char out[1024]){
    strcpy(out,"/TMP/mb-title-catalog-XXXXXX");
    return mkdtemp(out)!=NULL;
}
static int path_join(char out[2048],const char *root,const char *name){
    const int n=snprintf(out,2048,"%s/%s",root,name);return n>0 && n<2048;
}
static int add_dir(const char *root,const char *name){
    char path[2048];return path_join(path,root,name) && mkdir(path,0700)==0;
}
static int add_json(const char *root,const char *name,const char *json,size_t bytes){
    char dir[2048],path[2048];FILE *f;
    if(!path_join(dir,root,name) || mkdir(dir,0700))return 0;
    if(!path_join(path,dir,"param.json"))return 0;
    f=fopen(path,"wb");if(!f)return 0;
    if(bytes && fwrite(json,1,bytes,f)!=bytes){fclose(f);return 0;}
    return fclose(f)==0;
}
static int add_file(const char *root,const char *name){
    char path[2048];FILE *f;
    if(!path_join(path,root,name))return 0;
    f=fopen(path,"wb");if(!f)return 0;
    return fclose(f)==0;
}
static int remove_tree(const char *path){
    struct stat st;
    if(lstat(path,&st))return errno==ENOENT;
    if(S_ISDIR(st.st_mode)){
        DIR *d=opendir(path);struct dirent *e;int ok=1;
        if(!d)return 0;
        while((e=readdir(d))!=NULL){char child[2048];
            if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
            if(!path_join(child,path,e->d_name)||!remove_tree(child))ok=0;
        }
        if(closedir(d))ok=0;
        return rmdir(path)==0 && ok;
    }
    return unlink(path)==0;
}
static void init_call(mb_title_catalog_request_v1 *request,mb_title_catalog_ledger_v1 *ledger){
    memset(request,0,sizeof(*request));request->struct_size=sizeof(*request);request->abi_version=MB_TITLE_CATALOG_ABI_VERSION;
    request->profile=MB_TITLE_CACHE_SYSTEM_PRIVATE;request->scan_assertion=MB_TITLE_CATALOG_ASSERT_IMMUTABLE_SCAN;
    memset(ledger,0,sizeof(*ledger));ledger->struct_size=sizeof(*ledger);ledger->abi_version=MB_TITLE_CATALOG_ABI_VERSION;
    ledger->source_io.struct_size=sizeof(ledger->source_io);ledger->source_io.abi_version=MB_TITLE_SOURCE_ABI_VERSION;
    ledger->source_io.remaining_operations=MB_TITLE_CATALOG_OPERATION_LIMIT;
    ledger->source_io.remaining_read_bytes=MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT;
}
static mb_title_catalog_status scan(const char *root,mb_title_catalog_request_v1 *request,
    mb_title_catalog_ledger_v1 *ledger,mb_title_metadata_result_v1 *records,
    uint32_t *issues,mb_title_catalog_result_v1 *result,const mb_title_catalog_test_ops_v1 *ops){
    char names[MB_TITLE_CATALOG_ENTRY_NAME_BYTES],candidate[MB_TITLE_CATALOG_CANDIDATE_BYTES];
    uint8_t workspace[MB_TITLE_CATALOG_SOURCE_WORKSPACE_BYTES];
    memset(records,0xA5,sizeof(*records)*MB_TITLE_CATALOG_MAX_RECORDS);
    memset(issues,0xA5,sizeof(*issues)*MB_TITLE_CATALOG_MAX_ISSUES);memset(result,0xA5,sizeof(*result));
    return mb_title_catalog_scan_fixture_v1(request,ledger,names,sizeof(names),candidate,sizeof(candidate),
        workspace,sizeof(workspace),records,MB_TITLE_CATALOG_MAX_RECORDS,issues,MB_TITLE_CATALOG_MAX_ISSUES,
        result,root,strlen(root),NULL,ops);
}
static int test_clean_and_empty(void){
    static const char a[]={"{\"titleId\":\"OWNED_00001\",\"contentVersion\":\"v1\"}"};
    static const char b[]={"{\"titleId\":\"Future.Custom-2\"}"};
    char root[1024];mb_title_catalog_request_v1 req;mb_title_catalog_ledger_v1 led;
    mb_title_metadata_result_v1 records[MB_TITLE_CATALOG_MAX_RECORDS];uint32_t issues[MB_TITLE_CATALOG_MAX_ISSUES];
    mb_title_catalog_result_v1 result;mb_title_catalog_status status;
    CHECK(root_new(root));if(!root[0])return 0;
    CHECK(add_json(root,"OWNED_00001",a,sizeof(a)-1));CHECK(add_json(root,"Future.Custom-2",b,sizeof(b)-1));
    init_call(&req,&led);status=scan(root,&req,&led,records,issues,&result,NULL);
    CHECK(status==MB_TITLE_CATALOG_STATUS_OBSERVED);CHECK(result.status==MB_TITLE_CATALOG_STATUS_OBSERVED);
    CHECK(result.complete==1 && result.enumeration_eof==1 && result.root_identity_checked==1 && result.cleanup_complete==1);
    CHECK(result.source_scope==MB_TITLE_CATALOG_SOURCE_SCOPE_SYSTEM_PRIVATE_METADATA_CACHE);
    CHECK(result.record_count==2 && result.candidates_seen==2 && result.record_count==result.candidates_seen);
    CHECK(!strcmp(records[0].title_id,"OWNED_00001")||!strcmp(records[1].title_id,"OWNED_00001"));
    CHECK(result.operations_reserved>0 && result.operations_reserved<=MB_TITLE_CATALOG_OPERATION_LIMIT);
    CHECK(result.operations_attempted<=result.operations_reserved && result.entry_name_bytes==led.entry_name_bytes);
    CHECK(!records[0].installed_observed&&!records[0].running_observed&&!records[0].build_fingerprint_available);
    CHECK(remove_tree(root));
    CHECK(root_new(root));init_call(&req,&led);status=scan(root,&req,&led,records,issues,&result,NULL);
    CHECK(status==MB_TITLE_CATALOG_STATUS_OBSERVED);CHECK(result.complete==1&&result.record_count==0&&result.candidates_seen==0);
    CHECK(remove_tree(root));return 1;
}
static int test_optional_empty(void){
    static const char json[]={"{\"titleId\":\"OPTIONAL_1\",\"contentVersion\":\"\",\"localizedParameters\":{\"defaultLanguage\":\"en\",\"en\":{\"titleName\":\"\"}}}"};
    char root[1024];mb_title_catalog_request_v1 req;mb_title_catalog_ledger_v1 led;mb_title_metadata_result_v1 records[128];
    uint32_t issues[32];mb_title_catalog_result_v1 result;mb_title_catalog_status status;
    CHECK(root_new(root));CHECK(add_json(root,"OPTIONAL_1",json,sizeof(json)-1));init_call(&req,&led);
    status=scan(root,&req,&led,records,issues,&result,NULL);CHECK(status==MB_TITLE_CATALOG_STATUS_OBSERVED);
    CHECK(result.record_count==1&&records[0].name_available&&records[0].version_available);
    CHECK(records[0].name[0]=='\0'&&records[0].version[0]=='\0');CHECK(remove_tree(root));return 1;
}
static int test_partial_uncertainty(void){
    static const char valid[]={"{\"titleId\":\"VALID_1\"}"};
    static const char wrong[]={"{\"titleId\":\"OTHER_1\"}"};
    char root[1024],path[2048];mb_title_catalog_request_v1 req;mb_title_catalog_ledger_v1 led;
    mb_title_metadata_result_v1 records[128];uint32_t issues[32];mb_title_catalog_result_v1 result;mb_title_catalog_status status;
    CHECK(root_new(root));CHECK(add_json(root,"VALID_1",valid,sizeof(valid)-1));
    CHECK(add_dir(root,"bad name"));CHECK(path_join(path,root,"linked"));CHECK(symlink("VALID_1",path)==0);
    CHECK(add_json(root,"MISMATCH_1",wrong,sizeof(wrong)-1));CHECK(add_file(root,"ordinary.txt"));
    init_call(&req,&led);status=scan(root,&req,&led,records,issues,&result,NULL);
    CHECK(status==MB_TITLE_CATALOG_STATUS_PARTIAL);CHECK(result.complete==0&&result.record_count<result.candidates_seen);
    CHECK(result.reason==MB_TITLE_CATALOG_REASON_PROVIDER_OBSERVATION_INCOMPLETE);
    CHECK(result.issue_count==1&&issues[0]==MB_TITLE_CATALOG_ISSUE_PROVIDER_OBSERVATION_INCOMPLETE);
    CHECK(remove_tree(root));return 1;
}
static int test_duplicate_fatal(void){
    static const char a[]={"{\"titleId\":\"Generic_A\"}"};
    char root[1024];test_faults fault;mb_title_catalog_test_ops_v1 ops;
    mb_title_catalog_request_v1 req;mb_title_catalog_ledger_v1 led;mb_title_metadata_result_v1 records[128];
    uint32_t issues[32];mb_title_catalog_result_v1 result;mb_title_catalog_status status;
    CHECK(root_new(root));CHECK(add_json(root,"Generic_A",a,sizeof(a)-1));memset(&fault,0,sizeof(fault));fault.duplicate_after_first=1;
    fixture_ops(&fault,&ops);init_call(&req,&led);status=scan(root,&req,&led,records,issues,&result,&ops);
    CHECK(status==MB_TITLE_CATALOG_STATUS_ERROR);CHECK(result.reason==MB_TITLE_CATALOG_REASON_INVALID_PROVIDER_CAPTURE);
    CHECK(result.complete==0&&result.record_count==0&&result.issue_count==1&&issues[0]==MB_TITLE_CATALOG_ISSUE_INVALID_PROVIDER_CAPTURE);
    CHECK(remove_tree(root));return 1;
}
static int test_readdir_error_and_root_replacement(void){
    char root[1024],backup[1024];test_faults fault;mb_title_catalog_test_ops_v1 ops;
    mb_title_catalog_request_v1 req;mb_title_catalog_ledger_v1 led;mb_title_metadata_result_v1 records[128];
    uint32_t issues[32];mb_title_catalog_result_v1 result;mb_title_catalog_status status;
    CHECK(root_new(root));memset(&fault,0,sizeof(fault));strcpy(fault.root,root);snprintf(backup,sizeof(backup),"%s-old",root);strcpy(fault.backup,backup);
    fault.readdir_error_after=1;fixture_ops(&fault,&ops);init_call(&req,&led);status=scan(root,&req,&led,records,issues,&result,&ops);
    CHECK(status==MB_TITLE_CATALOG_STATUS_ERROR&&result.reason==MB_TITLE_CATALOG_REASON_PROVIDER_ERROR);
    CHECK(result.enumeration_eof==0&&result.cleanup_complete==1);CHECK(remove_tree(root));
    CHECK(root_new(root));memset(&fault,0,sizeof(fault));strcpy(fault.root,root);snprintf(backup,sizeof(backup),"%s-old",root);strcpy(fault.backup,backup);
    fault.replace_on_eof=1;fixture_ops(&fault,&ops);init_call(&req,&led);status=scan(root,&req,&led,records,issues,&result,&ops);
    CHECK(status==MB_TITLE_CATALOG_STATUS_PARTIAL&&result.reason==MB_TITLE_CATALOG_REASON_SOURCE_CHANGED);
    CHECK(result.complete==0&&result.root_identity_checked==0);CHECK(remove_tree(root));CHECK(remove_tree(backup));return 1;
}
static int test_global_operation_cap(void){
    char root[1024];mb_title_catalog_request_v1 req;mb_title_catalog_ledger_v1 led;mb_title_metadata_result_v1 records[128];
    uint32_t issues[32];mb_title_catalog_result_v1 result;mb_title_catalog_status status;
    CHECK(root_new(root));init_call(&req,&led);led.source_io.reserved_operations=MB_TITLE_CATALOG_OPERATION_LIMIT-1;
    led.source_io.remaining_operations=1;status=scan(root,&req,&led,records,issues,&result,NULL);
    CHECK(status==MB_TITLE_CATALOG_STATUS_ERROR&&result.reason==MB_TITLE_CATALOG_REASON_INVENTORY_RESOURCE_LIMIT);
    CHECK(result.record_count==0&&result.complete==0);CHECK(remove_tree(root));return 1;
}
static int test_byte_cap_clears_rows(void){
    char root[1024],id[65],json[65537],fname[256];size_t i;mb_title_catalog_request_v1 req;mb_title_catalog_ledger_v1 led;
    mb_title_metadata_result_v1 records[128];uint32_t issues[32];mb_title_catalog_result_v1 result;mb_title_catalog_status status;
    CHECK(root_new(root));
    for(i=0;i<128;++i){id[0]='A';memset(id+1,'a',59);snprintf(id+60,5,"%04x",(unsigned)i);id[64]='\0';
        snprintf(json,sizeof(json),"{\"titleId\":\"%s\"}",id);memset(json+strlen(json),' ',65536-strlen(json));json[65536]='\0';
        CHECK(add_json(root,id,json,65536));}
    for(i=0;i<33;++i){memset(fname,'z',255);fname[0]='b';snprintf(fname+252,4,"%03x",(unsigned)i);fname[255]='\0';CHECK(add_file(root,fname));}
    init_call(&req,&led);status=scan(root,&req,&led,records,issues,&result,NULL);
    CHECK(status==MB_TITLE_CATALOG_STATUS_ERROR&&result.reason==MB_TITLE_CATALOG_REASON_INVENTORY_RESOURCE_LIMIT);
    CHECK(result.complete==0&&result.record_count==0&&result.issue_count==1);
    CHECK(remove_tree(root));return 1;
}
static int test_invalid_preflight_no_write(void){
    char root[1024],names[256],candidate[65];uint8_t workspace[65536];mb_title_catalog_request_v1 req;mb_title_catalog_ledger_v1 led;
    mb_title_metadata_result_v1 records[128];uint32_t issues[32];mb_title_catalog_result_v1 result;mb_title_catalog_status status;
    CHECK(root_new(root));init_call(&req,&led);memset(records,0x5a,sizeof(records));memset(issues,0x5a,sizeof(issues));memset(&result,0x5a,sizeof(result));
    status=mb_title_catalog_scan_fixture_v1(&req,&led,names,sizeof(names),candidate,sizeof(candidate),workspace,sizeof(workspace),
        records,127,issues,32,&result,root,strlen(root),NULL,NULL);
    CHECK(status==MB_TITLE_CATALOG_STATUS_BAD_ARGUMENT);
    CHECK(((unsigned char *)records)[0]==0x5a&&((unsigned char *)issues)[0]==0x5a&&((unsigned char *)&result)[0]==0x5a);
    CHECK(remove_tree(root));return 1;
}

int main(void){
    CASE(test_clean_and_empty());CASE(test_optional_empty());CASE(test_partial_uncertainty());
    CASE(test_duplicate_fatal());CASE(test_readdir_error_and_root_replacement());
    CASE(test_global_operation_cap());CASE(test_byte_cap_clears_rows());CASE(test_invalid_preflight_no_write());
    if(failures){fprintf(stderr,"CATALOG_CASES %d FAILURES %d\n",cases,failures);return 1;}
    printf("CATALOG_CASES %d FAILURES 0\n",cases);return 0;
}
