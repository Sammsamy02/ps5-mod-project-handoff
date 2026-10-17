/*
 * NativeModAbi.h -- PS5 Mod Bridge native C++ mod ABI, version 1.
 *
 * Capability #9 (UE4SS C++ mods) and #11 (GUI tabs). Generic for every
 * UE4SS-style C++ mod and every Unreal game: no game, mod or offset appears
 * here. Plain C so that NO C++ type, vtable, RTTI, exception or allocator ever
 * crosses the host <-> mod module boundary.
 *
 * Why a C table instead of the upstream C++ ABI (start_mod returning a
 * CppUserModBase* whose methods live in UE4SS.dll):
 *   - the shipped runtime PRX exports no UE4SS C++ symbol (Mac-only reading of
 *     the R11 bare ELF: 210 dynamic symbols, 2 defined = module_start/stop), so a
 *     mod built against the real UE4SS headers has nothing to import from;
 *   - cross-module C++ (typeinfo, exceptions, std:: layouts, operator new)
 *     between two separately built PRX modules is unproved on the target.
 * The SDK (sdk/) keeps the upstream SOURCE shape (class MyMod : public
 * RC::CppUserModBase, extern "C" start_mod/uninstall_mod) and converts it to
 * this table inside the mod module.
 *
 * Rules that both sides follow:
 *   1. The mod exports exactly one entry: MBM_ENTRY_SYMBOL.
 *   2. Every table starts with magic/abi_version/struct_size; a reader checks
 *      all three and refuses (never guesses) on any mismatch. Tables only grow
 *      at the end; a reader never touches a field past struct_size.
 *   3. Every mod callback returns mbm_status. Anything except MBM_OK means the
 *      mod faulted (the SDK converts a thrown C++ exception into
 *      MBM_E_MOD_FAULT and stores the text for last_error). The host then stops
 *      calling that mod (fail closed) and logs the reason.
 *   4. Strings are UTF-8, NUL-terminated, owned by the caller for the duration
 *      of the call only. Output buffers are caller-owned with explicit capacity.
 *   5. Memory allocated by one module is freed by the same module.
 *   6. No callback may block waiting for the game thread (the game thread may be
 *      waiting for it). Use execute_in_game_thread instead.
 */
#ifndef MBM_NATIVE_MOD_ABI_H
#define MBM_NATIVE_MOD_ABI_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MBM_ABI_MAGIC 0x314D424Du /* "MBM1" little-endian */
#define MBM_ABI_VERSION 1u
#define MBM_ENTRY_SYMBOL "mbm_mod_entry_v1"
#define MBM_GUI_MAGIC 0x31495547u /* "GUI1" */

typedef int32_t mbm_status;
enum
{
    MBM_OK = 0,
    MBM_E_INVALID = -1,      /* bad argument / null / malformed table        */
    MBM_E_REFUSED = -2,      /* the route exists in PC UE4SS but is refused   */
    MBM_E_NOT_FOUND = -3,    /* object / property / function not found        */
    MBM_E_UNAVAILABLE = -4,  /* service not ready yet (e.g. before unreal init)*/
    MBM_E_CAPACITY = -5,     /* host table full                               */
    MBM_E_STATE = -6,        /* wrong lifecycle state (mod stopping, faulted) */
    MBM_E_TYPE = -7,         /* reflected type does not match requested type  */
    MBM_E_INTERNAL = -8,     /* host-side exception, contained                */
    MBM_E_TRUNCATED = -9,    /* output buffer too small; *len has the need    */
    MBM_E_MOD_FAULT = -10    /* the mod callback threw / reported failure      */
};

enum
{
    MBM_LOG_NORMAL = 0,
    MBM_LOG_VERBOSE = 1,
    MBM_LOG_WARNING = 2,
    MBM_LOG_ERROR = 3
};

/* An engine UObject*. Opaque to mods: only ever passed back to the host. */
typedef void* mbm_object;

