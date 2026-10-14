// NativeGuiProbe -- the #11 launch mod and the host coverage driver.
//
// A generic UE4SS-style C++ mod with one GUI tab (register_tab +
// UE4SS_ENABLE_IMGUI, the upstream pattern). Every frame its tab calls each of
// the 76 Dear ImGui functions the selected real UE4SS C++ mods use
// (EventViewerMod + KismetDebuggerMod, inventory reproduced in
// cap09/tests/output/reverify/ACTUAL-SURFACE.rerun.json) and counts the calls.
// It also shows something the player can act on: a button that doubles the
// player's movement speed through the native mod ABI (game thread), so the
// launch shows render + input + action.
//
// Log markers (LAUNCH-CARD.md): [NGUI] constructed / ui_init imgui=ok /
// frame n=1 / coverage 76/76 / button speed=... / tab closed+reopened.
#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
#include <UE4SSProgram.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <cstring>
#include <map>
#include <string>

using namespace RC;
using namespace RC::Unreal;

namespace
{
    std::map<std::string, uint64_t>& counters()
    {
        static std::map<std::string, uint64_t> c;
        return c;
    }
#define NG(name, ...) (++counters()[#name], ImGui::name(__VA_ARGS__))
} // namespace

class NativeGuiProbe : public CppUserModBase
{
    bool m_imgui{false};
    uint64_t m_frames{0};
    bool m_check{true};
    float m_value{0.5f};
    int m_scalar{7};
    char m_text[64] = "native mod text";
    int m_selected{0};
    bool m_fast{false};
    bool m_reported{false};

    auto toggle_speed() -> void
    {
        Sdk::execute_in_game_thread([this] {
            auto* pc = UObjectGlobals::FindFirstOf(STR("PlayerController"));
            auto pawn = pc ? pc->Get<UObject*>(STR("Pawn")) : std::nullopt;
            if (!pawn || !*pawn)
            {
                Output::send(STR("[NGUI] button: no player pawn\n"));
                return;
            }
            m_fast = !m_fast;
            const bool ok = (*pawn)->Set<float>(STR("CustomTimeDilation"), m_fast ? 2.0f : 1.0f);
            Output::send(STR("[NGUI] button speed={:.2f} write={}\n"), m_fast ? 2.0f : 1.0f, ok);
        });
    }

  public:
    NativeGuiProbe()
    {
        ModName = STR("NativeGuiProbe");
        ModVersion = STR("1.0");
        ModDescription = STR("Generic native GUI tab probe for the PS5 Mod Bridge (#11)");
        ModAuthors = STR("PS5 Mod Bridge");
        register_tab(STR("GUI Probe"), [](CppUserModBase* self) { static_cast<NativeGuiProbe*>(self)->render(); });
        Output::send(STR("[NGUI] constructed\n"));
    }
    ~NativeGuiProbe() override { Output::send(STR("[NGUI] destroyed frames={}\n"), m_frames); }

    auto on_ui_init() -> void override
    {
        m_imgui = UE4SS_ENABLE_IMGUI();
        Output::send(STR("[NGUI] ui_init imgui={}\n"), m_imgui ? STR("ok") : STR("refused"));
    }

