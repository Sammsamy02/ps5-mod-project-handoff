#include "PsScanAbi.h"
#include <type_traits>
namespace Original { using CharType=PsChar16;
    struct PsScanConfig
    {
        bool g_uobject_array{};
        bool fname_tostring_fstring{};
        bool fname_ctor_wchar{};
        bool gmalloc{};
        bool static_construct_object_internal{};
        bool ftext_fstring{};
        bool engine_version{};
        bool fuobject_hash_tables_get{};
        bool gnatives{};
        bool console_manager_singleton{};
        bool gameengine_tick{};
        bool build_configuration{};
        bool stats{};
    };

    struct PsCtx
    {
        void (*default_)(CharType* msg);
        void (*normal)(CharType* msg);
        void (*verbose)(CharType* msg);
        void (*warning)(CharType* msg);
        void (*error)(CharType* msg);
        PsScanConfig config{};
    };

    struct PsEngineVersion
    {
        uint16_t major{};
        uint16_t minor{};
    };

    struct PsScanResults
    {
        void* g_uobject_array{};
        void* fname_tostring_fstring{};
        void* fname_ctor_wchar{};
        void* gmalloc{};
        void* static_construct_object_internal{};
        void* ftext_fstring{};
        PsEngineVersion engine_version{};
        void* fuobject_hash_tables_get{};
        void* gnatives{};
        void* console_manager_singleton{};
        void* gameengine_tick{};
        // 0 unknown, 1 shipping, 2 development, 3 test
        uint32_t build_configuration{};
        // 0 unknown, 1 off, 2 on. Independent of the configuration.
        uint32_t stats{};
    };

}
static_assert(sizeof(PsScanConfig)==sizeof(Original::PsScanConfig));
static_assert(alignof(PsScanConfig)==alignof(Original::PsScanConfig));
static_assert(offsetof(PsScanConfig,g_uobject_array)==offsetof(Original::PsScanConfig,g_uobject_array));
static_assert(offsetof(PsScanConfig,fname_tostring_fstring)==offsetof(Original::PsScanConfig,fname_tostring_fstring));
static_assert(offsetof(PsScanConfig,fname_ctor_wchar)==offsetof(Original::PsScanConfig,fname_ctor_wchar));
static_assert(offsetof(PsScanConfig,gmalloc)==offsetof(Original::PsScanConfig,gmalloc));
static_assert(offsetof(PsScanConfig,static_construct_object_internal)==offsetof(Original::PsScanConfig,static_construct_object_internal));
static_assert(offsetof(PsScanConfig,ftext_fstring)==offsetof(Original::PsScanConfig,ftext_fstring));
static_assert(offsetof(PsScanConfig,engine_version)==offsetof(Original::PsScanConfig,engine_version));
static_assert(offsetof(PsScanConfig,fuobject_hash_tables_get)==offsetof(Original::PsScanConfig,fuobject_hash_tables_get));
static_assert(offsetof(PsScanConfig,gnatives)==offsetof(Original::PsScanConfig,gnatives));
static_assert(offsetof(PsScanConfig,console_manager_singleton)==offsetof(Original::PsScanConfig,console_manager_singleton));
static_assert(offsetof(PsScanConfig,gameengine_tick)==offsetof(Original::PsScanConfig,gameengine_tick));
static_assert(offsetof(PsScanConfig,build_configuration)==offsetof(Original::PsScanConfig,build_configuration));
static_assert(offsetof(PsScanConfig,stats)==offsetof(Original::PsScanConfig,stats));
static_assert(sizeof(PsCtx)==sizeof(Original::PsCtx));
static_assert(alignof(PsCtx)==alignof(Original::PsCtx));
static_assert(offsetof(PsCtx,default_)==offsetof(Original::PsCtx,default_));
static_assert(offsetof(PsCtx,normal)==offsetof(Original::PsCtx,normal));
static_assert(offsetof(PsCtx,verbose)==offsetof(Original::PsCtx,verbose));
static_assert(offsetof(PsCtx,warning)==offsetof(Original::PsCtx,warning));
static_assert(offsetof(PsCtx,error)==offsetof(Original::PsCtx,error));
static_assert(offsetof(PsCtx,config)==offsetof(Original::PsCtx,config));
static_assert(sizeof(PsEngineVersion)==sizeof(Original::PsEngineVersion));
static_assert(alignof(PsEngineVersion)==alignof(Original::PsEngineVersion));
static_assert(offsetof(PsEngineVersion,major)==offsetof(Original::PsEngineVersion,major));
static_assert(offsetof(PsEngineVersion,minor)==offsetof(Original::PsEngineVersion,minor));
static_assert(sizeof(PsScanResults)==sizeof(Original::PsScanResults));
static_assert(alignof(PsScanResults)==alignof(Original::PsScanResults));
static_assert(offsetof(PsScanResults,g_uobject_array)==offsetof(Original::PsScanResults,g_uobject_array));
static_assert(offsetof(PsScanResults,fname_tostring_fstring)==offsetof(Original::PsScanResults,fname_tostring_fstring));
static_assert(offsetof(PsScanResults,fname_ctor_wchar)==offsetof(Original::PsScanResults,fname_ctor_wchar));
static_assert(offsetof(PsScanResults,gmalloc)==offsetof(Original::PsScanResults,gmalloc));
static_assert(offsetof(PsScanResults,static_construct_object_internal)==offsetof(Original::PsScanResults,static_construct_object_internal));
static_assert(offsetof(PsScanResults,ftext_fstring)==offsetof(Original::PsScanResults,ftext_fstring));
static_assert(offsetof(PsScanResults,engine_version)==offsetof(Original::PsScanResults,engine_version));
static_assert(offsetof(PsScanResults,fuobject_hash_tables_get)==offsetof(Original::PsScanResults,fuobject_hash_tables_get));
static_assert(offsetof(PsScanResults,gnatives)==offsetof(Original::PsScanResults,gnatives));
static_assert(offsetof(PsScanResults,console_manager_singleton)==offsetof(Original::PsScanResults,console_manager_singleton));
static_assert(offsetof(PsScanResults,gameengine_tick)==offsetof(Original::PsScanResults,gameengine_tick));
static_assert(offsetof(PsScanResults,build_configuration)==offsetof(Original::PsScanResults,build_configuration));
static_assert(offsetof(PsScanResults,stats)==offsetof(Original::PsScanResults,stats));
static_assert(std::is_same_v<PsLogCallback,decltype(Original::PsCtx::default_)>);
