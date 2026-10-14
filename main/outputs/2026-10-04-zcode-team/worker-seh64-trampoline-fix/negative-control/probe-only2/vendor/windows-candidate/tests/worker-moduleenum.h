#ifndef MB_OWNED_WORKER_MODULEENUM_H
#define MB_OWNED_WORKER_MODULEENUM_H
/* Combined-payload fixture hook (worker-combined-dll-allfam): the KERNEL32
 * module enumeration / PSAPI family through actual PE imports. Five census
 * names are exercised — exactly the ones no other family claims
 * (GetModuleHandleExW, GetModuleFileNameA and the three K32 PSAPI names);
 * the shared module/loader names (GetModuleHandleA/W, GetProcAddress,
 * the LoadLibrary family and FreeLibrary) keep their pe_loader-backed
 * resolutions through
 * the loader bridge and are exercised there by the base fixture. The native
 * runner registers one static probe record ("mbmodenum.dll", one export
 * "MbProbe", image size 4096) while the consumer session is open, so every
 * lookup here is deterministic. Error returns -100..-104 are unique across
 * this tree's fixtures (crtstartup -90..-97, fileio -80..-89). */
typedef __UINT16_TYPE__ mb_mod_u16;
typedef __UINT32_TYPE__ mb_mod_u32;
typedef __INT32_TYPE__ mb_mod_i32;
__declspec(dllimport) mb_mod_i32 GetModuleHandleExW(mb_mod_u32, const mb_mod_u16 *, void **);
__declspec(dllimport) mb_mod_u32 GetModuleFileNameA(void *, char *, mb_mod_u32);
__declspec(dllimport) mb_mod_i32 K32EnumProcessModules(void *, void **, mb_mod_u32, mb_mod_u32 *);
__declspec(dllimport) mb_mod_u32 K32GetModuleBaseNameA(void *, void *, char *, mb_mod_u32);
__declspec(dllimport) mb_mod_i32 K32GetModuleInformation(void *, void *, void *, mb_mod_u32);
struct mb_mod_info { void *base; mb_mod_u32 size_of_image; void *entry_point; };
static const mb_mod_u16 mb_mod_name_wide[] = {'m','b','m','o','d','e','n','u','m','.','d','l','l',0};
static const char mb_mod_name_folded[] = "mbmodenum.dll";
static int prefix_equals(const char *buffer, const char *literal) {
    unsigned i = 0;
    while (literal[i]) { if (buffer[i] != literal[i])return 0; ++i; }
    return 1;
}
static mb_mod_i32 mb_worker_moduleenum_exercise(void) {
    void *base = 0;
    if(!GetModuleHandleExW(0, mb_mod_name_wide, &base) || !base)return -100;
    char name[64];
    mb_mod_u32 written = GetModuleFileNameA(base, name, (mb_mod_u32)sizeof(name));
    if(!written || !prefix_equals(name, mb_mod_name_folded))return -101;
    void *modules[1]; mb_mod_u32 needed = 0;
    if(!K32EnumProcessModules(0, modules, (mb_mod_u32)sizeof(modules), &needed) ||
       needed != (mb_mod_u32)sizeof(modules) || modules[0] != base)return -102;
    char base_name[64];
    written = K32GetModuleBaseNameA(0, base, base_name, (mb_mod_u32)sizeof(base_name));
    if(!written || !prefix_equals(base_name, mb_mod_name_folded))return -103;
    struct mb_mod_info info = {0, 0, 0};
    if(!K32GetModuleInformation(0, base, &info, (mb_mod_u32)sizeof(info)) ||
       info.base != base || info.size_of_image != 4096u || !info.entry_point)return -104;
    return 1;
}
#endif
