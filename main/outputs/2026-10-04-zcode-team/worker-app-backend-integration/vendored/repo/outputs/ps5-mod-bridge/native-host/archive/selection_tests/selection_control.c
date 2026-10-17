#include "archive_selection.h"
#include "mbedtls/sha256.h"
#include "actual_inventory_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAP 12u
static unsigned checks,failures;
static void check(int ok,const char *label){checks++;if(!ok){failures++;fprintf(stderr,"FAIL %s\n",label);}}
static mb_archive_inventory_v1 inv;
static mb_archive_row_v1 rows[CAP];static char names[2048],keys[2048];
static uint32_t sort[CAP],selected[CAP];static mb_archive_group_v1 groups[CAP];
static mb_archive_group_member_v1 group_members[CAP];static mb_iostore_archive_member_v1 files[CAP];
static uint8_t probe_workspace[65536],capture[65536];
static mb_archive_prepared_view_v1 view={&inv,rows,CAP,names,sizeof(names)};
static mb_iostore_archive_workspace_v1 work={sizeof(work),1,
 {sort,CAP,groups,CAP,group_members,CAP,keys,sizeof(keys)},probe_workspace,sizeof(probe_workspace),capture,sizeof(capture)};
static mb_iostore_archive_selection_v1 result;
static void setup(void){
 memset(&inv,0,sizeof(inv));memset(rows,0,sizeof(rows));memset(names,0,sizeof(names));
 inv.struct_size=sizeof(inv);inv.abi_version=1;inv.complete=1;
 memcpy(inv.source_sha256,actual_source_sha256,32);memcpy(inv.inventory_sha256,actual_inventory_sha256,32);
 memcpy(inv.target_token,actual_target_token,32);inv.source_size=actual_source_bytes;
}
static uint32_t add(const char *path,uint64_t size,const uint8_t *hash){
 uint32_t i=inv.row_count++;mb_archive_row_v1 *r=rows+i;size_t n=strlen(path);
 if(i>=CAP||n+1>sizeof(names)-inv.name_bytes)exit(2);
 r->kind=MB_ARCHIVE_REGULAR;r->path_offset=r->original_offset=inv.name_bytes;
 r->path_length=r->original_length=(uint32_t)n;r->size=size;
 memcpy(names+inv.name_bytes,path,n+1);inv.name_bytes+=(uint32_t)n+1;inv.expanded_bytes+=size;
 if(hash)memcpy(r->sha256,hash,32);return i;
}
static uint8_t *load(const char *path,size_t cap,size_t *bytes){
 FILE *f=fopen(path,"rb");long n;if(!f)return NULL;
 if(fseek(f,0,SEEK_END)||(n=ftell(f))<1||(uint64_t)n>cap||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
 uint8_t *p=malloc((size_t)n);if(!p||fread(p,1,(size_t)n,f)!=(size_t)n||fclose(f)){free(p);return NULL;}
 *bytes=(size_t)n;return p;
}
static mb_iostore_archive_status_v1 run(const mb_iostore_archive_probe_v1 *p,uint32_t n){
 return mb_iostore_archive_select_v1(&view,p,n,&work,selected,CAP,files,CAP,&result);
}
int main(int argc,char **argv){
 if(argc!=4)return 2;
 size_t pak_n=0,doc_n=0,material_n=0;
 uint8_t *pak=load(argv[1],MB_IOSTORE_ARCHIVE_MAX_EMPTY_PAK_BYTES,&pak_n);
 uint8_t *doc=load(argv[2],MB_IOSTORE_ARCHIVE_MAX_DOCUMENT_BYTES,&doc_n);
 uint8_t *material=load(argv[3],MB_IOSTORE_ARCHIVE_MAX_EMPTY_PAK_BYTES,&material_n);
 if(!pak||!doc||!material)return 2;
 setup();for(unsigned i=0;i<4;i++)add(actual_members[i].name,actual_members[i].bytes,actual_members[i].sha256);
 mb_iostore_archive_status_v1 st=run(NULL,0);
 check(st==MB_IOSTORE_ARCHIVE_PROBES_REQUIRED&&result.selection_complete&&!result.preparation_ready,"actual metadata waits for explicit PAK/readme probes");
 check(result.member_count==4&&result.probes_required==2&&result.toc_row==3&&result.ucas_row==2&&result.pak_row==1,"all four archive regular rows retained with automatic roles");
 for(unsigned i=0;i<4;i++)check(selected[i]==i&&files[i].row_index==i&&files[i].bytes==actual_members[i].bytes&&!memcmp(files[i].sha256,actual_members[i].sha256,32),"inventory-derived materialization row identity");
 check(!memcmp(result.source_sha256,actual_source_sha256,32)&&!memcmp(result.inventory_sha256,actual_inventory_sha256,32)&&!memcmp(result.target_token,actual_target_token,32),"completed inventory source tuple preserved");
 mb_iostore_archive_probe_v1 probes[2]={{1,0,pak,pak_n},{0,0,doc,doc_n}};
 st=run(probes,2);
 check(st==MB_IOSTORE_ARCHIVE_OK&&result.selection_complete&&result.preparation_ready,"actual empty PAK plus plaintext readme probes admit preparation");
 check(result.probes_verified==2&&result.omitted_empty_paks==1&&result.omitted_documents==1,"omissions explicitly format/content verified");
 check(files[0].probe_verified&&files[0].omitted_from_iostore_job&&files[1].probe_verified&&files[1].omitted_from_iostore_job&&!files[2].omitted_from_iostore_job&&!files[3].omitted_from_iostore_job,"only verified companion/document omitted from IoStore payload job");
 check(!result.payload_verified&&!result.authenticity_verified&&!result.runtime_supported&&!result.installation_supported,"no compatibility/installation authority");
 printf("{\"actual_scope_status\":%u,\"actual_member_count\":%u,\"toc_row\":%u,\"ucas_row\":%u,\"pak_row\":%u,\"verified_probes\":%u,\"empty_pak_bytes\":%zu,\"readme_bytes\":%zu,\"preparation_ready\":%u,\"omitted_empty_paks\":%u,\"omitted_documents\":%u}\n",st,result.member_count,result.toc_row,result.ucas_row,result.pak_row,result.probes_verified,pak_n,doc_n,result.preparation_ready,result.omitted_empty_paks,result.omitted_documents);
 uint8_t material_sha[32];if(mbedtls_sha256(material,material_n,material_sha,0))return 2;
 setup();add("bundle.utoc",12,NULL);add("bundle.ucas",16,NULL);add("bundle.pak",material_n,material_sha);
 mb_iostore_archive_probe_v1 mp={2,0,material,material_n};st=run(&mp,1);
 check(st==MB_IOSTORE_ARCHIVE_MATERIAL_PAK&&!result.preparation_ready&&!result.selection_complete&&!result.member_count&&result.problem_row==2,"real nonempty PAK refuses rather than silently dropping entries");
 setup();add("bundle.utoc",12,NULL);add("bundle.ucas",16,NULL);add("runtime.dll",3,NULL);st=run(NULL,0);
 check(st==MB_IOSTORE_ARCHIVE_UNSUPPORTED_EXTRA&&!result.preparation_ready&&!result.member_count&&result.problem_row==2,"unknown runtime regular member refuses whole scope");
 setup();add("a.utoc",12,NULL);add("a.ucas",16,NULL);add("b.utoc",12,NULL);add("b.ucas",16,NULL);st=run(NULL,0);
 check(st==MB_IOSTORE_ARCHIVE_SELECTION_REQUIRED&&result.candidate_count==2&&!result.preparation_ready&&!result.member_count,"two valid groups require selection and never pick first");
 setup();add("only.utoc",12,NULL);st=run(NULL,0);
 check(st==MB_IOSTORE_ARCHIVE_UNRESOLVED&&!result.preparation_ready&&!result.member_count,"missing UCAS companion unresolved");
 setup();for(unsigned i=0;i<4;i++)add(actual_members[i].name,actual_members[i].bytes,actual_members[i].sha256);
 probes[0].row_index=2;st=run(probes,2);
 check(st==MB_IOSTORE_ARCHIVE_PROBE_MISMATCH&&!result.preparation_ready&&!result.member_count,"probe row substitution refuses before successful scope");
 mb_archive_inventory_v1 snapshot=inv;
 st=mb_iostore_archive_select_v1(&view,NULL,0,&work,selected,CAP,files,CAP,(mb_iostore_archive_selection_v1 *)&inv);
 check(st==MB_IOSTORE_ARCHIVE_BAD_ARGUMENT&&!memcmp(&snapshot,&inv,sizeof(inv)),"result alias inventory refused without write");
 printf("{\"checks\":%u,\"failures\":%u,\"actual_owning_inventory_fixture\":true,\"new_archive_scan\":false,\"console_connected\":false}\n",checks,failures);
 free(pak);free(doc);free(material);return failures?1:0;
}
