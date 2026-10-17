/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_init_once.c; that original file is unchanged). */
#include "mb_win_fileio.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

_Static_assert(sizeof(void *)==8&&sizeof(size_t)==8&&sizeof(uint32_t)==4,"Windows x64 scalar widths");
_Static_assert(sizeof(mb_win_filetime)==8&&_Alignof(mb_win_filetime)==4,"Windows FILETIME layout");
_Static_assert(sizeof(mb_win_w32_file_attribute_data)==36&&_Alignof(mb_win_w32_file_attribute_data)==4,"WIN32_FILE_ATTRIBUTE_DATA layout");
_Static_assert(sizeof(mb_win_by_handle_file_information)==52&&_Alignof(mb_win_by_handle_file_information)==4,"BY_HANDLE_FILE_INFORMATION layout");
_Static_assert(sizeof(mb_win_w32_find_data)==592&&_Alignof(mb_win_w32_find_data)==4&&sizeof(((mb_win_w32_find_data *)0)->name)==520,"WIN32_FIND_DATAW layout");
_Static_assert(sizeof(mb_win_createfile2_extended)==32&&_Alignof(mb_win_createfile2_extended)==8,"CREATEFILE2_EXTENDED_PARAMETERS layout");
_Static_assert(sizeof(mb_win_file_basic_info)==40&&_Alignof(mb_win_file_basic_info)==8,"FILE_BASIC_INFO layout");
_Static_assert(sizeof(mb_win_file_standard_info)==24&&_Alignof(mb_win_file_standard_info)==8,"FILE_STANDARD_INFO layout");

#define KIND_FILE 1u
#define KIND_DIR 2u
#define KIND_FIND 3u
#define MAX_PATH_UNITS 32767u
#define MAX_COMPONENT 255u
#define MAX_TRANSFER UINT32_C(0x40000000)
#define EPOCH_DELTA UINT64_C(116444736000000000)
typedef struct entry {
    unsigned kind, generation, share, wants;
    uint64_t offset;
    int fd;
    char *path;      /* canonical lowercase virtual path, '/'-joined */
    DIR *dir;        /* KIND_FIND only */
    char *dir_host;  /* KIND_FIND only */
    char *pattern;   /* KIND_FIND only */
} entry;
typedef struct state {
    pthread_mutex_t mutex;
    mb_win_thread_domain *domain;
    mb_win_thread_pin domain_pin;
    mb_win_fileio_pin *pins;
    entry *table;
    size_t count, capacity;
    char *root;
} state;
static void must(int status){if(status)abort();}
static state *get(mb_win_fileio *o){if(!o||!o->state)abort();return o->state;}
static state *enter(mb_win_fileio *o){state *s=get(o);(void)mb_win_threads_last_error(s->domain);must(pthread_mutex_lock(&s->mutex));return s;}
static void unlock(state *s){must(pthread_mutex_unlock(&s->mutex));}
static uint32_t *errs(state *s){return mb_win_threads_last_error(s->domain);}

/* ---------- UTF-16 <-> UTF-8, ASCII-only folding (policy) ---------- */
static unsigned char fold(unsigned char c){return (unsigned char)((c>='A'&&c<='Z')?(c+'a'-'A'):c);}
static int fold_equal(const char *a,const char *b){while(*a&&*b){if(fold((unsigned char)*a)!=fold((unsigned char)*b))return 0;a++;b++;}return *a==*b;}
static size_t utf16_len(const uint16_t *w){size_t n=0;while(n<=MAX_PATH_UNITS+1u&&w[n])++n;return n;}
/* Returns malloc'd lowercase-folded UTF-8, NULL with *invalid set for an
 * ill-formed UTF-16 sequence, NULL alone on allocation failure. */
static char *utf16_to_utf8_fold(const uint16_t *w,size_t n,int *invalid){
    char *out=malloc(4*n+1);
    if(!out)return NULL;
    size_t o=0;
    for(size_t i=0;i<n;++i){
        uint32_t cp=w[i];
        if(w[i]>=0xD800u&&w[i]<=0xDBFFu){
            if(i+1>=n||w[i+1]<0xDC00u||w[i+1]>0xDFFFu){free(out);*invalid=1;return NULL;}
            cp=0x10000u+(((uint32_t)w[i]-0xD800u)<<10)+(w[i+1]-0xDC00u);++i;
        }else if(w[i]>=0xDC00u&&w[i]<=0xDFFFu){free(out);*invalid=1;return NULL;}
        if(cp<0x80u)out[o++]=(char)fold((unsigned char)cp);
        else if(cp<0x800u){out[o++]=(char)(0xC0u|(cp>>6));out[o++]=(char)(0x80u|(cp&0x3Fu));}
        else if(cp<0x10000u){out[o++]=(char)(0xE0u|(cp>>12));out[o++]=(char)(0x80u|((cp>>6)&0x3Fu));out[o++]=(char)(0x80u|(cp&0x3Fu));}
        else{out[o++]=(char)(0xF0u|(cp>>18));out[o++]=(char)(0x80u|((cp>>12)&0x3Fu));out[o++]=(char)(0x80u|((cp>>6)&0x3Fu));out[o++]=(char)(0x80u|(cp&0x3Fu));}
    }
    out[o]=0;return out;
}
/* Returns UTF-16 units written excluding the terminator, 0 on ill-formed
 * UTF-8 or insufficient capacity. */
static size_t utf8_to_utf16(const char *s,uint16_t *w,size_t cap){
    size_t o=0;
    for(size_t i=0;s[i];){
        unsigned char b=(unsigned char)s[i];uint32_t cp;size_t len;
        if(b<0x80u){cp=b;len=1;}
        else if((b&0xE0u)==0xC0u&&b>=0xC2u){cp=b&0x1Fu;len=2;}
        else if((b&0xF0u)==0xE0u){cp=b&0x0Fu;len=3;}
        else if((b&0xF8u)==0xF0u&&b<=0xF4u){cp=b&0x07u;len=4;}
        else return 0;
        if(o+len>cap)return 0;
        for(size_t k=1;k<len;++k){unsigned char c=(unsigned char)s[i+k];if((c&0xC0u)!=0x80u)return 0;cp=(cp<<6)|(c&0x3Fu);}
        if(len==2&&cp<0x80u)return 0;if(len==3&&cp<0x800u)return 0;if(len==4&&cp<0x10000u)return 0;
        if(cp>=0x10000u){w[o++]=(uint16_t)(0xD800u+((cp-0x10000u)>>10));w[o++]=(uint16_t)(0xDC00u+((cp-0x10000u)&0x3FFu));}
        else w[o++]=(uint16_t)cp;
        i+=len;
    }
    if(o>=cap)return 0;w[o]=0;return o;
}
/* ---------- virtual namespace normalization ---------- */
/* Flat "a\0b\0c\0" lowercase component list. Containment refusals: ':' and
 * '..' fail 87 (policy); oversize component 123 (policy); oversize total
 * 206 (documented); empty path 87 (policy). */
