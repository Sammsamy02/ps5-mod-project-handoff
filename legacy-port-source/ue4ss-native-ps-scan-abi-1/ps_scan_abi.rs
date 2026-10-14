#![allow(dead_code)]
use core::ffi::c_void;
pub type PsLogCallback = extern "C" fn(*mut u16);
#[repr(C)]
pub struct PsScanConfig {
 pub g_uobject_array:bool,
 pub fname_tostring_fstring:bool,
 pub fname_ctor_wchar:bool,
 pub gmalloc:bool,
 pub static_construct_object_internal:bool,
 pub ftext_fstring:bool,
 pub engine_version:bool,
 pub fuobject_hash_tables_get:bool,
 pub gnatives:bool,
 pub console_manager_singleton:bool,
 pub gameengine_tick:bool,
 pub build_configuration:bool,
 pub stats:bool,
}
#[repr(C)]
pub struct PsCtx {pub default_:PsLogCallback,pub normal:PsLogCallback,pub verbose:PsLogCallback,pub warning:PsLogCallback,pub error:PsLogCallback,pub config:PsScanConfig}
#[repr(C)]
pub struct PsEngineVersion {pub major:u16,pub minor:u16}
#[repr(C)]
pub struct PsScanResults {
 pub g_uobject_array:*mut c_void,
 pub fname_tostring_fstring:*mut c_void,
 pub fname_ctor_wchar:*mut c_void,
 pub gmalloc:*mut c_void,
 pub static_construct_object_internal:*mut c_void,
 pub ftext_fstring:*mut c_void,
 pub engine_version:PsEngineVersion,
 pub fuobject_hash_tables_get:*mut c_void,
 pub gnatives:*mut c_void,
 pub console_manager_singleton:*mut c_void,
 pub gameengine_tick:*mut c_void,
 pub build_configuration:u32,pub stats:u32}
#[repr(C)]
pub struct PsMappedSegment {pub bytes:*const u8,pub size:usize,pub runtime_address:u64,pub permissions:u32,pub reserved:u32}
#[repr(C)]
pub struct PsMappedExtent {pub address:u64,pub size:usize}
#[repr(C)]
pub struct PsMappedImageViewV1 {pub abi_version:u32,pub format:u32,pub segments:*const PsMappedSegment,pub segment_count:usize,pub text_base:u64,pub image_base:u64,pub eh_frame:PsMappedExtent,pub eh_frame_hdr:PsMappedExtent,pub pe_exception_directory:PsMappedExtent}
const _: () = {assert!(core::mem::size_of::<PsScanConfig>()==13);assert!(core::mem::align_of::<PsScanConfig>()==1);assert!(core::mem::offset_of!(PsScanConfig,g_uobject_array)==0);assert!(core::mem::offset_of!(PsScanConfig,fname_tostring_fstring)==1);assert!(core::mem::offset_of!(PsScanConfig,fname_ctor_wchar)==2);assert!(core::mem::offset_of!(PsScanConfig,gmalloc)==3);assert!(core::mem::offset_of!(PsScanConfig,static_construct_object_internal)==4);assert!(core::mem::offset_of!(PsScanConfig,ftext_fstring)==5);assert!(core::mem::offset_of!(PsScanConfig,engine_version)==6);assert!(core::mem::offset_of!(PsScanConfig,fuobject_hash_tables_get)==7);assert!(core::mem::offset_of!(PsScanConfig,gnatives)==8);assert!(core::mem::offset_of!(PsScanConfig,console_manager_singleton)==9);assert!(core::mem::offset_of!(PsScanConfig,gameengine_tick)==10);assert!(core::mem::offset_of!(PsScanConfig,build_configuration)==11);assert!(core::mem::offset_of!(PsScanConfig,stats)==12);};
const _: () = {assert!(core::mem::size_of::<PsCtx>()==56);assert!(core::mem::align_of::<PsCtx>()==8);assert!(core::mem::offset_of!(PsCtx,default_)==0);assert!(core::mem::offset_of!(PsCtx,normal)==8);assert!(core::mem::offset_of!(PsCtx,verbose)==16);assert!(core::mem::offset_of!(PsCtx,warning)==24);assert!(core::mem::offset_of!(PsCtx,error)==32);assert!(core::mem::offset_of!(PsCtx,config)==40);};
const _: () = {assert!(core::mem::size_of::<PsEngineVersion>()==4);assert!(core::mem::align_of::<PsEngineVersion>()==2);assert!(core::mem::offset_of!(PsEngineVersion,major)==0);assert!(core::mem::offset_of!(PsEngineVersion,minor)==2);};
const _: () = {assert!(core::mem::size_of::<PsScanResults>()==96);assert!(core::mem::align_of::<PsScanResults>()==8);assert!(core::mem::offset_of!(PsScanResults,g_uobject_array)==0);assert!(core::mem::offset_of!(PsScanResults,fname_tostring_fstring)==8);assert!(core::mem::offset_of!(PsScanResults,fname_ctor_wchar)==16);assert!(core::mem::offset_of!(PsScanResults,gmalloc)==24);assert!(core::mem::offset_of!(PsScanResults,static_construct_object_internal)==32);assert!(core::mem::offset_of!(PsScanResults,ftext_fstring)==40);assert!(core::mem::offset_of!(PsScanResults,engine_version)==48);assert!(core::mem::offset_of!(PsScanResults,fuobject_hash_tables_get)==56);assert!(core::mem::offset_of!(PsScanResults,gnatives)==64);assert!(core::mem::offset_of!(PsScanResults,console_manager_singleton)==72);assert!(core::mem::offset_of!(PsScanResults,gameengine_tick)==80);assert!(core::mem::offset_of!(PsScanResults,build_configuration)==88);assert!(core::mem::offset_of!(PsScanResults,stats)==92);};
const _: () = {assert!(core::mem::size_of::<PsMappedSegment>()==32);assert!(core::mem::align_of::<PsMappedSegment>()==8);assert!(core::mem::offset_of!(PsMappedSegment,bytes)==0);assert!(core::mem::offset_of!(PsMappedSegment,size)==8);assert!(core::mem::offset_of!(PsMappedSegment,runtime_address)==16);assert!(core::mem::offset_of!(PsMappedSegment,permissions)==24);assert!(core::mem::offset_of!(PsMappedSegment,reserved)==28);};
const _: () = {assert!(core::mem::size_of::<PsMappedExtent>()==16);assert!(core::mem::align_of::<PsMappedExtent>()==8);assert!(core::mem::offset_of!(PsMappedExtent,address)==0);assert!(core::mem::offset_of!(PsMappedExtent,size)==8);};
const _: () = {assert!(core::mem::size_of::<PsMappedImageViewV1>()==88);assert!(core::mem::align_of::<PsMappedImageViewV1>()==8);assert!(core::mem::offset_of!(PsMappedImageViewV1,abi_version)==0);assert!(core::mem::offset_of!(PsMappedImageViewV1,format)==4);assert!(core::mem::offset_of!(PsMappedImageViewV1,segments)==8);assert!(core::mem::offset_of!(PsMappedImageViewV1,segment_count)==16);assert!(core::mem::offset_of!(PsMappedImageViewV1,text_base)==24);assert!(core::mem::offset_of!(PsMappedImageViewV1,image_base)==32);assert!(core::mem::offset_of!(PsMappedImageViewV1,eh_frame)==40);assert!(core::mem::offset_of!(PsMappedImageViewV1,eh_frame_hdr)==56);assert!(core::mem::offset_of!(PsMappedImageViewV1,pe_exception_directory)==72);};
