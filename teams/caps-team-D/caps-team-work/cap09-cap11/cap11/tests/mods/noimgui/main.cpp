// A mod that registers a GUI tab but was built WITHOUT the SDK's ImGui
// (-DMBM_SDK_WITH_IMGUI missing): its tab must be refused, never crash. Test-only.
#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
class NoImGui : public RC::CppUserModBase
{
  public:
    NoImGui()
    {
        ModName = STR("NoImGui");
        register_tab(STR("No ImGui"), [](RC::CppUserModBase*) {});
    }
};
extern "C"
{
    UE4SS_EXPORT RC::CppUserModBase* start_mod() { return new NoImGui(); }
    UE4SS_EXPORT void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}
