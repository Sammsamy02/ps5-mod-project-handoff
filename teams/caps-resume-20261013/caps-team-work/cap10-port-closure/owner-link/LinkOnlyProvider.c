/* NEVER EXECUTED. Target link-only provider fixture, not the game provider. */
/* Mac HOST TEST ONLY. Not UE4SS readiness, not the production provider/observer. */
#include <stdint.h>
static int32_t kind, expected, abi_calls, probe_calls, bad, observed_cookie, observed_reply, abi_exits, probe_exits, abi_result, probe_status, reply_valid;
__attribute__((visibility("default"))) void cap10_host_begin(int32_t requested_kind, int32_t cookie)
{
    kind = requested_kind; expected = cookie; abi_calls = probe_calls = bad = observed_cookie = observed_reply = 0;
    abi_exits = probe_exits = abi_result = probe_status = reply_valid = 0;
}
__attribute__((visibility("default"))) int32_t modbridge_cap10_abi_version(void)
{
    if (kind != 1 || abi_calls != 0 || probe_calls != 0) ++bad;
    ++abi_calls;
    ++abi_exits; abi_result = 1;
    return 1;
}
__attribute__((visibility("default"))) int32_t modbridge_cap10_runtime_probe(int32_t cookie, int32_t* reply)
{
    if (kind != 1 || abi_calls != 1 || probe_calls != 0 || cookie != expected) ++bad;
    ++probe_calls; observed_cookie = cookie;
    if (!reply) { ++probe_exits; probe_status = -4; return -4; }
    *reply = cookie; observed_reply = cookie;
    ++probe_exits; probe_status = 0; reply_valid = 1;
    return 0;
}
__attribute__((visibility("default"))) int32_t cap10_host_read(int32_t index)
{
    switch (index) { case 0:return abi_calls; case 1:return probe_calls; case 2:return bad;
                     case 3:return observed_cookie; case 4:return observed_reply; case 5:return abi_exits;
                     case 6:return probe_exits; case 7:return abi_result; case 8:return probe_status;
                     case 9:return reply_valid; default:return -1; }
}
