#include "directory_merge.h"
#include <string.h>

#define INVALID UINT32_MAX
#define STRING_CAP 4096u
typedef struct region {uintptr_t begin,end;} region;
typedef struct text {const uint8_t *data;uint32_t bytes;} text;
typedef struct tuple {const uint8_t *data;uint32_t bytes,depth;} tuple;
typedef struct node {tuple path;uint32_t seen;} node;
typedef struct pending {uint32_t row,parent;} pending;
typedef struct file_ref {
 tuple path;text name;uint8_t id[12];
 uint32_t source,source_row,final_row,directory_row,name_index;
} file_ref;
typedef struct dir_ref {
 tuple path;uint32_t name_index,parent,child,sibling,first_file,last_child;
} dir_ref;
typedef struct id_row {uint8_t id[12];uint32_t row;} id_row;
typedef struct parsed {
 text mount,root;uint32_t root_invalid;
 uint32_t directories,files,strings;
 node *nodes;file_ref *file_rows;
} parsed;
typedef struct job {
 mb_directory_merge_workspace_v1 *workspace;
 mb_directory_merge_result_v1 *result;
 size_t used,path_used;uint32_t source;
 mb_directory_merge_status_v1 status;
} job;
typedef int (*compare_fn)(const void *,const void *);

static int address(const void *p,size_t n,size_t alignment,region *r) {
 uintptr_t start=(uintptr_t)p;
 if((n&&!p)||(p&&start%alignment)||n>UINTPTR_MAX-start)return 0;
 *r=(region){start,start+n};return 1;
}
static int overlap(region a,region b) {
 return a.begin<a.end&&b.begin<b.end&&a.begin<b.end&&b.begin<a.end;
}
static int preflight(const mb_directory_merge_params_v1 *p,
 const mb_directory_merge_workspace_v1 *w,uint8_t *out,size_t cap,
 mb_directory_merge_result_v1 *r) {
 region controls[6],inputs[5];
 if(!p||!w||!r||
  !address(p,sizeof(*p),_Alignof(mb_directory_merge_params_v1),&controls[0])||
  !address(w,sizeof(*w),_Alignof(mb_directory_merge_workspace_v1),&controls[1])||
  !address(r,sizeof(*r),_Alignof(mb_directory_merge_result_v1),&controls[2])||
  !address(out,cap,1,&controls[3]))return 0;
 if(p->struct_size!=sizeof(*p)||p->abi_version!=1||w->struct_size!=sizeof(*w)||w->abi_version!=1||
  w->storage_bytes>MB_DIRECTORY_MERGE_MAX_WORKSPACE_BYTES||w->path_bytes>MB_DIRECTORY_MERGE_MAX_PATH_ARENA_BYTES||
  cap>MB_DIRECTORY_MERGE_MAX_INDEX_BYTES||
  p->base_ids.count>MB_DIRECTORY_MERGE_MAX_ID_ROWS||p->mod_ids.count>MB_DIRECTORY_MERGE_MAX_ID_ROWS||
  p->final_ids.count>MB_DIRECTORY_MERGE_MAX_ID_ROWS||
  !address(w->storage,w->storage_bytes,_Alignof(uint64_t),&controls[4])||
  !address(w->paths,w->path_bytes,1,&controls[5])||
  !address(p->base_directory.data,p->base_directory.bytes,1,&inputs[0])||
  !address(p->mod_directory.data,p->mod_directory.bytes,1,&inputs[1])||
  !address(p->base_ids.data,(size_t)p->base_ids.count*12,1,&inputs[2])||
  !address(p->mod_ids.data,(size_t)p->mod_ids.count*12,1,&inputs[3])||
  !address(p->final_ids.data,(size_t)p->final_ids.count*12,1,&inputs[4]))return 0;
 for(unsigned i=0;i<6;++i)for(unsigned k=i+1;k<6;++k)if(overlap(controls[i],controls[k]))return 0;
 for(unsigned i=0;i<5;++i)for(unsigned k=0;k<6;++k)if(overlap(inputs[i],controls[k]))return 0;
 return 1;
}
static uint32_t le32(const uint8_t *p) {
 return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static uint32_t le16(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8;}
static void put32(uint8_t *p,uint32_t v) {
 for(unsigned i=0;i<4;++i){p[i]=(uint8_t)v;v>>=8;}
}
static int fail(job *j,mb_directory_merge_status_v1 s,mb_directory_merge_reason_v1 reason) {
 if(j->status==MB_DIRECTORY_MERGE_OK){j->status=s;j->result->reason=reason;j->result->source_index=j->source;}
 return 0;
}
static void *allocate(job *j,size_t count,size_t width) {
 if(count&&width>SIZE_MAX/count){fail(j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_WORKSPACE);return NULL;}
 size_t n=count*width;
 if(j->used>SIZE_MAX-7){fail(j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_WORKSPACE);return NULL;}
 size_t aligned=(j->used+7)&~(size_t)7;
 if(n>MB_DIRECTORY_MERGE_MAX_WORKSPACE_BYTES||aligned>MB_DIRECTORY_MERGE_MAX_WORKSPACE_BYTES-n){
  fail(j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_WORKSPACE);return NULL;}
 if(aligned+n>j->workspace->storage_bytes){j->result->required_workspace_bytes=aligned+n;
  fail(j,MB_DIRECTORY_MERGE_WORKSPACE_TOO_SMALL,MB_DIRECTORY_MERGE_REASON_WORKSPACE);return NULL;}
 if(!n)return NULL;
 void *result=j->workspace->storage+aligned;j->used=aligned+n;return result;
}
static int text_compare(text a,text b) {
 size_t n=a.bytes<b.bytes?a.bytes:b.bytes;int c=n?memcmp(a.data,b.data,n):0;
 return c?c:(a.bytes>b.bytes)-(a.bytes<b.bytes);
}
static int tuple_compare(tuple a,tuple b) {
 return text_compare((text){a.data,a.bytes},(text){b.data,b.bytes});
}
static int cmp_text(const void *a,const void *b) {return text_compare(*(const text *)a,*(const text *)b);}
static int cmp_dir(const void *a,const void *b) {
 const dir_ref *x=a,*y=b;
 if(x->path.depth!=y->path.depth)return (x->path.depth>y->path.depth)-(x->path.depth<y->path.depth);
 return tuple_compare(x->path,y->path);
}
static int file_key_compare(const file_ref *a,const file_ref *b) {
 int c=tuple_compare(a->path,b->path);return c?c:text_compare(a->name,b->name);
}
static int cmp_file(const void *a,const void *b) {
 const file_ref *x=a,*y=b;int c=file_key_compare(x,y);
 return c?c:(x->source>y->source)-(x->source<y->source);
}
static int cmp_id(const void *a,const void *b) {return memcmp(((const id_row *)a)->id,((const id_row *)b)->id,12);}
static void swap(uint8_t *a,uint8_t *b,size_t width) {
 uint8_t temporary[128];memcpy(temporary,a,width);memcpy(a,b,width);memcpy(b,temporary,width);
}
static void sift(uint8_t *a,size_t root,size_t count,size_t width,compare_fn compare) {
 while(root<count/2){size_t child=root*2+1;
  if(child+1<count&&compare(a+child*width,a+(child+1)*width)<0)++child;
  if(compare(a+root*width,a+child*width)>=0)return;
  swap(a+root*width,a+child*width,width);root=child;
 }
}
static void sort(void *rows,size_t count,size_t width,compare_fn compare) {
 uint8_t *a=rows;
 for(size_t i=count/2;i>0;--i)sift(a,i-1,count,width,compare);
 for(size_t end=count;end>1;--end){swap(a,a+(end-1)*width,width);sift(a,0,end-1,width,compare);}
}
static int utf8_valid(const uint8_t *p,size_t n) {
 size_t at=0;
 while(at<n){uint32_t value=p[at++],extra=0,minimum=0;
  if(value<0x80){if(!value)return 0;continue;}
  if(value>=0xc2&&value<=0xdf){value&=0x1f;extra=1;minimum=0x80;}
  else if(value>=0xe0&&value<=0xef){value&=0x0f;extra=2;minimum=0x800;}
  else if(value>=0xf0&&value<=0xf4){value&=7;extra=3;minimum=0x10000;}else return 0;
  if(extra>n-at)return 0;
  for(uint32_t i=0;i<extra;++i){uint8_t b=p[at++];if((b&0xc0)!=0x80)return 0;value=(value<<6)|(b&0x3f);}
  if(value<minimum||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return 0;
 }
 return 1;
}
static int utf16_point(const uint8_t *p,size_t bytes,size_t *offset,uint32_t *point) {
 if(bytes-*offset<2)return 0;
 uint32_t first=le16(p+*offset);*offset+=2;
 if(!first)return 0;
 if(first>=0xdc00&&first<=0xdfff)return 0;
 if(first>=0xd800&&first<=0xdbff){
  if(bytes-*offset<2)return 0;
  uint32_t second=le16(p+*offset);*offset+=2;
  if(second<0xdc00||second>0xdfff)return 0;
  *point=0x10000+((first-0xd800)<<10)+(second-0xdc00);
 }else *point=first;
 return 1;
}
static int read_u32(job *j,const uint8_t *data,size_t bytes,size_t *offset,uint32_t *value) {
 if(*offset>bytes||bytes-*offset<4)return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_CAPTURE);
 *value=le32(data+*offset);*offset+=4;return 1;
}
static int read_text(job *j,const uint8_t *data,size_t bytes,size_t *offset,text *value) {
 uint32_t count;if(!read_u32(j,data,bytes,offset,&count))return 0;
 if(!count){*value=(text){NULL,0};return 1;}
 if(count==UINT32_C(0x80000000))return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_STRING);
 int wide=(count&UINT32_C(0x80000000))!=0;
 uint64_t length=wide?(uint64_t)(UINT32_MAX-count+1)*2:count;
 if(length>STRING_CAP||length>bytes-*offset)
  return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_STRING);
 const uint8_t *p=data+*offset;*offset+=(size_t)length;
 if(!wide){
  if(!length||p[length-1]||!utf8_valid(p,(size_t)length-1))
   return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_STRING);
  *value=(text){p,(uint32_t)length-1};return 1;
 }
 if(length<2||p[length-1]||p[length-2])return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_STRING);
 size_t at=0,payload=(size_t)length-2,out_bytes=0;
 while(at<payload){uint32_t cp;if(!utf16_point(p,payload,&at,&cp))
   return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_STRING);
  out_bytes+=cp<0x80?1:(cp<0x800?2:(cp<0x10000?3:4));
 }
 if(out_bytes>STRING_CAP)return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_STRING);
 if(!out_bytes){*value=(text){NULL,0};return 1;}
 uint8_t *decoded=allocate(j,out_bytes,1);if(!decoded)return 0;
 at=0;size_t write=0;
 while(at<payload){uint32_t cp;if(!utf16_point(p,payload,&at,&cp))return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_STRING);
  if(cp<0x80)decoded[write++]=(uint8_t)cp;
  else if(cp<0x800){decoded[write++]=(uint8_t)(0xc0|(cp>>6));decoded[write++]=(uint8_t)(0x80|(cp&63));}
  else if(cp<0x10000){decoded[write++]=(uint8_t)(0xe0|(cp>>12));decoded[write++]=(uint8_t)(0x80|((cp>>6)&63));decoded[write++]=(uint8_t)(0x80|(cp&63));}
  else {decoded[write++]=(uint8_t)(0xf0|(cp>>18));decoded[write++]=(uint8_t)(0x80|((cp>>12)&63));decoded[write++]=(uint8_t)(0x80|((cp>>6)&63));decoded[write++]=(uint8_t)(0x80|(cp&63));}
 }
 *value=(text){decoded,(uint32_t)out_bytes};return 1;
}
static int component(job *j,text value,int allow_empty) {
 if((!value.bytes&&!allow_empty)||(value.bytes==1&&value.data[0]=='.')||
  (value.bytes==2&&value.data[0]=='.'&&value.data[1]=='.'))
  return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_PATH);
 for(uint32_t i=0;i<value.bytes;++i)if(!value.data[i]||value.data[i]=='/'||value.data[i]=='\\')
  return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_PATH);
 return 1;
}
static int append_path(job *j,tuple parent,text name,tuple *result) {
 if(!name.bytes){*result=parent;return 1;}
 uint64_t needed=(uint64_t)parent.bytes+name.bytes+1;
 if(needed>MB_DIRECTORY_MERGE_MAX_PATH_BYTES||needed>MB_DIRECTORY_MERGE_MAX_PATH_ARENA_BYTES-j->path_used)
  return fail(j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_PATH);
 if(needed>j->workspace->path_bytes-j->path_used){j->result->required_path_bytes=j->path_used+needed;
  return fail(j,MB_DIRECTORY_MERGE_WORKSPACE_TOO_SMALL,MB_DIRECTORY_MERGE_REASON_WORKSPACE);}
 uint8_t *p=j->workspace->paths+j->path_used;
 if(parent.bytes)memcpy(p,parent.data,parent.bytes);
 memcpy(p+parent.bytes,name.data,name.bytes);p[needed-1]=0;j->path_used+=(size_t)needed;
 *result=(tuple){p,(uint32_t)needed,parent.depth+1};return 1;
}
static int parse(job *j,mb_directory_merge_bytes_v1 input,mb_directory_merge_ids_v1 ids,parsed *out) {
 if(!input.bytes||input.bytes>MB_DIRECTORY_MERGE_MAX_INDEX_BYTES)
  return fail(j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_CAPTURE);
 size_t at=0;const uint8_t *data=input.data;
 if(!read_text(j,data,input.bytes,&at,&out->mount))return 0;
 if(!out->mount.bytes||(out->mount.data[out->mount.bytes-1]!='/'&&out->mount.data[out->mount.bytes-1]!='\\'))
  return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_MOUNT_ROOT);
 uint32_t dc,fc,sc;if(!read_u32(j,data,input.bytes,&at,&dc))return 0;
 if(!dc||dc>MB_DIRECTORY_MERGE_MAX_ROWS||(uint64_t)dc*16>input.bytes-at)
  return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_COUNTS);
 const uint8_t *dirs=data+at;at+=(size_t)dc*16;
 if(!read_u32(j,data,input.bytes,&at,&fc))return 0;
 if(fc>MB_DIRECTORY_MERGE_MAX_ROWS||(uint64_t)fc*12>input.bytes-at)
  return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_COUNTS);
 const uint8_t *files=data+at;at+=(size_t)fc*12;
 if(!read_u32(j,data,input.bytes,&at,&sc))return 0;
 if(sc>MB_DIRECTORY_MERGE_MAX_ROWS)return fail(j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_COUNTS);
 text *strings=sc?allocate(j,sc,sizeof(*strings)):NULL;if(sc&&!strings)return 0;
 uint64_t string_bytes=out->mount.bytes;
 for(uint32_t i=0;i<sc;++i){if(!read_text(j,data,input.bytes,&at,strings+i))return 0;
  string_bytes+=strings[i].bytes;if(string_bytes>MB_DIRECTORY_MERGE_MAX_INDEX_BYTES)
   return fail(j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_STRING);}
 if(at!=input.bytes)return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_CAPTURE);
 for(uint32_t i=0;i<dc;++i){const uint8_t *d=dirs+(size_t)i*16;
  uint32_t name=le32(d),child=le32(d+4),sibling=le32(d+8),first=le32(d+12);
  if((name!=INVALID&&name>=sc)||(i&&name==INVALID)||
   (child!=INVALID&&child>=dc)||(sibling!=INVALID&&sibling>=dc)||(first!=INVALID&&first>=fc)||
   (!i&&sibling!=INVALID))return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_GRAPH);
 }
 for(uint32_t i=0;i<fc;++i){const uint8_t *f=files+(size_t)i*12;
  if(le32(f)>=sc||(le32(f+4)!=INVALID&&le32(f+4)>=fc)||le32(f+8)>=ids.count)
   return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_GRAPH);}
 uint32_t root=le32(dirs);out->root_invalid=root==INVALID;
 out->root=out->root_invalid?(text){NULL,0}:strings[root];
 if(!component(j,out->root,1))return 0;
 out->directories=dc;out->files=fc;out->strings=sc;
 out->nodes=allocate(j,dc,sizeof(*out->nodes));if(!out->nodes)return 0;
 memset(out->nodes,0,(size_t)dc*sizeof(*out->nodes));
 out->file_rows=fc?allocate(j,fc,sizeof(*out->file_rows)):NULL;if(fc&&!out->file_rows)return 0;
 uint8_t *seen=fc?allocate(j,fc,1):NULL;if(fc&&!seen)return 0;if(fc)memset(seen,0,fc);
 pending *stack=allocate(j,dc,sizeof(*stack));if(!stack)return 0;
 uint32_t stack_n=1,seen_dirs=0,seen_files=0;stack[0]=(pending){0,INVALID};uint64_t path_bytes=0;
 while(stack_n){pending current=stack[--stack_n];uint32_t row=current.row;
  if(out->nodes[row].seen)return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_GRAPH);
  out->nodes[row].seen=1;++seen_dirs;const uint8_t *d=dirs+(size_t)row*16;
  uint32_t name=le32(d),child=le32(d+4),sibling=le32(d+8),first=le32(d+12);
  text label=name==INVALID?(text){NULL,0}:strings[name];if(!component(j,label,1))return 0;
  tuple parent=current.parent==INVALID?(tuple){NULL,0,0}:out->nodes[current.parent].path;
  if(!row)out->nodes[row].path=parent;
  else if(!append_path(j,parent,label,&out->nodes[row].path))return 0;
  for(uint32_t f=first;f!=INVALID;f=le32(files+(size_t)f*12+4)){
   if(seen[f])return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_GRAPH);
   seen[f]=1;const uint8_t *raw=files+(size_t)f*12;uint32_t user=le32(raw+8);text filename=strings[le32(raw)];
   if(!component(j,filename,0))return 0;
   uint64_t virtual_bytes=(uint64_t)out->mount.bytes+(out->root.bytes?out->root.bytes+1:0)+out->nodes[row].path.bytes+filename.bytes;
   if(virtual_bytes>MB_DIRECTORY_MERGE_MAX_PATH_BYTES||virtual_bytes>MB_DIRECTORY_MERGE_MAX_PATH_ARENA_BYTES-path_bytes)
    return fail(j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_PATH);
   path_bytes+=virtual_bytes;file_ref *target=out->file_rows+seen_files++;
   *target=(file_ref){.path=out->nodes[row].path,.name=filename,.source=j->source,.source_row=f};
   memcpy(target->id,ids.data+(size_t)user*12,12);
  }
  if(sibling!=INVALID){if(stack_n>=dc)return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_GRAPH);
   stack[stack_n++]=(pending){sibling,current.parent};}
  if(child!=INVALID){if(stack_n>=dc)return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_GRAPH);
   stack[stack_n++]=(pending){child,row};}
 }
 if(seen_dirs!=dc||seen_files!=fc)return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_GRAPH);
 sort(out->file_rows,fc,sizeof(*out->file_rows),cmp_file);
 for(uint32_t i=1;i<fc;++i)if(!file_key_compare(out->file_rows+i-1,out->file_rows+i))
  return fail(j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_PATH);
 return 1;
}
static text last_component(tuple path) {
 uint32_t end=path.bytes-1,start=end;while(start&&path.data[start-1])--start;
 return (text){path.data+start,end-start};
}
static uint32_t find_directory(const dir_ref *dirs,uint32_t count,tuple key) {
 dir_ref wanted={.path=key};uint32_t lo=0,hi=count;
 while(lo<hi){uint32_t mid=lo+(hi-lo)/2;int c=cmp_dir(dirs+mid,&wanted);
  if(c<0)lo=mid+1;else if(c>0)hi=mid;else return mid;}
 return INVALID;
}
static uint32_t find_string(const text *strings,uint32_t count,text key) {
 uint32_t lo=0,hi=count;
 while(lo<hi){uint32_t mid=lo+(hi-lo)/2;int c=text_compare(strings[mid],key);
  if(c<0)lo=mid+1;else if(c>0)hi=mid;else return mid;}
 return INVALID;
}
static uint32_t find_id(const id_row *rows,uint32_t count,const uint8_t id[12]) {
 uint32_t lo=0,hi=count;
 while(lo<hi){uint32_t mid=lo+(hi-lo)/2;int c=memcmp(rows[mid].id,id,12);
  if(c<0)lo=mid+1;else if(c>0)hi=mid;else return rows[mid].row;}
 return INVALID;
}
static uint8_t *write_text(uint8_t *out,text value) {
 put32(out,value.bytes+1);out+=4;if(value.bytes)memcpy(out,value.data,value.bytes);
 out+=value.bytes;*out++=0;return out;
}