static char *normalize(const uint16_t *w,int *win32_error,size_t *components){
    if(!w){*win32_error=MB_WIN_ERROR_INVALID_PARAMETER;return NULL;}
    size_t n=utf16_len(w);
    if(n>MAX_PATH_UNITS){*win32_error=MB_WIN_ERROR_FILENAME_EXCED_RANGE;return NULL;}
    int invalid=0;
    char *folded=utf16_to_utf8_fold(w,n,&invalid);
    if(invalid){*win32_error=MB_WIN_ERROR_INVALID_NAME;return NULL;}
    if(!folded){*win32_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return NULL;}
    size_t bytes=strlen(folded);
    char *list=calloc(bytes+2,1);
    if(!list){free(folded);*win32_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return NULL;}
    size_t used=0,count=0,run=0,start=0;
    for(size_t i=0;;){
        char c=folded[i];
        if(c=='/'||c=='\\'||!c){
            if(run){
                if(run>MAX_COMPONENT){free(list);free(folded);*win32_error=MB_WIN_ERROR_INVALID_NAME;return NULL;}
                memcpy(list+used,folded+start,run);used+=run;list[used++]=0;++count;run=0;
            }
            if(!c)break;
            i++;continue;
        }
        if(c==':'){free(list);free(folded);*win32_error=MB_WIN_ERROR_INVALID_PARAMETER;return NULL;}
        if(!run){
            start=i;
            if(c=='.'&&!folded[i+1]){i++;continue;}               /* '.' component dropped */
            if(c=='.'&&folded[i+1]=='.'&&(!folded[i+2]||folded[i+2]=='/'||folded[i+2]=='\\')){
                free(list);free(folded);*win32_error=MB_WIN_ERROR_INVALID_PARAMETER;return NULL;
            }
        }
        run++;i++;
    }
    free(folded);
    if(!count||used>MAX_PATH_UNITS){free(list);*win32_error=count?MB_WIN_ERROR_FILENAME_EXCED_RANGE:MB_WIN_ERROR_INVALID_PARAMETER;return NULL;}
    *components=count;return list;
}
static char *component_at(char *list,size_t count,size_t index){
    for(size_t i=0;i<count;++i){if(i==index)return list;list+=strlen(list)+1;}
    return NULL;
}
static char *canonical(const char *list,size_t count){
    size_t total=2;const char *c=list;
    for(size_t i=0;i<count;++i){size_t l=strlen(c);total+=l+1;c+=l+1;}
    char *out=malloc(total);
    if(!out)return NULL;
    char *o=out;c=list;
    for(size_t i=0;i<count;++i){
        size_t l=strlen(c);
        if(o!=out)*o++='/';
        memcpy(o,c,l);o+=l;c+=l+1;
    }
    *o=0;return out;
}
static char *join_host(const char *root,const char *component){
    size_t a=strlen(root),b=strlen(component);
    char *p=malloc(a+1+b+1);
    if(!p)return NULL;
    memcpy(p,root,a);p[a]='/';memcpy(p+a+1,component,b);p[a+1+b]=0;return p;
}
typedef enum { RES_OK, RES_LEAF_MISSING, RES_PATH_MISSING, RES_SYMLINK, RES_FAULT } res;
/* Per-component case-insensitive walk under the host root (host-FS-
 * independent), refusing symlinks (containment policy). When the leaf is
 * found: *leaf_host is its host path and *st its lstat. When missing and
 * allow_missing_leaf: *host_dir plus mod-spelled *leaf for creation. */
