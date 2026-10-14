// Host test for the cap05 crash-test core, compiled from the EXACT bytes the
// stage script inserts into LuaMod.cpp (extracted between the
// [CAP05-CRASHTEST-BEGIN]/[CAP05-CRASHTEST-END] markers of the patched file
// into gen/cap05_core.inc by run_tests.sh). Each case runs in a forked child.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

namespace RC
{
#include "cap05_core.inc"
}

static void on_signal(int sig, siginfo_t* si, void*)
{
    char buf[96];
    int n = std::snprintf(buf, sizeof buf, "SIG %d ADDR 0x%lx\n", sig, (unsigned long)(uintptr_t)si->si_addr);
    (void)!write(1, buf, (size_t)n);
    _exit(100 + sig);
}

struct Case
{
    const char* name;
    const char* kind;
    const char* token;
    int want;            // 0 = must return (refused); 1 = segv-class; 2 = trap-class
};

int main()
{
    const Case cases[] = {
        {"wrong-token-refused", "segv", "wrong-token", 0},
        {"empty-token-refused", "segv", "", 0},
        {"bad-kind-refused", "bogus", "CRASH-THE-GAME-NOW", 0},
        {"segv-faults-at-0xc08", "segv", "CRASH-THE-GAME-NOW", 1},
        {"trap-fires", "trap", "CRASH-THE-GAME-NOW", 2},
    };
    int checked = 0, failed = 0;
    for (const Case& c : cases)
    {
        int pfd[2];
        if (pipe(pfd) != 0) return 2;
        pid_t pid = fork();
        if (pid == 0)
        {
            dup2(pfd[1], 1);
            struct sigaction sa{};
            sa.sa_sigaction = on_signal;
            sa.sa_flags = SA_SIGINFO;
            for (int s : {SIGSEGV, SIGBUS, SIGILL, SIGTRAP}) sigaction(s, &sa, nullptr);
            const char* msg = RC::cap05_crashtest::dispatch(c.kind, c.token);
            std::printf("RET %s\n", msg ? msg : "(null)");
            std::fflush(stdout);
            _exit(0);
        }
        close(pfd[1]);
        char out[512] = {0};
        ssize_t got = 0, r;
        while ((r = read(pfd[0], out + got, sizeof(out) - 1 - (size_t)got)) > 0) got += r;
        close(pfd[0]);
        int st = 0;
        waitpid(pid, &st, 0);
        int code = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
        bool ok = false;
        if (c.want == 0)
            ok = code == 0 && std::strncmp(out, "RET ", 4) == 0 && std::strstr(out, "nothing happened");
        else if (c.want == 1)
            ok = (code == 100 + SIGSEGV || code == 100 + SIGBUS) && std::strstr(out, "ADDR 0xc08");
        else
            ok = (code == 100 + SIGILL || code == 100 + SIGTRAP);
        ++checked;
        failed += !ok;
        std::fprintf(stderr, "%-24s %s exit=%d out=%s", c.name, ok ? "PASS" : "FAIL", code, got ? out : "(none)\n");
    }
    std::fprintf(stderr, "crashtest_core_host: checked %d cases, %d failed\n", checked, failed);
    return (checked == 0 || failed) ? 1 : 0;
}
