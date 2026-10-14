from pathlib import Path
import json,hashlib,difflib
r=Path(__file__).resolve().parent;s=r.parent/'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/src/JMapGenerator/JMapGenerator.cpp'
original=s.read_text();text=original.replace("#include <DynamicOutput/DynamicOutput.hpp>","#include <DynamicOutput/Output.hpp>")
text=text.replace('#include <Windows.h>', '''#ifdef __PROSPERO__
#include <JMapGenerator/NativeSupport.hpp>
#include <Unreal/UnrealInitializer.hpp>
#include <Unreal/PlatformModuleSnapshot.hpp>
#else
#include <Windows.h>
#endif''')
a=text.index('        class MemoryChecker\n');b=text.index('        // Raw views of engine containers',a)
text=text[:a]+'''#ifdef __PROSPERO__
        using MemoryChecker = Native::MemoryChecker;
#else
'''+text[a:b]+'#endif\n\n'+text[b:]
text=text.replace('''            std::time_t now = std::time(nullptr);
            std::tm utc{};''','''            std::time_t now = std::time(nullptr);
#ifdef __PROSPERO__
            return Native::utc_timestamp(now);
#else
            std::tm utc{};''')
text=text.replace('''            return buf;
        }

        // Extracts the changelist''','''            return buf;
#endif
        }

        // Extracts the changelist''')
a=text.index('            wchar_t module_path[MAX_PATH]{};');b=text.index('\n\n            JsonObject metadata{};',a)
text=text[:a]+'''#ifdef __PROSPERO__
            const auto source = Native::source_filename(UE4SSProgram::get_program().get_game_executable_path());
#else
'''+text[a:b]+'\n#endif'+text[b:]
old='''            json_escape_to(sink, hex_address(reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr))));'''
text=text.replace(old,'''#ifdef __PROSPERO__
            const auto& module_source = m_native_module_owner;
            if (!module_source)
                throw std::runtime_error("Native JMap module identity is unavailable");
            const auto& main_image = module_source->Views().array[static_cast<size_t>(ScanTarget::MainExe)];
            json_escape_to(sink, hex_address(reinterpret_cast<uint64_t>(main_image.image_base)));
#else
'''+old+'\n#endif')
text=text.replace('''        auto JMapDumper::dump() -> void
        {
''','''        auto JMapDumper::dump() -> void
        {
#ifdef __PROSPERO__
            // Retain the loader-supplied module bytes for this whole traversal.
            // This gate does not establish game identity or arbitrary heap coverage.
            m_native_module_owner = UnrealInitializer::StaticStorage::GlobalConfig.ModuleSource;
            if (!UnrealInitializer::IsInitialized().load() || !m_native_module_owner)
                throw std::runtime_error("Native JMap requires an initialized engine and module owner");
#endif
''')
text=text.replace('            MemoryChecker m_memory{};', '''            MemoryChecker m_memory{};
#ifdef __PROSPERO__
            std::shared_ptr<const Unreal::Platform::ModuleSnapshot> m_native_module_owner;
#endif''')
(r/'src/JMapGenerator.cpp').write_text(text)
(r/'source.patch').write_text(''.join(difflib.unified_diff(original.splitlines(True),text.splitlines(True),fromfile='upstream/JMapGenerator.cpp',tofile='native/JMapGenerator.cpp')))
(r/'source-origin.json').write_text(json.dumps({'source':str(s),'sha256':hashlib.sha256(s.read_bytes()).hexdigest(),'upstream_pin':'e3ba1016562d6c0868c410d0a71e88bfcdbf691b','canonical_unchanged':True,'scope':'native platform boundary only; no generator execution'},indent=2)+'\n')
print('Prepared genuine JMap source with conditional native platform boundaries')
