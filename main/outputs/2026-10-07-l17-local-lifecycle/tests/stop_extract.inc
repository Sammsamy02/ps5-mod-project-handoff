extern "C" int modbridge_runtime_stop(void)
{
    auto &slot = owner_slot();
    if (!slot)
    {
        bridge_copyin_directory_release();
        return 0;  // nothing started; nothing to join
    }
    bool stopped = false;
    try
    {
        // Loader control thread (module_stop), never the init thread.
        // Emits at most one 'stopped' ack line, then destroys the owner.
        stopped = modbridge::native::StopRuntimeWithAck(slot);
    }
    catch (...)
    {
        slot.reset();
        // RuntimeOwner destruction stops/joins the initializer (or terminates
        // rather than returning). Only after that may its probe descriptors close.
        bridge_copyin_directory_release();
        return MRS_ERR_STOP_EXN;
    }
    if (stopped || !slot) bridge_copyin_directory_release();
    return stopped ? 0 : 1;
}

