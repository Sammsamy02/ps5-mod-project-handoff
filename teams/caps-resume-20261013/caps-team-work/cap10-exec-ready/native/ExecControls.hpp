#pragma once
#include "ExecTrace.h"
#include <cerrno>
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>

// Only genuine forwarded system calls. Executable controls deliberately fail
// without creating a mapping or dereferencing memory; no CPU fault is induced.
static bool check_exec_trace_controls()
{
    const auto page = static_cast<size_t>(getpagesize());
    cap10_exec_reset();
    cap10_exec_start();
    void* memory = mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    const bool mapped = memory != MAP_FAILED;
    const int protect = mapped ? mprotect(memory, page, PROT_READ) : -1;
    errno = 0;
    void* failed_mapping = mmap(nullptr, 0, PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
    const int mmap_error = errno;
    errno = 0;
    const int failed_protection = mprotect(nullptr, page, PROT_READ | PROT_EXEC);
    const int protect_error = errno;
    cap10_exec_stop();
    const auto stats = cap10_exec_stats();
    if (mapped) munmap(memory, page);
    const bool valid = mapped && protect == 0 && failed_mapping == MAP_FAILED && failed_protection == -1 &&
        stats.mmap_calls == 2 && stats.mprotect_calls == 2 && stats.executable_requests == 2 &&
        stats.failed_requests == 2 && stats.recorded_events == 4 && stats.dropped_events == 0;
    std::printf("{\"tracer_control\":true,\"passed\":%s,\"mmap_calls\":%llu,\"mprotect_calls\":%llu,\"executable_requests\":%llu,\"failed_requests\":%llu,\"mmap_errno\":%d,\"mprotect_errno\":%d,\"executable_mapping_created\":false,\"signals_raised\":0}\n",
        valid ? "true" : "false", static_cast<unsigned long long>(stats.mmap_calls),
        static_cast<unsigned long long>(stats.mprotect_calls), static_cast<unsigned long long>(stats.executable_requests),
        static_cast<unsigned long long>(stats.failed_requests), mmap_error, protect_error);
    cap10_exec_reset();
    return valid;
}
