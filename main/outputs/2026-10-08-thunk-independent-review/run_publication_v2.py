from pathlib import Path
import hashlib, json, os, subprocess
HERE = Path(__file__).resolve().parent
original = (HERE / 'publication-actual.cpp').read_text()
prefix = original[:original.index('#include <thread>')]
source = HERE / 'publication-load-only-v2.cpp'
source.write_text(prefix + r'''
#include <thread>
#include <iostream>
using namespace RC::Unreal;
void original(UObject*, FFrame&, void*) {}
__attribute__((noinline)) NativeHandler engine_read(UFunction& function) {
    auto pointer = function.GetFuncPtr();
    asm volatile("" : "+r"(pointer) : : "memory");
    return pointer;
}
int main() {
    UFunction function; function.func = original; function.GetFunctionHookData();
    std::atomic<bool> go{};
    std::thread writer([&] {
        while (!go.load()) {}
        for (int i = 0; i < 6; ++i) { function.UnregisterAllHooks(); function.GetFunctionHookData(); }
    });
    go.store(true);
    NativeHandler pointer{};
    // Model the engine's field load before call dispatch. No registry mutex is
    // acquired after this load. The compiler barrier preserves each load;
    // it supplies no inter-thread synchronization or atomic field semantics.
    for (int i = 0; i < 100000; ++i) pointer = engine_read(function);
    writer.join();
    return pointer ? 0 : 1;
}
''')
binary = HERE / 'publication-load-only-v2-tests'
command = ['/usr/bin/clang++', '-std=c++20', '-O1', '-g', '-fsanitize=thread', '-fno-omit-frame-pointer', '-pthread', str(source), '-o', str(binary)]
build = subprocess.run(command, capture_output=True, text=True, timeout=60)
(HERE / 'publication-v2-compile.stdout').write_text(build.stdout)
(HERE / 'publication-v2-compile.stderr').write_text(build.stderr)
assert build.returncode == 0, build.stderr
run = subprocess.run([str(binary)], capture_output=True, text=True, env=dict(os.environ, TSAN_OPTIONS='halt_on_error=1:exitcode=66'), timeout=30)
(HERE / 'publication-v2.stdout').write_text(run.stdout)
(HERE / 'publication-v2.stderr').write_text(run.stderr)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
receipt = {'pass': 'ThreadSanitizer: data race' in run.stderr, 'scope': 'Exact prototype bodies plus explicit HOST raw Func read/store seam. Models the load before dispatch, not native hardware atomicity or tearing; compiler barrier is not synchronization.', 'prior_control': {'path': str(HERE / 'CONTROLS.json'), 'sha256': sha(HERE / 'CONTROLS.json'), 'result': 'Two policy observations PASS; initial dispatching publication control observed no race and was inconclusive'}, 'compile_command': command, 'compile_returncode': build.returncode, 'command': [str(binary)], 'returncode': run.returncode, 'race_detected': 'ThreadSanitizer: data race' in run.stderr, 'source_sha256': sha(source), 'binary_sha256': sha(binary), 'stdout_sha256': sha(HERE / 'publication-v2.stdout'), 'stderr_sha256': sha(HERE / 'publication-v2.stderr')}
(HERE / 'PUBLICATION-CONTROL.v2.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps({'compile_returncode': build.returncode, 'returncode': run.returncode, 'race_detected': receipt['race_detected']}))
assert receipt['pass'], 'No observed race; this remains an inconclusive diagnostic'
