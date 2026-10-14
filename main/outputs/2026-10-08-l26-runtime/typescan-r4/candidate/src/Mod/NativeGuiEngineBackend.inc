// NativeGuiEngineBackend.inc -- runtime binding of the native GUI host (#11).
//
// Included once by the candidate src/Mod/CppMod.cpp, AFTER
// NativeModEngineServices.inc (#9), inside #ifdef __PROSPERO__.
//
// Everything is drawn through the engine's OWN reflected functions, the same
// families the shipping Lua minimap / spawner menus already use on this
// console (canvas render target + UMG image + AddToViewport):
//   surfaces : CanvasRenderTarget2D::CreateCanvasRenderTarget2D, UMG UserWidget /
//              WidgetTree / CanvasPanel / Image built by StaticConstructObject,
//              CanvasPanel:AddChildToCanvas, CanvasPanelSlot:SetPosition/SetSize,
//              Image:SetBrushResourceObject, UserWidget:AddToViewport / SetVisibility
//   frame    : KismetRenderingLibrary ClearRenderTarget2D,
//              BeginDrawCanvasToRenderTarget -> Canvas:K2_DrawTriangle(texture,
//              TArray<CanvasUVTri>) -> EndDrawCanvasToRenderTarget
//   atlas    : KismetRenderingLibrary ImportBufferAsTexture2D(PNG bytes) (primary);
//              untextured runs drawn into a second render target (fallback)
//   input    : UE4SSProgram::register_keydown_event (pad mapping) -> GuiHost::post_key;
//              PlayerController SetIgnoreMoveInput / SetIgnoreLookInput while open
//   density  : WidgetLayoutLibrary GetViewportScale -> the panel is laid out in Slate units and its render
//              target has units x scale pixels, shown 1:1 (sharp text at any DPI) [FU-DPI]; re-checked on
//              open and every 120 GUI frames [FU3]
// Every function, parameter and struct member is resolved by NAME through
// reflection at run time; parameter frames and struct layouts come from the
// engine's own FProperty data (no offset, size or address in this file). A
// missing function / parameter fails closed with one "[GUI] refused: ..." line
// and the GUI stays off. Objects are re-validated every frame (a map change
// destroys viewport widgets) and rebuilt when stale.
#include <Platform/NativeGuiHost.hpp>
#include <Platform/StoredPng.hpp>
#include <Unreal/Core/Containers/ScriptArray.hpp>

#include <chrono>
#include <unordered_map>

// Dear ImGui itself, compiled into this unit (pinned 1.92.1, mbm_imconfig.h).
#include "NativeGuiImGuiUnity.inc"

void mbm_imgui_assert_failed(const char* expr, const char* file, int line)
{
    static std::atomic<int> logged{0};
    if (logged.fetch_add(1) >= 32) return;
    RC::Output::send<RC::LogLevel::Warning>(STR("[GUI] imgui assert (host) {} at {}:{}\n"), RC::Unicode::DecodeUtf8(expr ? expr : "?"),
                                            RC::Unicode::DecodeUtf8(file ? file : "?"), line);
}

