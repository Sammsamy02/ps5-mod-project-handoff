// pe_loader.h -- minimal PE32+ image mapper for the cap13 native DLL route.
//
// Design contract (DESIGN/native-dll-loader.md section 3):
//   * the source DLL file is READ ONLY -- bytes on disk are never modified;
//     relocations and import resolution happen exclusively in the mapped copy;
//   * a load failure is a precise refusal: LoadResult carries a named reason
//     plus every missing import, never a null-filled IAT or silent stub;
//   * images are mapped once and never unmapped (retained-module law of the
//     #9 host: a mod may have started threads; unloading is never safe);
//   * a PE with a static TLS directory is refused (TEB/gs wall, design 3e).
//
// Host-portable by design: the memory primitives are isolated in Os namespace
// so macOS (SysV x86_64) can unit-test the whole mapper without a console.
// CAP13 proposal-only. Not built, not integrated, no console authorization.
#ifndef CAP13_PE_LOADER_H
#define CAP13_PE_LOADER_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace cap13 {

// Resolution source for one import: the shim registry or a mapped PE module.
class ImportResolver
{
  public:
    virtual ~ImportResolver() = default;
    // Return the address for "dll_name!symbol" (or ordinal), or nullptr when
    // unknown. The loader treats nullptr as a refusal, not as a stub.
    virtual void* resolve(const char* dll_name, const char* symbol, uint64_t ordinal) = 0;
};

struct LoadOptions
{
    // PS5 page size; macOS uses 16 KiB too on arm64 hosts, matching tests.
    uint32_t page_size = 0x4000;
    // When true, a permission partition that would require W+X in one page
    // is refused (default: refuse unless the J1 console probe proved RWX).
    bool allow_rwx_partition = false;
};

struct LoadResult
{
    bool ok = false;
    std::string reason;               // human-readable refusal (design 3: named)
    std::vector<std::string> missing; // every "dll!symbol" not resolved
    uint8_t* image = nullptr;         // mapped image base (load address)
    uint64_t image_size = 0;
    uint64_t preferred_base = 0;
    uint64_t slide = 0;               // load - ImageBase
    uint32_t entry_rva = 0;           // AddressOfEntryPoint (0 = none)
    // Export lookup after a successful map (start_mod / uninstall_mod /
    // mbm_mod_entry_v1). Names are matched exactly (x64 exports are flat).
    void* find_export(const char* name) const;

    // Export/bookkeeping state; defined in pe_loader.cpp. Owned by the
    // LoadResult and intentionally never freed: the image is retained for
    // the process lifetime (retained-module law).
    struct Impl
    {
        uint8_t* image = nullptr;
        uint32_t export_dir_rva = 0;
        uint32_t export_dir_size = 0;
    };
    Impl* impl = nullptr;
};

// Map the PE at 'file_bytes/file_size' through 'resolver'. Refuses with a
// named reason on: not PE32+/x64, CLR/COM descriptor, static TLS directory,
// RWX-requiring 16 KiB partition (unless allowed), unknown relocation type,
// unresolvable import (all missing symbols listed in one pass).
LoadResult pe_load(const void* file_bytes, size_t file_size, ImportResolver& resolver,
                   const LoadOptions& options);

// Read-only helpers reused by the classifier handoff (section 10 of the
// design): true when a TLS directory / COM descriptor is present.
bool pe_has_tls_directory(const void* file_bytes, size_t file_size);
bool pe_has_com_descriptor(const void* file_bytes, size_t file_size);

} // namespace cap13

#endif // CAP13_PE_LOADER_H