mb_directory_merge_status_v1 mb_iostore_directory_merge_v1(
 const mb_directory_merge_params_v1 *p,mb_directory_merge_workspace_v1 *w,
 uint8_t *output,size_t capacity,mb_directory_merge_result_v1 *r) {
 if(!preflight(p,w,output,capacity,r))return MB_DIRECTORY_MERGE_BAD_ARGUMENT;
 memset(r,0,sizeof(*r));r->struct_size=sizeof(*r);r->abi_version=1;
 job j={w,r,0,0,0,MB_DIRECTORY_MERGE_OK};parsed base={0},mod={0};
 id_row *ids=p->final_ids.count?allocate(&j,p->final_ids.count,sizeof(*ids)):NULL;
 if(p->final_ids.count&&!ids)goto done;
 for(uint32_t i=0;i<p->final_ids.count;++i){memcpy(ids[i].id,p->final_ids.data+(size_t)i*12,12);ids[i].row=i;}
 sort(ids,p->final_ids.count,sizeof(*ids),cmp_id);
 for(uint32_t i=1;i<p->final_ids.count;++i)if(!memcmp(ids[i-1].id,ids[i].id,12)){
  fail(&j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_ID);goto done;}
 if(!parse(&j,p->base_directory,p->base_ids,&base))goto done;
 j.source=1;if(!parse(&j,p->mod_directory,p->mod_ids,&mod))goto done;
 r->base_directories=base.directories;r->base_files=base.files;r->base_strings=base.strings;
 r->mod_directories=mod.directories;r->mod_files=mod.files;r->mod_strings=mod.strings;
 if(text_compare(base.mount,mod.mount)||text_compare(base.root,mod.root)||base.root_invalid!=mod.root_invalid){
  fail(&j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_MOUNT_ROOT);goto done;}
 j.source=2;
 uint32_t file_candidates=base.files+mod.files;
 file_ref *files=file_candidates?allocate(&j,file_candidates,sizeof(*files)):NULL;
 if(file_candidates&&!files)goto done;
 if(base.files)memcpy(files,base.file_rows,(size_t)base.files*sizeof(*files));
 if(mod.files)memcpy(files+base.files,mod.file_rows,(size_t)mod.files*sizeof(*files));
 sort(files,file_candidates,sizeof(*files),cmp_file);
 uint32_t file_count=0,overrides=0;
 for(uint32_t i=0;i<file_candidates;){uint32_t next=i+1;
  while(next<file_candidates&&!file_key_compare(files+i,files+next))++next;
  if(next-i>2||(next-i==2&&files[i].source==files[i+1].source)){
   fail(&j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_PATH);goto done;}
  if(next-i==2)++overrides;
  files[file_count++]=files[next-1];i=next;
 }
 if(file_count>MB_DIRECTORY_MERGE_MAX_ROWS){fail(&j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_COUNTS);goto done;}
 uint32_t dir_candidates=base.directories+mod.directories;
 dir_ref *dirs=allocate(&j,dir_candidates,sizeof(*dirs));if(!dirs)goto done;
 for(uint32_t i=0;i<base.directories;++i)dirs[i]=(dir_ref){.path=base.nodes[i].path};
 for(uint32_t i=0;i<mod.directories;++i)dirs[base.directories+i]=(dir_ref){.path=mod.nodes[i].path};
 sort(dirs,dir_candidates,sizeof(*dirs),cmp_dir);uint32_t dir_count=0;
 for(uint32_t i=0;i<dir_candidates;++i)if(!dir_count||cmp_dir(dirs+i,dirs+dir_count-1))dirs[dir_count++]=dirs[i];
 if(dir_count>MB_DIRECTORY_MERGE_MAX_ROWS){fail(&j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_COUNTS);goto done;}
 uint32_t string_candidates=dir_count-1+file_count+(base.root_invalid?0:1);
 text *strings=string_candidates?allocate(&j,string_candidates,sizeof(*strings)):NULL;
 if(string_candidates&&!strings)goto done;uint32_t cursor=0;
 for(uint32_t i=1;i<dir_count;++i)strings[cursor++]=last_component(dirs[i].path);
 for(uint32_t i=0;i<file_count;++i)strings[cursor++]=files[i].name;
 if(!base.root_invalid)strings[cursor++]=base.root;
 sort(strings,string_candidates,sizeof(*strings),cmp_text);uint32_t string_count=0;
 for(uint32_t i=0;i<string_candidates;++i)if(!string_count||text_compare(strings[i],strings[string_count-1]))strings[string_count++]=strings[i];
 if(string_count>MB_DIRECTORY_MERGE_MAX_ROWS){fail(&j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_COUNTS);goto done;}
 uint64_t text_bytes=0;for(uint32_t i=0;i<string_count;++i){
  if(strings[i].bytes>=STRING_CAP){fail(&j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_STRING);goto done;}
  text_bytes+=strings[i].bytes;
 }
 if(text_bytes>MB_DIRECTORY_MERGE_MAX_INDEX_BYTES||base.mount.bytes>=STRING_CAP){
  fail(&j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_STRING);goto done;}
 for(uint32_t i=0;i<dir_count;++i){dirs[i].parent=dirs[i].child=dirs[i].sibling=dirs[i].first_file=dirs[i].last_child=INVALID;
  if(!i)dirs[i].name_index=base.root_invalid?INVALID:find_string(strings,string_count,base.root);
  else {text name=last_component(dirs[i].path);dirs[i].name_index=find_string(strings,string_count,name);
   tuple parent={dirs[i].path.data,dirs[i].path.bytes-name.bytes-1,dirs[i].path.depth-1};
   uint32_t parent_row=find_directory(dirs,dir_count,parent);
   if(parent_row==INVALID||parent_row>=i){fail(&j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_GRAPH);goto done;}
   dirs[i].parent=parent_row;
   if(dirs[parent_row].last_child==INVALID)dirs[parent_row].child=i;
   else dirs[dirs[parent_row].last_child].sibling=i;
   dirs[parent_row].last_child=i;
  }
 }
 for(uint32_t i=0;i<file_count;++i){
  files[i].directory_row=find_directory(dirs,dir_count,files[i].path);
  files[i].name_index=find_string(strings,string_count,files[i].name);
  files[i].final_row=find_id(ids,p->final_ids.count,files[i].id);
  if(files[i].directory_row==INVALID||files[i].name_index==INVALID||files[i].final_row==INVALID){
   fail(&j,MB_DIRECTORY_MERGE_INVALID,MB_DIRECTORY_MERGE_REASON_ID);goto done;}
  if(dirs[files[i].directory_row].first_file==INVALID)dirs[files[i].directory_row].first_file=i;
 }
 uint64_t needed=(uint64_t)base.mount.bytes+5+12+(uint64_t)dir_count*16+(uint64_t)file_count*12+text_bytes+(uint64_t)string_count*5;
 if(needed>MB_DIRECTORY_MERGE_MAX_INDEX_BYTES||needed>SIZE_MAX){fail(&j,MB_DIRECTORY_MERGE_RESOURCE_LIMIT,MB_DIRECTORY_MERGE_REASON_OUTPUT);goto done;}
 r->required_output_bytes=needed;
 if(capacity<needed){fail(&j,MB_DIRECTORY_MERGE_OUTPUT_TOO_SMALL,MB_DIRECTORY_MERGE_REASON_OUTPUT);goto done;}
 /* All graph/identity/text/map/size checks finish before any output write. */
 uint8_t *out=write_text(output,base.mount);put32(out,dir_count);out+=4;
 for(uint32_t i=0;i<dir_count;++i){put32(out,dirs[i].name_index);put32(out+4,dirs[i].child);put32(out+8,dirs[i].sibling);put32(out+12,dirs[i].first_file);out+=16;}
 put32(out,file_count);out+=4;
 for(uint32_t i=0;i<file_count;++i){put32(out,files[i].name_index);
  put32(out+4,i+1<file_count&&files[i].directory_row==files[i+1].directory_row?i+1:INVALID);
  put32(out+8,files[i].final_row);out+=12;}
 put32(out,string_count);out+=4;for(uint32_t i=0;i<string_count;++i)out=write_text(out,strings[i]);
 r->output_bytes=needed;r->merged_directories=dir_count;r->merged_files=file_count;r->merged_strings=string_count;
 r->exact_path_overrides=overrides;r->metadata_complete=1;r->userdata_remapped_by_id=1;
done:
 r->status=j.status;r->workspace_used=j.used;r->path_arena_used=j.path_used;
 return j.status;
}
