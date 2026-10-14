// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Agc;

/// <summary>
/// Graphics driver bindings (libSceAgcDriver). The submit, queue, flip, and resource-registration layer
/// beneath the command builders: it hands finished command buffers to the GPU, creates and manages
/// queues, drives the display flip, and registers GPU resources. Signatures were recovered from the module.
/// </summary>
public static unsafe partial class SceAgcDriver
{
    private const string Lib = "libSceAgcDriver";

    /// <summary>Acquire/reserve an async compute queue on a given pipe (best-guess: symbol not present in this module).</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverAcquireComputeQueue(uint pipeId, uint queueId, void* outQueue);

    /// <summary>Register a GPU event source with the given id and user data onto an event queue.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverAddEqEvent(void* eq, int eventId, void* udata);

    /// <summary>
    /// Submits one recorded command buffer through the async-graphics ring. Takes the same description
    /// as <see cref="sceAgcDriverSubmitDcb"/>.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverAgrSubmitDcb(void* submitDescription);

    /// <summary>Submit multiple draw command buffers through the async-graphics (AGR) ring and return status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverAgrSubmitMultiDcbs(void* dcbList, void* sizeList, uint count);

    /// <summary>Create a GPU submission queue of the requested type and return a handle/status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverCreateQueue(uint queueType, void* queueDesc, void* outQueue);

    /// <summary>Resume an async compute queue from a CWSR (compute wave save/restore) suspend and return status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverCwsrResumeAcq();

    /// <summary>Suspend an async compute queue via CWSR, saving state to the provided area, and return status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverCwsrSuspendAcq(void* saveArea);

    /// <summary>Dumps GPU hardware status for the given target selector (acts only when target is 0).</summary>
    [LibraryImport(Lib)]
    public static partial void sceAgcDriverDebugHardwareStatus(uint target);

    /// <summary>Remove a previously registered GPU event source from an event queue.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverDeleteEqEvent(void* eq, int eventId);

    /// <summary>Destroy a previously created GPU submission queue and return status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverDestroyQueue(void* queue);

    /// <summary>Public query to find registered resources; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverFindResourcesPublic();

    /// <summary>Extract the context id from the current event-queue event data (event data &gt;&gt; 16).</summary>
    [LibraryImport(Lib)]
    public static partial ulong sceAgcDriverGetEqContextId();

    /// <summary>Return the event type derived from the current event-queue event data.</summary>
    [LibraryImport(Lib)]
    public static partial ulong sceAgcDriverGetEqEventType();

    /// <summary>Returns the GPU reference clock value, or an AGC status/error code when unavailable.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetGpuRefClks();

    /// <summary>Retrieves the hull-shader off-chip tessellation parameters into the caller's output pointers.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetHsOffchipParam(void* outParam1, void* outParam2);

    /// <summary>Retrieves the name of a resource owner; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetOwnerName();

    /// <summary>Fills the caller's structure with register-shadow region information.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetRegShadowInfo(void* outInfo);

    /// <summary>Fills the caller's structure with register-shadow information for the async graphics ring.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetRegShadowInfoAgr(void* outInfo);

    /// <summary>Returns the base address and size of the DMEM region reserved for AGC.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetReservedDmemForAgc(void* outAddr, void* outSize);

    /// <summary>Retrieves a resource's base GPU address and size in bytes; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetResourceBaseAddressAndSizeInBytes();

    /// <summary>Retrieves a resource's name; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetResourceName();

    /// <summary>Retrieves a resource's shader GUID; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetResourceShaderGuid();

    /// <summary>Retrieves a resource's type; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetResourceType();

    /// <summary>Retrieves a resource's user-data value; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetResourceUserData();

    /// <summary>Returns the fixed size in dwords (0x40) of a SetFlip command packet.</summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverGetSetFlipPacketSizeInDwords();

    /// <summary>Returns the size in dwords of a SetWorkloadComplete packet (user-data size + 9).</summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverGetSetWorkloadCompletePacketSize();

    /// <summary>Returns the size in dwords of a SetWorkloadsActive packet (user-data size + 9).</summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverGetSetWorkloadsActivePacketSize();

    /// <summary>Retrieves the tessellation-factor ring buffer descriptor for the given index.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetTFRing(void* outRing, ulong index);

