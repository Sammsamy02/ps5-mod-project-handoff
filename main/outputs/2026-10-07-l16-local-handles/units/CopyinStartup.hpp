#pragma once

namespace l13_sink {
// module_start failures never set runtime_active, so module_stop will not call
// the bridge stop function. Release here unless an owner can still use the fd.
template <typename Owner, typename Release, typename Start>
auto with_startup_cleanup(Owner &owner, Release release, Start start) -> decltype(start())
{
    struct Cleanup {
        Owner &owner;
        Release &release;
        ~Cleanup() { if (!owner) release(); }
    } cleanup{owner, release};
    return start();
}
} // namespace l13_sink