enum mbm_value_type
{
    MBM_VT_NONE = 0,
    MBM_VT_BOOL = 1,   /* v.b (0/1); bitfield bools supported by get/set only */
    MBM_VT_U8 = 2,
    MBM_VT_I32 = 3,
    MBM_VT_I64 = 4,
    MBM_VT_F32 = 5,
    MBM_VT_F64 = 6,
    MBM_VT_OBJECT = 7, /* v.object                                             */
    MBM_VT_NAME = 8,   /* FName  <-> utf8_in / utf8_out                         */
    MBM_VT_STRING = 9  /* FString<-> utf8_in / utf8_out                         */
};

typedef struct mbm_value
{
    uint32_t type; /* enum mbm_value_type */
    uint32_t reserved;
    union
    {
        int32_t b;
        uint8_t u8;
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        mbm_object object;
    } v;
    const char* utf8_in; /* NAME/STRING input                      */
    char* utf8_out;      /* NAME/STRING output buffer (caller-owned)*/
    int32_t utf8_out_cap;
    int32_t utf8_out_len; /* bytes needed, excluding NUL            */
} mbm_value;

/* Hook families a PC UE4SS C++ mod can ask for. The host answers each with an
 * admission (see HookAdmission.hpp). Numbers are ABI: never renumber. */
enum mbm_hook_family
{
    MBM_HOOK_UFUNCTION = 1,               /* per-UFunction pre/post (RegisterHook)     */
    MBM_HOOK_GAME_THREAD_TICK = 2,        /* once per frame on the game thread         */
    MBM_HOOK_ENGINE_TICK = 3,             /* inside UEngine::Tick (pre/post)           */
    MBM_HOOK_GAME_VIEWPORT_TICK = 4,      /* UGameViewportClient::Tick                 */
    MBM_HOOK_PROCESS_EVENT_GLOBAL = 5,    /* every UObject::ProcessEvent               */
    MBM_HOOK_PROCESS_INTERNAL_GLOBAL = 6, /* every UObject::ProcessInternal            */
    MBM_HOOK_PROCESS_LOCAL_SCRIPT_GLOBAL = 7,
    MBM_HOOK_STATIC_CONSTRUCT_OBJECT = 8, /* object creation                           */
    MBM_HOOK_OBJECT_DELETE = 9,           /* object deletion                           */
    MBM_HOOK_LOAD_MAP = 10,
    MBM_HOOK_BEGIN_PLAY = 11,
    MBM_HOOK_INIT_GAME_STATE = 12,
    MBM_HOOK_CALL_FUNCTION_BY_NAME = 13,
    MBM_HOOK_PROCESS_CONSOLE_EXEC = 14,
    MBM_HOOK_LOCAL_PLAYER_EXEC = 15,
    MBM_HOOK_GNATIVES_SLOT = 16,          /* bytecode opcode table (Kismet debugger)   */
    MBM_HOOK_CODE_ADDRESS = 17,           /* arbitrary code-address detour             */
    MBM_HOOK_FAMILY_COUNT = 18
};

enum mbm_route
{
    MBM_ROUTE_REFUSED = 0,           /* no data-only route exists: refused          */
    MBM_ROUTE_DATA_ONLY = 1,         /* delivered by a data-only route, full timing */
    MBM_ROUTE_DATA_ONLY_PARTIAL = 2, /* data-only, documented semantic difference   */
    MBM_ROUTE_PENDING_VALUE = 3      /* data-only route designed, but a required    */
                                     /* value lacks two sources: refused for now    */
};

typedef struct mbm_admission
{
    uint32_t family;        /* enum mbm_hook_family */
    uint32_t route;         /* enum mbm_route       */
    uint32_t live;          /* 1 = the route is armed in THIS process now */
    uint32_t reserved;
    const char* route_name; /* static string        */
    const char* reason;     /* static string        */
} mbm_admission;

typedef struct mbm_hook_call
{
    mbm_object context;  /* the object the function runs on              */
    mbm_object function; /* the UFunction                                */
    void* params;        /* the parameter frame (FFrame::Locals)         */
    void* return_value;  /* RESULT_DECL (may be null)                    */
} mbm_hook_call;

