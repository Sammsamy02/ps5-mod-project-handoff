
#include <memory>
#include <iostream>
using namespace RC::Unreal;
void original(UObject*, FFrame&, void*) { ++native_calls; trace.emplace_back("original"); }
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    UFunction function; function.func = original;
    UObject context, other; FFrame frame;
    std::string scenario = argv[1];
    if (scenario == "phase_snapshots") {
        bool added = false;
        function.RegisterPreHook([&](UnrealScriptFunctionCallableContext&, void*) {
            trace.emplace_back("pre");
            if (!added) {
                added = true;
                function.RegisterPreHook([](UnrealScriptFunctionCallableContext&, void*) { trace.emplace_back("new-pre"); }, nullptr);
                function.RegisterPostHook([](UnrealScriptFunctionCallableContext&, void*) { trace.emplace_back("new-post"); }, nullptr);
            }
        }, nullptr);
        auto thunk = function.func;
        thunk(&context, frame, nullptr);
        if (trace != std::vector<std::string>{"pre", "original", "new-post"}) return 1;
        trace.clear();
        thunk(&context, frame, nullptr);
        if (trace != std::vector<std::string>{"pre", "new-pre", "original", "new-post"}) return 1;
        std::cout << "PASS phase snapshot: pre-add-post same dispatch; pre-add-pre next Pre invocation\n";
    } else if (scenario == "retired_filtered_capture") {
        auto& data = function.GetFunctionHookData();
        auto token = std::make_shared<int>(42); std::weak_ptr<int> weak = token;
        int filtered_invocations = 0;
        auto id = data.AddPreCallback([token, &filtered_invocations](UnrealScriptFunctionCallableContext&, void*) { ++filtered_invocations; }, nullptr, &other);
        function.RegisterPreHook([&](UnrealScriptFunctionCallableContext&, void*) { function.UnregisterAllHooks(); }, nullptr);
        token.reset();
        auto thunk = function.func;
        thunk(&context, frame, nullptr);
        auto* retained = data.GetCallbackData(id);
        if (!retained || !retained->UnregistrationRequested || data.bIsMidExecution || weak.expired() || filtered_invocations || native_calls != 1 || Internal::GetHookedFunctionsMap().contains(&function)) return 1;
        std::cout << "OBSERVED filtered callback capture retained after active count zero and retirement\n";
        data.RemoveAllCallbacks();
        if (!weak.expired()) return 1;
        std::cout << "PASS quiescent explicit retained-data cleanup releases capture; thunk node remains\n";
    } else return 2;
}
