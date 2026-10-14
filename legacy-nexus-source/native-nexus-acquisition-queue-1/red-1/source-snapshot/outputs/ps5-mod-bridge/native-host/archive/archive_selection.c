#include "archive_selection.h"
#include "pak_secondary_integrity.h"
#include "mbedtls/sha256.h"
#include <string.h>

typedef struct span {uintptr_t begin,end;} span;
static int region(const void *p,size_t count,size_t width,size_t alignment,span *s) {
 uintptr_t a=(uintptr_t)p;size_t n;
 if(width&&count>SIZE_MAX/width)return 0;n=count*width;
 if((n&&!p)||(p&&a%alignment)||n>UINTPTR_MAX-a)return 0;
 *s=(span){a,a+n};return 1;
}
static int apart(span *s,size_t n) {
 for(size_t i=0;i<n;i++)for(size_t j=0;j<i;j++)
  if(s[i].begin<s[i].end&&s[j].begin<s[j].end&&s[i].begin<s[j].end&&s[j].begin<s[i].end)return 0;
 return 1;
}
static int preflight(const mb_archive_prepared_view_v1 *v,
 const mb_iostore_archive_probe_v1 *p,uint32_t pc,const mb_iostore_archive_workspace_v1 *w,
 uint32_t *selected,uint32_t sc,mb_iostore_archive_member_v1 *files,uint32_t fc,
 mb_iostore_archive_selection_v1 *r) {
 span s[34];size_t n=0;
 if(!v||!w||!r||(uintptr_t)v%_Alignof(mb_archive_prepared_view_v1)||
  (uintptr_t)w%_Alignof(mb_iostore_archive_workspace_v1)||!v->inventory||
  (uintptr_t)v->inventory%_Alignof(mb_archive_inventory_v1))return 0;
 if(w->struct_size!=sizeof(*w)||w->abi_version!=1||pc>MB_IOSTORE_ARCHIVE_MAX_PROBES||
  v->row_capacity>MB_ARCHIVE_MAX_ENTRIES||v->name_capacity>MB_ARCHIVE_MAX_NAME_BYTES||
  w->grouping.sort_capacity>MB_ARCHIVE_MAX_ENTRIES||w->grouping.group_capacity>MB_ARCHIVE_MAX_ENTRIES||
  w->grouping.member_capacity>MB_ARCHIVE_MAX_ENTRIES||w->grouping.key_capacity>MB_ARCHIVE_MAX_NAME_BYTES||
  sc>MB_ARCHIVE_MAX_ENTRIES||fc>MB_ARCHIVE_MAX_ENTRIES||
  w->probe_workspace_bytes!=MB_IOSTORE_ARCHIVE_PROBE_WORKSPACE_BYTES||
  w->primary_capture_bytes!=MB_IOSTORE_ARCHIVE_PRIMARY_CAPTURE_BYTES||
  v->inventory->struct_size!=sizeof(*v->inventory)||v->inventory->abi_version!=1)return 0;
#define SP(P,C,T) do{if(!region(P,C,sizeof(T),_Alignof(T),s+n))return 0;n++;}while(0)
 SP(v,1,mb_archive_prepared_view_v1);SP(w,1,mb_iostore_archive_workspace_v1);
 SP(v->inventory,1,mb_archive_inventory_v1);SP(v->rows,v->row_capacity,mb_archive_row_v1);
 SP(v->names,v->name_capacity,char);SP(p,pc,mb_iostore_archive_probe_v1);
 SP(w->grouping.sort_indices,w->grouping.sort_capacity,uint32_t);
 SP(w->grouping.groups,w->grouping.group_capacity,mb_archive_group_v1);
 SP(w->grouping.members,w->grouping.member_capacity,mb_archive_group_member_v1);
 SP(w->grouping.keys,w->grouping.key_capacity,char);
 SP(w->probe_workspace,w->probe_workspace_bytes,uint8_t);SP(w->primary_capture,w->primary_capture_bytes,uint8_t);
 SP(selected,sc,uint32_t);SP(files,fc,mb_iostore_archive_member_v1);SP(r,1,mb_iostore_archive_selection_v1);
 for(uint32_t i=0;i<pc;i++) {
  if(p[i].reserved||p[i].bytes>MB_IOSTORE_ARCHIVE_MAX_EMPTY_PAK_BYTES)return 0;
  SP(p[i].data,p[i].bytes,uint8_t);
 }
#undef SP
 return apart(s,n);
}
static unsigned char fold(unsigned char c){return c>='A'&&c<='Z'?(unsigned char)(c+32):c;}
static int word(const char *s,size_t n,const char *literal) {
 size_t m=strlen(literal);if(n!=m)return 0;
 for(size_t i=0;i<n;i++)if(fold((unsigned char)s[i])!=(unsigned char)literal[i])return 0;
 return 1;
}
static int document_name(const char *p,uint32_t n) {
 uint32_t leaf=0,dot=UINT32_MAX;
 for(uint32_t i=0;i<n;i++)if(p[i]=='/'){leaf=i+1;dot=UINT32_MAX;}else if(p[i]=='.')dot=i;
 if(dot==UINT32_MAX||dot<leaf||dot-leaf<6||!word(p+leaf,6,"readme")||
  !(word(p+dot+1,n-dot-1,"txt")||word(p+dot+1,n-dot-1,"md")))return 0;
 if(dot-leaf==6)return 1;
 if(p[leaf+6]!='_'&&p[leaf+6]!='-'&&p[leaf+6]!='.')return 0;
 for(uint32_t i=leaf+7;i<dot;i++) {
  unsigned char c=(unsigned char)p[i];
  if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;
 }
 return 1;
}
static int plain_utf8(const uint8_t *p,size_t n) {
 size_t i=0;if(n>=3&&p[0]==0xef&&p[1]==0xbb&&p[2]==0xbf)i=3;
 while(i<n) {
  uint32_t cp;unsigned extra;uint8_t lead=p[i++];
  if(lead<0x80){cp=lead;extra=0;}
  else if(lead>=0xc2&&lead<=0xdf){cp=lead&31;extra=1;}
  else if(lead>=0xe0&&lead<=0xef){cp=lead&15;extra=2;}
  else if(lead>=0xf0&&lead<=0xf4){cp=lead&7;extra=3;}else return 0;
  if(extra>n-i)return 0;
  for(unsigned j=0;j<extra;j++){uint8_t c=p[i++];if((c&0xc0)!=0x80)return 0;cp=(cp<<6)|(c&63);}
  if((extra==1&&cp<0x80)||(extra==2&&cp<0x800)||(extra==3&&cp<0x10000)||
   cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return 0;
  if((cp<0x20&&cp!='\t'&&cp!='\r'&&cp!='\n')||(cp>=0x7f&&cp<=0x9f))return 0;
 }
 return 1;
}
static void reset(mb_iostore_archive_selection_v1 *r) {
 memset(r,0,sizeof(*r));r->struct_size=sizeof(*r);r->abi_version=1;
 r->toc_row=r->ucas_row=r->pak_row=r->problem_row=UINT32_MAX;
}
static mb_iostore_archive_status_v1 refuse(mb_iostore_archive_selection_v1 *r,
 uint32_t *rows,uint32_t rc,mb_iostore_archive_member_v1 *members,uint32_t mc,
 mb_iostore_archive_status_v1 status) {
 if(rc)memset(rows,0,rc*sizeof(*rows));if(mc)memset(members,0,mc*sizeof(*members));
 r->status=status;r->selection_complete=r->preparation_ready=r->member_count=0;
 r->toc_row=r->ucas_row=r->pak_row=UINT32_MAX;
 r->probes_verified=r->omitted_empty_paks=r->omitted_documents=0;return status;
}
static int zero(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)if(p[i])return 0;return 1;}
typedef struct memory {const uint8_t *data;size_t bytes;} memory;
static pak_provider_status memory_read(void *opaque,uint64_t offset,uint8_t *out,uint32_t n,uint32_t *actual) {
 const memory *m=opaque;if(offset>m->bytes||n>m->bytes-(size_t)offset)return PAK_PROVIDER_CONTRACT_ERROR;
 memcpy(out,m->data+(size_t)offset,n);*actual=n;return PAK_PROVIDER_OK;
}
static mb_iostore_archive_status_v1 empty_pak(const mb_iostore_archive_probe_v1 *probe,
 const mb_iostore_archive_workspace_v1 *w,mb_iostore_archive_selection_v1 *r) {
 if(probe->bytes<PAK_V11_FOOTER_BYTES)return MB_IOSTORE_ARCHIVE_UNSUPPORTED_PAK;
 memory context={probe->data,probe->bytes};
 pak_source_v1 s={sizeof(s),1,probe->bytes,1,PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE,0,&context,memory_read};
 pak_limits_v1 l;pak_limits_v1_default(&l);l.max_primary_bytes=MB_IOSTORE_ARCHIVE_PRIMARY_CAPTURE_BYTES;
 pak_primary_capture_result_v1 captured;
 pak_primary_capture_status cs=pak_capture_v11_primary(&s,&l,w->probe_workspace,w->probe_workspace_bytes,
  w->primary_capture,w->primary_capture_bytes,&captured);
 r->pak_inner_status=captured.reader_status;r->pak_inner_reason=captured.reader_reason;
 if(cs!=PAK_PRIMARY_CAPTURE_OK||!captured.primary_validated||!captured.primary_capture_complete||
  !captured.encoded_span_located)return MB_IOSTORE_ARCHIVE_UNSUPPORTED_PAK;
 const pak_primary_result_v1 *p=&captured.primary;
 if(p->declared_file_count||captured.encoded_span_bytes)return MB_IOSTORE_ARCHIVE_MATERIAL_PAK;
 uint64_t first_end=p->primary_index_offset+p->primary_index_size;
 if(p->primary_index_offset||p->path_hash_index.offset!=first_end||p->path_hash_index.size!=8||
  p->directory_index.offset!=first_end+8||p->directory_index.size!=4||
  p->directory_index.offset+4!=probe->bytes-PAK_V11_FOOTER_BYTES||
  !zero(p->encryption_key_guid,16))return MB_IOSTORE_ARCHIVE_UNSUPPORTED_PAK;
 pak_secondary_limits_v1 secondary;pak_secondary_limits_v1_default(&secondary);
 pak_read_budget_v1 budget={sizeof(budget),1,12,2,0};pak_secondary_result_v1 checked;
 const pak_secondary_index *indices[2]={&p->path_hash_index,&p->directory_index};
 for(unsigned i=0;i<2;i++) {
  pak_status status=pak_verify_v11_secondary_raw_sha1(&s,indices[i],&secondary,&budget,
   w->probe_workspace,PAK_SECONDARY_WORKSPACE_BYTES,&checked);
  r->pak_inner_status=status;r->pak_inner_reason=checked.reason;
  if(status!=PAK_STATUS_OK||!checked.digest_valid||!checked.span_complete)return MB_IOSTORE_ARCHIVE_UNSUPPORTED_PAK;
 }
 /* Retained repak schema: path-hash row count followed by pruned-directory
  * count; full directory has its own count. Exact zero forms forbid aliases,
  * deletion/tombstone rows and extra secondary data, even with declared0. */
 if(!zero(probe->data+(size_t)p->path_hash_index.offset,8)||
  !zero(probe->data+(size_t)p->directory_index.offset,4))return MB_IOSTORE_ARCHIVE_MATERIAL_PAK;
 return MB_IOSTORE_ARCHIVE_OK;
}
static void add_member(const mb_archive_prepared_view_v1 *v,uint32_t row,uint32_t role,
 uint32_t *selected,mb_iostore_archive_member_v1 *files,mb_iostore_archive_selection_v1 *r) {
 mb_iostore_archive_member_v1 *m=files+r->member_count;const mb_archive_row_v1 *src=v->rows+row;
 selected[r->member_count++]=row;m->row_index=row;m->role=role;m->bytes=src->size;
 memcpy(m->sha256,src->sha256,32);m->probe_required=role>=MB_IOSTORE_ARCHIVE_ROLE_PAK_COMPANION;
 if(m->probe_required)r->probes_required++;
}
mb_iostore_archive_status_v1 mb_iostore_archive_select_v1(const mb_archive_prepared_view_v1 *v,
 const mb_iostore_archive_probe_v1 *probes,uint32_t pc,const mb_iostore_archive_workspace_v1 *w,
 uint32_t *selected,uint32_t sc,mb_iostore_archive_member_v1 *files,uint32_t fc,
 mb_iostore_archive_selection_v1 *r) {
 if(!preflight(v,probes,pc,w,selected,sc,files,fc,r))return MB_IOSTORE_ARCHIVE_BAD_ARGUMENT;
 reset(r);if(sc)memset(selected,0,sc*sizeof(*selected));if(fc)memset(files,0,fc*sizeof(*files));
#define FAIL(S) return refuse(r,selected,sc,files,fc,S)
 const mb_archive_inventory_v1 *inv=v->inventory;
 if(inv->status!=MB_ARCHIVE_OK||inv->complete!=1||inv->installation_supported||inv->reserved||
  inv->row_count>v->row_capacity||inv->name_bytes>v->name_capacity)FAIL(MB_IOSTORE_ARCHIVE_INCOMPLETE);
 r->source_bytes=inv->source_size;memcpy(r->source_sha256,inv->source_sha256,32);
 memcpy(r->inventory_sha256,inv->inventory_sha256,32);memcpy(r->target_token,inv->target_token,32);
 if(sc<inv->row_count||fc<inv->row_count||w->grouping.sort_capacity<inv->row_count)FAIL(MB_IOSTORE_ARCHIVE_LIMIT);
 mb_archive_groups_result_v1 groups;
 int e=mb_archive_groups_v1(inv,v->rows,v->row_capacity,v->names,v->name_capacity,
  w->grouping.sort_indices,w->grouping.sort_capacity,w->grouping.groups,w->grouping.group_capacity,
  w->grouping.members,w->grouping.member_capacity,w->grouping.keys,w->grouping.key_capacity,&groups);
 r->group_inner_status=(uint32_t)e;
 if(e)FAIL(e==MB_ARCHIVE_GROUPS_LIMIT?MB_IOSTORE_ARCHIVE_LIMIT:MB_IOSTORE_ARCHIVE_GROUP_REJECTED);
 r->group_count=groups.group_count;r->path_work_used=groups.path_work_used;
 uint32_t chosen=UINT32_MAX,unresolved=0,other=0;
 for(uint32_t i=0;i<groups.group_count;i++) {
  const mb_archive_group_v1 *g=w->grouping.groups+i;
  if(g->status!=MB_ARCHIVE_GROUP_STATUS_CANDIDATE||g->members_complete!=1||g->reason_flags){unresolved++;continue;}
  if(g->kind==MB_ARCHIVE_GROUP_IOSTORE){chosen=i;r->candidate_count++;}else other++;
 }
 if(unresolved)FAIL(MB_IOSTORE_ARCHIVE_UNRESOLVED);
 if(r->candidate_count>1)FAIL(MB_IOSTORE_ARCHIVE_SELECTION_REQUIRED);
 if(!r->candidate_count)FAIL(MB_IOSTORE_ARCHIVE_NO_CANDIDATE);
 if(other)FAIL(MB_IOSTORE_ARCHIVE_UNSUPPORTED_EXTRA);
 /* Finished classifier sort scratch becomes a bounded row role map. */
 for(uint32_t i=0;i<inv->row_count;i++)w->grouping.sort_indices[i]=0;
 const mb_archive_group_v1 *g=w->grouping.groups+chosen;
 for(uint32_t i=0;i<g->member_count;i++) {
  const mb_archive_group_member_v1 *m=w->grouping.members+g->member_begin+i;
  uint32_t role=m->role==MB_ARCHIVE_GROUP_ROLE_UTOC?MB_IOSTORE_ARCHIVE_ROLE_UTOC:
   m->role==MB_ARCHIVE_GROUP_ROLE_UCAS?MB_IOSTORE_ARCHIVE_ROLE_UCAS:MB_IOSTORE_ARCHIVE_ROLE_PAK_COMPANION;
  w->grouping.sort_indices[m->row_index]=role;
  if(role==MB_IOSTORE_ARCHIVE_ROLE_UTOC)r->toc_row=m->row_index;
  else if(role==MB_IOSTORE_ARCHIVE_ROLE_UCAS)r->ucas_row=m->row_index;else r->pak_row=m->row_index;
 }
 for(uint32_t i=0;i<inv->row_count;i++) {
  const mb_archive_row_v1 *row=v->rows+i;uint32_t role=w->grouping.sort_indices[i];
  if(row->kind!=MB_ARCHIVE_REGULAR)continue;
  if(!role) {
   if(!document_name(v->names+row->path_offset,row->path_length)){r->problem_row=i;FAIL(MB_IOSTORE_ARCHIVE_UNSUPPORTED_EXTRA);}
   if(!row->size||row->size>MB_IOSTORE_ARCHIVE_MAX_DOCUMENT_BYTES||r->document_count>=MB_IOSTORE_ARCHIVE_MAX_DOCUMENTS){r->problem_row=i;FAIL(MB_IOSTORE_ARCHIVE_LIMIT);}
   role=MB_IOSTORE_ARCHIVE_ROLE_DOCUMENT;r->document_count++;
  }
  if(role==MB_IOSTORE_ARCHIVE_ROLE_PAK_COMPANION&&row->size>MB_IOSTORE_ARCHIVE_MAX_EMPTY_PAK_BYTES){r->problem_row=i;FAIL(MB_IOSTORE_ARCHIVE_UNSUPPORTED_PAK);}
  add_member(v,i,role,selected,files,r);
 }
 if(r->toc_row==UINT32_MAX||r->ucas_row==UINT32_MAX)FAIL(MB_IOSTORE_ARCHIVE_UNRESOLVED);
 if(!pc&&r->probes_required){r->status=MB_IOSTORE_ARCHIVE_PROBES_REQUIRED;r->selection_complete=1;return MB_IOSTORE_ARCHIVE_PROBES_REQUIRED;}
 if(pc!=r->probes_required)FAIL(MB_IOSTORE_ARCHIVE_PROBE_MISMATCH);
 for(uint32_t i=0;i<inv->row_count;i++)w->grouping.sort_indices[i]=UINT32_MAX;
 for(uint32_t i=0;i<pc;i++) {
  if(probes[i].row_index>=inv->row_count||w->grouping.sort_indices[probes[i].row_index]!=UINT32_MAX)FAIL(MB_IOSTORE_ARCHIVE_PROBE_MISMATCH);
  w->grouping.sort_indices[probes[i].row_index]=i;
 }
 for(uint32_t i=0;i<r->member_count;i++) {
  mb_iostore_archive_member_v1 *m=files+i;uint32_t p=w->grouping.sort_indices[m->row_index];
  if(!m->probe_required){if(p!=UINT32_MAX){r->problem_row=m->row_index;FAIL(MB_IOSTORE_ARCHIVE_PROBE_MISMATCH);}continue;}
  if(p==UINT32_MAX){r->problem_row=m->row_index;FAIL(MB_IOSTORE_ARCHIVE_PROBE_MISMATCH);}
  const mb_iostore_archive_probe_v1 *probe=probes+p;uint8_t hash[32];
  if(probe->bytes!=m->bytes){r->problem_row=m->row_index;FAIL(MB_IOSTORE_ARCHIVE_PROBE_MISMATCH);}
  if(mbedtls_sha256(probe->data,probe->bytes,hash,0))FAIL(MB_IOSTORE_ARCHIVE_HASH_ERROR);
  if(memcmp(hash,m->sha256,32)){r->problem_row=m->row_index;FAIL(MB_IOSTORE_ARCHIVE_PROBE_MISMATCH);}
  mb_iostore_archive_status_v1 ps;
  if(m->role==MB_IOSTORE_ARCHIVE_ROLE_PAK_COMPANION)ps=empty_pak(probe,w,r);
  else ps=plain_utf8(probe->data,probe->bytes)?MB_IOSTORE_ARCHIVE_OK:MB_IOSTORE_ARCHIVE_INVALID_DOCUMENT;
  if(ps!=MB_IOSTORE_ARCHIVE_OK){r->problem_row=m->row_index;FAIL(ps);}
  m->probe_verified=m->omitted_from_iostore_job=1;r->probes_verified++;
  if(m->role==MB_IOSTORE_ARCHIVE_ROLE_PAK_COMPANION)r->omitted_empty_paks++;else r->omitted_documents++;
 }
 r->selection_complete=r->preparation_ready=1;r->status=MB_IOSTORE_ARCHIVE_OK;
 return MB_IOSTORE_ARCHIVE_OK;
#undef FAIL
}
