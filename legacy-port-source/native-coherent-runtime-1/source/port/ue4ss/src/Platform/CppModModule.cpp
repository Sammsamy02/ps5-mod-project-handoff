#include <Platform/CppModModule.hpp>
#include <dlfcn.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

namespace RC::Platform {
namespace {
// A real acquired loader reference is retained for this loaded process lifetime.
// No callback name/count or exported uninstall return proves OS/worker retirement.
// Reserve bounded storage before dlopen, whose initializers may reenter Open.
constexpr unsigned CodeRetentionCapacity = 64;
struct CodeReferenceSlot {
    std::atomic<unsigned> phase{}; // 0 free, 1 reserved, 2 retained
    void* handle{};
};
constinit std::array<CodeReferenceSlot, CodeRetentionCapacity> CodeReferences{};
class CodeReservation final {
public:
    CodeReservation() {
        for (auto& slot : CodeReferences) {
            unsigned expected = 0;
            if (slot.phase.compare_exchange_strong(expected, 1,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                m_slot = &slot;
                return;
            }
        }
        throw std::runtime_error("Native module code retention capacity exhausted before load");
    }
    CodeReservation(const CodeReservation&) = delete;
    auto operator=(const CodeReservation&) -> CodeReservation& = delete;
    ~CodeReservation() {
        if (!m_retained) m_slot->phase.store(0, std::memory_order_release);
    }
    void Retain(void* handle) noexcept {
        // Successful dlopen ownership is committed without allocation, locks,
        // callbacks or any other throwing work after activation.
        m_slot->handle = handle;
        m_retained = true;
        m_slot->phase.store(2, std::memory_order_release);
    }
private:
    CodeReferenceSlot* m_slot{};
    bool m_retained{};
};
auto loader_error(const char* operation) -> std::runtime_error {
    const char* detail=dlerror();
    return std::runtime_error(std::string(operation)+": "+(detail ? detail : "native loader supplied no detail"));
}
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
    CodeReservation reservation;
    (void)dlerror();
    module->m_handle=dlopen(path.c_str(),RTLD_NOW | RTLD_LOCAL);
    if (!module->m_handle) throw loader_error("Native mod load failed");
    reservation.Retain(module->m_handle);
    return module;
}
auto CppModModule::FindExport(std::string_view name) const -> void* {
    if (!m_handle) throw std::logic_error("Native mod module is closed");
    if (name.empty() || name.find('\0')!=name.npos)
        throw std::invalid_argument("Mod export name must be nonempty without NUL");
    const std::string terminated(name);
    (void)dlerror();
    auto* symbol=dlsym(m_handle,terminated.c_str());
    if (!symbol) throw loader_error("Native mod export lookup failed");
    // The retained SDK does not clear stale errors on successful lookup.
    // A lifecycle export must be non-null; do not reject it due to old error text.
    return symbol;
}
auto CppModModule::Close() -> void {
    // Release only this wrapper's lookup capability. Its real loader reference
    // remains in CodeReferences even after missing exports or startup unwind.
    // No retirement authority is available here, so never invoke dlclose.
    m_handle = nullptr;
}
CppModModule::~CppModModule() noexcept {
    try { Close(); }
    catch (const std::exception& error) { std::fprintf(stderr,"[UE4SS native module] %s\n",error.what()); }
    catch (...) { std::fputs("[UE4SS native module] unload failed\n",stderr); }
}
}