typedef mbm_status (*mbm_hook_fn)(void* user, const mbm_hook_call* call);
typedef mbm_status (*mbm_task_fn)(void* user);
typedef mbm_status (*mbm_tick_fn)(void* user, float delta_seconds);
typedef mbm_status (*mbm_key_fn)(void* user);

/* modifier mask for register_key_down */
#define MBM_KEYMOD_SHIFT 0x1u
#define MBM_KEYMOD_CONTROL 0x2u
#define MBM_KEYMOD_ALT 0x4u

/* ---------------- GUI (#11): shared Dear ImGui context ---------------- */
typedef void* (*mbm_gui_alloc_fn)(size_t size, void* user);
typedef void (*mbm_gui_free_fn)(void* ptr, void* user);
typedef mbm_status (*mbm_gui_render_fn)(void* user);

typedef struct mbm_gui_v1
{
    uint32_t magic; /* MBM_GUI_MAGIC */
    uint32_t struct_size;
    const char* imgui_version; /* host IMGUI_VERSION, e.g. "1.92.1" */
    uint32_t imgui_version_num;
    /* data-layout fingerprint; the SDK compares its own values and refuses
     * (no IM_ASSERT abort) on any difference */
    uint32_t sizeof_io, sizeof_style, sizeof_vec2, sizeof_vec4, sizeof_drawvert, sizeof_drawidx;
    uint32_t sizeof_context; /* [F7] sizeof(ImGuiContext) of the host (was 'reserved', always 0 before) */
    void* ctx;
    /* Returns the host ImGuiContext* (null before the GUI host is up). */
    void* (*get_context)(void* ctx);
    void (*get_allocators)(void* ctx, mbm_gui_alloc_fn* alloc_fn, mbm_gui_free_fn* free_fn, void** user);
    /* The render callback runs on the game thread inside the host's ImGui
     * frame, inside an open tab item; the mod draws with ImGui:: calls. */
    mbm_status (*register_tab)(void* ctx, uint64_t mod, const char* name, mbm_gui_render_fn fn, void* user, uint64_t* out_id);
    mbm_status (*unregister_tab)(void* ctx, uint64_t mod, uint64_t id);
    int32_t (*is_open)(void* ctx);
    /* [F7] Platform/mbm_imgui_fingerprint.h: config macros + version + sizes
     * of every shared ImGui structure. The SDK refuses on any difference. */
    uint32_t config_fingerprint;
    uint32_t reserved2;
} mbm_gui_v1;

