#include "directory_merge.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Ordinary host fixture caller, not part of the portable module. */
typedef struct { uint8_t *data; size_t bytes; } loaded;
static unsigned checks, failures;
static void check(int truth, const char *label) {
 checks++; if (!truth) { failures++; fprintf(stderr,"FAIL %s\n",label); }
}
static void put32(uint8_t *p,uint32_t n) {
 p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8); p[2]=(uint8_t)(n>>16); p[3]=(uint8_t)(n>>24);
}
static loaded load(const char *path,size_t cap) {
 loaded b={0}; FILE *f=fopen(path,"rb"); long n;
 if (!f) { perror(path); exit(2); }
 if (fseek(f,0,SEEK_END) || (n=ftell(f))<0 || (unsigned long)n>cap || !n || fseek(f,0,SEEK_SET)) {
  fprintf(stderr,"invalid bounded fixture: %s\n",path); fclose(f); exit(2);
 }
 b.bytes=(size_t)n; b.data=(uint8_t *)malloc(b.bytes);
 if (!b.data || fread(b.data,1,b.bytes,f)!=b.bytes || fclose(f)) { fprintf(stderr,"fixture read failed\n"); exit(2); }
 return b;
}
static void params(mb_directory_merge_params_v1 *p,const uint8_t *base,size_t bn,
 const uint8_t *mod,size_t mn,const uint8_t *base_ids,uint32_t bc,
 const uint8_t *mod_ids,uint32_t mc,const uint8_t *final_ids,uint32_t fc) {
 memset(p,0,sizeof(*p)); p->struct_size=sizeof(*p); p->abi_version=1;
 p->base_directory=(mb_directory_merge_bytes_v1){base,bn};
 p->mod_directory=(mb_directory_merge_bytes_v1){mod,mn};
 p->base_ids=(mb_directory_merge_ids_v1){base_ids,bc};
 p->mod_ids=(mb_directory_merge_ids_v1){mod_ids,mc};
 p->final_ids=(mb_directory_merge_ids_v1){final_ids,fc};
}
static mb_directory_merge_workspace_v1 workspace(void) {
 mb_directory_merge_workspace_v1 w={0}; w.struct_size=sizeof(w); w.abi_version=1;
 w.storage_bytes=MB_DIRECTORY_MERGE_MAX_WORKSPACE_BYTES;
 w.path_bytes=MB_DIRECTORY_MERGE_MAX_PATH_ARENA_BYTES;
 w.storage=(uint8_t *)malloc(w.storage_bytes); w.paths=(uint8_t *)malloc(w.path_bytes);
 if (!w.storage || !w.paths) { fprintf(stderr,"workspace allocation failed\n"); exit(2); }
 return w;
}
static void release_workspace(mb_directory_merge_workspace_v1 *w) { free(w->storage); free(w->paths); }
static void report(const mb_directory_merge_result_v1 *r) {
 printf("{\"status\":%u,\"reason\":%u,\"source_index\":%u,\"metadata_complete\":%u,"
        "\"output_bytes\":%llu,\"required_output_bytes\":%llu,\"workspace_used\":%llu,"
        "\"path_arena_used\":%llu,\"base_directories\":%u,\"base_files\":%u,"
        "\"base_strings\":%u,\"mod_directories\":%u,\"mod_files\":%u,\"mod_strings\":%u,"
        "\"merged_directories\":%u,\"merged_files\":%u,\"merged_strings\":%u,"
        "\"exact_path_overrides\":%u,\"userdata_remapped_by_id\":%u,"
        "\"payload_verified\":%u,\"authenticity_verified\":%u,\"runtime_supported\":%u,"
        "\"installation_supported\":%u}\n",r->status,r->reason,r->source_index,r->metadata_complete,
        (unsigned long long)r->output_bytes,(unsigned long long)r->required_output_bytes,
        (unsigned long long)r->workspace_used,(unsigned long long)r->path_arena_used,
        r->base_directories,r->base_files,r->base_strings,r->mod_directories,r->mod_files,r->mod_strings,
        r->merged_directories,r->merged_files,r->merged_strings,r->exact_path_overrides,
        r->userdata_remapped_by_id,r->payload_verified,r->authenticity_verified,r->runtime_supported,
        r->installation_supported);
}
static size_t fs8(uint8_t *p,const char *s) {
 size_t n=strlen(s)+1; put32(p,(uint32_t)n); memcpy(p+4,s,n); return 4+n;
}
static size_t fs16(uint8_t *p,const uint16_t *s,size_t n) {
 size_t i; put32(p,(uint32_t)-(int32_t)(n+1));
 for(i=0;i<n;i++) { p[4+i*2]=(uint8_t)s[i]; p[5+i*2]=(uint8_t)(s[i]>>8); }
 p[4+n*2]=p[5+n*2]=0; return 4+(n+1)*2;
}
static size_t empty(uint8_t *p) {
 size_t n=fs8(p,"../../../"); put32(p+n,1); n+=4;
 put32(p+n,UINT32_MAX); put32(p+n+4,UINT32_MAX); put32(p+n+8,UINT32_MAX); put32(p+n+12,UINT32_MAX); n+=16;
 put32(p+n,0); n+=4; put32(p+n,0); return n+4;
}
/* Direct literal fixture construction: does not implement any merge algorithm. */
static size_t unicode_tree(uint8_t *p,int utf16,int expected) {
 static const uint16_t mount[]={'.','.','/','.','.','/','.','.','/'};
 static const uint16_t dir[]={0x754c};
 static const uint16_t file[]={0x00e9,'.','b','i','n'};
 size_t n=utf16?fs16(p,mount,9):fs8(p,"../../../");
 put32(p+n,2); n+=4;
 put32(p+n,UINT32_MAX); put32(p+n+4,1); put32(p+n+8,UINT32_MAX); put32(p+n+12,UINT32_MAX); n+=16;
 put32(p+n,expected?1:0); put32(p+n+4,UINT32_MAX); put32(p+n+8,UINT32_MAX); put32(p+n+12,0); n+=16;
 put32(p+n,1); n+=4;
 put32(p+n,expected?0:1); put32(p+n+4,UINT32_MAX); put32(p+n+8,expected?1:0); n+=12;
 put32(p+n,2); n+=4;
 if(expected) { n+=fs8(p+n,"\xc3\xa9.bin"); n+=fs8(p+n,"\xe7\x95\x8c"); }
 else if(utf16) { n+=fs16(p+n,dir,1); n+=fs16(p+n,file,5); }
 else { n+=fs8(p+n,"\xe7\x95\x8c"); n+=fs8(p+n,"\xc3\xa9.bin"); }
 return n;
}
static void synthetic(void) {
 uint8_t base[256],mod[256],expected[256],broken[320],ids[24]={1};
 uint8_t *out=(uint8_t *)malloc(MB_DIRECTORY_MERGE_MAX_INDEX_BYTES);
 size_t bn=empty(base),mn=unicode_tree(mod,1,0),en=unicode_tree(expected,0,1);
 size_t head=4+20; /* UTF16 mount count+10 code units =>24B. */
 mb_directory_merge_params_v1 p; mb_directory_merge_workspace_v1 w=workspace();
 mb_directory_merge_result_v1 r; mb_directory_merge_status_v1 st;
 if(!out) exit(2); ids[12]=2;
 params(&p,base,bn,mod,mn,ids,1,ids+12,1,ids,2);
 memset(out,0xa5,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES);
 st=mb_iostore_directory_merge_v1(&p,&w,out,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&r);
 check(st==MB_DIRECTORY_MERGE_OK && r.metadata_complete && r.userdata_remapped_by_id,"UTF8/UTF16 positive complete");
 check(r.output_bytes==en && !memcmp(out,expected,en),"Unicode canonical exact UTF8 bytes and UserData remap");
 check(!r.payload_verified&&!r.authenticity_verified&&!r.runtime_supported&&!r.installation_supported,"no runtime/install authority");
 /* Root's first child points at itself. */
 memcpy(broken,mod,mn); put32(broken+head+4+4,0); p.mod_directory=(mb_directory_merge_bytes_v1){broken,mn};
 memset(out,0xa5,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES);
 st=mb_iostore_directory_merge_v1(&p,&w,out,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&r);
 check(st==MB_DIRECTORY_MERGE_INVALID && r.reason==MB_DIRECTORY_MERGE_REASON_GRAPH && !r.metadata_complete,"root child cycle refused");
 check(out[0]==0xa5 && out[MB_DIRECTORY_MERGE_MAX_INDEX_BYTES-1]==0xa5,"cycle output unpublished");
 /* File's next link points at itself. */
 memcpy(broken,mod,mn); put32(broken+head+4+32+4+4,0); p.mod_directory=(mb_directory_merge_bytes_v1){broken,mn};
 memset(out,0xa5,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES);
 st=mb_iostore_directory_merge_v1(&p,&w,out,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&r);
 check(st==MB_DIRECTORY_MERGE_INVALID && r.reason==MB_DIRECTORY_MERGE_REASON_GRAPH && !r.metadata_complete,"file self-cycle refused");
 check(out[0]==0xa5 && out[MB_DIRECTORY_MERGE_MAX_INDEX_BYTES-1]==0xa5,"file cycle output unpublished");
 /* Disconnect an otherwise bounded directory row. */
 memcpy(broken,mod,mn); put32(broken+head+4+4,UINT32_MAX); p.mod_directory=(mb_directory_merge_bytes_v1){broken,mn};
 memset(out,0xa5,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES);
 st=mb_iostore_directory_merge_v1(&p,&w,out,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&r);
 check(st==MB_DIRECTORY_MERGE_INVALID && r.reason==MB_DIRECTORY_MERGE_REASON_GRAPH && !r.metadata_complete,"unreachable directory refused");
 check(out[0]==0xa5 && out[MB_DIRECTORY_MERGE_MAX_INDEX_BYTES-1]==0xa5,"unreachable output unpublished");
 printf("{\"synthetic_checks\":%u,\"failures\":%u,\"positive\":\"UTF8 base/UTF16 mod, canonical UTF8 and ID remap\","
        "\"malformed_controls\":[\"root-child cycle\",\"file self-cycle\",\"unreachable directory\"]}\n",checks,failures);
 release_workspace(&w); free(out);
}
int main(int argc,char **argv) {
 if(argc==2 && !strcmp(argv[1],"--synthetic")) { synthetic(); return failures?1:0; }
 if(argc!=7) { fprintf(stderr,"usage: merge_control base.dir base.ids mod.dir mod.ids final.ids output\n"); return 2; }
 loaded base=load(argv[1],MB_DIRECTORY_MERGE_MAX_INDEX_BYTES),base_ids=load(argv[2],MB_DIRECTORY_MERGE_MAX_ID_ROWS*12u);
 loaded mod=load(argv[3],MB_DIRECTORY_MERGE_MAX_INDEX_BYTES),mod_ids=load(argv[4],MB_DIRECTORY_MERGE_MAX_ID_ROWS*12u);
 loaded final_ids=load(argv[5],MB_DIRECTORY_MERGE_MAX_ID_ROWS*12u);
 mb_directory_merge_params_v1 p; mb_directory_merge_workspace_v1 w=workspace(); mb_directory_merge_result_v1 r;
 uint8_t *output=(uint8_t *)malloc(MB_DIRECTORY_MERGE_MAX_INDEX_BYTES); mb_directory_merge_status_v1 st;
 if(!output || base_ids.bytes%12 || mod_ids.bytes%12 || final_ids.bytes%12) { fprintf(stderr,"invalid ID table/output\n"); return 2; }
 params(&p,base.data,base.bytes,mod.data,mod.bytes,base_ids.data,(uint32_t)(base_ids.bytes/12),
        mod_ids.data,(uint32_t)(mod_ids.bytes/12),final_ids.data,(uint32_t)(final_ids.bytes/12));
 st=mb_iostore_directory_merge_v1(&p,&w,output,MB_DIRECTORY_MERGE_MAX_INDEX_BYTES,&r); report(&r);
 if(st==MB_DIRECTORY_MERGE_OK && r.metadata_complete && r.output_bytes<=MB_DIRECTORY_MERGE_MAX_INDEX_BYTES) {
  FILE *f=fopen(argv[6],"wbx"); if(!f || fwrite(output,1,(size_t)r.output_bytes,f)!=r.output_bytes || fclose(f)) { fprintf(stderr,"output failed\n"); return 2; }
 }
 free(base.data);free(base_ids.data);free(mod.data);free(mod_ids.data);free(final_ids.data);free(output);release_workspace(&w);
 return st==MB_DIRECTORY_MERGE_OK && r.metadata_complete?0:1;
}
