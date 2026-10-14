/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_init_once_abi.c; that original file is unchanged). */
#include "mb_win_fileio_abi.h"
#if !defined(__x86_64__)
#error "File I/O Windows ABI requires AMD64"
#endif
#define WIN64 __attribute__((ms_abi))
_Static_assert(sizeof(void *)==8&&sizeof(uint32_t)==4,"Windows x64 scalar widths");
_Static_assert(sizeof(mb_win_filetime)==8&&_Alignof(mb_win_filetime)==4,"FILETIME Windows layout");
_Static_assert(sizeof(mb_win_w32_find_data)==592&&_Alignof(mb_win_w32_find_data)==4,"WIN32_FIND_DATAW Windows layout");
static mb_win_fileio *installed;
static mb_win_fileio_session *sessions;
static mb_win_fileio_session **find(mb_win_fileio_session *s){mb_win_fileio_session **p=&sessions;while(*p&&*p!=s)p=&(*p)->next;return p;}
mb_win_fileio_status mb_win_fileio_begin_session(mb_win_fileio_session *s,mb_win_fileio *o,mb_pe_module *m){
    if(!s||!o||!m)return MB_WIN_FILEIO_INVALID_ARGUMENT;
    if(s->active||s->pin.owner||s->pin.next||s->module||s->next||m->state!=MB_PE_MODULE_EMPTY)return MB_WIN_FILEIO_BAD_STATE;
    for(mb_win_fileio_session *p=sessions;p;p=p->next)if(p==s||p->module==m)return MB_WIN_FILEIO_BAD_STATE;
    if(sessions&&installed!=o)return MB_WIN_FILEIO_BUSY;
    mb_win_fileio_status status=mb_win_fileio_acquire_pin(o,&s->pin);if(status)return status;
    if(!sessions)installed=o;s->module=m;s->next=sessions;s->active=1;sessions=s;return MB_WIN_FILEIO_OK;
}
mb_win_fileio_status mb_win_fileio_end_session(mb_win_fileio_session *s){
    if(!s)return MB_WIN_FILEIO_INVALID_ARGUMENT;mb_win_fileio_session **p=find(s);
    if(!*p||!s->active)return MB_WIN_FILEIO_BAD_STATE;
    if(s->module->state!=MB_PE_MODULE_EMPTY)return MB_WIN_FILEIO_BUSY;
    mb_win_fileio_status status=mb_win_fileio_release_pin(&s->pin);if(status)return status;
    *p=s->next;*s=(mb_win_fileio_session){0};if(!sessions)installed=NULL;return MB_WIN_FILEIO_OK;
}
/* Documented Microsoft x64 signatures; every thunk keeps win64cc. */
static WIN64 void *create_file_w(const uint16_t *name,uint32_t access,uint32_t share,const void *security,uint32_t disposition,uint32_t flags,const void *template_){return mb_win_fileio_create_file_w(installed,name,access,share,security,disposition,flags,template_);}
static WIN64 void *create_file_2(const uint16_t *name,uint32_t access,uint32_t share,uint32_t disposition,const mb_win_createfile2_extended *parameters){return mb_win_fileio_create_file_2(installed,name,access,share,disposition,parameters);}
static WIN64 int32_t read_file(void *handle,void *buffer,uint32_t to_read,uint32_t *read_,const void *overlapped){return mb_win_fileio_read_file(installed,handle,buffer,to_read,read_,overlapped);}
static WIN64 int32_t write_file(void *handle,const void *buffer,uint32_t to_write,uint32_t *written,const void *overlapped){return mb_win_fileio_write_file(installed,handle,buffer,to_write,written,overlapped);}
static WIN64 void *find_first_file_w(const uint16_t *pattern,mb_win_w32_find_data *data){return mb_win_fileio_find_first_file_w(installed,pattern,data);}
static WIN64 void *find_first_file_ex_w(const uint16_t *pattern,int32_t level,void *data,int32_t search_op,const void *filter,uint32_t flags){return mb_win_fileio_find_first_file_ex_w(installed,pattern,level,data,search_op,filter,flags);}
static WIN64 int32_t find_next_file_w(void *handle,mb_win_w32_find_data *data){return mb_win_fileio_find_next_file_w(installed,handle,data);}
static WIN64 int32_t find_close(void *handle){return mb_win_fileio_find_close(installed,handle);}
static WIN64 int32_t are_file_apis_ansi(void){return mb_win_fileio_are_file_apis_ansi(installed);}
static WIN64 int32_t get_file_attributes_ex_w(const uint16_t *name,int32_t level,void *information){return mb_win_fileio_get_file_attributes_ex_w(installed,name,level,information);}
static WIN64 int32_t get_file_information_by_handle(void *handle,mb_win_by_handle_file_information *information){return mb_win_fileio_get_file_information_by_handle(installed,handle,information);}
static WIN64 int32_t get_file_information_by_handle_ex(void *handle,int32_t class_,void *information,uint32_t size){return mb_win_fileio_get_file_information_by_handle_ex(installed,handle,class_,information,size);}
static WIN64 int32_t create_directory_w(const uint16_t *path,const void *security){return mb_win_fileio_create_directory_w(installed,path,security);}
static WIN64 int32_t delete_file_w(const uint16_t *name){return mb_win_fileio_delete_file_w(installed,name);}
static WIN64 uint32_t get_current_directory_w(uint32_t length,uint16_t *buffer){return mb_win_fileio_get_current_directory_w(installed,length,buffer);}
static WIN64 uint32_t get_final_path_name_by_handle_w(void *handle,uint16_t *buffer,uint32_t length,uint32_t flags){return mb_win_fileio_get_final_path_name_by_handle_w(installed,handle,buffer,length,flags);}
static WIN64 int32_t set_file_information_by_handle(void *handle,int32_t class_,const void *information,uint32_t size){return mb_win_fileio_set_file_information_by_handle(installed,handle,class_,information,size);}
static WIN64 void *find_first_change_notification_w(const uint16_t *path,int32_t subtree,uint32_t filter){return mb_win_fileio_find_first_change_notification_w(installed,path,subtree,filter);}
static WIN64 int32_t find_next_change_notification(void *handle){return mb_win_fileio_find_next_change_notification(installed,handle);}
static WIN64 int32_t find_close_change_notification(void *handle){return mb_win_fileio_find_close_change_notification(installed,handle);}
static int match(mb_pe_string v,const char *s,size_t n,int fold){if(v.size!=n)return 0;for(size_t i=0;i<n;++i){unsigned char c=(unsigned char)v.data[i];if(fold&&c>='A'&&c<='Z')c=(unsigned char)(c+'a'-'A');if(c!=(unsigned char)s[i])return 0;}return 1;}
#define MATCH(v,s,f) match((v),(s),sizeof(s)-1,(f))
mb_pe_bind_result mb_win_fileio_bind(void *context,const mb_pe_import *i,uint64_t *out){
    mb_win_fileio_session *s=context;
    if(!s||!*find(s)||!s->active||!i||!out||(!i->library.data&&i->library.size)||(!i->name.data&&i->name.size)||(s->module->state!=MB_PE_MODULE_LOADING&&s->module->state!=MB_PE_MODULE_ACTIVE))return MB_PE_BIND_ERROR;
    if(i->by_ordinal||!MATCH(i->library,"kernel32.dll",1))return MB_PE_BIND_UNRESOLVED;
    if(MATCH(i->name,"CreateFileW",0))*out=(uint64_t)(uintptr_t)&create_file_w;
    else if(MATCH(i->name,"CreateFile2",0))*out=(uint64_t)(uintptr_t)&create_file_2;
    else if(MATCH(i->name,"ReadFile",0))*out=(uint64_t)(uintptr_t)&read_file;
    else if(MATCH(i->name,"WriteFile",0))*out=(uint64_t)(uintptr_t)&write_file;
    else if(MATCH(i->name,"FindFirstFileW",0))*out=(uint64_t)(uintptr_t)&find_first_file_w;
    else if(MATCH(i->name,"FindFirstFileExW",0))*out=(uint64_t)(uintptr_t)&find_first_file_ex_w;
    else if(MATCH(i->name,"FindNextFileW",0))*out=(uint64_t)(uintptr_t)&find_next_file_w;
    else if(MATCH(i->name,"FindClose",0))*out=(uint64_t)(uintptr_t)&find_close;
    else if(MATCH(i->name,"AreFileApisANSI",0))*out=(uint64_t)(uintptr_t)&are_file_apis_ansi;
    else if(MATCH(i->name,"GetFileAttributesExW",0))*out=(uint64_t)(uintptr_t)&get_file_attributes_ex_w;
    else if(MATCH(i->name,"GetFileInformationByHandle",0))*out=(uint64_t)(uintptr_t)&get_file_information_by_handle;
    else if(MATCH(i->name,"GetFileInformationByHandleEx",0))*out=(uint64_t)(uintptr_t)&get_file_information_by_handle_ex;
    else if(MATCH(i->name,"CreateDirectoryW",0))*out=(uint64_t)(uintptr_t)&create_directory_w;
    else if(MATCH(i->name,"DeleteFileW",0))*out=(uint64_t)(uintptr_t)&delete_file_w;
    else if(MATCH(i->name,"GetCurrentDirectoryW",0))*out=(uint64_t)(uintptr_t)&get_current_directory_w;
    else if(MATCH(i->name,"GetFinalPathNameByHandleW",0))*out=(uint64_t)(uintptr_t)&get_final_path_name_by_handle_w;
    else if(MATCH(i->name,"SetFileInformationByHandle",0))*out=(uint64_t)(uintptr_t)&set_file_information_by_handle;
    else if(MATCH(i->name,"FindFirstChangeNotificationW",0))*out=(uint64_t)(uintptr_t)&find_first_change_notification_w;
    else if(MATCH(i->name,"FindNextChangeNotification",0))*out=(uint64_t)(uintptr_t)&find_next_change_notification;
    else if(MATCH(i->name,"FindCloseChangeNotification",0))*out=(uint64_t)(uintptr_t)&find_close_change_notification;
    else return MB_PE_BIND_UNRESOLVED;
    return MB_PE_BIND_BOUND;
}