namespace RC::Platform::NativeGui
{
    // Engine names (FName) compare case-insensitively: the display spelling is whichever spelling the game's name
    // table saw first (console-proven 2026-10-13: Townfall reflects CreateCanvasRenderTarget2D's "Width"/"Height"
    // as "width"/"height"). Compare the way the engine does: ASCII case folded, everything else exact.
    template <typename A, typename B>
    inline auto engine_name_eq(const A& a, const B& b) -> bool
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            auto x = static_cast<uint32_t>(a[i]), y = static_cast<uint32_t>(b[i]);
            if (x >= 'A' && x <= 'Z') x += 32;
            if (y >= 'A' && y <= 'Z') y += 32;
            if (x != y) return false;
        }
        return true;
    }

    class RuntimeGuiBackend final : public Backend
    {
        using UObject = Unreal::UObject;
        using UFunction = Unreal::UFunction;
        using FProperty = Unreal::FProperty;

        // ---------------- reflection helpers ----------------
        static auto W(const char* s) -> StringType { return Unicode::DecodeUtf8(std::string_view{s}); }
        static auto valid(UObject* o) -> bool
        {
            if (!o) return false;
            auto* item = Unreal::FUObjectArray::IndexToObject(o->GetInternalIndex());
            return item && item->GetUObject() == o && !item->IsUnreachable();
        }
        static auto find(const char* path) -> UObject* { return Unreal::UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, W(path)); }

        struct Frame
        {
            UFunction* fn{};
            std::vector<std::max_align_t> storage;
            auto data() -> uint8_t* { return reinterpret_cast<uint8_t*>(storage.data()); }
            std::vector<Unreal::FScriptArray*> arrays; // destroyed after the call (engine allocator)
            ~Frame()
            {
                for (auto* a : arrays) a->~FScriptArray();
            }
        };

        // (UFunction*, parameter name) -> FProperty*, resolved once. UFunctions
        // are engine-lifetime objects of native classes; the cache is dropped
        // whenever surfaces are rebuilt.
        std::unordered_map<std::string, FProperty*> m_param_cache;
        auto param(UFunction* fn, const char* name) -> FProperty*
        {
            std::string key = std::to_string(reinterpret_cast<uintptr_t>(fn)) + ":" + name;
            if (auto it = m_param_cache.find(key); it != m_param_cache.end()) return it->second;
            const auto w = W(name);
            FProperty* found{};
            for (FProperty* p : Unreal::TFieldRange<FProperty>(fn, Unreal::EFieldIterationFlags::IncludeDeprecated))
                if (p->HasAnyPropertyFlags(Unreal::CPF_Parm) && engine_name_eq(p->GetName(), w)) found = p;
            m_param_cache.emplace(std::move(key), found);
            return found;
        }

        // Zeroed parameter frame sized from the function's own reflected ParmsSize.
        static void frame_init(Frame& f, UFunction* fn)
        {
            f.fn = fn;
            const size_t size = static_cast<size_t>(fn->GetParmsSize());
            f.storage.assign(size / sizeof(std::max_align_t) + 2, std::max_align_t{});
            std::memset(f.data(), 0, f.storage.size() * sizeof(std::max_align_t));
        }
        auto frame_for(UObject* target, const char* fn_name, Frame& f) -> bool
        {
            if (!valid(target)) return refuse(std::string("target object for ") + fn_name + " is not live");
            UFunction* fn = target->GetFunctionByNameInChain(W(fn_name).c_str());
            if (!fn) return refuse(std::string("function not found: ") + fn_name);
            frame_init(f, fn);
            return true;
        }

        template <typename T>
        auto put(Frame& f, const char* name, T value) -> bool
        {
            FProperty* p = param(f.fn, name);
            if (!p) return refuse(std::string("parameter not found: ") + name);
            if (static_cast<size_t>(p->GetSize()) < sizeof(T)) return refuse(std::string("parameter too small: ") + name);
            std::memcpy(p->ContainerPtrToValuePtr<void>(f.data()), &value, sizeof(T));
            return true;
        }
        auto put_bool(Frame& f, const char* name, bool v) -> bool
        {
            FProperty* p = param(f.fn, name);
            if (!p || !p->IsA<Unreal::FBoolProperty>()) return refuse(std::string("bool parameter not found: ") + name);
            auto* bp = static_cast<Unreal::FBoolProperty*>(p);
            uint8_t* byte_value = static_cast<uint8_t*>(p->ContainerPtrToValuePtr<void>(f.data())) + bp->GetByteOffset();
            *byte_value = static_cast<uint8_t>((*byte_value & ~bp->GetFieldMask()) | (v ? bp->GetByteMask() : 0));
            return true;
        }

        // Write named float members of a struct (FVector2D X/Y, FLinearColor
        // R/G/B/A) honouring each member's own reflected type (float or double:
        // UE5 FVector2D is double, UE4 float).
        static auto write_struct_members(Unreal::UStruct* st, uint8_t* base, std::initializer_list<std::pair<const char*, double>> values) -> bool
        {
            for (auto& [n, v] : values)
            {
                FProperty* m{};
                const auto w = W(n);
                for (FProperty* p : Unreal::TFieldRange<FProperty>(st, Unreal::EFieldIterationFlags::IncludeDeprecated))
                    if (engine_name_eq(p->GetName(), w)) m = p;
                if (!m) return false;
                void* at = m->ContainerPtrToValuePtr<void>(base);
                if (m->IsA<Unreal::FDoubleProperty>()) std::memcpy(at, &v, sizeof v);
                else if (m->IsA<Unreal::FFloatProperty>()) { const float fv = static_cast<float>(v); std::memcpy(at, &fv, sizeof fv); }
                else return false;
            }
            return true;
        }
        auto put_struct(Frame& f, const char* name, std::initializer_list<std::pair<const char*, double>> values) -> bool
        {
            FProperty* p = param(f.fn, name);
            if (!p || !p->IsA<Unreal::FStructProperty>()) return refuse(std::string("struct parameter not found: ") + name);
            Unreal::UScriptStruct* st = static_cast<Unreal::FStructProperty*>(p)->GetStruct(); // TObjectPtr -> raw (same form as LuaUObject.cpp)
            if (!st || !write_struct_members(st, static_cast<uint8_t*>(p->ContainerPtrToValuePtr<void>(f.data())), values))
                return refuse(std::string("struct members not found for ") + name);
            return true;
        }
        template <typename T>
        auto get(Frame& f, const char* name, T* out) -> bool
        {
            FProperty* p = param(f.fn, name);
            if (!p) return refuse(std::string("parameter not found: ") + name);
            std::memcpy(out, p->ContainerPtrToValuePtr<void>(f.data()), sizeof(T));
            return true;
        }
        auto call(UObject* target, Frame& f) -> bool
        {
            target->ProcessEvent(f.fn, f.data());
            return true;
        }

        auto refuse(const std::string& why) -> bool
        {
            if (m_refusals++ < 8) log(MBM_LOG_ERROR, "[GUI] refused: " + why);
            return false;
        }

        // ---------------- state ----------------
        UObject* m_pc{};
        // [NOTE N7] FindFirstOf walks the whole object array: while the window
        // is open in a menu with no player controller, probe at most every
        // 500 ms instead of every frame.
        std::chrono::steady_clock::time_point m_next_pc_probe{};
        UObject* m_krl{};    // Default__KismetRenderingLibrary
        UObject* m_crt_cdo{};
        UObject* m_crt_class{};
        UObject* m_target{}; // GUI render target
        UObject* m_widget{};
        UObject* m_image{};
        UObject* m_keep_image{}; // hidden image that keeps the atlas referenced (GC)
        UObject* m_atlas{};
        UObject* m_blocked_pc{};  // the player the input block was applied to [F1]
        bool m_blocked_move{}, m_blocked_look{};
        int m_w{}, m_h{};         // panel size in Slate units (ImGui DisplaySize)
        int m_refusals{};
        bool m_visible{};
        int m_panel_x{}, m_panel_y{};
        // [FU1] true only after EVERY build step succeeded; a part-way build is never treated as built
        bool m_built{};
        // [FU1, review #1] part-way failures in a row (for m_failed_pc); at k_max_build_failures the GUI stops
        // trying (fail closed) until the window is reopened or the player changes
        static constexpr int k_max_build_failures = 3;
        int m_build_failures{};
        UObject* m_failed_pc{};
        // [FU-DPI] render target in pixels = units x m_px_scale (shown 1:1 by the units-sized slot)
        static constexpr int k_max_rt_side = 4096;
        float m_px_scale{1.0f};   // render-target pixels per unit (1.0 in the >4096 fallback)
        float m_scale{1.0f};      // viewport (DPI) scale the layout was built with
        int m_rt_w{}, m_rt_h{};
        // [FU3] cheap layout re-check while open: on open and every k_recheck_frames GUI frames
        static constexpr int k_recheck_frames = 120;
        int32_t m_vx{}, m_vy{};   // viewport size the layout was built with
        int m_recheck_frames{};
        bool m_recheck_now{};
        // [FU2] WidgetLayoutLibrary CDO, and whether the one "scale unavailable" warning was logged
        UObject* m_wll{};
        bool m_scale_warned{};

        static auto world_of(UObject* o) -> UObject*
        {
            for (UObject* outer = o ? o->GetOuterPrivate() : nullptr; outer && valid(outer); outer = outer->GetOuterPrivate())
            {
                auto* cls = outer->GetClassPrivate();
                if (cls && engine_name_eq(cls->GetName(), StringViewType{STR("World")})) return outer;
            }
            return nullptr;
        }

        auto construct(const char* class_path, UObject* outer) -> UObject*
        {
            auto* cls = find(class_path);
            if (!cls) { refuse(std::string("class not found: ") + class_path); return nullptr; }
            Unreal::FStaticConstructObjectParameters params{static_cast<Unreal::UClass*>(cls), outer};
            auto* o = Unreal::UObjectGlobals::StaticConstructObject(params);
            if (!o) refuse(std::string("could not construct ") + class_path);
            return o;
        }
        auto set_object_property(UObject* o, const char* prop, UObject* value) -> bool
        {
            auto* p = o->GetPropertyByNameInChain(W(prop).c_str());
            if (!p || !p->IsA<Unreal::FObjectProperty>()) return refuse(std::string("object property not found: ") + prop);
            std::memcpy(p->ContainerPtrToValuePtr<void>(static_cast<void*>(o)), &value, sizeof value);
            return true;
        }
        auto call_obj1(UObject* target, const char* fn, const char* pname, UObject* arg, UObject** ret = nullptr) -> bool
        {
            Frame f;
            if (!frame_for(target, fn, f) || !put(f, pname, arg) || !call(target, f)) return false;
            if (ret) return get(f, "ReturnValue", ret);
            return true;
        }

        auto make_crt(int w, int h) -> UObject*
        {
            Frame f;
            UObject* ret{};
            if (!frame_for(m_crt_cdo, "CreateCanvasRenderTarget2D", f) || !put(f, "WorldContextObject", m_pc) ||
                !put(f, "CanvasRenderTarget2DClass", m_crt_class) || !put<int32_t>(f, "Width", w) || !put<int32_t>(f, "Height", h) || !call(m_crt_cdo, f) ||
                !get(f, "ReturnValue", &ret) || !ret)
            {
                refuse("CreateCanvasRenderTarget2D returned nothing");
                return nullptr;
            }
            return ret;
        }

        auto clear(UObject* rt, double r, double g, double b, double a) -> bool
        {
            Frame f;
            return frame_for(m_krl, "ClearRenderTarget2D", f) && put(f, "WorldContextObject", m_pc) && put(f, "TextureRenderTarget", rt) &&
                   put_struct(f, "ClearColor", {{"R", r}, {"G", g}, {"B", b}, {"A", a}}) && call(m_krl, f);
        }

        // Begin -> K2_DrawTriangle per batch -> End, into `rt`.
        auto draw_into(UObject* rt, UObject* texture, const std::vector<Batch>& batches) -> bool
        {
            // Same preparation as the shipping Lua minimap: reset the world's shared
            // render-target canvas size so Begin re-sizes it for this target. The
            // world is found by the outer-chain walk the runtime's Lua GetWorld uses.
            if (UObject* world = world_of(m_pc); valid(world))
            {
                if (auto* cp = world->GetPropertyByNameInChain(STR("CanvasForRenderingToTarget")); cp && cp->IsA<Unreal::FObjectProperty>())
                {
                    UObject* canvas_obj{};
                    std::memcpy(&canvas_obj, cp->ContainerPtrToValuePtr<void>(static_cast<void*>(world)), sizeof canvas_obj);
                    if (valid(canvas_obj))
                        if (auto* sx = canvas_obj->GetPropertyByNameInChain(STR("SizeX")); sx && sx->IsA<Unreal::FIntProperty>())
                            std::memset(sx->ContainerPtrToValuePtr<void>(static_cast<void*>(canvas_obj)), 0, sizeof(int32_t));
                }
            }
            Frame begin;
            if (!frame_for(m_krl, "BeginDrawCanvasToRenderTarget", begin) || !put(begin, "WorldContextObject", m_pc) ||
                !put(begin, "TextureRenderTarget", rt) || !call(m_krl, begin))
                return false;
            UObject* canvas{};
            if (!get(begin, "Canvas", &canvas) || !valid(canvas)) return refuse("BeginDrawCanvasToRenderTarget gave no canvas");
            FProperty* ctx_param = param(begin.fn, "Context");
            if (!ctx_param) return refuse("BeginDrawCanvasToRenderTarget has no Context");
            bool ok = true;
            for (const auto& b : batches)
                if (!draw_triangles(canvas, texture, b.tris)) { ok = false; break; }
            Frame end;
            if (!frame_for(m_krl, "EndDrawCanvasToRenderTarget", end) || !put(end, "WorldContextObject", m_pc)) return false;
            FProperty* end_ctx = param(end.fn, "Context");
            if (!end_ctx || end_ctx->GetSize() != ctx_param->GetSize()) return refuse("EndDrawCanvasToRenderTarget Context mismatch");
            std::memcpy(end_ctx->ContainerPtrToValuePtr<void>(end.data()), ctx_param->ContainerPtrToValuePtr<void>(begin.data()),
                        static_cast<size_t>(ctx_param->GetSize()));
            call(m_krl, end);
            return ok;
        }

        // CanvasUVTri element layout, resolved ONCE from the parameter's own
        // reflected struct: offsets of V{0,1,2}_{Pos,UV,Color} and of their
        // X/Y and R/G/B/A members, each with its own float/double width.
        struct Member { int32_t off{-1}; bool dbl{}; };
        struct UvTriLayout
        {
            Unreal::UStruct* st{};
            int32_t esz{}, align{};
            Member pos[3][2], uv[3][2], col[3][4];
            bool ok{};
        } m_tri;
        static auto member_of(Unreal::UStruct* st, const char* name) -> FProperty*
        {
            const auto w = W(name);
            for (FProperty* p : Unreal::TFieldRange<FProperty>(st, Unreal::EFieldIterationFlags::IncludeDeprecated))
                if (engine_name_eq(p->GetName(), w)) return p;
            return nullptr;
        }
        auto resolve_tri_layout(FProperty* inner) -> bool
        {
            Unreal::UScriptStruct* st = static_cast<Unreal::FStructProperty*>(inner)->GetStruct(); // TObjectPtr -> raw (same form as LuaUObject.cpp)
            if (m_tri.ok && m_tri.st == st) return true;
            m_tri = {};
            m_tri.st = st;
            m_tri.esz = inner->GetSize();
            m_tri.align = inner->GetMinAlignment();
            if (!st || m_tri.esz <= 0) return refuse("CanvasUVTri layout unavailable");
            static const char* vnames[3][3] = {{"V0_Pos", "V0_UV", "V0_Color"}, {"V1_Pos", "V1_UV", "V1_Color"}, {"V2_Pos", "V2_UV", "V2_Color"}};
            static const char* xy[2] = {"X", "Y"};
            static const char* rgba[4] = {"R", "G", "B", "A"};
            auto fill = [&](FProperty* sp, Member* out, const char* const* names, int n) -> bool {
                if (!sp || !sp->IsA<Unreal::FStructProperty>()) return false;
                Unreal::UScriptStruct* ss = static_cast<Unreal::FStructProperty*>(sp)->GetStruct(); // TObjectPtr -> raw (same form as LuaUObject.cpp)
                for (int i = 0; i < n; ++i)
                {
                    auto* m = ss ? member_of(ss, names[i]) : nullptr;
                    if (!m) return false;
                    const bool dbl = m->IsA<Unreal::FDoubleProperty>();
                    if (!dbl && !m->IsA<Unreal::FFloatProperty>()) return false;
                    out[i].off = sp->GetOffset_Internal() + m->GetOffset_Internal();
                    out[i].dbl = dbl;
                    if (out[i].off < 0 || out[i].off + (dbl ? 8 : 4) > m_tri.esz) return false;
                }
                return true;
            };
            for (int k = 0; k < 3; ++k)
                if (!fill(member_of(st, vnames[k][0]), m_tri.pos[k], xy, 2) || !fill(member_of(st, vnames[k][1]), m_tri.uv[k], xy, 2) ||
                    !fill(member_of(st, vnames[k][2]), m_tri.col[k], rgba, 4))
                    return refuse("CanvasUVTri members not found / unexpected types");
            m_tri.ok = true;
            log(MBM_LOG_NORMAL, "[GUI] CanvasUVTri layout element=" + std::to_string(m_tri.esz) + " pos=" + (m_tri.pos[0][0].dbl ? "double" : "float"));
            return true;
        }
        static void put_num(uint8_t* e, const Member& m, double v)
        {
            if (m.dbl) std::memcpy(e + m.off, &v, sizeof v);
            else { const float f = static_cast<float>(v); std::memcpy(e + m.off, &f, sizeof f); }
        }

        // One Canvas:K2_DrawTriangle call with an engine-allocated TArray.
        auto draw_triangles(UObject* canvas, UObject* texture, const std::vector<UVTri>& tris) -> bool
        {
            if (tris.empty()) return true;
            Frame f;
            if (!frame_for(canvas, "K2_DrawTriangle", f) || !put(f, "RenderTexture", texture)) return false;
            FProperty* arr = param(f.fn, "Triangles");
            if (!arr || !arr->IsA<Unreal::FArrayProperty>()) return refuse("K2_DrawTriangle has no Triangles array");
            auto* inner = static_cast<Unreal::FArrayProperty*>(arr)->GetInner();
            if (!inner || !inner->IsA<Unreal::FStructProperty>()) return refuse("Triangles inner is not a struct");
            if (!resolve_tri_layout(inner)) return false;
            auto* a = new (arr->ContainerPtrToValuePtr<void>(f.data())) Unreal::FScriptArray{};
            f.arrays.push_back(a);
            a->AddZeroed(static_cast<int32_t>(tris.size()), m_tri.esz, static_cast<uint32_t>(m_tri.align));
            auto* base = static_cast<uint8_t*>(a->GetData());
            for (size_t i = 0; i < tris.size(); ++i)
            {
                uint8_t* e = base + i * static_cast<size_t>(m_tri.esz);
                const UVTri& t = tris[i];
                for (int k = 0; k < 3; ++k)
                {
                    put_num(e, m_tri.pos[k][0], t.pos[k][0]);
                    put_num(e, m_tri.pos[k][1], t.pos[k][1]);
                    put_num(e, m_tri.uv[k][0], t.uv[k][0]);
                    put_num(e, m_tri.uv[k][1], t.uv[k][1]);
                    for (int c = 0; c < 4; ++c) put_num(e, m_tri.col[k][c], t.color[k][c]);
                }
            }
            return call(canvas, f);
        }

        // Quiet read of PlayerController::GetViewportSize (no refusal line; callers decide).
        auto viewport_size(int32_t* vx, int32_t* vy) -> bool
        {
            if (!valid(m_pc)) return false;
            UFunction* fn = m_pc->GetFunctionByNameInChain(STR("GetViewportSize"));
            if (!fn) return false;
            FProperty* sx = param(fn, "SizeX");
            FProperty* sy = param(fn, "SizeY");
            if (!sx || !sy || static_cast<size_t>(sx->GetSize()) < sizeof(int32_t) || static_cast<size_t>(sy->GetSize()) < sizeof(int32_t)) return false;
            Frame f;
            frame_init(f, fn);
            call(m_pc, f);
            std::memcpy(vx, sx->ContainerPtrToValuePtr<void>(f.data()), sizeof(int32_t));
            std::memcpy(vy, sy->ContainerPtrToValuePtr<void>(f.data()), sizeof(int32_t));
            return *vx > 0 && *vy > 0;
        }

        // UMG canvas slots are laid out in Slate units, not viewport pixels: units = pixels / viewport (DPI)
        // scale (console-proven 2026-10-13: on a 3840x2160 viewport the pixel layout put the panel off-screen).
        // The scale comes from the engine's own WidgetLayoutLibrary::GetViewportScale, read by its reflected
        // return type (float in UE5 today; double accepted too).
        // [FU2] Unavailable or implausible = 1.0 with ONE warning per session. It is not a refusal (the GUI works,
        // only layout and sharpness differ), so it never logs a "[GUI] refused" line and uses no refusal-log slot.
        auto viewport_scale() -> float
        {
            std::string why;
            double s{};
            if (!valid(m_wll)) m_wll = find("/Script/UMG.Default__WidgetLayoutLibrary");
            UFunction* fn = valid(m_wll) ? m_wll->GetFunctionByNameInChain(STR("GetViewportScale")) : nullptr;
            FProperty* wc = fn ? param(fn, "WorldContextObject") : nullptr;
            FProperty* rp = fn ? param(fn, "ReturnValue") : nullptr;
            if (!valid(m_wll)) why = "WidgetLayoutLibrary not found";
            else if (!fn) why = "GetViewportScale not found";
            else if (!wc || !rp || static_cast<size_t>(wc->GetSize()) < sizeof(UObject*)) why = "GetViewportScale parameters not found";
            else
            {
                Frame f;
                frame_init(f, fn);
                std::memcpy(wc->ContainerPtrToValuePtr<void>(f.data()), &m_pc, sizeof m_pc);
                call(m_wll, f);
                const void* at = rp->ContainerPtrToValuePtr<void>(f.data());
                if (rp->IsA<Unreal::FFloatProperty>()) { float fv{}; std::memcpy(&fv, at, sizeof fv); s = fv; }
                else if (rp->IsA<Unreal::FDoubleProperty>()) std::memcpy(&s, at, sizeof s);
                else why = "GetViewportScale returns neither float nor double";
                if (why.empty() && !(s >= 0.25 && s <= 8.0)) why = "GetViewportScale returned " + std::to_string(static_cast<float>(s)); // float: the to_string overload already used
            }
            if (why.empty()) return static_cast<float>(s);
            if (!m_scale_warned)
            {
                m_scale_warned = true;
                log(MBM_LOG_WARNING, "[GUI] viewport scale unavailable (" + why + "); laying out with scale 1.0");
            }
            return 1.0f;
        }

        auto build_surfaces() -> bool
        {
            m_built = false; // [FU1] set again only after every step succeeded
            const auto now = std::chrono::steady_clock::now();
            if (now < m_next_pc_probe) return false; // [NOTE N7] throttled retry
            m_param_cache.clear();
            m_tri = {};
            m_pc = Unreal::UObjectGlobals::FindFirstOf(STR("PlayerController"));
            if (!valid(m_pc))
            {
                m_next_pc_probe = now + std::chrono::milliseconds{500};
                return false; // not in a world yet: retry in 500 ms, no log
            }
            if (m_pc != m_failed_pc) m_build_failures = 0; // another player: fresh tries
            // [FU1] a build that fails part-way is retried in 2 s, not every frame (each try may create a render
            // target and widgets), at most k_max_build_failures times in a row; then the GUI stays off (fail
            // closed, nothing shown, no input block) until the window is reopened or the player changes.
            m_next_pc_probe = now + std::chrono::milliseconds{2000};
            if (m_build_failures >= k_max_build_failures) return false;
            if (!build_steps())
            {
                m_failed_pc = m_pc;
                if (++m_build_failures == k_max_build_failures)
                    log(MBM_LOG_WARNING, "[GUI] surfaces could not be built " + std::to_string(k_max_build_failures) +
                                                 " times in a row; no more tries until the window is reopened or the player changes");
                return false;
            }
            m_build_failures = 0;
            m_failed_pc = nullptr;
            m_next_pc_probe = {};
            m_built = true; // [FU1]
            return true;
        }

        // Every build step; false = failed part-way (build_surfaces counts it, nothing is shown).
        auto build_steps() -> bool
        {
            m_krl = find("/Script/Engine.Default__KismetRenderingLibrary");
            m_crt_cdo = find("/Script/Engine.Default__CanvasRenderTarget2D");
            m_crt_class = find("/Script/Engine.CanvasRenderTarget2D");
            if (!valid(m_krl) || !valid(m_crt_cdo) || !m_crt_class) return refuse("rendering library / render target class not found");
            int32_t vx{}, vy{};
            if (!viewport_size(&vx, &vy)) return refuse("GetViewportSize failed");
            const float scale = viewport_scale();
            const int ux = std::max(1, static_cast<int>(static_cast<float>(vx) / scale));
            const int uy = std::max(1, static_cast<int>(static_cast<float>(vy) / scale));
            // panel: right part of the screen, like the shipping mod menus (position and size in Slate units)
            m_w = std::min(1100, ux * 6 / 10);
            m_h = std::min(900, uy * 8 / 10);
            m_panel_x = ux - m_w - ux / 40;
            m_panel_y = (uy - m_h) / 2;
            // [FU-DPI] The render target has units x scale PIXELS while its slot stays m_w x m_h units, so the
            // viewport shows it 1:1 (before: a units-sized target was magnified by the scale, 2x at 4K = blurry).
            // Capped at k_max_rt_side per side; above that it is drawn at scale 1 (softer, as before).
            float px_scale = scale;
            int rt_w = std::max(1, static_cast<int>(static_cast<float>(m_w) * px_scale + 0.5f));
            int rt_h = std::max(1, static_cast<int>(static_cast<float>(m_h) * px_scale + 0.5f));
            if (rt_w > k_max_rt_side || rt_h > k_max_rt_side)
            {
                log(MBM_LOG_WARNING, "[GUI] render target " + std::to_string(rt_w) + "x" + std::to_string(rt_h) + " would exceed " +
                                             std::to_string(k_max_rt_side) + " per side; drawing at scale 1.0 (text softer)");
                px_scale = 1.0f;
                rt_w = std::max(1, m_w);
                rt_h = std::max(1, m_h);
            }
            // [review #1] a retry after a part-way failure reuses the target it already made (same size), so
            // retries never pile up render targets before GC; a fresh build (first open, map change, layout
            // change) always makes a new one, as before.
            if (!(m_build_failures > 0 && valid(m_target) && m_rt_w == rt_w && m_rt_h == rt_h)) m_target = make_crt(rt_w, rt_h);
            if (!m_target) return false;
            m_rt_w = rt_w;
            m_rt_h = rt_h;
            m_widget = construct("/Script/UMG.UserWidget", m_pc);
            if (!m_widget) return false;
            auto* tree = construct("/Script/UMG.WidgetTree", m_widget);
            auto* canvas = tree ? construct("/Script/UMG.CanvasPanel", tree) : nullptr;
            m_image = canvas ? construct("/Script/UMG.Image", tree) : nullptr;
            m_keep_image = m_image ? construct("/Script/UMG.Image", tree) : nullptr;
            if (!m_keep_image) return false;
            if (!set_object_property(m_widget, "WidgetTree", tree) || !set_object_property(tree, "RootWidget", canvas)) return false;
            UObject* slot{};
            if (!call_obj1(canvas, "AddChildToCanvas", "Content", m_image, &slot) || !valid(slot)) return refuse("AddChildToCanvas failed");
            {
                Frame f;
                if (!frame_for(slot, "SetPosition", f) || !put_struct(f, "InPosition", {{"X", m_panel_x}, {"Y", m_panel_y}}) || !call(slot, f)) return false;
                Frame g;
                if (!frame_for(slot, "SetSize", g) || !put_struct(g, "InSize", {{"X", m_w}, {"Y", m_h}}) || !call(slot, g)) return false;
            }
            if (!call_obj1(m_image, "SetBrushResourceObject", "ResourceObject", m_target)) return false;
            UObject* keep_slot{};
            if (!call_obj1(canvas, "AddChildToCanvas", "Content", m_keep_image, &keep_slot)) return false;
            {
                // the keep image only holds a UPROPERTY reference to the font atlas (GC); it must never draw
                // (console-proven 2026-10-13: visible, it showed the atlas small at the top left). 1 = Collapsed.
                Frame f;
                if (!frame_for(m_keep_image, "SetVisibility", f) || !put<uint8_t>(f, "InVisibility", 1) || !call(m_keep_image, f))
                    return refuse("could not hide the atlas keep image");
            }
            {
                Frame f;
                if (!frame_for(m_widget, "AddToViewport", f) || !put<int32_t>(f, "ZOrder", 10000) || !call(m_widget, f)) return false;
            }
            set_visibility(m_visible);
            m_scale = scale;
            m_px_scale = px_scale;
            m_vx = vx;
            m_vy = vy;
            m_recheck_frames = 0;
            m_recheck_now = false;
            log(MBM_LOG_NORMAL, "[GUI] surfaces built panel=" + std::to_string(m_w) + "x" + std::to_string(m_h) + " rt=" + std::to_string(rt_w) + "x" +
                                        std::to_string(rt_h) + " viewport=" + std::to_string(vx) + "x" + std::to_string(vy) + " scale=" + std::to_string(scale) +
                                        " units=" + std::to_string(ux) + "x" + std::to_string(uy) + " pos=" + std::to_string(m_panel_x) + "," +
                                        std::to_string(m_panel_y));
            return true;
        }

        // [FU3] Cheap re-check while the window is open (on open, then every k_recheck_frames GUI frames): two
        // reflected calls. A changed viewport size or scale means the panel must be laid out again.
        auto layout_changed() -> bool
        {
            if (!m_recheck_now && ++m_recheck_frames < k_recheck_frames) return false;
            m_recheck_now = false;
            m_recheck_frames = 0;
            int32_t vx{}, vy{};
            if (!viewport_size(&vx, &vy)) return false; // unreadable now: keep the current surfaces
            const float scale = viewport_scale();
            if (vx == m_vx && vy == m_vy && scale == m_scale) return false;
            log(MBM_LOG_NORMAL, "[GUI] viewport changed " + std::to_string(m_vx) + "x" + std::to_string(m_vy) + " scale=" + std::to_string(m_scale) + " -> " +
                                        std::to_string(vx) + "x" + std::to_string(vy) + " scale=" + std::to_string(scale) + ": laying the panel out again");
            return true;
        }

        // [FU3] The old widget is still in the viewport when the layout changes (a map change destroys it, this
        // does not): hide it and drop its render-target / atlas references so GC can free them. Only functions the
        // build already uses (SetBrushResourceObject, SetVisibility).
        void retire_surfaces()
        {
            bool hidden{};
            if (valid(m_widget))
            {
                Frame f;
                if (frame_for(m_widget, "SetVisibility", f) && put<uint8_t>(f, "InVisibility", 1)) hidden = call(m_widget, f);
            }
            // [review #4] clear the brushes only once the widget is collapsed: an image with no resource paints a
            // white box. Not hidden = keep them (the old panel stays drawn rather than turning white).
            if (hidden && valid(m_image)) call_obj1(m_image, "SetBrushResourceObject", "ResourceObject", nullptr);
            if (hidden && valid(m_keep_image)) call_obj1(m_keep_image, "SetBrushResourceObject", "ResourceObject", nullptr);
            m_target = m_widget = m_image = m_keep_image = m_atlas = nullptr;
            m_built = false;
        }

        void set_visibility(bool v)
        {
            if (!valid(m_widget)) return;
            Frame f;
            // ESlateVisibility: 1 = Collapsed, 3 = HitTestInvisible (shown, never takes the mouse)
            if (frame_for(m_widget, "SetVisibility", f) && put<uint8_t>(f, "InVisibility", v ? 3 : 1)) call(m_widget, f);
        }

      public:
        std::atomic<bool> ready{false};

        void log(int32_t level, std::string_view text) override
        {
            const auto line = Unicode::DecodeUtf8(std::string(text));
            if (level == MBM_LOG_ERROR) Output::send<LogLevel::Error>(STR("{}\n"), line);
            else if (level == MBM_LOG_WARNING) Output::send<LogLevel::Warning>(STR("{}\n"), line);
            else Output::send(STR("{}\n"), line);
        }

        auto prepare(int* w, int* h, bool* rebuilt) -> bool override
        {
            *rebuilt = false;
            // [FU1] a part-way build (m_built false) is never "built": no frame, no input block, retried
            bool live = m_built && valid(m_target) && valid(m_widget) && valid(m_image) && valid(m_pc);
            if (live && layout_changed()) // [FU3] resolution / DPI scale changed while running
            {
                retire_surfaces();
                live = false;
            }
            if (!live)
            {
                m_atlas = nullptr; // surfaces gone (map change): atlas must be re-referenced too
                if (!build_surfaces()) return false;
                *rebuilt = true; // [FIX F2] the host re-uploads the atlas and redoes the input block
            }
            *w = m_w; // Slate units; the render target is m_rt_w x m_rt_h pixels
            *h = m_h;
            return true;
        }

        auto pixel_scale() -> float override { return m_px_scale; } // [FU-DPI]

        auto create_atlas(const std::vector<Run>& runs, int w, int h) -> ImTextureID override
        {
            // [review #2] fail closed instead of a huge RGBA / PNG allocation on the game thread (the host checks too)
            if (w <= 0 || h <= 0 || static_cast<int64_t>(w) * h > int64_t{4096} * 4096) return 0;
            // primary: the engine's own PNG import -> UTexture2D with real alpha
            std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4, 255);
            {
                std::vector<uint8_t> alpha(static_cast<size_t>(w) * h, 0);
                for (const auto& r : runs)
                    for (int y = r.y; y < r.y + r.h; ++y)
                        for (int x = r.x; x < r.x + r.w; ++x) alpha[static_cast<size_t>(y) * w + x] = r.alpha;
                for (size_t i = 0; i < alpha.size(); ++i) rgba[i * 4 + 3] = alpha[i];
            }
            const auto png = encode_png_rgba(rgba.data(), static_cast<uint32_t>(w), static_cast<uint32_t>(h));
            Frame f;
            UObject* tex{};
            if (frame_for(m_krl, "ImportBufferAsTexture2D", f) && put(f, "WorldContextObject", m_pc))
            {
                if (FProperty* arr = param(f.fn, "Buffer"); arr && arr->IsA<Unreal::FArrayProperty>())
                {
                    auto* a = new (arr->ContainerPtrToValuePtr<void>(f.data())) Unreal::FScriptArray{};
                    f.arrays.push_back(a);
                    a->AddZeroed(static_cast<int32_t>(png.size()), 1, 1);
                    std::memcpy(a->GetData(), png.data(), png.size());
                    call(m_krl, f);
                    get(f, "ReturnValue", &tex);
                }
            }
            if (valid(tex))
            {
                log(MBM_LOG_NORMAL, "[GUI] atlas route=png-import bytes=" + std::to_string(png.size()));
            }
            else
            {
                // fallback: draw the runs, untextured, into a transparent render target
                tex = make_crt(w, h);
                if (!tex || !clear(tex, 0, 0, 0, 0)) return 0;
                Batch b;
                for (const auto& r : runs)
                {
                    const float a = r.alpha / 255.0f;
                    const float x0 = r.x, y0 = r.y, x1 = static_cast<float>(r.x + r.w), y1 = static_cast<float>(r.y + r.h);
                    UVTri t1{{{x0, y0}, {x1, y0}, {x1, y1}}, {{0, 0}, {0, 0}, {0, 0}}, {{1, 1, 1, a}, {1, 1, 1, a}, {1, 1, 1, a}}};
                    UVTri t2{{{x0, y0}, {x1, y1}, {x0, y1}}, {{0, 0}, {0, 0}, {0, 0}}, {{1, 1, 1, a}, {1, 1, 1, a}, {1, 1, 1, a}}};
                    b.tris.push_back(t1);
                    b.tris.push_back(t2);
                }
                if (!draw_into(tex, nullptr, {b})) return 0;
                log(MBM_LOG_WARNING, "[GUI] atlas route=runs-render-target (png import unavailable) runs=" + std::to_string(runs.size()));
            }
            // keep it referenced from a UPROPERTY so GC never collects it
            call_obj1(m_keep_image, "SetBrushResourceObject", "ResourceObject", tex);
            m_atlas = tex;
            return 1; // atlas texture id (the only id the GUI host draws)
        }

        void submit(const std::vector<Batch>& batches) override
        {
            if (!valid(m_target) || !valid(m_atlas)) return;
            const auto& bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
            clear(m_target, bg.x, bg.y, bg.z, 1.0); // opaque panel: no render-target alpha dependency
            draw_into(m_target, m_atlas, batches);
        }

        void set_visible(bool v) override
        {
            m_visible = v;
            if (v)
            {
                m_recheck_now = true;  // [FU3] re-check the layout on every open
                m_build_failures = 0;  // [FU1] every open gets fresh build tries
            }
            set_visibility(v);
        }

        // [FIX F1] AController::SetIgnoreMoveInput/SetIgnoreLookInput keep a
        // counter (true = +1, false = -1), so only a matched pair is neutral.
        // The block is applied to the player resolved by prepare(); exactly the
        // calls that succeeded are undone, and only on that same player.
        auto apply_ignore(UObject* pc, const char* fn, bool b) -> bool
        {
            Frame f;
            // both take one bool parameter (named bNewMoveInput / bNewLookInput)
            if (!frame_for(pc, fn, f)) return false;
            int bools = 0;
            for (FProperty* p : Unreal::TFieldRange<FProperty>(f.fn, Unreal::EFieldIterationFlags::IncludeDeprecated))
                if (p->HasAnyPropertyFlags(Unreal::CPF_Parm) && p->IsA<Unreal::FBoolProperty>())
                {
                    auto* bp = static_cast<Unreal::FBoolProperty*>(p);
                    uint8_t* byte_value = static_cast<uint8_t*>(p->ContainerPtrToValuePtr<void>(f.data())) + bp->GetByteOffset();
                    *byte_value = static_cast<uint8_t>((*byte_value & ~bp->GetFieldMask()) | (b ? bp->GetByteMask() : 0));
                    ++bools;
                }
            if (bools != 1) return false; // not the expected one-bool signature: do not call
            return call(pc, f);
        }
        void release_block()
        {
            if (m_blocked_pc && valid(m_blocked_pc))
            {
                if (m_blocked_move) apply_ignore(m_blocked_pc, "SetIgnoreMoveInput", false);
                if (m_blocked_look) apply_ignore(m_blocked_pc, "SetIgnoreLookInput", false);
            }
            // a destroyed player took its counters with it: nothing to undo
            m_blocked_pc = nullptr;
            m_blocked_move = m_blocked_look = false;
        }
        auto set_gameplay_input_blocked(bool b) -> bool override
        {
            if (!b)
            {
                release_block();
                return true;
            }
            if (!valid(m_pc)) return false;
            if (m_blocked_pc == m_pc && (m_blocked_move || m_blocked_look)) return true; // already applied to this player
            release_block(); // a previous, still-living player gets its matching false
            m_blocked_move = apply_ignore(m_pc, "SetIgnoreMoveInput", true);
            m_blocked_look = apply_ignore(m_pc, "SetIgnoreLookInput", true);
            if (!m_blocked_move && !m_blocked_look) return false;
            m_blocked_pc = m_pc;
            return true;
        }
    };

    inline auto runtime_gui_backend() -> RuntimeGuiBackend&
    {
        static RuntimeGuiBackend b;
        return b;
    }
    inline auto runtime_gui() -> GuiHost&
    {
        static GuiHost g{NativeMods::runtime_host(), runtime_gui_backend()};
        return g;
    }

    // Called once from the game thread after Unreal init (see CppMod.cpp diff):
    // publishes the GUI table, creates the context, binds the pad keys and fires
    // on_ui_init on every native mod.
    inline void start_runtime_gui() noexcept
    {
        try
        {
            auto& gui = runtime_gui();
            if (!gui.start())
            {
                runtime_gui_backend().log(MBM_LOG_ERROR, "[GUI] refused: Dear ImGui context could not be created");
                return;
            }
            struct Bind { Input::Key key; Key gui_key; };
            // Generic defaults; the pad chords are assigned in PS5Pad.ini by keymap-gen.
            static const Bind binds[] = {{Input::Key::F10, Key::Toggle},         {Input::Key::UP_ARROW, Key::Up},     {Input::Key::DOWN_ARROW, Key::Down},
                                         {Input::Key::LEFT_ARROW, Key::Left},    {Input::Key::RIGHT_ARROW, Key::Right}, {Input::Key::RETURN, Key::Activate},
                                         {Input::Key::PAGE_UP, Key::PrevTab},    {Input::Key::PAGE_DOWN, Key::NextTab}};
            for (const auto& b : binds)
            {
                const Key k = b.gui_key;
                UE4SSProgram::get_program().register_keydown_event(b.key, Input::Handler::ModifierKeyArray{}, [k] { runtime_gui().post_key(k); }, 3, nullptr);
            }
            const auto n = NativeMods::runtime_host().fire_all(NativeMods::Event::UiInit);
            runtime_gui_backend().log(MBM_LOG_NORMAL, "[GUI] host ready toggle=F10 on_ui_init_mods=" + std::to_string(n));
            runtime_gui_backend().ready = true;
        }
        catch (...)
        {
        }
    }
    // Per-frame (game thread), installed into #9's after_frame extension point.
    inline void runtime_gui_frame(float dt) noexcept
    {
        try
        {
            static bool started = false;
            if (!started)
            {
                if (!NativeMods::Runtime::unreal_ready.load(std::memory_order_acquire)) return;
                started = true;
                start_runtime_gui();
            }
            if (runtime_gui_backend().ready) runtime_gui().frame(dt);
        }
        catch (...)
        {
        }
    }

    // Called by CppMod::start_mod before the first native mod attaches, so a
    // mod's constructor can register_tab (upstream mods do).
    inline void publish_runtime_gui() noexcept
    {
        static std::once_flag once;
        try
        {
            std::call_once(once, [] {
                NativeMods::runtime_host().set_gui(runtime_gui().table());
                NativeMods::Runtime::after_frame.store(&runtime_gui_frame, std::memory_order_release);
            });
        }
        catch (...)
        {
        }
    }
} // namespace RC::Platform::NativeGui
