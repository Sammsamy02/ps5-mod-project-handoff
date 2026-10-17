#ifndef MB_OWNED_WORKER_FILEIO_H
#define MB_OWNED_WORKER_FILEIO_H
/* Owned PE fixture: KERNEL32 file I/O through actual imports (10 of the 20
 * census names; the four fail-fast stubs and the six remaining names stay
 * unexercised this pass). File handles are intentionally NOT closed here:
 * the workers family owns the CloseHandle import for thread handles, so this
 * fixture hands its two open file handles back to the native caller in out[],
 * which retires them through mb_win_fileio_close_handle, the documented
 * delegation service for the future CloseHandle owner. Find handles close
 * with FindClose exactly as Windows requires. The exercise is a dedicated
 * export outside the worker thread flow, so the gate, the wait chain and
 * every other family's fixtures remain untouched (composition independence
 * with and without --sync/--init-once/--mutex). Error returns -80..-89 are
 * unique across this tree's fixtures (sync -40..-44, initonce -45..-54,
 * mutex -60..-77, base worker -1..-18). First call: current directory,
 * create/truncate, write, by-handle size, CreateFile2 OPEN_ALWAYS 183 quirk,
 * independent-pointer readback, wildcard find + exhaustion + FindClose;
 * returns both open handles. Second call (after the native side closed both):
 * delete, then prove the name is gone (FindFirst no-match = INVALID + 2). */
typedef __UINT16_TYPE__ mb_worker_u16;
__declspec(dllimport) void *CreateFileW(const mb_worker_u16 *,mb_worker_u32,mb_worker_u32,const void *,mb_worker_u32,mb_worker_u32,const void *);
__declspec(dllimport) void *CreateFile2(const mb_worker_u16 *,mb_worker_u32,mb_worker_u32,mb_worker_u32,const void *);
__declspec(dllimport) mb_worker_i32 ReadFile(void *,void *,mb_worker_u32,mb_worker_u32 *,const void *);
__declspec(dllimport) mb_worker_i32 WriteFile(void *,const void *,mb_worker_u32,mb_worker_u32 *,const void *);
__declspec(dllimport) mb_worker_i32 GetFileInformationByHandle(void *,void *);
__declspec(dllimport) mb_worker_u32 GetCurrentDirectoryW(mb_worker_u32,mb_worker_u16 *);
__declspec(dllimport) void *FindFirstFileW(const mb_worker_u16 *,void *);
__declspec(dllimport) mb_worker_i32 FindNextFileW(void *,void *);
__declspec(dllimport) mb_worker_i32 FindClose(void *);
__declspec(dllimport) mb_worker_i32 DeleteFileW(const mb_worker_u16 *);
#define MB_WORKER_FILEIO_GENERIC_READ 0x80000000u
#define MB_WORKER_FILEIO_GENERIC_WRITE 0x40000000u
#define MB_WORKER_FILEIO_SHARE_READ 0x1u
#define MB_WORKER_FILEIO_SHARE_WRITE 0x2u
#define MB_WORKER_FILEIO_CREATE_ALWAYS 2u
#define MB_WORKER_FILEIO_OPEN_ALWAYS 4u
#define MB_WORKER_FILEIO_ATTRIBUTE_NORMAL 0x80u
#define MB_WORKER_FILEIO_ERROR_FILE_NOT_FOUND 2u
#define MB_WORKER_FILEIO_ERROR_NO_MORE_FILES 18u
#define MB_WORKER_FILEIO_ERROR_ALREADY_EXISTS 183u
#define MB_WORKER_FILEIO_INVALID_HANDLE_VALUE ((void *)(mb_worker_ptr)0xffffffffffffffffULL)
#define MB_WORKER_FILEIO_CALL1_MARKER 0x46494f31ULL /* "FIO1" */
#define MB_WORKER_FILEIO_CALL2_MARKER 0x46494f32ULL /* "FIO2" */
struct mb_worker_fileio_filetime{mb_worker_u32 low,high;};
struct mb_worker_fileio_by_handle{ /* BY_HANDLE_FILE_INFORMATION 52/4 */
 mb_worker_u32 attributes;
 struct mb_worker_fileio_filetime creation,last_access,last_write;
 mb_worker_u32 volume_serial,size_high,size_low,links,index_high,index_low;
};
struct mb_worker_fileio_find_data{ /* WIN32_FIND_DATAW 592/4 */
 mb_worker_u32 attributes;
 struct mb_worker_fileio_filetime creation,last_access,last_write;
 mb_worker_u32 size_high,size_low,reserved0,reserved1;
 mb_worker_u16 name[260];
 mb_worker_u16 alternate[14];
};
struct mb_worker_fileio_createfile2{ /* CREATEFILE2_EXTENDED_PARAMETERS 32/8 */
 mb_worker_u32 size,file_attributes,file_flags,security_qos_flags;
 const void *security_attributes,*template_file;
};
_Static_assert(sizeof(struct mb_worker_fileio_by_handle)==52 && _Alignof(struct mb_worker_fileio_by_handle)==4,"BY_HANDLE_FILE_INFORMATION Windows width");
_Static_assert(sizeof(struct mb_worker_fileio_find_data)==592 && _Alignof(struct mb_worker_fileio_find_data)==4,"WIN32_FIND_DATAW Windows width");
_Static_assert(sizeof(struct mb_worker_fileio_createfile2)==32,"CREATEFILE2_EXTENDED_PARAMETERS Windows width");
static const mb_worker_u16 mb_worker_fileio_name[]={'m','b','w','o','r','k','e','r','.','f','i','l','e','i','o','.','b','i','n',0};
static const mb_worker_u16 mb_worker_fileio_pattern[]={'m','b','w','o','r','k','e','r','.','f','i','l','e','i','o','.','*',0};
static const char mb_worker_fileio_payload[8]={'M','b','F','i','0','1','0','1'};
static const struct mb_worker_fileio_createfile2 mb_worker_fileio_parameters={
 sizeof(struct mb_worker_fileio_createfile2),MB_WORKER_FILEIO_ATTRIBUTE_NORMAL,0,0,(void *)0,(void *)0};
