extern "C" int bridge_copyin_initializer_enter(void)
{
    if (!g_copyin_initializer_owner.enter()) return 0;
    // The gate stores the actual std::thread::id. This token is only a log
    // identity, not an OS numeric TID or evidence of descriptor headroom.
    const uint64_t token = std::hash<std::thread::id>{}(std::this_thread::get_id());
    ::diagnostic_event("copyin_initializer_id_hash_lo", static_cast<uint32_t>(token));
    ::diagnostic_event("copyin_initializer_id_hash_hi", static_cast<uint32_t>(token >> 32));
    return 1;
}
extern "C" void bridge_copyin_initializer_complete(void)
{
    if (!g_copyin_initializer_owner.allows()) return;
    bridge_copyin_directory_release();
    // [L25 item 7] ONE-TIME observed probe state, emitted AFTER the release so
    // every field is the true post-retirement state. Semantics:
    //   copyin_probe_state        bit0 = initializer owner gate still armed on
    //                             this init thread (stays armed until
    //                             bridge_copyin_initializer_leave at shutdown),
    //                             bit1 = bank_retired (this function ran),
    //                             bit2 = parent fd STILL open (retirement failed
    //                             to close it - abnormal, investigate),
    //                             bit3 = any sink fd STILL open (abnormal)
    //   copyin_probe_sinks_open   number of single-use sink descriptors still
    //                             open after retirement (0 expected)
    //   copyin_probe_nonce_used   sinks already consumed by probe dispatches
    //                             during this initialization
    //   copyin_probe_attempts_total  probe_until dispatches so far (any phase)
    // A normal successful admission logs state=3 (armed gate, retired bank,
    // 0 sinks): kernel-write dispatch is structurally stopped - a later call
    // returns at the parent-fd check in bridge_kernel_write_copyin_probe_until
    // BEFORE any kernel write. Anything else is visible evidence, not silence.
    if (!g_copyin_retirement_logged) {
        g_copyin_retirement_logged = true;
        uint32_t sinks_open = 0;
        for (const auto &handle : g_copyin_handles)
            if (handle.fd >= 0) ++sinks_open;
        const uint32_t state = (g_copyin_initializer_owner.allows() ? 1u : 0u)
                             | 2u /* retirement reached */
                             | (g_copyin_parent_fd >= 0 ? 4u : 0u)
                             | (sinks_open != 0 ? 8u : 0u);
        ::diagnostic_event("copyin_probe_state", state);
        ::diagnostic_event("copyin_probe_gate_armed", g_copyin_initializer_owner.allows() ? 1u : 0u);
        ::diagnostic_event("copyin_probe_bank_retired", 1u);
        ::diagnostic_event("copyin_probe_parent_fd_open", g_copyin_parent_fd >= 0 ? 1u : 0u);
        ::diagnostic_event("copyin_probe_sinks_open", sinks_open);
        ::diagnostic_event("copyin_probe_nonce_used", g_copyin_probe_nonce);
        ::diagnostic_event("copyin_probe_attempts_total", g_copyin_probe_attempts);
    }
    // Keep both owned captures and fault evidence for the rest of initialization.
    // [L26 gating proposal] Admission is complete and the bank is retired:
    // no legitimate copyin consumer remains for this launch (agent-Q l25
    // probe: no post-admission caller exists across the composed runtime;
    // seed refusals are terminal-per-launch; runtime FName goes through the
    // native ctor; the next launch re-runs prepare() in a new process).
    // Disarm the owner gate NOW instead of holding it armed until shutdown:
    // probe_until and every gate sibling then fail closed (-1 / 0 / faulted)
    // for the rest of the session. Idempotent with the shutdown fallback:
    // the init thread's CopyinOwnerScope destructor calls
    // bridge_copyin_initializer_leave(), whose allows() guard makes that a
    // no-op. In a gated build the shutdown-time copyin_probe_gate_released /
    // copyin_probe_attempts_late_total events never fire; this event is the
    // positive proof the gate closed at admission.
    ::diagnostic_event("copyin_probe_gate_l26_early_release", 1u);
    g_copyin_initializer_owner.leave();
}
extern "C" void bridge_copyin_initializer_leave(void)
{
    if (!g_copyin_initializer_owner.allows()) return;
    // Also covers exceptions before seed-check entry. Completion and close
    // sentinels make this an idempotent fallback on the initializer itself.
    bridge_copyin_directory_release();
    // [L25 item 7] shutdown-time stop proof: the owner gate is the last armed
    // piece of the copyin machinery; after this event the probe refuses every
    // caller (allows() == false for every thread, including the init thread).
    ::diagnostic_event("copyin_probe_gate_released", 1u);
    ::diagnostic_event("copyin_probe_attempts_late_total", g_copyin_probe_attempts_late);
    g_copyin_initializer_owner.leave();
}
