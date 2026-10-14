#include <Platform/CppModModule.hpp>
#ifndef __PROSPERO__
#include <dlfcn.h>
#endif
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

#ifdef __PROSPERO__
// Public MIT prosperodev/libkernel.h, commit
// 3cdd09cc86ba90ef371bd0a834e07cc3883c49e7, lines 94 and 156-161.
// Fixed-width spellings preserve the published SceKernelModule ABI without
// replacing SDK headers. The retained SDK rtld_sprx call sites also use zero
// argc/argv/flags/options/result for these module load/unload operations.
extern "C" int sceKernelLoadStartModule(const char*, std::size_t, const void*, std::uint32_t, void*, int*);
extern "C" int sceKernelDlsym(std::uint32_t, const char*, void**);
extern "C" int sceKernelStopUnloadModule(std::uint32_t, std::size_t, const void*, std::uint32_t, void*, int*);
#endif

namespace RC::Platform {
namespace {
#ifdef __PROSPERO__
auto loader_error(const char* operation, int code) -> std::runtime_error {
    return std::runtime_error(std::string(operation)+": native result "+std::to_string(code));
}
#else
auto loader_error(const char* operation) -> std::runtime_error {
    const char* detail=dlerror();
    return std::runtime_error(std::string(operation)+": "+(detail ? detail : "native loader supplied no detail"));
}
#endif
}
auto CppModModule::Open(const std::filesystem::path& path) -> std::unique_ptr<CppModModule> {
    const auto& native=path.native();
    if (!path.is_absolute() || native.empty() || native.size()>1023 || native.find('\0')!=native.npos)
        throw std::invalid_argument("Native mod module requires an absolute path of at most 1023 bytes without NUL");
    // Inspect a format marker only; native loader parsing remains authoritative.
    // No PE module is passed off as a native artifact or a successful translation.
    {
        std::unique_ptr<std::FILE, decltype(&std::fclose)> file(std::fopen(path.c_str(),"rb"),&std::fclose);
        if (!file) throw std::runtime_error("Cannot open native mod module");
        unsigned char marker[2];
        if (std::fread(marker,1,sizeof(marker),file.get())!=sizeof(marker))
            throw std::runtime_error("Native mod module is unreadable or truncated");
        if (marker[0]=='M' && marker[1]=='Z')
            throw std::runtime_error("Windows PE mod requires an unimplemented binary/ABI translation; native loader cannot run it");
    }
    // Allocate the owner before loading so allocation failure cannot leak a module.
    auto module=std::unique_ptr<CppModModule>(new CppModModule);
#ifdef __PROSPERO__
    const int handle=sceKernelLoadStartModule(path.c_str(),0,nullptr,0,nullptr,nullptr);
    if (handle<0) throw loader_error("Native mod load failed",handle);
    module->m_native_handle=static_cast<std::uint32_t>(handle);
    module->m_native_state=NativeState::Loaded;
#else
    (void)dlerror();
    module->m_handle=dlopen(path.c_str(),RTLD_NOW | RTLD_LOCAL);
    if (!module->m_handle) throw loader_error("Native mod load failed");
#endif
    return module;
}
auto CppModModule::FindExport(std::string_view name) const -> void* {
#ifdef __PROSPERO__
    if (m_native_state!=NativeState::Loaded)
        throw std::logic_error("Native mod module is closed or an unload was already attempted");
#else
    if (!m_handle) throw std::logic_error("Native mod module is closed");
#endif
    if (name.empty() || name.find('\0')!=name.npos)
        throw std::invalid_argument("Mod export name must be nonempty without NUL");
    const std::string terminated(name);
#ifdef __PROSPERO__
    void* symbol=nullptr;
    const int code=sceKernelDlsym(m_native_handle,terminated.c_str(),&symbol);
    if (code!=0) throw loader_error("Native mod export lookup failed",code);
    if (!symbol) throw std::runtime_error("Native mod export lookup returned null");
#else
    (void)dlerror();
    auto* symbol=dlsym(m_handle,terminated.c_str());
    if (!symbol) throw loader_error("Native mod export lookup failed");
    // The retained SDK does not clear stale errors on successful lookup.
    // A lifecycle export must be non-null; do not reject it due to old error text.
#endif
    return symbol;
}
auto CppModModule::Close() -> void {
#ifdef __PROSPERO__
    if (m_native_state==NativeState::Empty) return;
    if (m_native_state==NativeState::RetainedAfterFailedUnload)
        throw loader_error("Native mod unload previously failed; handle retained without retry",m_native_unload_error);
    // Mark the attempt before calling module_stop. On error retain the handle
    // and never implicitly retry a potentially non-idempotent stop operation.
    m_native_state=NativeState::RetainedAfterFailedUnload;
    m_native_unload_error=sceKernelStopUnloadModule(m_native_handle,0,nullptr,0,nullptr,nullptr);
    if (m_native_unload_error!=0)
        throw loader_error("Native mod unload failed; handle retained without retry",m_native_unload_error);
    m_native_handle=0;
    m_native_state=NativeState::Empty;
#else
    // The SDK consumes its handle even when the underlying close fails.
    auto* handle=std::exchange(m_handle,nullptr);
    if (handle && dlclose(handle)!=0) throw loader_error("Native mod unload failed");
#endif
}
CppModModule::~CppModModule() noexcept {
#ifdef __PROSPERO__
    if (m_native_state!=NativeState::Loaded) return;
#endif
    try { Close(); }
    catch (const std::exception& error) { std::fprintf(stderr,"[UE4SS native module] %s\n",error.what()); }
    catch (...) { std::fputs("[UE4SS native module] unload failed\n",stderr); }
}
}