    /// <summary>Returns the current GPU trace initiator identifier.</summary>
    [LibraryImport(Lib)]
    public static partial ulong sceAgcDriverGetTraceInitiator();

    /// <summary>Returns the fixed size in dwords (0x20) of a wait-until-safe-for-rendering packet.</summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverGetWaitRenderingPacketSizeInDwords();

    /// <summary>Copies the workload-stream name for streamIndex into nameBuffer (capped at min(nameBufferSize,32)) and writes the stream's info handle to *infoOut; returns a status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverGetWorkloadStreamInfo(uint streamIndex, void* nameBuffer, ulong nameBufferSize, void* infoOut);

    /// <summary>Submit an IDHS (indirect/host-driven) request described by the pointed-to structure and return status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverIDHSSubmit(void* submitInfo);

    /// <summary>Initializes the driver's resource-registration subsystem; in this build it is a not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverInitResourceRegistration();

    /// <summary>Returns non-zero if a GPU capture is currently in progress.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverIsCaptureInProgress();

    /// <summary>Returns non-zero if command-buffer submit validation is enabled.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverIsSubmitValidationEnabled();

    /// <summary>Returns non-zero if a GPU trace is currently in progress.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverIsTraceInProgress();

    /// <summary>Registers the AgcDriver module: moduleIndex selects the slot, interfacePtr is the driver interface (null -&gt; 'Invalid interface', else 'Invalid index'); returns a status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverModuleRegistration(ulong moduleIndex, void* interfacePtr);

    /// <summary>Registers three default register-segment arrays (with their entry counts) with the driver; returns a status code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverNotifyDefaultStates(void* segments0, void* segments1, void* segments2, uint count0, uint count1, uint count2);

    /// <summary>Passes an array of 16-byte {id,value} info records down to the driver (updates a global state); returns 0.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverPassInfoDownward(void* infoAddr, uint count);

    /// <summary>Patches clear-state register writes from an array of (register, value) pairs; returns a status code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverPatchClearState(void* registers, uint count);

    /// <summary>Writes the user-memory size required for resource registration to *requirementsOut (set to 0 here) and returns a status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(void* requirementsOut);

    /// <summary>Registers a GDS (global data share) resource with the driver; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverRegisterGdsResource();

    /// <summary>Registers a resource owner with the driver; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverRegisterOwner();

    /// <summary>Registers a graphics resource with the driver; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverRegisterResource();

    /// <summary>Registers workload-stream slot streamIndex, copying the name C-string (up to 32 bytes) into the driver's table; returns a status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverRegisterWorkloadStream(uint streamIndex, void* name);

    /// <summary>Releases a previously acquired async compute queue. Not exported by every driver build; check before calling.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverReleaseComputeQueue(void* queue);

    /// <summary>Requests the start of a GPU capture; returns an AGC status code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverRequestCaptureStart();

    /// <summary>Requests the stop of a GPU capture; returns an AGC status code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverRequestCaptureStop();