/* ---------------- host -> mod services ---------------- */
typedef struct mbm_host_v1
{
    uint32_t magic;       /* MBM_ABI_MAGIC   */
    uint32_t abi_version; /* MBM_ABI_VERSION */
    uint32_t struct_size; /* sizeof(mbm_host_v1) of the host */
    uint32_t reserved;
    const char* host_build_id;
    void* ctx;

    void (*log)(void* ctx, uint64_t mod, int32_t level, const char* utf8);

    /* objects (call from the game thread or from a lifecycle callback) */
    mbm_status (*find_object)(void* ctx, uint64_t mod, const char* path, mbm_object* out);
    mbm_status (*find_first_of)(void* ctx, uint64_t mod, const char* class_short_name, mbm_object* out);
    mbm_status (*find_all_of)(void* ctx, uint64_t mod, const char* class_short_name, mbm_object* out, int32_t cap, int32_t* total);
    mbm_status (*object_name)(void* ctx, uint64_t mod, mbm_object obj, int32_t full, char* buf, int32_t cap, int32_t* len);
    mbm_status (*object_class)(void* ctx, uint64_t mod, mbm_object obj, mbm_object* out);
    mbm_status (*object_is_a)(void* ctx, uint64_t mod, mbm_object obj, const char* class_short_name, int32_t* out);
    mbm_status (*object_is_valid)(void* ctx, uint64_t mod, mbm_object obj, int32_t* out);

    /* reflected properties, by name, searched through the class chain */
    mbm_status (*property_get)(void* ctx, uint64_t mod, mbm_object obj, const char* property, mbm_value* inout);
    mbm_status (*property_set)(void* ctx, uint64_t mod, mbm_object obj, const char* property, const mbm_value* in);
    /* address of a non-bitfield scalar/object property, after a type check */
    mbm_status (*property_address)(void* ctx, uint64_t mod, mbm_object obj, const char* property, uint32_t type, void** out);
    /* call a UFunction with scalar/object/name/string parameters (game thread) */
    mbm_status (*call_function)(void* ctx, uint64_t mod, mbm_object obj, const char* function, const mbm_value* args, int32_t argc, mbm_value* ret);

    /* hooks: every registration goes through admission first */
    mbm_status (*hook_admission)(void* ctx, uint32_t family, mbm_admission* out);
    mbm_status (*register_ufunction_hook)(void* ctx, uint64_t mod, const char* function_path, mbm_hook_fn pre, mbm_hook_fn post, void* user, uint64_t* out_id);
    mbm_status (*register_hook)(void* ctx, uint64_t mod, uint32_t family, mbm_tick_fn fn, void* user, uint64_t* out_id);
    mbm_status (*register_key_down)(void* ctx, uint64_t mod, uint32_t key, uint32_t modifier_mask, mbm_key_fn fn, void* user, uint64_t* out_id);
    mbm_status (*execute_in_game_thread)(void* ctx, uint64_t mod, mbm_task_fn fn, void* user);
    mbm_status (*unregister)(void* ctx, uint64_t mod, uint64_t id);

    const mbm_gui_v1* gui; /* null = GUI host not in this build (#11) */

    /* Optional append-only service: creates an engine object, game-thread only.
     * This is a STATE CHANGE, not hook admission or a DATA_ONLY capability.
     * Default parameters only: Name=None, no flags/template/instance graph.
     * class_object must identify UClass; outer may be null. out is null on error.
     * ABI version remains 1. Check struct_size through this field before use. */
    mbm_status (*construct_object)(void* ctx, uint64_t mod,
                                  mbm_object class_object,
                                  mbm_object outer_object, mbm_object* out);
} mbm_host_v1;

/* Consumers must call this before touching the optional extension field. */
static inline int mbm_host_has_construct_object(const mbm_host_v1* host)
{
    uint32_t header[3];
    if (!host) return 0;
    /* Copy only the ABI prefix until an older/smaller table is excluded. */
    memcpy(header, (const void*)host, sizeof(header));
    return header[0] == MBM_ABI_MAGIC && header[1] == MBM_ABI_VERSION &&
           header[2] >= offsetof(mbm_host_v1, construct_object) + sizeof(host->construct_object) &&
           host->construct_object != 0;
}

/* ---------------- mod -> host description ---------------- */
typedef struct mbm_mod_v1
{
    uint32_t magic;       /* MBM_ABI_MAGIC   */
    uint32_t abi_version; /* MBM_ABI_VERSION */
    uint32_t struct_size; /* sizeof(mbm_mod_v1) of the SDK */
    uint32_t reserved;
    const char* name;
    const char* version;
    const char* authors;
    const char* description;
    const char* intended_sdk;
    void* instance;
    mbm_status (*on_program_start)(void* instance);
    mbm_status (*on_unreal_init)(void* instance);
    mbm_status (*on_ui_init)(void* instance);
    mbm_status (*on_update)(void* instance);
    mbm_status (*on_cpp_mods_loaded)(void* instance);
    mbm_status (*on_dll_load)(void* instance, const char* utf8_name);
    /* = upstream uninstall_mod: destroys the mod object. Called once, after
     * the host revoked every registration and drained in-flight callbacks. */
    mbm_status (*destroy)(void* instance);
    /* text of the last fault (static or instance-owned), or null */
    const char* (*last_error)(void* instance);
} mbm_mod_v1;

/* The single export. mod_token identifies this mod in every host call. On
 * MBM_OK *out_mod stays valid until destroy() returns. */
typedef mbm_status (*mbm_mod_entry_v1_fn)(const mbm_host_v1* host, uint64_t mod_token, const mbm_mod_v1** out_mod);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MBM_NATIVE_MOD_ABI_H */
