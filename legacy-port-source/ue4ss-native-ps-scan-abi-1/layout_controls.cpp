#include "PsScanAbi.h"
#ifdef __cplusplus
#define ASSERT static_assert
#define ALIGNOF alignof
#else
#define ASSERT _Static_assert
#define ALIGNOF _Alignof
#endif
ASSERT(sizeof(void*)==8,"64-bit binding required");
ASSERT(sizeof(bool)==1,"one-byte bool binding required");
ASSERT(sizeof(PsChar16)==2,"UTF16 required");
ASSERT(sizeof(PsScanConfig)==13,"PsScanConfig size");
ASSERT(ALIGNOF(PsScanConfig)==1,"PsScanConfig align");
ASSERT(offsetof(PsScanConfig,g_uobject_array)==0,"PsScanConfig.g_uobject_array");
ASSERT(offsetof(PsScanConfig,fname_tostring_fstring)==1,"PsScanConfig.fname_tostring_fstring");
ASSERT(offsetof(PsScanConfig,fname_ctor_wchar)==2,"PsScanConfig.fname_ctor_wchar");
ASSERT(offsetof(PsScanConfig,gmalloc)==3,"PsScanConfig.gmalloc");
ASSERT(offsetof(PsScanConfig,static_construct_object_internal)==4,"PsScanConfig.static_construct_object_internal");
ASSERT(offsetof(PsScanConfig,ftext_fstring)==5,"PsScanConfig.ftext_fstring");
ASSERT(offsetof(PsScanConfig,engine_version)==6,"PsScanConfig.engine_version");
ASSERT(offsetof(PsScanConfig,fuobject_hash_tables_get)==7,"PsScanConfig.fuobject_hash_tables_get");
ASSERT(offsetof(PsScanConfig,gnatives)==8,"PsScanConfig.gnatives");
ASSERT(offsetof(PsScanConfig,console_manager_singleton)==9,"PsScanConfig.console_manager_singleton");
ASSERT(offsetof(PsScanConfig,gameengine_tick)==10,"PsScanConfig.gameengine_tick");
ASSERT(offsetof(PsScanConfig,build_configuration)==11,"PsScanConfig.build_configuration");
ASSERT(offsetof(PsScanConfig,stats)==12,"PsScanConfig.stats");
ASSERT(sizeof(PsCtx)==56,"PsCtx size");
ASSERT(ALIGNOF(PsCtx)==8,"PsCtx align");
ASSERT(offsetof(PsCtx,default_)==0,"PsCtx.default_");
ASSERT(offsetof(PsCtx,normal)==8,"PsCtx.normal");
ASSERT(offsetof(PsCtx,verbose)==16,"PsCtx.verbose");
ASSERT(offsetof(PsCtx,warning)==24,"PsCtx.warning");
ASSERT(offsetof(PsCtx,error)==32,"PsCtx.error");
ASSERT(offsetof(PsCtx,config)==40,"PsCtx.config");
ASSERT(sizeof(PsEngineVersion)==4,"PsEngineVersion size");
ASSERT(ALIGNOF(PsEngineVersion)==2,"PsEngineVersion align");
ASSERT(offsetof(PsEngineVersion,major)==0,"PsEngineVersion.major");
ASSERT(offsetof(PsEngineVersion,minor)==2,"PsEngineVersion.minor");
ASSERT(sizeof(PsScanResults)==96,"PsScanResults size");
ASSERT(ALIGNOF(PsScanResults)==8,"PsScanResults align");
ASSERT(offsetof(PsScanResults,g_uobject_array)==0,"PsScanResults.g_uobject_array");
ASSERT(offsetof(PsScanResults,fname_tostring_fstring)==8,"PsScanResults.fname_tostring_fstring");
ASSERT(offsetof(PsScanResults,fname_ctor_wchar)==16,"PsScanResults.fname_ctor_wchar");
ASSERT(offsetof(PsScanResults,gmalloc)==24,"PsScanResults.gmalloc");
ASSERT(offsetof(PsScanResults,static_construct_object_internal)==32,"PsScanResults.static_construct_object_internal");
ASSERT(offsetof(PsScanResults,ftext_fstring)==40,"PsScanResults.ftext_fstring");
ASSERT(offsetof(PsScanResults,engine_version)==48,"PsScanResults.engine_version");
ASSERT(offsetof(PsScanResults,fuobject_hash_tables_get)==56,"PsScanResults.fuobject_hash_tables_get");
ASSERT(offsetof(PsScanResults,gnatives)==64,"PsScanResults.gnatives");
ASSERT(offsetof(PsScanResults,console_manager_singleton)==72,"PsScanResults.console_manager_singleton");
ASSERT(offsetof(PsScanResults,gameengine_tick)==80,"PsScanResults.gameengine_tick");
ASSERT(offsetof(PsScanResults,build_configuration)==88,"PsScanResults.build_configuration");
ASSERT(offsetof(PsScanResults,stats)==92,"PsScanResults.stats");
ASSERT(sizeof(PsMappedSegment)==32,"PsMappedSegment size");
ASSERT(ALIGNOF(PsMappedSegment)==8,"PsMappedSegment align");
ASSERT(offsetof(PsMappedSegment,bytes)==0,"PsMappedSegment.bytes");
ASSERT(offsetof(PsMappedSegment,size)==8,"PsMappedSegment.size");
ASSERT(offsetof(PsMappedSegment,runtime_address)==16,"PsMappedSegment.runtime_address");
ASSERT(offsetof(PsMappedSegment,permissions)==24,"PsMappedSegment.permissions");
ASSERT(offsetof(PsMappedSegment,reserved)==28,"PsMappedSegment.reserved");
ASSERT(sizeof(PsMappedExtent)==16,"PsMappedExtent size");
ASSERT(ALIGNOF(PsMappedExtent)==8,"PsMappedExtent align");
ASSERT(offsetof(PsMappedExtent,address)==0,"PsMappedExtent.address");
ASSERT(offsetof(PsMappedExtent,size)==8,"PsMappedExtent.size");
ASSERT(sizeof(PsMappedImageViewV1)==88,"PsMappedImageViewV1 size");
ASSERT(ALIGNOF(PsMappedImageViewV1)==8,"PsMappedImageViewV1 align");
ASSERT(offsetof(PsMappedImageViewV1,abi_version)==0,"PsMappedImageViewV1.abi_version");
ASSERT(offsetof(PsMappedImageViewV1,format)==4,"PsMappedImageViewV1.format");
ASSERT(offsetof(PsMappedImageViewV1,segments)==8,"PsMappedImageViewV1.segments");
ASSERT(offsetof(PsMappedImageViewV1,segment_count)==16,"PsMappedImageViewV1.segment_count");
ASSERT(offsetof(PsMappedImageViewV1,text_base)==24,"PsMappedImageViewV1.text_base");
ASSERT(offsetof(PsMappedImageViewV1,image_base)==32,"PsMappedImageViewV1.image_base");
ASSERT(offsetof(PsMappedImageViewV1,eh_frame)==40,"PsMappedImageViewV1.eh_frame");
ASSERT(offsetof(PsMappedImageViewV1,eh_frame_hdr)==56,"PsMappedImageViewV1.eh_frame_hdr");
ASSERT(offsetof(PsMappedImageViewV1,pe_exception_directory)==72,"PsMappedImageViewV1.pe_exception_directory");
int ps_scan_abi_layout_only(void) { return 0; }
