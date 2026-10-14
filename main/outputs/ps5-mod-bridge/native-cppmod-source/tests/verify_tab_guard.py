#!/usr/bin/env python3
"""Run the actual register_tab method body with host-only GUI test doubles.

The doubles are never a runtime provider and are not included in any artifact.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    source = args.source.read_text()
    start = source.index("    auto CppUserModBase::register_tab(")
    end = source.index("    auto CppUserModBase::register_keydown_event(", start)
    method = source[start:end]
    fixture = r'''
#include <cassert>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>
namespace RC {
using StringViewType = std::string_view;
class CppUserModBase;
namespace GUI { struct GUITab { using RenderFunctionType = void(*)();
    GUITab(StringViewType, RenderFunctionType, CppUserModBase*) {} }; }
class UE4SSProgram {
public:
    static inline int attempts{};
    static UE4SSProgram& get_program() { static UE4SSProgram instance; return instance; }
    void add_gui_tab(const std::shared_ptr<GUI::GUITab>&) { ++attempts;
#ifdef __PROSPERO__
        throw std::runtime_error("unavailable test GUI");
#endif
    }
};
class CppUserModBase {
public:
    std::vector<std::shared_ptr<GUI::GUITab>> GUITabs;
    void register_tab(StringViewType, GUI::GUITab::RenderFunctionType);
};
'''
    fixture += method + r'''
}
int main() {
    RC::CppUserModBase mod;
    bool refused = false;
    try { mod.register_tab("test", +[] {}); }
    catch (const std::runtime_error&) { refused = true; }
#ifdef __PROSPERO__
    assert(refused && mod.GUITabs.empty() && RC::UE4SSProgram::attempts == 0);
#else
    assert(!refused && mod.GUITabs.size() == 1 && RC::UE4SSProgram::attempts == 1);
#endif
}
'''
    with tempfile.TemporaryDirectory(prefix="modbridge-tab-guard-") as directory:
        directory = Path(directory)
        text = directory / "actual_method.cpp"
        text.write_text(fixture)
        for native in (True, False):
            executable = directory / ("native" if native else "original")
            command = ["c++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter"]
            if native: command += ["-D__PROSPERO__"]
            subprocess.run(command + [str(text), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)
    print("native refusal before mutation and original GUI behavior passed")

if __name__ == "__main__":
    main()
