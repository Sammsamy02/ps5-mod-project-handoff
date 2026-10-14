#include "MonoOwner.hpp"
#include <mono/utils/mono-signal-handler.h>
#include <csignal>
#include <cstring>
extern "C" gboolean MONO_SIG_HANDLER_SIGNATURE(mono_chain_signal);

static bool dispatched;
static bool dispatch_ok;
static int ordinary_calls, info_calls;
static siginfo_t* expected_info;
static void* expected_context;
static bool callback_arguments_ok = true;
static void ordinary_handler(int signal)
{ ++ordinary_calls; callback_arguments_ok &= signal == SIGILL; }
static void info_handler(int signal, siginfo_t* info, void* context)
{ ++info_calls; callback_arguments_ok &= signal == SIGBUS && info == expected_info && context == expected_context; }
static void test_direct_dispatch_once()
{
    if (dispatched) return;
    dispatched = true;
    siginfo_t info{};
    info.si_signo = SIGBUS;
    int context{};
    expected_info = &info;
    expected_context = &context;
    // This is the genuine hidden Mono function, statically linked from the real
    // changed runtime archive. The real owner has already initialized the runtime.
    // No OS signal is delivered. Only the saved ordinary/info callbacks run.
    const bool initialized = mono_get_root_domain() != nullptr;
    const bool ignored = mono_chain_signal(SIGFPE, &info, &context);
    const bool ignored_did_not_call = ordinary_calls == 0 && info_calls == 0;
    const bool default_unhandled = !mono_chain_signal(SIGQUIT, &info, &context);
    const bool missing_unhandled = !mono_chain_signal(SIGSYS, &info, &context);
    const bool ordinary = mono_chain_signal(SIGILL, &info, &context);
    const bool siginfo = mono_chain_signal(SIGBUS, &info, &context);
    dispatch_ok = initialized && ignored && ignored_did_not_call && default_unhandled && missing_unhandled && ordinary && siginfo && ordinary_calls == 1 && info_calls == 1 && callback_arguments_ok;
    std::printf("DISPATCH initialized=%u ignored_handled=%u ignored_callbacks=0:%u default_unhandled=%u missing_unhandled=%u ordinary_handled=%u ordinary_calls=%d siginfo_handled=%u siginfo_calls=%d exact_callback_arguments=%u\n", unsigned(initialized), unsigned(ignored), unsigned(ignored_did_not_call), unsigned(default_unhandled), unsigned(missing_unhandled), unsigned(ordinary), ordinary_calls, unsigned(siginfo), info_calls, unsigned(callback_arguments_ok));
}
extern "C" void diagnostic_event(const char* tag, unsigned value)
{ std::printf("%s %u\n", tag, value); }
int main()
{
    struct sigaction action{};
    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGFPE, &action, nullptr) != 0) return 90;
    // SA_SIGINFO forces Mono's genuine add handler to save the otherwise-default
    // action, so this separately covers saved DFL and an absent saved action.
    action.sa_handler = SIG_DFL;
    action.sa_flags = SA_SIGINFO;
    if (sigaction(SIGQUIT, &action, nullptr) != 0) return 91;
    struct sigaction queried{};
    if (sigaction(SIGQUIT, nullptr, &queried) != 0 || !(queried.sa_flags & SA_SIGINFO) || queried.sa_handler != SIG_DFL) return 92;
    action.sa_flags = 0;
    if (sigaction(SIGSYS, &action, nullptr) != 0) return 93;
    action.sa_handler = ordinary_handler;
    if (sigaction(SIGILL, &action, nullptr) != 0) return 94;
    action.sa_sigaction = info_handler;
    action.sa_flags = SA_SIGINFO;
    if (sigaction(SIGBUS, &action, nullptr) != 0) return 95;
    const auto result = RC::Cap10Host::run_embedded_probe_once();
    std::printf("{\"verdict\":\"%s\",\"completed\":%s,\"direct_genuine_runtime_dispatch\":true,\"runtime_initialized_during_dispatch\":%s,\"dispatch_cases_passed\":%s,\"ordinary_callback_calls\":%d,\"siginfo_callback_calls\":%d,\"exact_callback_arguments\":%s,\"signals_delivered\":0,\"run_passed\":%s,\"throw_passed\":%s,\"recovery_passed\":%s,\"real_managed_runs\":%u,\"real_managed_exceptions\":%u,\"host_only\":true,\"target_execution\":false}\n", result.completed && dispatched && dispatch_ok ? "GENUINE_SIGNAL_DISPATCH_PASS" : "GENUINE_SIGNAL_DISPATCH_FAIL", result.completed ? "true" : "false", dispatched ? "true" : "false", dispatch_ok ? "true" : "false", ordinary_calls, info_calls, callback_arguments_ok ? "true" : "false", result.calls.first.passed ? "true" : "false", result.calls.deliberate.passed ? "true" : "false", result.calls.recovery.passed ? "true" : "false", unsigned(result.calls.first.invoked) + unsigned(result.calls.recovery.invoked), unsigned(result.calls.deliberate.had_exception));
    return result.completed && dispatched && dispatch_ok ? 0 : 1;
}