static struct {
 mb_worker_u32 phase;
 struct mb_worker_fileio_by_handle info;
 struct mb_worker_fileio_find_data find;
 char buffer[8];
} mb_worker_fileio_state;
static mb_worker_i32 mb_worker_fileio_name_is(const mb_worker_u16 *name,const mb_worker_u16 *expect){
 mb_worker_u32 i=0;for(;expect[i];++i)if(!name[i]||name[i]!=expect[i])return 0;return name[i]==0;
}
static mb_worker_i32 mb_worker_fileio_call(mb_worker_u64 *out){
 if(!out)return -80;
 out[0]=out[1]=out[2]=out[3]=0;
 if(mb_worker_fileio_state.phase==0){
  mb_worker_u16 directory[8];mb_worker_u32 written=0,read_back=0;void *write_handle,*read_handle,*find;
  if(GetCurrentDirectoryW(8,directory)!=1 || directory[0]!='\\' || directory[1])return -81;
  write_handle=CreateFileW(mb_worker_fileio_name,MB_WORKER_FILEIO_GENERIC_WRITE,
   MB_WORKER_FILEIO_SHARE_READ|MB_WORKER_FILEIO_SHARE_WRITE,(void *)0,MB_WORKER_FILEIO_CREATE_ALWAYS,
   MB_WORKER_FILEIO_ATTRIBUTE_NORMAL,(void *)0);
  if(!write_handle || write_handle==MB_WORKER_FILEIO_INVALID_HANDLE_VALUE)return -82;
  if(!WriteFile(write_handle,mb_worker_fileio_payload,8,&written,(void *)0) || written!=8)return -83;
  if(!GetFileInformationByHandle(write_handle,&mb_worker_fileio_state.info) ||
   mb_worker_fileio_state.info.size_high || mb_worker_fileio_state.info.size_low!=8)return -84;
  read_handle=CreateFile2(mb_worker_fileio_name,MB_WORKER_FILEIO_GENERIC_READ,
   MB_WORKER_FILEIO_SHARE_READ|MB_WORKER_FILEIO_SHARE_WRITE,MB_WORKER_FILEIO_OPEN_ALWAYS,&mb_worker_fileio_parameters);
  if(!read_handle || read_handle==MB_WORKER_FILEIO_INVALID_HANDLE_VALUE ||
   GetLastError()!=MB_WORKER_FILEIO_ERROR_ALREADY_EXISTS)return -85; /* documented OPEN_ALWAYS quirk */
  if(!ReadFile(read_handle,mb_worker_fileio_state.buffer,8,&read_back,(void *)0) || read_back!=8)return -86;
  for(mb_worker_u32 i=0;i<8;++i)if(mb_worker_fileio_state.buffer[i]!=mb_worker_fileio_payload[i])return -86;
  find=FindFirstFileW(mb_worker_fileio_pattern,&mb_worker_fileio_state.find);
  if(!find || find==MB_WORKER_FILEIO_INVALID_HANDLE_VALUE)return -87;
  if(!mb_worker_fileio_name_is(mb_worker_fileio_state.find.name,mb_worker_fileio_name) ||
   mb_worker_fileio_state.find.size_high || mb_worker_fileio_state.find.size_low!=8)return -87;
  if(FindNextFileW(find,&mb_worker_fileio_state.find) || GetLastError()!=MB_WORKER_FILEIO_ERROR_NO_MORE_FILES)return -87;
  if(!FindClose(find))return -87;
  mb_worker_fileio_state.phase=1;
  out[0]=(mb_worker_u64)(mb_worker_ptr)write_handle;out[1]=(mb_worker_u64)(mb_worker_ptr)read_handle;
  out[2]=1;out[3]=MB_WORKER_FILEIO_CALL1_MARKER;
  return 1; /* both file handles stay OPEN: the native caller closes them */
 }
 if(mb_worker_fileio_state.phase==1){
  void *find;
  if(!DeleteFileW(mb_worker_fileio_name))return -88;
  find=FindFirstFileW(mb_worker_fileio_pattern,&mb_worker_fileio_state.find);
  if(find!=MB_WORKER_FILEIO_INVALID_HANDLE_VALUE || GetLastError()!=MB_WORKER_FILEIO_ERROR_FILE_NOT_FOUND)return -89;
  mb_worker_fileio_state.phase=0;out[0]=1;out[3]=MB_WORKER_FILEIO_CALL2_MARKER;
  return 1;
 }
 return -80;
}
#endif
