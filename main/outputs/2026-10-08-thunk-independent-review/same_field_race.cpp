
#include <thread>
#include <iostream>
using namespace RC::Unreal;
void original(UObject*, FFrame&, void*) { ++native_calls; }
int main() {
    UFunction function; function.func = original; function.GetFunctionHookData();
    UObject context; FFrame frame; std::atomic<bool> go{};
    std::thread writer([&] {
        while (!go.load()) {}
        for (int i = 0; i < 6; ++i) { function.UnregisterAllHooks(); function.GetFunctionHookData(); }
    });
    go.store(true);
    for (int i = 0; i < 100000; ++i) function.GetFuncPtr()(&context, frame, nullptr);
    writer.join();
    return native_calls == 100000 ? 0 : 1;
}
