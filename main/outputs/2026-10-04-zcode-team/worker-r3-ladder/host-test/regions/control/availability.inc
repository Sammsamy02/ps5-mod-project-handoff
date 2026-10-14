    // Local convenience wrappers for Capabilities functions
    static auto is_engine_tick_hook_available() -> bool
    {
        return UE4SSRuntime::IsEngineTickAvailable();
    }

    static auto is_process_event_hook_available() -> bool
    {
        return UE4SSRuntime::IsProcessEventAvailable();
    }
