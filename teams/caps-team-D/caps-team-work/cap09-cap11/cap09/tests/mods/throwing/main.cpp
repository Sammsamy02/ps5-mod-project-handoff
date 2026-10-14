// A mod whose on_update throws: the SDK must contain the exception, the host
// must disable the mod and keep running. Test-only.
#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
#include <stdexcept>
class Thrower : public RC::CppUserModBase
{
  public:
    Thrower() { ModName = STR("Thrower"); }
    auto on_update() -> void override { throw std::runtime_error("deliberate on_update failure"); }
};
extern "C"
{
    UE4SS_EXPORT RC::CppUserModBase* start_mod() { return new Thrower(); }
    UE4SS_EXPORT void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}
