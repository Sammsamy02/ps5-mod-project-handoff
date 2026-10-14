#include "MonoOwner.hpp"
#include <csignal>
#include <pthread.h>
#include <cstring>
#include <initializer_list>

// Private-process setup/query only. No signal is raised and no OS fault is induced.
static void harmless_handler(int) {}
static void harmless_siginfo(int, siginfo_t*, void*) {}
#if defined(CAP10_TEST_REPLACE_PIPE)
static void replacement_handler(int) {}
#endif
static struct sigaction replacement_pipe{};
static bool pipe_replaced;
static void test_replace_pipe_once()
{
#if defined(CAP10_TEST_REPLACE_PIPE)
    if (pipe_replaced) return;
    struct sigaction action{};
    action.sa_handler = replacement_handler;
    action.sa_flags = SA_RESTART;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGUSR1);
    if (sigaction(SIGPIPE, &action, nullptr) != 0 || sigaction(SIGPIPE, nullptr, &replacement_pipe) != 0) std::exit(94);
    pipe_replaced = true;
#endif
}
static bool same_mask(const sigset_t& left, const sigset_t& right)
{
    for (int signal = 1; signal < NSIG; ++signal)
        if (sigismember(&left, signal) != sigismember(&right, signal)) return false;
    return true;
}
static bool same_action(const struct sigaction& left, const struct sigaction& right)
{
    if (left.sa_flags != right.sa_flags || !same_mask(left.sa_mask, right.sa_mask)) return false;
    return (left.sa_flags & SA_SIGINFO) ? left.sa_sigaction == right.sa_sigaction : left.sa_handler == right.sa_handler;
}
extern "C" void diagnostic_event(const char* tag, unsigned value)
{ std::printf("%s %u\n", tag, value); }
int main()
{
    const int signals[] = {SIGFPE, SIGQUIT, SIGILL, SIGBUS, SIGSYS, SIGABRT, SIGSEGV, SIGPIPE, SIGUSR1};
    struct sigaction before[sizeof(signals) / sizeof(signals[0])]{};
    for (unsigned i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i)
    {
        struct sigaction action{};
        sigemptyset(&action.sa_mask);
        sigaddset(&action.sa_mask, SIGUSR2);
        action.sa_flags = SA_RESTART;
        if (signals[i] == SIGSEGV) { action.sa_sigaction = harmless_siginfo; action.sa_flags |= SA_SIGINFO; }
        else action.sa_handler = signals[i] == SIGBUS ? SIG_IGN : harmless_handler;
#if defined(CAP10_TEST_DEFAULT_PIPE)
        if (signals[i] == SIGPIPE) action.sa_handler = SIG_DFL;
#elif defined(CAP10_TEST_IGNORE_PIPE)
        if (signals[i] == SIGPIPE) action.sa_handler = SIG_IGN;
#endif
        if (sigaction(signals[i], &action, nullptr) != 0 || sigaction(signals[i], nullptr, &before[i]) != 0) return 90;
    }
    sigset_t blocked{}, mask_before{};
    sigemptyset(&blocked);
    for (int signal : {SIGFPE, SIGCHLD, SIGPIPE, SIGUSR1}) sigaddset(&blocked, signal);
    if (pthread_sigmask(SIG_BLOCK, &blocked, nullptr) != 0 || pthread_sigmask(SIG_SETMASK, nullptr, &mask_before) != 0) return 91;
    const auto result = RC::Cap10Host::run_embedded_probe_once();
    bool actions_ok = true;
    for (unsigned i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i)
    {
        struct sigaction after{};
        if (sigaction(signals[i], nullptr, &after) != 0) return 92;
        const bool equal = same_action(signals[i] == SIGPIPE && pipe_replaced ? replacement_pipe : before[i], after);
        actions_ok &= equal;
        const char* disposition = (after.sa_flags & SA_SIGINFO) ? "siginfo" : after.sa_handler == SIG_DFL ? "default" : after.sa_handler == SIG_IGN ? "ignore" : "handler";
        std::printf("SIGNAL %d preserved=%u flags_before=%d flags_after=%d after=%s mask_preserved=%u\n", signals[i], unsigned(equal), before[i].sa_flags, after.sa_flags, disposition, unsigned(same_mask(before[i].sa_mask, after.sa_mask)));
    }
    sigset_t mask_after{};
    if (pthread_sigmask(SIG_SETMASK, nullptr, &mask_after) != 0) return 93;
    const bool mask_ok = same_mask(mask_before, mask_after);
    for (int signal : {SIGFPE, SIGCHLD, SIGPIPE, SIGUSR1})
        std::printf("MASK %d before=%d after=%d\n", signal, sigismember(&mask_before, signal), sigismember(&mask_after, signal));
    const bool completion_ok = pipe_replaced ? !result.completed && result.stage == RC::Cap10Host::Stage::SignalStateFailed : result.completed;
    std::printf("{\"verdict\":\"%s\",\"completed\":%s,\"stage\":%u,\"run_passed\":%s,\"throw_passed\":%s,\"recovery_passed\":%s,\"real_managed_runs\":%u,\"real_managed_exceptions\":%u,\"actions_preserved\":%s,\"thread_mask_preserved\":%s,\"replacement_case\":%s,\"signals_raised\":0,\"host_only\":true,\"target_execution\":false}\n",
        completion_ok && actions_ok && mask_ok ? "SIGNAL_LIFECYCLE_PASS" : "SIGNAL_LIFECYCLE_FAIL",
        result.completed ? "true" : "false", unsigned(result.stage), result.calls.first.passed ? "true" : "false", result.calls.deliberate.passed ? "true" : "false", result.calls.recovery.passed ? "true" : "false",
        unsigned(result.calls.first.invoked) + unsigned(result.calls.recovery.invoked), unsigned(result.calls.deliberate.had_exception), actions_ok ? "true" : "false", mask_ok ? "true" : "false", pipe_replaced ? "true" : "false");
    return completion_ok && actions_ok && mask_ok ? 0 : 1;
}