    auto render() -> void
    {
        ++m_frames;
        if (m_frames == 1) Output::send(STR("[NGUI] frame n=1\n"));

        NG(TextUnformatted, "Native C++ mod GUI tab");
        NG(Text, "frame %llu", static_cast<unsigned long long>(m_frames));
        NG(TextColored, ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "colored text");
        NG(TextDisabled, "disabled text");
        if (NG(Button, "Double player speed")) toggle_speed();
        NG(SetItemDefaultFocus);
        NG(SetItemTooltip, "toggles CustomTimeDilation 1.0 <-> 2.0");
        NG(SameLine);
        NG(Checkbox, "check", &m_check);
        NG(Separator);

        NG(PushID, "ids");
        NG(Indent);
        NG(SetNextItemWidth, 200.0f);
        NG(InputScalar, "scalar", ImGuiDataType_S32, &m_scalar);
        NG(InputText, "text", m_text, sizeof m_text);
        NG(Unindent);
        NG(PopID);

        NG(PushStyleColor, ImGuiCol_Text, ImVec4(1, 0.8f, 0.2f, 1));
        NG(PushStyleVar, ImGuiStyleVar_FrameRounding, 4.0f);
        NG(PushTextWrapPos, 400.0f);
        NG(TextUnformatted, "wrapped and styled text, long enough to wrap at the configured position inside this tab");
        NG(PopTextWrapPos);
        NG(PopStyleVar);
        NG(PopStyleColor);

        NG(BeginGroup);
        NG(Dummy, ImVec2(10, 10));
        NG(EndGroup);
        (void)NG(GetItemRectMin);
        (void)NG(GetItemRectMax);
        (void)NG(IsItemHovered);
        (void)NG(IsItemActive);
        (void)NG(IsItemActivated);
        (void)NG(IsWindowFocused);

        const ImVec2 avail = NG(GetContentRegionAvail);
        (void)NG(GetContentRegionMax);
        (void)NG(GetCursorPosX);
        NG(SetCursorPosX, NG(GetCursorPosX));
        const ImVec2 sp = NG(GetCursorScreenPos);
        (void)NG(GetCursorStartPos);
        const float fs = NG(GetFontSize);
        (void)NG(GetFrameHeightWithSpacing);
        (void)NG(GetTextLineHeight);
        (void)NG(GetTextLineHeightWithSpacing);
        (void)NG(GetStyle);
        (void)NG(GetMainViewport);
        NG(GetWindowDrawList)->AddRect(sp, ImVec2(sp.x + fs, sp.y + fs), IM_COL32(255, 255, 0, 255));

        // Coverage burst (frames 1..8): force the closed-by-default widgets open
        // once so their Begin/End pairs run; afterwards the tab stays calm so
        // pad navigation can reach the button.
        const bool burst = m_frames <= 8;
        if (NG(CollapsingHeader, "Lists", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (burst) ImGui::SetNextItemOpen(true);
            if (NG(TreeNode, "tree"))
            {
                for (int i = 0; i < 3; ++i)
                    if (NG(Selectable, i == 0 ? "row 0" : i == 1 ? "row 1" : "row 2", m_selected == i)) m_selected = i;
                NG(TreePop);
            }
            if (burst && m_frames == 2)
            {
                const ImGuiID combo_id = ImGui::GetID("combo");
                ImGui::OpenPopupEx(ImHashStr("##ComboPopup", 0, combo_id)); // what clicking the combo does
            }
            if (NG(BeginCombo, "combo", "item"))
            {
                (void)NG(Selectable, "item", true);
                if (m_frames > 3) ImGui::CloseCurrentPopup();
                NG(EndCombo);
            }
            if (NG(BeginTable, "table", 2))
            {
                NG(TableNextRow);
                NG(TableSetColumnIndex, 0);
                NG(TextUnformatted, "cell 0");
                NG(TableSetColumnIndex, 1);
                NG(TextUnformatted, "cell 1");
                NG(EndTable);
            }
        }

        if (NG(BeginChild, "child", ImVec2(avail.x > 50 ? avail.x - 20 : 30, 80), ImGuiChildFlags_Borders))
        {
            for (int i = 0; i < 8; ++i) NG(Text, "child line %d", i);
            NG(SetScrollHereY, 0.5f);
        }
        NG(EndChild);

        NG(BeginDisabled, true);
        (void)NG(Button, "disabled button");
        NG(EndDisabled);

        if (NG(BeginItemTooltip))
        {
            NG(TextUnformatted, "item tooltip");
            NG(EndTooltip);
        }
        if (burst && m_frames % 2 == 0 && NG(BeginTooltip))
        {
            NG(TextUnformatted, "tooltip");
            NG(EndTooltip);
        }

        // popups: open on some frames, render, close
        if (burst && m_frames % 3 == 1) NG(OpenPopup, "popup");
        if (NG(BeginPopup, "popup"))
        {
            (void)NG(MenuItem, "menu item");
            NG(CloseCurrentPopup);
            NG(EndPopup);
        }
        if (burst && m_frames % 3 == 2)
        {
            NG(SetNextWindowPos, NG(GetMainViewport)->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            NG(OpenPopup, "modal");
        }
        if (NG(BeginPopupModal, "modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            NG(TextUnformatted, "modal");
            NG(CloseCurrentPopup);
            NG(EndPopup);
        }
        (void)NG(Selectable, "context target");
        if (NG(BeginPopupContextItem, "ctx"))
        {
            NG(TextUnformatted, "context");
            NG(EndPopup);
        }

        // logging / clipboard family (EventViewer "copy" buttons)
        NG(LogToClipboard);
        ImGui::LogRenderedText(nullptr, "logged text");
        ++counters()["LogRenderedText"];
        NG(LogFinish);
        NG(SetClipboardText, "clipboard text");
        NG(ClearActiveID);

        if (!m_reported && m_frames >= 9)
        {
            m_reported = true;
            Output::send("[NGUI] coverage {}/76 distinct ImGui functions exercised\n", counters().size());
            std::string list;
            for (auto& [name, n] : counters()) list += name + "=" + std::to_string(n) + " ";
            Output::send("[NGUI] coverage-list {}\n", list);
        }
    }
};

extern "C"
{
    UE4SS_EXPORT RC::CppUserModBase* start_mod() { return new NativeGuiProbe(); }
    UE4SS_EXPORT void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}
