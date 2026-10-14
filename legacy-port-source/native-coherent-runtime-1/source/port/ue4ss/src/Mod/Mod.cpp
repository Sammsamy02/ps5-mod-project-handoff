#include <Mod/Mod.hpp>
#include <utility>

namespace RC
{

    Mod::Mod(UE4SSProgram& program, StringType&& mod_name, std::filesystem::path&& mod_path)
        : m_program(program), m_mod_id(s_next_mod_id++), m_mod_name(mod_name), m_mod_path(mod_path)
    {
    }

    auto Mod::get_name() const -> StringViewType
    {
        return m_mod_name;
    }

    auto Mod::set_installable(bool is_installable) -> void
    {
        m_installable = is_installable;
    }

    auto Mod::is_installable() const -> bool
    {
        return m_installable;
    }

    auto Mod::set_installed(bool is_installed) -> void
    {
        m_installed = is_installed;
    }

    auto Mod::is_installed() const -> bool
    {
        return m_installed;
    }

    auto Mod::is_started() const -> bool
    {
        return m_is_started;
    }

    auto Mod::fire_update() -> void
    {
    }

    auto Mod::update_async() -> void
    {
    }
} // namespace RC
