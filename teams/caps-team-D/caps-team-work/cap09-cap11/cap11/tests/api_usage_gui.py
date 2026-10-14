"""Mac-only API-usage check for the staged GUI engine binding (NativeGuiEngineBackend.inc).

The binding cannot be host-compiled (the port headers require the PS5
platform profile; target compiles are the integrator's first gate). This
check lowers the risk lexically: every runtime API the binding calls must
(a) be declared in the pinned port / candidate headers, and (b) already be
called by shipping runtime code (so its behaviour on the console is not new).
Prints items_checked; fails on zero or on any miss.
"""
import re
import sys
from pathlib import Path

inc = Path(sys.argv[1]).read_text()
roots = [Path(p) for p in sys.argv[2:]]

# (symbol as used by the binding, declaration regex, shipping-usage regex)
APIS = [
    ("FScriptArray::AddZeroed", r"int32 AddZeroed\(int32 Count, int32 NumBytesPerElement, uint32 AlignmentOfElement\)", r"->AddZeroed\("),
    ("FScriptArray::GetData", r"void\* GetData\(\)", r"->GetData\(\)"),
    ("FScriptArray", r"FScriptArray\(\) = default", r"new \(params\.data\) Unreal::FScriptArray\{\}"),
    ("FArrayProperty::GetInner", r"FProperty\* GetInner\(\)", r"->GetInner\(\)"),
    ("FStructProperty::GetStruct", r"GetStruct\(\)", r"struct_property->GetStruct\(\)"),
    ("FProperty::GetMinAlignment", r"GetMinAlignment", r"inner->GetMinAlignment\(\)"),
    ("FProperty::GetSize", r"GetSize\(\)", r"inner->GetSize\(\)"),
    ("UObjectGlobals::StaticConstructObject(params)", r"auto StaticConstructObject\(const FStaticConstructObjectParameters& Params\) -> UObject\*", r"UObjectGlobals::StaticConstructObject\(params\)"),
    ("FStaticConstructObjectParameters", r"struct RC_UE_API FStaticConstructObjectParameters", r"FStaticConstructObjectParameters params\{"),
    ("UObject::GetOuterPrivate", r"GetOuterPrivate", r"->GetOuterPrivate\(\)"),
    ("UObject::GetClassPrivate", r"GetClassPrivate", r"->GetClassPrivate\(\)"),
    ("GetFunctionByNameInChain(const TCHAR*)", r"UFunction\*\s+GetFunctionByNameInChain\(const TCHAR\*\)", r"GetFunctionByNameInChain\("),
    ("GetPropertyByNameInChain(const TCHAR*)", r"FProperty\*\s+GetPropertyByNameInChain\(const TCHAR\*\)", r"GetPropertyByNameInChain\("),
    ("UObject::ProcessEvent", r"ProcessEvent\(", r"->ProcessEvent\(func, "),
    ("UFunction::GetParmsSize", r"GetParmsSize", r"func->GetParmsSize\(\)"),
    ("TFieldRange<FProperty>(fn, IncludeDeprecated)", r"(class|struct) TFieldRange", r"TFieldRange<Unreal::FProperty>\(func, Unreal::EFieldIterationFlags::IncludeDeprecated\)"),
    ("FProperty::HasAnyPropertyFlags", r"bool HasAnyPropertyFlags\(", r"HasAnyPropertyFlags\(Unreal::CPF_Parm\)"),
    ("FProperty::ContainerPtrToValuePtr", r"ValueType\* ContainerPtrToValuePtr\(void\* ContainerPtr", r"ContainerPtrToValuePtr<"),
    ("FBoolProperty::GetFieldMask", r"GetFieldMask", r"bp->GetFieldMask\(\)"),
    ("FBoolProperty::GetByteOffset", r"GetByteOffset", r"bp->GetByteOffset\(\)"),
    ("FBoolProperty::GetByteMask", r"GetByteMask", r"bp->GetByteMask\(\)"),
    ("FUObjectArray::IndexToObject", r"static FUObjectItem\* IndexToObject\(int32_t Index\)", r"IndexToObject\("),
    ("FUObjectItem::GetUObject", r"class UObject\* GetUObject\(\) const", r"->GetUObject\(\)"),
    ("FUObjectItem::IsUnreachable", r"bool IsUnreachable\(\) const", r"->IsUnreachable\(\)"),
    ("UObjectGlobals::FindFirstOf(StringViewType)", r"auto FindFirstOf\(StringViewType ClassName\) -> UObject\*", r"FindFirstOf\("),
    ("UObjectGlobals::StaticFindObject<T>", r"auto StaticFindObject\(UClass\* ObjectClass, UObject\* InObjectPackage, const StringType& OrigInName", r"StaticFindObject<"),
    ("UE4SSProgram::register_keydown_event(key, mods, cb, tag, ptr)", r"register_keydown_event\(Input::Key,\s*const Input::Handler::ModifierKeyArray&", r"register_keydown_event\("),
    ("Unicode::DecodeUtf8", r"inline RC::StringType DecodeUtf8\(std::string_view Input\)", r"Unicode::DecodeUtf8\("),
]

texts = {}
for r in roots:
    for p in r.rglob("*"):
        if p.suffix in (".hpp", ".h", ".inl", ".cpp", ".inc") and p.is_file():
            try:
                texts[p] = p.read_text(errors="replace")
            except OSError:
                pass
items = 0
fails = []
for name, decl, use in APIS:
    token = re.escape(name.split("::")[-1].split("(")[0].split("<")[0])
    if not re.search(token, inc):
        print(f"  skip {name} (not used by the binding)")
        continue
    d = [p for p, t in texts.items() if p.suffix in (".hpp", ".h", ".inl") and re.search(decl, t)]
    u = [p for p, t in texts.items() if p.suffix == ".cpp" and re.search(use, t)]
    items += 1
    ok = bool(d) and bool(u)
    print(("  pass " if ok else "  FAIL ") + f"{name}: declared in {d[0].name if d else 'NONE'}; used by shipping {u[0].name if u else 'NONE'} (+{max(0, len(u) - 1)})")
    if not ok:
        fails.append(name)
print(f"api_usage_check items_checked={items} failed={len(fails)} files_scanned={len(texts)}")
if items == 0 or not texts:
    print("FAIL: zero items")
    sys.exit(2)
sys.exit(1 if fails else 0)