    /// <summary>
    /// Records a flip so the display shows a buffer once the processor reaches this point. As with the
    /// wait above, the first argument is not the command buffer but the slot holding its write cursor,
    /// and the packet size and queue come before the flip's own arguments. It does no bounds checking,
    /// so the caller checks there is room first.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverSetFlip(uint** writeCursorSlot, uint packetSizeInDwords,
        uint queueType, uint videoOutHandle, int displayBufferIndex, uint flipMode, long flipArg);

    /// <summary>Sets the hull-shader off-chip tessellation parameters via an indirect dispatch thunk.</summary>
    [LibraryImport(Lib)]
    public static partial void sceAgcDriverSetHsOffchipParam();

    /// <summary>Sets a resource's user-data value; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverSetResourceUserData();

    /// <summary>Configures the tessellation-factor ring base address and size (clamped to 0x4000).</summary>
    [LibraryImport(Lib)]
    public static partial void sceAgcDriverSetTFRing(ulong ringAddr, uint size);

    /// <summary>Writes a workload-complete packet into the command buffer; returns a status code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverSetWorkloadComplete(void* commandBuffer, uint queueType, uint streamId, uint workloadIndex);

    /// <summary>Writes a workloads-active packet from an array of workload ids into the command buffer; returns a status code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverSetWorkloadsActive(void* commandBuffer, uint queueType, uint streamId, void* workloadsAddr, uint count);

    /// <summary>Sets up a GPU register-shadow region with the given type, enable/mode flags and size.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverSetupRegisterShadow(uint type, byte enable, byte flag1, byte flag2, uint size);

    /// <summary>Submit an async/compute command buffer identified by a queue index.</summary>
    [LibraryImport(Lib)]
    public static partial void sceAgcDriverSubmitAcb(uint queueId);

    /// <summary>Submit one command buffer to the given driver queue context and return a status code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverSubmitCommandBuffer(void* context, void* commandBuffer);

    /// <summary>
    /// Submits one recorded command buffer through the driver's default context. The argument describes
    /// the buffer rather than being it: the driver reads an address, a word count and a flag out of it.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverSubmitDcb(void* submitDescription);

    /// <summary>Submit multiple async/compute command buffers for the given queue.</summary>
    [LibraryImport(Lib)]
    public static partial void sceAgcDriverSubmitMultiAcbs(uint queueId);

    /// <summary>Submit an array of command buffers (with matching sizes) to a queue context in a single call.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverSubmitMultiCommandBuffers(void* context, void* dcbList, void* sizeList, uint count);

    /// <summary>Submit multiple draw command buffers via the default submit context.</summary>
    [LibraryImport(Lib)]
    public static partial void sceAgcDriverSubmitMultiDcbs(void* dcbList, void* sizeList, uint count);

    /// <summary>Submit a suspend-point marker for the given queue.</summary>
    [LibraryImport(Lib)]
    public static partial void sceAgcDriverSuspendPointSubmit(void* queue, ulong value);

    /// <summary>Enables the submit-done GPU exception (interrupt 45); returns a status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverSysEnableSubmitDone45Exception();

    /// <summary>Returns the AGC client number assigned to the calling process.</summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverSysGetClientNumber();

    /// <summary>Returns non-zero if the game/application has been closed.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverSysIsGameClosed();

    /// <summary>Proxy for submitting a flip handle; in this build it is compiled to a stub that returns 0.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverSysSubmitFlipHandleProxy();

    /// <summary>Temporary/one-time initialization of the IDHS subsystem for the given context.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverTmpInitIdhs(void* context);

    /// <summary>Triggers a one-shot GPU capture; returns an AGC status code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverTriggerCapture();

    /// <summary>Unregisters every resource belonging to an owner; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverUnregisterAllResourcesForOwner();

    /// <summary>Unregisters an owner together with all of its resources; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverUnregisterOwnerAndResources();

    /// <summary>Unregisters a single registered resource; not-implemented stub returning error 0x8a6c9018.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverUnregisterResource();

    /// <summary>Unregisters the workload-stream slot at streamIndex under the driver mutex, clearing its registration bit; returns a status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAgcDriverUnregisterWorkloadStream(uint streamIndex);

    /// <summary>Returns the size in dwords of a user-data packet carrying the given byte count.</summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverUserDataGetPacketSize(uint dataSize);

    /// <summary>Immediate user-data write; in this build it is compiled to a stub that returns 0.</summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverUserDataImmediateWrite();

    /// <summary>Writes a user-data packet (header plus optional data blob) into the command buffer; returns the number of dwords written.</summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverUserDataWritePacket(void* commandBuffer, uint queueType, uint header, void* srcAddr, uint size);

    /// <summary>Writes a debug pop-marker user-data packet; returns the number of dwords written (3).</summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverUserDataWritePopMarker(void* commandBuffer, uint queueType);

    /// <summary>Writes a debug push-marker (label-begin) user-data packet; returns the number of dwords written.</summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverUserDataWritePushMarker(void* commandBuffer, uint queueType, void* srcAddr, uint size, uint modifier);

    /// <summary>Writes a debug set-marker user-data packet (payload plus packed color); returns the number of dwords written.</summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverUserDataWriteSetMarker(void* commandBuffer, uint queueType, void* srcAddr, uint size, uint modifier);

    /// <summary>
    /// Records a wait until the display has released a buffer, so the processor may render into it.
    /// The first argument is not the command buffer but the slot holding its write cursor: the routine
    /// reads the cursor out of it, appends the packet there, and writes the advanced cursor back. It
    /// does no bounds checking, so the caller checks there is room first.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial uint sceAgcDriverWaitUntilSafeForRendering(uint** writeCursorSlot,
        uint packetSizeInDwords, uint queueType, uint videoOutHandle, int displayBufferIndex);
}