static res resolve(state *s,const char *list,size_t count,int allow_missing_leaf,
                   char **leaf_host,char **host_dir,char **leaf,struct stat *st){
    char *cur=strdup(s->root);
    if(!cur)return RES_FAULT;
    *leaf_host=NULL;*host_dir=NULL;*leaf=NULL;
    for(size_t i=0;i<count;++i){
        const char *comp=component_at((char *)list,count,i);
        int last=(i+1==count);
        DIR *d=opendir(cur);
        if(!d){free(cur);return last?RES_LEAF_MISSING:RES_PATH_MISSING;}
        struct dirent *de;char *actual=NULL;
        while((de=readdir(d))){if(fold_equal(de->d_name,comp)){actual=strdup(de->d_name);break;}}
        closedir(d);
        if(!actual){
            if(last&&allow_missing_leaf){
                *host_dir=cur;*leaf=strdup(comp);
                if(!*leaf){free(cur);*host_dir=NULL;return RES_FAULT;}
                return RES_OK;
            }
            free(cur);return last?RES_LEAF_MISSING:RES_PATH_MISSING;
        }
        char *joined=join_host(cur,actual);free(actual);
        if(!joined){free(cur);return RES_FAULT;}
        struct stat buf;
        if(lstat(joined,&buf)!=0){free(joined);free(cur);return last?RES_LEAF_MISSING:RES_PATH_MISSING;}
        if(S_ISLNK(buf.st_mode)){free(joined);free(cur);return RES_SYMLINK;}
        if(!last&&!S_ISDIR(buf.st_mode)){free(joined);free(cur);return RES_PATH_MISSING;}
        if(last){*leaf_host=joined;free(cur);if(st)*st=buf;return RES_OK;}
        free(cur);cur=joined;
    }
    return RES_FAULT;
}
/* ---------- metadata fill ---------- */
static mb_win_filetime filetime(time_t sec,long nsec){
    mb_win_filetime t={0,0};
    if(sec<0)return t; /* negative host times clamp to zero (policy) */
    uint64_t v=(uint64_t)sec*UINT64_C(10000000)+(uint64_t)(nsec/100)+EPOCH_DELTA;
    t.low=(uint32_t)v;t.high=(uint32_t)(v>>32);return t;
}
static uint32_t attributes_of(const struct stat *st){
    if(S_ISDIR(st->st_mode)){
        uint32_t a=MB_WIN_FILE_ATTRIBUTE_DIRECTORY;
        if(!(st->st_mode&S_IWUSR))a|=MB_WIN_FILE_ATTRIBUTE_READONLY;
        return a;
    }
    if(!(st->st_mode&S_IWUSR))return MB_WIN_FILE_ATTRIBUTE_READONLY;
    return MB_WIN_FILE_ATTRIBUTE_NORMAL;
}
static void fill_attribute_data(const struct stat *st,mb_win_w32_file_attribute_data *out){
    memset(out,0,sizeof(*out));
    out->attributes=attributes_of(st);
    out->creation=filetime(st->st_ctimespec.tv_sec,st->st_ctimespec.tv_nsec);
    out->last_access=filetime(st->st_atimespec.tv_sec,st->st_atimespec.tv_nsec);
    out->last_write=filetime(st->st_mtimespec.tv_sec,st->st_mtimespec.tv_nsec);
    out->size_high=(uint32_t)((uint64_t)st->st_size>>32);out->size_low=(uint32_t)st->st_size;
}
static void fill_find_data(const struct stat *st,const char *name,mb_win_w32_find_data *out){
    mb_win_w32_file_attribute_data base;
    fill_attribute_data(st,&base);
    memset(out,0,sizeof(*out));
    out->attributes=base.attributes;out->creation=base.creation;out->last_access=base.last_access;out->last_write=base.last_write;
    out->size_high=base.size_high;out->size_low=base.size_low;
    if(!utf8_to_utf16(name,out->name,260))out->name[0]=0;
}
/* ---------- handle table ---------- */
static void *token_of(entry *e,size_t index){return (void *)(uintptr_t)(((uint64_t)e->generation<<32)|(uint64_t)(index+1));}
static entry *lookup(state *s,void *handle){
    uint64_t token=(uint64_t)(uintptr_t)handle;
    if(!handle||token==UINT64_MAX)return NULL;
    uint64_t idx=(token&UINT64_C(0xFFFFFFFF))-1,gen=token>>32;
    if(idx>=s->capacity||!s->table[idx].kind||s->table[idx].generation!=(uint32_t)gen)return NULL;
    return &s->table[idx];
}
static void release_entry(state *s,entry *e){
    if(e->kind==KIND_FIND){if(e->dir)closedir(e->dir);}
    else if(e->fd>=0)close(e->fd);
    free(e->path);free(e->dir_host);free(e->pattern);
    e->path=e->dir_host=e->pattern=NULL;e->dir=NULL;e->fd=-1;e->offset=0;e->share=0;e->wants=0;
    e->generation++;if(!e->generation)e->generation=1;
    e->kind=0;--s->count;
}
static int shares_allow(state *s,const char *path,uint32_t desired){
    for(size_t i=0;i<s->capacity;++i){
        entry *e=&s->table[i];
        if(!e->kind||e->kind==KIND_FIND||!e->path||strcmp(e->path,path))continue;
        if((desired&MB_WIN_GENERIC_READ)&&!(e->share&MB_WIN_FILE_SHARE_READ))return 0;
        if((desired&MB_WIN_GENERIC_WRITE)&&!(e->share&MB_WIN_FILE_SHARE_WRITE))return 0;
    }
    return 1;
}
static int delete_allowed(state *s,const char *path){
    for(size_t i=0;i<s->capacity;++i){
        entry *e=&s->table[i];
        if(!e->kind||e->kind==KIND_FIND||!e->path||strcmp(e->path,path))continue;
        if(!(e->share&MB_WIN_FILE_SHARE_DELETE))return 0;
    }
    return 1;
}
/* ---------- lifecycle ---------- */
mb_win_fileio_status mb_win_fileio_open(mb_win_fileio *o,const mb_win_fileio_config *c,int *error){
    if(!o||!c||!c->domain||!c->root||!c->capacity||c->capacity>UINT32_C(0x7FFFFFFF))return MB_WIN_FILEIO_INVALID_ARGUMENT;
    if(o->state)return MB_WIN_FILEIO_BAD_STATE;
    struct stat st;
    if(lstat(c->root,&st)!=0||!S_ISDIR(st.st_mode)||S_ISLNK(st.st_mode))return MB_WIN_FILEIO_INVALID_ARGUMENT; /* containment: real directory root */
    state *s=calloc(1,sizeof(*s));
    if(!s){if(error)*error=ENOMEM;return MB_WIN_FILEIO_NATIVE_ERROR;}
    int e=pthread_mutex_init(&s->mutex,NULL);
    if(e){free(s);if(error)*error=e;return MB_WIN_FILEIO_NATIVE_ERROR;}
    if(mb_win_threads_acquire(c->domain,&s->domain_pin)!=MB_WIN_THREADS_OK){must(pthread_mutex_destroy(&s->mutex));free(s);return MB_WIN_FILEIO_BAD_STATE;}
    s->table=calloc(c->capacity,sizeof(*s->table));
    s->root=s->table?strdup(c->root):NULL;
    if(!s->table||!s->root){
        free(s->table);free(s->root);must(pthread_mutex_destroy(&s->mutex));free(s);
        if(error)*error=ENOMEM;return MB_WIN_FILEIO_NATIVE_ERROR;
    }
    for(size_t i=0;i<c->capacity;++i)s->table[i].fd=-1;
    s->domain=c->domain;s->capacity=c->capacity;o->state=s;if(error)*error=0;return MB_WIN_FILEIO_OK;
}
mb_win_fileio_status mb_win_fileio_close(mb_win_fileio *o,int *error){
    if(!o)return MB_WIN_FILEIO_INVALID_ARGUMENT;if(!o->state)return MB_WIN_FILEIO_BAD_STATE;
    state *s=o->state;must(pthread_mutex_lock(&s->mutex));
    if(s->pins||s->count){unlock(s);return MB_WIN_FILEIO_BUSY;}
    unlock(s);
    int e=pthread_mutex_destroy(&s->mutex);
    if(mb_win_threads_release_pin(&s->domain_pin)!=MB_WIN_THREADS_OK)abort();
    free(s->table);free(s->root);free(s);o->state=NULL;
    if(e){if(error)*error=e;return MB_WIN_FILEIO_NATIVE_ERROR;}
    if(error)*error=0;return MB_WIN_FILEIO_OK;
}
mb_win_fileio_status mb_win_fileio_acquire_pin(mb_win_fileio *o,mb_win_fileio_pin *p){
    if(!o||!p)return MB_WIN_FILEIO_INVALID_ARGUMENT;if(!o->state||p->owner||p->next)return MB_WIN_FILEIO_BAD_STATE;
    state *s=o->state;must(pthread_mutex_lock(&s->mutex));for(mb_win_fileio_pin *n=s->pins;n;n=n->next)if(n==p){unlock(s);return MB_WIN_FILEIO_BAD_STATE;}
    p->owner=o;p->next=s->pins;s->pins=p;unlock(s);return MB_WIN_FILEIO_OK;
}
mb_win_fileio_status mb_win_fileio_release_pin(mb_win_fileio_pin *p){
    if(!p)return MB_WIN_FILEIO_INVALID_ARGUMENT;if(!p->owner||!p->owner->state)return MB_WIN_FILEIO_BAD_STATE;
    state *s=p->owner->state;must(pthread_mutex_lock(&s->mutex));mb_win_fileio_pin **at=&s->pins;while(*at&&*at!=p)at=&(*at)->next;
    if(!*at){unlock(s);return MB_WIN_FILEIO_BAD_STATE;}*at=p->next;*p=(mb_win_fileio_pin){0};unlock(s);return MB_WIN_FILEIO_OK;
}
mb_win_thread_domain *mb_win_fileio_domain(mb_win_fileio *o){return o&&o->state?((state*)o->state)->domain:NULL;}
/* ---------- shared create core ---------- */
static void *insert_entry(state *s,unsigned kind,const char *path,uint32_t *win32_error,size_t *index){
    if(s->count==s->capacity){*win32_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return NULL;}
    for(size_t i=0;i<s->capacity;++i){
        entry *e=&s->table[i];
        if(e->kind)continue;
        e->kind=kind;e->fd=-1;e->offset=0;e->share=0;e->wants=0;e->dir=NULL;e->dir_host=NULL;e->pattern=NULL;
        e->path=path?strdup(path):NULL;
        if(path&&!e->path){e->kind=0;*win32_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return NULL;}
        if(!e->generation)e->generation=1;
        ++s->count;*index=i;return token_of(e,i);
    }
    *win32_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return NULL;
}
static void *create_common(mb_win_fileio *o,const uint16_t *name,uint32_t access,uint32_t share,const void *security_attributes,uint32_t disposition,uint32_t flags){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le,fail=0,quirk=0;
    void *token=MB_WIN_INVALID_HANDLE_VALUE;
    do{
        if(security_attributes){fail=MB_WIN_ERROR_INVALID_PARAMETER;break;}          /* policy: refuse instead of fake */
        if(disposition<MB_WIN_CREATE_NEW||disposition>MB_WIN_TRUNCATE_EXISTING){fail=MB_WIN_ERROR_INVALID_PARAMETER;break;}
        if(access&~(MB_WIN_GENERIC_READ|MB_WIN_GENERIC_WRITE)){fail=MB_WIN_ERROR_INVALID_PARAMETER;break;} /* policy */
        if(flags&MB_WIN_FILE_FLAG_OVERLAPPED){fail=MB_WIN_ERROR_NOT_SUPPORTED;break;} /* policy: overlapped out of scope */
        if(flags&~(UINT32_C(0xFFFF)|MB_WIN_FILE_FLAG_BACKUP_SEMANTICS)){fail=MB_WIN_ERROR_INVALID_PARAMETER;break;} /* policy */
        int we=0;size_t comps=0;
        char *list=normalize(name,&we,&comps);
        if(!list){fail=(uint32_t)we;break;}
        char *canon=canonical(list,comps);
        if(!canon){free(list);fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;break;}
        char *leaf_host=NULL,*host_dir=NULL,*leaf=NULL;struct stat st;
        int allow=(disposition==MB_WIN_CREATE_NEW||disposition==MB_WIN_CREATE_ALWAYS||disposition==MB_WIN_OPEN_ALWAYS);
        res r=resolve(s,list,comps,allow,&leaf_host,&host_dir,&leaf,&st);
        if(r==RES_SYMLINK){fail=MB_WIN_ERROR_ACCESS_DENIED;free(canon);free(list);break;}        /* containment policy */
        if(r==RES_FAULT){fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;free(canon);free(list);break;}
        if(r==RES_PATH_MISSING){fail=MB_WIN_ERROR_PATH_NOT_FOUND;free(canon);free(list);break;}  /* documented */
        if(r==RES_LEAF_MISSING){fail=MB_WIN_ERROR_FILE_NOT_FOUND;free(canon);free(list);break;}  /* documented */
        if(leaf){
            char *host=join_host(host_dir,leaf);
            free(leaf);
            if(!host){fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;}
            else{
                if(!shares_allow(s,canon,access)){fail=MB_WIN_ERROR_SHARING_VIOLATION;}          /* documented */
                else{
                    int fl=(access&MB_WIN_GENERIC_WRITE)?((access&MB_WIN_GENERIC_READ)?O_RDWR:O_WRONLY):O_RDONLY;
                    fl|=O_CREAT;
                    if(disposition==MB_WIN_CREATE_NEW)fl|=O_EXCL;
                    int fd=open(host,fl,0666);
                    if(fd<0)fail=(errno==EACCES)?MB_WIN_ERROR_ACCESS_DENIED:MB_WIN_ERROR_GEN_FAILURE; /* policy mapping */
                    else{
                        size_t index=0;uint32_t ie=0;
                        token=insert_entry(s,KIND_FILE,canon,&ie,&index);
                        if(token){s->table[index].fd=fd;s->table[index].share=share;s->table[index].wants=access;}
                        else{fail=ie;close(fd);}
                    }
                }
                free(host);
            }
            free(host_dir);free(canon);free(list);break;
        }
        /* Leaf exists. */
        if(S_ISDIR(st.st_mode)){
            if(!(flags&MB_WIN_FILE_FLAG_BACKUP_SEMANTICS))fail=MB_WIN_ERROR_ACCESS_DENIED;      /* documented */
            else if(disposition==MB_WIN_CREATE_NEW)fail=MB_WIN_ERROR_ALREADY_EXISTS;             /* documented ERROR_FILE_EXISTS */
            else if(disposition==MB_WIN_CREATE_ALWAYS||disposition==MB_WIN_TRUNCATE_EXISTING)fail=MB_WIN_ERROR_ACCESS_DENIED; /* policy */
            else if(!shares_allow(s,canon,access))fail=MB_WIN_ERROR_SHARING_VIOLATION;           /* documented */
            else{
                int fd=open(leaf_host,O_RDONLY|O_DIRECTORY);
                if(fd<0)fail=MB_WIN_ERROR_ACCESS_DENIED;
                else{
                    size_t index=0;uint32_t ie=0;
                    token=insert_entry(s,KIND_DIR,canon,&ie,&index);
                    if(token){s->table[index].fd=fd;s->table[index].share=share;s->table[index].wants=access;}
                    else{fail=ie;close(fd);}
                }
            }
        }else{
            if(disposition==MB_WIN_CREATE_NEW)fail=MB_WIN_ERROR_ALREADY_EXISTS;                  /* documented ERROR_FILE_EXISTS */
            else if(!(access&MB_WIN_GENERIC_WRITE)&&(disposition==MB_WIN_CREATE_ALWAYS||disposition==MB_WIN_TRUNCATE_EXISTING))fail=MB_WIN_ERROR_ACCESS_DENIED; /* documented */
            else if((access&MB_WIN_GENERIC_WRITE)&&!(st.st_mode&S_IWUSR))fail=MB_WIN_ERROR_ACCESS_DENIED; /* documented: readonly */
            else if(!shares_allow(s,canon,access))fail=MB_WIN_ERROR_SHARING_VIOLATION;            /* documented */
            else{
                int fl=(access&MB_WIN_GENERIC_WRITE)?((access&MB_WIN_GENERIC_READ)?O_RDWR:O_WRONLY):O_RDONLY;
                if(disposition==MB_WIN_CREATE_ALWAYS||disposition==MB_WIN_TRUNCATE_EXISTING)fl|=O_TRUNC;
                int fd=open(leaf_host,fl);
                if(fd<0)fail=MB_WIN_ERROR_ACCESS_DENIED;
                else{
                    if(disposition==MB_WIN_CREATE_ALWAYS||disposition==MB_WIN_OPEN_ALWAYS)quirk=MB_WIN_ERROR_ALREADY_EXISTS; /* documented success-path last error */
                    size_t index=0;uint32_t ie=0;
                    token=insert_entry(s,KIND_FILE,canon,&ie,&index);
                    if(token){s->table[index].fd=fd;s->table[index].share=share;s->table[index].wants=access;}
                    else{fail=ie;close(fd);}
                }
            }
        }
        free(leaf_host);free(canon);free(list);
    }while(0);
    unlock(s);
    *le=fail?fail:(quirk?quirk:saved);
    return token;
}
void *mb_win_fileio_create_file_w(mb_win_fileio *o,const uint16_t *file_name,uint32_t desired_access,uint32_t share_mode,const void *security_attributes,uint32_t disposition,uint32_t flags_and_attributes,const void *template_file){
    if(template_file){ /* documented: template supplies extended attrs; refusing instead of faking */
        state *s=enter(o);uint32_t *le=errs(s);*le=MB_WIN_ERROR_INVALID_PARAMETER;unlock(s);
        return MB_WIN_INVALID_HANDLE_VALUE;
    }
    return create_common(o,file_name,desired_access,share_mode,security_attributes,disposition,flags_and_attributes);
}
void *mb_win_fileio_create_file_2(mb_win_fileio *o,const uint16_t *file_name,uint32_t desired_access,uint32_t share_mode,uint32_t disposition,const mb_win_createfile2_extended *p){
    state *s=enter(o);
    uint32_t *le=errs(s);
    if(!p||p->size!=sizeof(mb_win_createfile2_extended)||p->security_attributes||p->template_file){
        *le=MB_WIN_ERROR_INVALID_PARAMETER;unlock(s);return MB_WIN_INVALID_HANDLE_VALUE; /* policy */
    }
    unlock(s);
    return create_common(o,file_name,desired_access,share_mode,NULL,disposition,p->file_flags|p->file_attributes);
}
/* ---------- read/write (synchronous, per-handle offsets) ---------- */
int32_t mb_win_fileio_read_file(mb_win_fileio *o,void *handle,void *buffer,uint32_t bytes_to_read,uint32_t *bytes_read,const void *overlapped){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le,fail=0;
    entry *e=NULL;
    size_t done=0;
    int ok=0;
    if(overlapped)fail=MB_WIN_ERROR_NOT_SUPPORTED; /* policy: overlapped out of scope */
    else if(bytes_to_read>MAX_TRANSFER)fail=MB_WIN_ERROR_INVALID_PARAMETER; /* policy bound */
    else if(bytes_to_read&&(!buffer||!bytes_read))fail=MB_WIN_ERROR_INVALID_PARAMETER; /* policy guard */
    else if(!(e=lookup(s,handle))||e->kind==KIND_FIND)fail=MB_WIN_ERROR_INVALID_HANDLE; /* documented */
    else if(e->kind==KIND_DIR)fail=MB_WIN_ERROR_ACCESS_DENIED; /* documented */
    else if(!(e->wants&MB_WIN_GENERIC_READ))fail=MB_WIN_ERROR_ACCESS_DENIED; /* documented */
    else{
        while(done<bytes_to_read){
            ssize_t r=pread(e->fd,(char *)buffer+done,bytes_to_read-done,(off_t)(e->offset+done));
            if(r<0){if(errno==EINTR)continue;fail=MB_WIN_ERROR_GEN_FAILURE;break;} /* policy mapping */
            if(r==0)break;
            done+=(size_t)r;
        }
        if(!fail){e->offset+=done;if(bytes_read)*bytes_read=(uint32_t)done;ok=1;}
    }
    unlock(s);
    *le=ok?saved:fail;
    return ok;
}
int32_t mb_win_fileio_write_file(mb_win_fileio *o,void *handle,const void *buffer,uint32_t bytes_to_write,uint32_t *bytes_written,const void *overlapped){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le,fail=0;
    entry *e=NULL;
    size_t done=0;
    int ok=0;
    if(overlapped)fail=MB_WIN_ERROR_NOT_SUPPORTED; /* policy */
    else if(bytes_to_write>MAX_TRANSFER)fail=MB_WIN_ERROR_INVALID_PARAMETER; /* policy bound */
    else if(bytes_to_write&&(!buffer||!bytes_written))fail=MB_WIN_ERROR_INVALID_PARAMETER; /* policy guard */
    else if(!(e=lookup(s,handle))||e->kind==KIND_FIND)fail=MB_WIN_ERROR_INVALID_HANDLE; /* documented */
    else if(e->kind==KIND_DIR)fail=MB_WIN_ERROR_ACCESS_DENIED; /* documented */
    else if(!(e->wants&MB_WIN_GENERIC_WRITE))fail=MB_WIN_ERROR_ACCESS_DENIED; /* documented */
    else{
        while(done<bytes_to_write){
            ssize_t r=pwrite(e->fd,(const char *)buffer+done,bytes_to_write-done,(off_t)(e->offset+done));
            if(r<0){if(errno==EINTR)continue;fail=(errno==ENOSPC)?112u:MB_WIN_ERROR_GEN_FAILURE;break;} /* policy mapping */
            if(r==0){fail=MB_WIN_ERROR_GEN_FAILURE;break;}
            done+=(size_t)r;
        }
        if(!fail){e->offset+=done;if(bytes_written)*bytes_written=(uint32_t)done;ok=1;}
    }
    unlock(s);
    *le=ok?saved:fail;
    return ok;
}
/* ---------- enumeration ---------- */
static int wildcard_component(const char *c){for(;*c;++c)if(*c=='*'||*c=='?')return 1;return 0;}
static int match_pattern(const char *pattern,const char *name){
    /* case-insensitive '*'/'?' glob against long names; the 8.3 short-name
     * emulation ('*.*' matching extensionless names) is NOT modeled. */
    if(!*pattern&&!*name)return 1;
    if(*pattern=='*'){
        for(const char *n=name;;++n){
            if(match_pattern(pattern+1,n))return 1;
            if(!*n)return 0;
        }
    }
    if(!*pattern||!*name)return 0;
    if(*pattern=='?')return match_pattern(pattern+1,name+1);
    if(fold((unsigned char)*pattern)!=fold((unsigned char)*name))return 0;
    return match_pattern(pattern+1,name+1);
}
static int fill_dir_entry(mb_win_w32_find_data *out,const char *dir_host,const char *name){
    char *host=join_host(dir_host,name);
    if(!host)return -1;
    struct stat st;
    if(lstat(host,&st)!=0||S_ISLNK(st.st_mode)){free(host);return 0;} /* containment: symlinks never reported */
    fill_find_data(&st,name,out);
    free(host);
    return 1;
}
static void *find_open(mb_win_fileio *o,const uint16_t *pattern,mb_win_w32_find_data *find_data){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le,fail=0;
    void *token=MB_WIN_INVALID_HANDLE_VALUE;
    int we=0;size_t comps=0;
    char *list=normalize(pattern,&we,&comps);
    mb_win_w32_find_data probe;
    memset(&probe,0,sizeof(probe));
    if(!list)fail=(uint32_t)we;
    else{
        for(size_t i=0;i+1<comps;++i)
            if(wildcard_component(component_at(list,comps,i))){fail=MB_WIN_ERROR_INVALID_PARAMETER;break;} /* policy */
        if(!fail){
            char *last=component_at(list,comps,comps-1);
            if(!wildcard_component(last)){
                /* exact-name form: resolve the whole path */
                char *leaf_host=NULL,*host_dir=NULL,*leaf=NULL;struct stat st;
                res r=resolve(s,list,comps,0,&leaf_host,&host_dir,&leaf,&st);
                if(r==RES_SYMLINK)fail=MB_WIN_ERROR_ACCESS_DENIED;
                else if(r==RES_FAULT)fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;
                else if(r==RES_PATH_MISSING)fail=MB_WIN_ERROR_PATH_NOT_FOUND;
                else if(r==RES_LEAF_MISSING)fail=MB_WIN_ERROR_FILE_NOT_FOUND;
                else{
                    const char *base=strrchr(leaf_host,'/');
                    fill_find_data(&st,base?base+1:leaf_host,&probe);
                    free(leaf_host);
                    char *canon=canonical(list,comps-1);
                    if(!canon)fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;
                    else{
                        size_t index=0;uint32_t ie=0;
                        token=insert_entry(s,KIND_FIND,canon,&ie,&index);
                        if(token){s->table[index].pattern=strdup(last);if(!s->table[index].pattern){release_entry(s,&s->table[index]);token=MB_WIN_INVALID_HANDLE_VALUE;fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;}}
                        else fail=ie;
                        free(canon);
                    }
                }
                free(host_dir);free(leaf);
            }else{
                /* enumerate the parent directory */
                char *dir_host=NULL,*leaf_host=NULL,*leaf=NULL;struct stat st;
                res r=comps==1?((dir_host=strdup(s->root))?RES_OK:RES_FAULT)
                    :resolve(s,list,comps-1,0,&leaf_host,&dir_host,&leaf,&st);
                (void)leaf;
                if(r==RES_SYMLINK)fail=MB_WIN_ERROR_ACCESS_DENIED;
                else if(r==RES_PATH_MISSING||r==RES_LEAF_MISSING)fail=MB_WIN_ERROR_PATH_NOT_FOUND;
                else if(r==RES_FAULT||!dir_host)fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;
                else{
                    DIR *d=opendir(dir_host);
                    if(!d)fail=MB_WIN_ERROR_ACCESS_DENIED;
                    else{
                        struct dirent *de;int filled=0;
                        while(!filled&&(de=readdir(d))){
                            if(!match_pattern(last,de->d_name))continue;
                            int rc=fill_dir_entry(&probe,dir_host,de->d_name);
                            if(rc<0){fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;break;}
                            filled=rc;
                        }
                        if(!fail&&!filled)fail=MB_WIN_ERROR_FILE_NOT_FOUND; /* documented */
                        if(!fail){
                            char *canon=canonical(list,comps-1);
                            if(!canon)fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;
                            else{
                                size_t index=0;uint32_t ie=0;
                                token=insert_entry(s,KIND_FIND,canon,&ie,&index);
                                if(token){
                                    entry *e=&s->table[index];
                                    e->dir=d;e->dir_host=dir_host;e->pattern=strdup(last);
                                    dir_host=NULL;
                                    if(!e->pattern){release_entry(s,e);token=MB_WIN_INVALID_HANDLE_VALUE;fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;}
                                }else fail=ie;
                                free(canon);
                            }
                        }
                        if(!token&&d)closedir(d);
                    }
                }
                free(dir_host);free(leaf_host);free(leaf);
            }
        }
    }
    free(list);
    unlock(s);
    *le=fail?fail:saved;
    if(!fail&&find_data)*find_data=probe;
    return token;
}
void *mb_win_fileio_find_first_file_w(mb_win_fileio *o,const uint16_t *file_pattern,mb_win_w32_find_data *find_data){
    if(!find_data){ /* policy guard: Windows would fault; we fail fast */
        state *s=enter(o);uint32_t *le=errs(s);*le=MB_WIN_ERROR_INVALID_PARAMETER;unlock(s);
        return MB_WIN_INVALID_HANDLE_VALUE;
    }
    return find_open(o,file_pattern,find_data);
}
void *mb_win_fileio_find_first_file_ex_w(mb_win_fileio *o,const uint16_t *file_pattern,int32_t info_level,void *find_data,int32_t search_op,const void *search_filter,uint32_t additional_flags){
    state *s=enter(o);
    uint32_t *le=errs(s);
    if((info_level!=0&&info_level!=1)||search_op!=0||search_filter||additional_flags){
        *le=MB_WIN_ERROR_INVALID_PARAMETER;unlock(s);return MB_WIN_INVALID_HANDLE_VALUE; /* policy: unsupported forms fail fast */
    }
    unlock(s);
    return mb_win_fileio_find_first_file_w(o,file_pattern,find_data);
}
int32_t mb_win_fileio_find_next_file_w(mb_win_fileio *o,void *handle,mb_win_w32_find_data *find_data){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le,fail=0;
    entry *e=lookup(s,handle);
    if(!e||e->kind!=KIND_FIND)fail=MB_WIN_ERROR_INVALID_HANDLE; /* documented */
    else if(!find_data)fail=MB_WIN_ERROR_INVALID_PARAMETER; /* policy guard */
    else if(!e->dir){ /* exact-name form: a single shot was already returned */
        fail=MB_WIN_ERROR_NO_MORE_FILES; /* documented */
    }else{
        struct dirent *de;int filled=0;
        while(!filled&&(de=readdir(e->dir))){
            if(!match_pattern(e->pattern?e->pattern:"",de->d_name))continue;
            int rc=fill_dir_entry(find_data,e->dir_host,de->d_name);
            if(rc<0){fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;break;}
            filled=rc;
        }
        if(!fail&&!filled)fail=MB_WIN_ERROR_NO_MORE_FILES; /* documented */
    }
    unlock(s);
    *le=fail?fail:saved;
    return fail?0:1;
}
int32_t mb_win_fileio_find_close(mb_win_fileio *o,void *handle){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le;
    entry *e=lookup(s,handle);
    if(!e||e->kind!=KIND_FIND){unlock(s);*le=MB_WIN_ERROR_INVALID_HANDLE;return 0;} /* documented */
    release_entry(s,e);
    unlock(s);
    *le=saved;
    return 1;
}
int32_t mb_win_fileio_are_file_apis_ansi(mb_win_fileio *o){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le;
    unlock(s);
    *le=saved;
    return 1; /* policy: fixed TRUE, the common ANSI code page state */
}
/* ---------- metadata ---------- */
int32_t mb_win_fileio_get_file_attributes_ex_w(mb_win_fileio *o,const uint16_t *file_name,int32_t information_level,void *information){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le,fail=0;
    int we=0;size_t comps=0;
    char *list=NULL;
    if(information_level!=0||!information)fail=MB_WIN_ERROR_INVALID_PARAMETER; /* documented level check; policy guard */
    else if(!(list=normalize(file_name,&we,&comps)))fail=(uint32_t)we;
    else{
        char *leaf_host=NULL,*host_dir=NULL,*leaf=NULL;struct stat st;
        res r=resolve(s,list,comps,0,&leaf_host,&host_dir,&leaf,&st);
        if(r==RES_SYMLINK)fail=MB_WIN_ERROR_ACCESS_DENIED; /* containment policy */
        else if(r==RES_FAULT)fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;
        else if(r==RES_PATH_MISSING)fail=MB_WIN_ERROR_PATH_NOT_FOUND; /* documented */
        else if(r==RES_LEAF_MISSING)fail=MB_WIN_ERROR_FILE_NOT_FOUND; /* documented */
        else fill_attribute_data(&st,information);
        free(leaf_host);free(host_dir);free(leaf);
        free(list);
    }
    unlock(s);
    *le=fail?fail:saved;
    return fail?0:1;
}
int32_t mb_win_fileio_get_file_information_by_handle(mb_win_fileio *o,void *handle,mb_win_by_handle_file_information *information){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le,fail=0;
    entry *e=lookup(s,handle);
    struct stat st;
    if(!e||e->kind==KIND_FIND)fail=MB_WIN_ERROR_INVALID_HANDLE; /* documented */
    else if(!information)fail=MB_WIN_ERROR_INVALID_PARAMETER; /* policy guard */
    else if(fstat(e->fd,&st)!=0)fail=MB_WIN_ERROR_ACCESS_DENIED; /* policy mapping */
    else{
        memset(information,0,sizeof(*information));
        information->attributes=attributes_of(&st);
        information->creation=filetime(st.st_ctimespec.tv_sec,st.st_ctimespec.tv_nsec);
        information->last_access=filetime(st.st_atimespec.tv_sec,st.st_atimespec.tv_nsec);
        information->last_write=filetime(st.st_mtimespec.tv_sec,st.st_mtimespec.tv_nsec);
        information->volume_serial=(uint32_t)st.st_dev;  /* policy: opaque */
        information->size_high=(uint32_t)((uint64_t)st.st_size>>32);information->size_low=(uint32_t)st.st_size;
        information->links=(uint32_t)st.st_nlink;
        information->index_high=(uint32_t)((uint64_t)st.st_ino>>32);information->index_low=(uint32_t)st.st_ino; /* policy: opaque */
    }
    unlock(s);
    *le=fail?fail:saved;
    return fail?0:1;
}
int32_t mb_win_fileio_get_file_information_by_handle_ex(mb_win_fileio *o,void *handle,int32_t information_class,void *information,uint32_t size){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le,fail=0;
    entry *e=lookup(s,handle);
    struct stat st;
    if(!e||e->kind==KIND_FIND)fail=MB_WIN_ERROR_INVALID_HANDLE; /* documented */
    else if(!information)fail=MB_WIN_ERROR_INVALID_PARAMETER; /* policy guard */
    else if(fstat(e->fd,&st)!=0)fail=MB_WIN_ERROR_ACCESS_DENIED; /* policy mapping */
    else if(information_class==0){ /* FileBasicInfo */
        if(size<sizeof(mb_win_file_basic_info))fail=MB_WIN_ERROR_INSUFFICIENT_BUFFER; /* policy */
        else{
            mb_win_file_basic_info *out=information;
            mb_win_filetime c=filetime(st.st_ctimespec.tv_sec,st.st_ctimespec.tv_nsec);
            mb_win_filetime a=filetime(st.st_atimespec.tv_sec,st.st_atimespec.tv_nsec);
            mb_win_filetime w=filetime(st.st_mtimespec.tv_sec,st.st_mtimespec.tv_nsec);
            memset(out,0,sizeof(*out));
            out->creation=(uint64_t)c.low|((uint64_t)c.high<<32);
            out->last_access=(uint64_t)a.low|((uint64_t)a.high<<32);
            out->last_write=(uint64_t)w.low|((uint64_t)w.high<<32);
            out->change=out->last_write; /* policy: host ctime mapped to creation; change mirrors write */
            out->attributes=attributes_of(&st);
        }
    }else if(information_class==1){ /* FileStandardInfo */
        if(size<sizeof(mb_win_file_standard_info))fail=MB_WIN_ERROR_INSUFFICIENT_BUFFER; /* policy */
        else{
            mb_win_file_standard_info *out=information;
            memset(out,0,sizeof(*out));
            out->allocation_size=((uint64_t)st.st_size+4095u)&~(uint64_t)4095u; /* policy */
            out->end_of_file=(uint64_t)st.st_size;
            out->links=(uint32_t)st.st_nlink;
            out->delete_pending=0; /* policy */
            out->directory=S_ISDIR(st.st_mode)?1:0;
        }
    }else fail=MB_WIN_ERROR_INVALID_PARAMETER; /* policy: unimplemented classes fail fast */
    unlock(s);
    *le=fail?fail:saved;
    return fail?0:1;
}
/* ---------- directory and delete ---------- */
int32_t mb_win_fileio_create_directory_w(mb_win_fileio *o,const uint16_t *path_name,const void *security_attributes){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le,fail=0;
    int we=0;size_t comps=0;
    char *list=security_attributes?NULL:normalize(path_name,&we,&comps);
    if(security_attributes)fail=MB_WIN_ERROR_INVALID_PARAMETER; /* policy: refuse instead of fake */
    else if(!list)fail=(uint32_t)we;
    else{
        char *leaf_host=NULL,*host_dir=NULL,*leaf=NULL;struct stat st;
        res r=resolve(s,list,comps,1,&leaf_host,&host_dir,&leaf,&st);
        if(r==RES_SYMLINK)fail=MB_WIN_ERROR_ACCESS_DENIED; /* containment policy */
        else if(r==RES_FAULT)fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;
        else if(r==RES_PATH_MISSING)fail=MB_WIN_ERROR_PATH_NOT_FOUND; /* documented */
        else if(leaf_host)fail=MB_WIN_ERROR_ALREADY_EXISTS; /* documented */
        else{
            char *host=join_host(host_dir,leaf);
            if(!host)fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;
            else{
                if(mkdir(host,0755)!=0)fail=(errno==EEXIST)?MB_WIN_ERROR_ALREADY_EXISTS:((errno==ENOENT||errno==ENOTDIR)?MB_WIN_ERROR_PATH_NOT_FOUND:MB_WIN_ERROR_GEN_FAILURE); /* policy mapping */
                free(host);
            }
        }
        free(leaf_host);free(host_dir);free(leaf);
        free(list);
    }
    unlock(s);
    *le=fail?fail:saved;
    return fail?0:1;
}
int32_t mb_win_fileio_delete_file_w(mb_win_fileio *o,const uint16_t *file_name){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le,fail=0;
    int we=0;size_t comps=0;
    char *list=normalize(file_name,&we,&comps);
    if(!list)fail=(uint32_t)we;
    else{
        char *canon=canonical(list,comps);
        char *leaf_host=NULL,*host_dir=NULL,*leaf=NULL;struct stat st;
        res r=canon?resolve(s,list,comps,0,&leaf_host,&host_dir,&leaf,&st):RES_FAULT;
        if(r==RES_SYMLINK)fail=MB_WIN_ERROR_ACCESS_DENIED; /* containment policy */
        else if(r==RES_FAULT||!canon)fail=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;
        else if(r==RES_PATH_MISSING)fail=MB_WIN_ERROR_PATH_NOT_FOUND; /* documented */
        else if(r==RES_LEAF_MISSING)fail=MB_WIN_ERROR_FILE_NOT_FOUND; /* documented */
        else if(S_ISDIR(st.st_mode))fail=MB_WIN_ERROR_ACCESS_DENIED; /* documented */
        else if(!(st.st_mode&S_IWUSR))fail=MB_WIN_ERROR_ACCESS_DENIED; /* documented: readonly */
        else if(!delete_allowed(s,canon))fail=MB_WIN_ERROR_SHARING_VIOLATION; /* documented */
        else if(unlink(leaf_host)!=0)fail=MB_WIN_ERROR_GEN_FAILURE; /* policy mapping */
        free(canon);free(leaf_host);free(host_dir);free(leaf);
        free(list);
    }
    unlock(s);
    *le=fail?fail:saved;
    return fail?0:1;
}
/* ---------- current directory and final path ---------- */
uint32_t mb_win_fileio_get_current_directory_w(mb_win_fileio *o,uint32_t buffer_length,uint16_t *buffer){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le;
    unlock(s);
    /* Documented return semantics; the value '\' is the fixed virtual root
     * (SetCurrentDirectory is not census-observed, so it never changes). */
    if(!buffer||buffer_length<2){*le=saved;return 2;}
    buffer[0]='\\';buffer[1]=0;
    *le=saved;
    return 1;
}
uint32_t mb_win_fileio_get_final_path_name_by_handle_w(mb_win_fileio *o,void *handle,uint16_t *buffer,uint32_t buffer_length,uint32_t flags){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le;
    entry *e=lookup(s,handle);
    if(!e||e->kind==KIND_FIND){unlock(s);*le=MB_WIN_ERROR_INVALID_HANDLE;return 0;} /* documented */
    if(flags&~UINT32_C(8)){unlock(s);*le=MB_WIN_ERROR_INVALID_PARAMETER;return 0;} /* policy: DOS name, normalized/opened only */
    /* Policy format: \\?\vfs\ + canonical virtual path with '\' separators. */
    size_t n=e->path?strlen(e->path):0;
    char *ascii=malloc(8+n+1);
    if(!ascii){unlock(s);*le=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return 0;}
    memcpy(ascii,"\\\\?\\vfs\\",8);
    memcpy(ascii+8,e->path,n+1);
    for(size_t i=8;i<8+n;++i)if(ascii[i]=='/')ascii[i]='\\';
    uint16_t scratch[8+MAX_PATH_UNITS];
    size_t units=utf8_to_utf16(ascii,scratch,sizeof(scratch)/sizeof(scratch[0]));
    free(ascii);
    if(!units){unlock(s);*le=MB_WIN_ERROR_GEN_FAILURE;return 0;} /* policy: canonical paths are convertible */
    if(!buffer||buffer_length<(uint32_t)units+1u){unlock(s);*le=saved;return (uint32_t)units+1u;} /* documented too-small return */
    for(size_t i=0;i<=units;++i)buffer[i]=scratch[i];
    unlock(s);
    *le=saved;
    return (uint32_t)units;
}
/* ---------- census-observed but explicitly unsupported (fail fast) ---------- */
int32_t mb_win_fileio_set_file_information_by_handle(mb_win_fileio *o,void *handle,int32_t information_class,const void *information,uint32_t size){
    (void)handle;(void)information_class;(void)information;(void)size;
    state *s=enter(o);uint32_t *le=errs(s);*le=MB_WIN_ERROR_CALL_NOT_IMPLEMENTED;unlock(s);
    return 0;
}
void *mb_win_fileio_find_first_change_notification_w(mb_win_fileio *o,const uint16_t *path_name,int32_t watch_subtree,uint32_t notify_filter){
    (void)path_name;(void)watch_subtree;(void)notify_filter;
    state *s=enter(o);uint32_t *le=errs(s);*le=MB_WIN_ERROR_CALL_NOT_IMPLEMENTED;unlock(s);
    return MB_WIN_INVALID_HANDLE_VALUE;
}
int32_t mb_win_fileio_find_next_change_notification(mb_win_fileio *o,void *handle){
    (void)handle;
    state *s=enter(o);uint32_t *le=errs(s);*le=MB_WIN_ERROR_CALL_NOT_IMPLEMENTED;unlock(s);
    return 0;
}
int32_t mb_win_fileio_find_close_change_notification(mb_win_fileio *o,void *handle){
    (void)handle;
    state *s=enter(o);uint32_t *le=errs(s);*le=MB_WIN_ERROR_CALL_NOT_IMPLEMENTED;unlock(s);
    return 0;
}
/* ---------- close service for the separate CloseHandle owner ---------- */
int32_t mb_win_fileio_close_handle(mb_win_fileio *o,void *handle){
    state *s=enter(o);
    uint32_t *le=errs(s),saved=*le;
    entry *e=lookup(s,handle);
    if(!e||e->kind==KIND_FIND){unlock(s);*le=MB_WIN_ERROR_INVALID_HANDLE;return 0;} /* find handles are FindClose-only, as on Windows */
    release_entry(s,e);
    unlock(s);
    *le=saved;
    return 1;
}
