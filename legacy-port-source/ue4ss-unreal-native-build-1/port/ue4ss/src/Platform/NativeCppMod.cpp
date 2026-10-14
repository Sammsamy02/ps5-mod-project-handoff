#include <Platform/NativeCppMod.hpp>
#include <Platform/CppModModule.hpp>
#include <utility>
#include <stdexcept>
namespace RC::Platform
{
    NativeCppMod::NativeCppMod(NativeCppModSpec spec) : m_spec(std::move(spec)) {}
    NativeCppMod::~NativeCppMod() = default;
    auto NativeCppMod::start_function() const -> Start
    {
        if (m_retired) throw std::logic_error("Native C++ module was retired");
        return m_start;
    }
    auto NativeCppMod::uninstall_function() const -> Uninstall
    {
        if (m_retired) throw std::logic_error("Native C++ module was retired");
        return m_uninstall;
    }
    auto NativeCppMod::Retire() const -> void
    {
        // Serialize with adoption/callbacks. Existing owners may retain this
        // record, but cannot obtain usable exports after host shutdown begins.
        m_retired = true;
        if (m_module) m_module->Close();
        m_module.reset();
    }

    auto NativeCppMod::Prepare(NativeCppModSpec spec) -> std::shared_ptr<const NativeCppMod>
    {
        // Allocate both record and shared control block before target loading.
        auto prepared = std::shared_ptr<NativeCppMod>(new NativeCppMod(std::move(spec)));
        prepared->m_module = CppModModule::Open(prepared->m_spec.module_path);
        prepared->m_start = reinterpret_cast<Start>(prepared->m_module->FindExport("start_mod"));
        prepared->m_uninstall = reinterpret_cast<Uninstall>(prepared->m_module->FindExport("uninstall_mod"));
        return prepared;
    }
}
