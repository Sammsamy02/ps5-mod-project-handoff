// Console-side entry for the standalone pad probe (worker-real-input-pad,
// 2026-10-04). Compiles to an empty TU off __PROSPERO__ (host builds include
// it only to prove no console path leaks into host binaries). PREPARED ONLY —
// never executed here; see PROBE-PRX-BUILD.md for the exact PRX build/link
// route and HARDWARE-TEST-PLAN.md for the console procedure.
//
// The exported C entry keeps a stable symbol for the payload route: the probe
// payload's module_start calls modbridge_pad_probe_run with a /data JSONL
// path and a bounded poll budget, then treats the returned summary as the
// final record. Nothing throws across the C boundary.
#ifdef __PROSPERO__
#include "PadProbe.hpp"
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{
using RC::Input::PS5PadSource;

struct FileSink final : modbridge::padprobe::RecordSink
{
    int fd{-1};
    explicit FileSink(const char* path)
    {
        if (path) fd = ::open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    }
    ~FileSink() override
    {
        if (fd >= 0) ::close(fd);
    }
    void write_line(const std::string& line) override
    {
        if (fd < 0) return;
        (void)::write(fd, line.data(), line.size());
        (void)::write(fd, "\n", 1);
    }
};

// Real hardware factory: PadBackend filled with scePad*/sceUserService* and
// eagerly activated (PS5PadNative.cpp:16-21). This is the first-ever hardware
// contact for native_ps5_pad_provider when it runs.
std::shared_ptr<PS5PadSource> native_factory(std::vector<RC::Input::PadBinding> bindings, int user)
{
    return RC::Input::make_native_ps5_pad_source(std::move(bindings), user);
}
} // namespace

extern "C" __attribute__((visibility("default"))) const char*
modbridge_pad_probe_run(const char* jsonl_path, unsigned long long poll_budget, int requested_user)
{
    static std::string summary;
    FileSink sink{jsonl_path};
    modbridge::padprobe::Config config;
    config.poll_budget = poll_budget ? poll_budget : 60000; // bounded default
    config.requested_user = requested_user;
    try
    {
        summary = modbridge::padprobe::run_probe(config, {native_factory, &sink});
    }
    catch (...)
    {
        summary = "\"event\":\"probe_failed\",\"stage\":\"boundary\"";
    }
    return summary.c_str();
}
#endif
