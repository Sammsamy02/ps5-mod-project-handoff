#!/usr/bin/env python3
import difflib
import hashlib
from pathlib import Path

BASE=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT=Path(__file__).resolve().parent
source=BASE/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUObject.cpp'
assert hashlib.sha256(source.read_bytes()).hexdigest()=='50192ef184ed2cd5267a4eb21c2466de29466419726fd1afac981258070f18e3'
old=source.read_text()
anchor='''        uint8_t num_ufunc_params = func->GetNumParms();
'''
guard='''        // Reflected offsets must fit both the declared parameter frame and
        // this fixed Lua buffer. Validate every parameter (including return/out
        // slots) before any pusher can write or ProcessEvent can read it.
        constexpr auto parameter_capacity = sizeof(DynamicUnrealFunctionData::data);
        const auto declared_parameter_size = static_cast<size_t>(func->GetParmsSize());
        if (declared_parameter_size > parameter_capacity)
        {
            lua.throw_error("UFunction parameter frame exceeds the Lua buffer capacity");
            return 0;
        }
        if (func->HasChildren())
        {
            for (Unreal::FProperty* parameter : Unreal::TFieldRange<Unreal::FProperty>(func, Unreal::EFieldIterationFlags::IncludeDeprecated))
            {
                if (!parameter->HasAnyPropertyFlags(Unreal::CPF_Parm))
                {
                    continue;
                }
                const auto offset = parameter->GetOffset_Internal();
                const auto element_size = parameter->GetElementSize();
                const auto array_dim = parameter->GetArrayDim();
                if (offset < 0 || element_size <= 0 || array_dim <= 0 ||
                    static_cast<size_t>(offset) > declared_parameter_size)
                {
                    lua.throw_error("UFunction parameter has invalid reflected bounds");
                    return 0;
                }
                const auto remaining = declared_parameter_size - static_cast<size_t>(offset);
                // Division avoids signed GetSize() multiplication overflow and
                // offset+extent overflow on inconsistent reflected metadata.
                if (static_cast<size_t>(array_dim) > remaining / static_cast<size_t>(element_size))
                {
                    lua.throw_error("UFunction parameter extends beyond the declared frame");
                    return 0;
                }
            }
        }

'''
assert old.count(anchor)==1
new=old.replace(anchor,guard+anchor)
(OUT/'candidate').mkdir(exist_ok=True)
(OUT/'candidate/LuaUObject.cpp').write_text(new)
(OUT/'BOUNDS.patch').write_text(''.join(difflib.unified_diff(old.splitlines(True),new.splitlines(True),fromfile=str(source),tofile=str(OUT/'candidate/LuaUObject.cpp'))))
print('one source insertion; no header/ownership/FText/GC/mod edits')
