// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Audio;

/// <summary>The sample form of a waveform an Ngs2 voice plays.</summary>
public enum Ngs2WaveformType : uint
{
    /// <summary>Signed 8-bit.</summary>
    PcmI8 = 0x10,
    /// <summary>Unsigned 8-bit.</summary>
    PcmU8 = 0x11,
    /// <summary>Signed 16-bit, little-endian.</summary>
    PcmI16L = 0x12,
    /// <summary>Signed 16-bit, big-endian.</summary>
    PcmI16B = 0x13,
    /// <summary>Signed 24-bit, little-endian.</summary>
    PcmI24L = 0x14,
    /// <summary>Signed 24-bit, big-endian.</summary>
    PcmI24B = 0x15,
    /// <summary>Signed 32-bit, little-endian.</summary>
    PcmI32L = 0x16,
    /// <summary>Signed 32-bit, big-endian.</summary>
    PcmI32B = 0x17,
    /// <summary>32-bit floating-point, little-endian.</summary>
    PcmF32L = 0x18,
    /// <summary>32-bit floating-point, big-endian.</summary>
    PcmF32B = 0x19,
}

/// <summary>The built-in Ngs2 rack kinds.</summary>
public enum Ngs2RackId : uint
{
    /// <summary>Plays waveforms as voices.</summary>
    Sampler = 0x1000,
    /// <summary>Mixes voices down.</summary>
    Submixer = 0x2000,
    /// <summary>Adds reverberation.</summary>
    Reverb = 0x2001,
    /// <summary>The final mastering step.</summary>
    Mastering = 0x3000,
    /// <summary>A custom rack.</summary>
    Custom = 0x4000,
    /// <summary>A custom sampler rack.</summary>
    CustomSampler = 0x4001,
    /// <summary>A custom submixer rack.</summary>
    CustomSubmixer = 0x4002,
    /// <summary>A custom mastering rack.</summary>
    CustomMastering = 0x4003,
    /// <summary>An extended custom rack.</summary>
    CustomEx = 0x4004,
}

/// <summary>Points the engine at the block of memory it works in, and reports how big it needs to be.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceNgs2ContextBufferInfo
{
    /// <summary>The buffer the engine uses.</summary>
    public void* HostBuffer;
    /// <summary>The buffer size in bytes.</summary>
    public nuint HostBufferSize;
    private nuint _reserved0;
    private nuint _reserved1;
    private nuint _reserved2;
    private nuint _reserved3;
    private nuint _reserved4;
    /// <summary>Caller data carried through.</summary>
    public nuint UserData;
}

/// <summary>Handlers the engine calls to allocate and free its own memory, for the allocator-driven create path.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNgs2BufferAllocator
{
    /// <summary>An <c>int32(SceNgs2ContextBufferInfo*)</c> allocation callback.</summary>
    public nint AllocHandler;
    /// <summary>An <c>int32(SceNgs2ContextBufferInfo*)</c> free callback.</summary>
    public nint FreeHandler;
    /// <summary>Caller data passed to the handlers.</summary>
    public nuint UserData;
}

/// <summary>Where a render pass writes its output and in what form.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceNgs2RenderBufferInfo
{
    /// <summary>The output buffer.</summary>
    public void* Buffer;
    /// <summary>The output buffer size in bytes.</summary>
    public nuint BufferSize;
    /// <summary>The sample form, from <see cref="Ngs2WaveformType"/>.</summary>
    public uint WaveformType;
    /// <summary>The channel count (1, 2, 6, or 8).</summary>
    public uint NumChannels;
}

/// <summary>How far a source's level falls off with distance.</summary>
public enum Ngs2RolloffModel : uint
{
    /// <summary>Level falls with the reciprocal of distance.</summary>
    Inverse = 0,
    /// <summary>Level falls in a straight line with distance.</summary>
    Linear = 1,
    /// <summary>Level falls with a power of distance.</summary>
    Exponential = 2,
    /// <summary>As <see cref="Inverse"/>, held at the maximum distance.</summary>
    ClampedInverse = 3,
    /// <summary>As <see cref="Linear"/>, held at the maximum distance.</summary>
    ClampedLinear = 4,
    /// <summary>As <see cref="Exponential"/>, held at the maximum distance.</summary>
    ClampedExponential = 5,
}

/// <summary>What a report handler is registered for.</summary>
public enum Ngs2ReportType : uint
{
    /// <summary>An error or an informational message.</summary>
    Message = 0,
    /// <summary>A call that sets something.</summary>
    ApiSet = 1,
    /// <summary>A call that reads something back.</summary>
    ApiGet = 2,
    /// <summary>A render pass.</summary>
    ApiRender = 3,
    /// <summary>An event or a parameter change.</summary>
    Control = 4,
    /// <summary>How much processor time rendering took.</summary>
    CpuLoad = 5,
    /// <summary>The state of a render pass.</summary>
    RenderState = 6,
    /// <summary>A voice's waveform.</summary>
    VoiceWaveform = 7,
    /// <summary>A stream handler being called.</summary>
    StreamCallback = 8,
    /// <summary>A finished output buffer.</summary>
    Output = 9,
}

/// <summary>What a geometry apply works out. Combine with a bitwise or.</summary>
[System.Flags]
public enum Ngs2GeomApplyFlags : uint
{
    /// <summary>Nothing.</summary>
    None = 0,
    /// <summary>Where the source is, relative to the way the listener faces.</summary>
    SourceAngle = 1 << 0,
    /// <summary>The pitch shift the two moving apart or together causes.</summary>
    SourceDoppler = 1 << 1,
    /// <summary>The level for each speaker.</summary>
    VolumeMatrix = 1 << 2,
    /// <summary>The placing handed on to the three-dimensional mixer.</summary>
    A3dAttribute = 1 << 3,
    /// <summary>The ambisonic form of that placing.</summary>
    A3dAmbisonics = 1 << 4,
    /// <summary>Everything but the ambisonic form, which is what most callers want.</summary>
    Default = SourceAngle | SourceDoppler | VolumeMatrix | A3dAttribute,
}

/// <summary>A point or a direction in the scene.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNgs2GeomVector
{
    /// <summary>The first axis.</summary>
    public float X;
    /// <summary>The second axis.</summary>
    public float Y;
    /// <summary>The third axis.</summary>
    public float Z;
}

/// <summary>How a source's level varies with the direction it points.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNgs2GeomCone
{
    /// <summary>The level inside the inner angle, 0 to 2.</summary>
    public float InnerLevel;
    /// <summary>The inner angle in degrees, 0 to 360.</summary>
    public float InnerAngle;
    /// <summary>The level outside the outer angle, 0 to 2.</summary>
    public float OuterLevel;
    /// <summary>The outer angle in degrees, 0 to 360.</summary>
    public float OuterAngle;
}

/// <summary>How a source's level falls off with distance.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNgs2GeomRolloff
{
    /// <summary>Which curve, from <see cref="Ngs2RolloffModel"/>.</summary>
    public uint Model;
    /// <summary>The distance past which nothing more is taken off.</summary>
    public float MaxDistance;
    /// <summary>How steeply the curve falls.</summary>
    public float RolloffFactor;
    /// <summary>The distance the level is stated at.</summary>
    public float ReferenceDistance;
}

/// <summary>Where the listener is, which way it faces, and how fast it moves.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceNgs2GeomListenerParam
{
    /// <summary>Where it is.</summary>
    public SceNgs2GeomVector Position;
    /// <summary>The direction it faces.</summary>
    public SceNgs2GeomVector OrientFront;
    /// <summary>The direction that is up for it.</summary>
    public SceNgs2GeomVector OrientUp;
    /// <summary>How fast it moves, for the pitch shift.</summary>
    public SceNgs2GeomVector Velocity;
    /// <summary>How fast sound travels, in the same units.</summary>
    public float SoundSpeed;
    /// <summary>Reserved. Leave zero.</summary>
    public fixed uint Reserved[2];
}

/// <summary>A listener worked out once and applied to every source.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceNgs2GeomListenerWork
{
    /// <summary>The placing as a four-by-four matrix, rows first.</summary>
    public fixed float Matrix[16];
    /// <summary>How fast it moves.</summary>
    public SceNgs2GeomVector Velocity;
    /// <summary>How fast sound travels.</summary>
    public float SoundSpeed;
    /// <summary>Which handedness the matrix was built for.</summary>
    public uint Coordinate;
    /// <summary>Reserved. Leave zero.</summary>
    public fixed uint Reserved[3];
}

/// <summary>Where one sound is and how it carries.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceNgs2GeomSourceParam
{
    /// <summary>Where it is.</summary>
    public SceNgs2GeomVector Position;
    /// <summary>How fast it moves, for the pitch shift.</summary>
    public SceNgs2GeomVector Velocity;
    /// <summary>The direction it points, for the cone below.</summary>
    public SceNgs2GeomVector Direction;
    /// <summary>How its level varies with that direction.</summary>
    public SceNgs2GeomCone Cone;
    /// <summary>How its level falls off with distance.</summary>
    public SceNgs2GeomRolloff Rolloff;
    /// <summary>How much of the pitch shift is applied, 1 for all of it.</summary>
    public float DopplerFactor;
    /// <summary>The level sent to the full-range speakers, 0 to 1.</summary>
    public float FbwLevel;
    /// <summary>The level sent to the low-frequency speaker, 0 to 1.</summary>
    public float LfeLevel;
    /// <summary>The level it never goes above.</summary>
    public float MaxLevel;
    /// <summary>The level it never falls below.</summary>
    public float MinLevel;
    /// <summary>How wide it sounds, 0 for a point.</summary>
    public float Radius;
    /// <summary>How many speakers to spread it across.</summary>
    public uint NumSpeakers;
    /// <summary>How many levels a row of the matrix holds (1, 2, 6 or 8).</summary>
    public uint MatrixFormat;
    /// <summary>Reserved. Leave zero.</summary>
    public fixed uint Reserved[2];
}

/// <summary>Where a source ended up for the three-dimensional mixer.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceNgs2GeomA3dAttribute
{
    /// <summary>Where it is, relative to the listener.</summary>
    public SceNgs2GeomVector Position;
    /// <summary>Its level.</summary>
    public float Volume;
    /// <summary>Reserved. Leave zero.</summary>
    public fixed uint Reserved[4];
}

/// <summary>What a source sounds like to a listener: the answer a voice is driven with.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceNgs2GeomAttribute
{
    /// <summary>The pitch shift, 1 for none.</summary>
    public float PitchRatio;
    /// <summary>The level for each input and output channel pair, eight by eight.</summary>
    public fixed float Level[64];
    /// <summary>Where it ended up for the three-dimensional mixer.</summary>
    public SceNgs2GeomA3dAttribute A3dAttrib;
    /// <summary>Reserved. Leave zero.</summary>
    public fixed uint Reserved[4];
}

/// <summary>Where one sound sits, without a scene: an angle and a distance.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNgs2PanParam
{
    /// <summary>Where it sits, in the units the workspace was prepared with.</summary>
    public float Angle;
    /// <summary>How far away it is.</summary>
    public float Distance;
    /// <summary>The level sent to the full-range speakers, 0 to 1.</summary>
    public float FbwLevel;
    /// <summary>The level sent to the low-frequency speaker, 0 to 1.</summary>
    public float LfeLevel;
}

/// <summary>Where the speakers are, prepared once and used for every source.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceNgs2PanWork
{
    /// <summary>Where each speaker sits.</summary>
    public fixed float SpeakerAngle[8];
    /// <summary>A whole turn, in the units the angles are given in.</summary>
    public float UnitAngle;
    /// <summary>How many speakers there are (2, 4, 5 or 7).</summary>
    public uint NumSpeakers;
}

/// <summary>How one module of a custom rack is wired.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNgs2CustomModuleInfo
{
    /// <summary>Which module it is.</summary>
    public uint ModuleId;
    /// <summary>The buffer it reads from.</summary>
    public uint SourceBufferId;
    /// <summary>The second buffer it reads from.</summary>
    public uint ExtraBufferId;
    /// <summary>The buffer it writes to.</summary>
    public uint DestBufferId;
    /// <summary>Where its state sits, or zero when it publishes none.</summary>
    public uint StateOffset;
    /// <summary>How large that state is, or zero when it publishes none.</summary>
    public uint StateSize;
    /// <summary>Reserved.</summary>
    public uint Reserved;
    /// <summary>Reserved.</summary>
    public uint Reserved2;
}

/// <summary>
/// The audio synthesis and mixing engine. A system owns racks (sampler, submixer, reverb, mastering); a
/// rack owns voices that play waveforms; a render pass mixes them into an output buffer. Handles are opaque
/// integers. This is the flat interface exactly as the engine headers declare it; the sized option and
/// command structures are passed as pointers the caller builds and initializes through the reset-option and
/// query-info calls.
/// </summary>
public static unsafe partial class Ngs2
{
    private const string Lib = "libSceNgs2";

    /// <summary>One (mono) channel.</summary>
    public const uint Channels1 = 1;
    /// <summary>Two (stereo) channels.</summary>
    public const uint Channels2 = 2;
    /// <summary>Six (5.1) channels.</summary>
    public const uint Channels51 = 6;
    /// <summary>Eight (7.1) channels.</summary>
    public const uint Channels71 = 8;

    // System

    /// <summary>Lists the live system handles.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemEnumHandles(nuint* outHandles, uint maxHandles);
    /// <summary>Fills an option structure with its defaults.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemResetOption(void* outOption);
    /// <summary>Reports the buffer a system with these options needs.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemQueryBufferSize(void* option, SceNgs2ContextBufferInfo* outBufferInfo);
    /// <summary>Creates a system in a caller-provided buffer.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemCreate(void* option, SceNgs2ContextBufferInfo* bufferInfo, nuint* outHandle);
    /// <summary>Creates a system, letting the engine allocate through the given handlers.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemCreateWithAllocator(void* option, SceNgs2BufferAllocator* allocator, nuint* outHandle);
    /// <summary>Destroys a system and returns its buffer.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemDestroy(nuint systemHandle, SceNgs2ContextBufferInfo* outBufferInfo);
    /// <summary>Runs a batch of system commands.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemRunCommands(nuint systemHandle, void* @params, nuint numParams);
    /// <summary>Reads a system information field.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemQueryInfo(nuint systemHandle, uint queryId, void* outInfo, nuint infoSize);
    /// <summary>Takes the system lock.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemLock(nuint systemHandle);
    /// <summary>Releases the system lock.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemUnlock(nuint systemHandle);
    /// <summary>Attaches caller data to a system.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemSetUserData(nuint systemHandle, nuint userData);
    /// <summary>Reads the caller data attached to a system.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemGetUserData(nuint systemHandle, nuint* outUserData);
    /// <summary>Reads the system information block.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemGetInfo(nuint systemHandle, void* outInfo, nuint infoSize);
    /// <summary>Sets the grain (block) size in samples.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemSetGrainSamples(nuint systemHandle, uint numGrainSamples);
    /// <summary>Sets the output sample rate.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemSetSampleRate(nuint systemHandle, uint sampleRate);
    /// <summary>Lists a system's rack handles.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemEnumRackHandles(nuint systemHandle, nuint* aOutHandle, uint maxHandles);
    /// <summary>Mixes the system into the render buffers - the call that produces audio.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2SystemRender(nuint systemHandle, SceNgs2RenderBufferInfo* aBufferInfo, uint numBufferInfo);

    // Rack

    /// <summary>Reports the buffer a rack of this kind needs.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2RackQueryBufferSize(uint rackId, void* option, SceNgs2ContextBufferInfo* outBufferInfo);
    /// <summary>Creates a rack in a caller-provided buffer.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2RackCreate(nuint systemHandle, uint rackId, void* option, SceNgs2ContextBufferInfo* bufferInfo, nuint* outHandle);
    /// <summary>Creates a rack, letting the engine allocate through the given handlers.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2RackCreateWithAllocator(nuint systemHandle, uint rackId, void* option, SceNgs2BufferAllocator* allocator, nuint* outHandle);
    /// <summary>Destroys a rack and returns its buffer.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2RackDestroy(nuint rackHandle, SceNgs2ContextBufferInfo* outBufferInfo);
    /// <summary>Runs a batch of rack commands.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2RackRunCommands(nuint rackHandle, void* @params, nuint numParams);
    /// <summary>Reads a rack information field.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2RackQueryInfo(nuint rackHandle, uint queryId, void* outInfo, nuint infoSize);
    /// <summary>Takes the rack lock.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2RackLock(nuint rackHandle);
    /// <summary>Releases the rack lock.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2RackUnlock(nuint rackHandle);
    /// <summary>Attaches caller data to a rack.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2RackSetUserData(nuint rackHandle, nuint userData);
    /// <summary>Reads the caller data attached to a rack.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2RackGetUserData(nuint rackHandle, nuint* outUserData);
    /// <summary>Reads the rack information block.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2RackGetInfo(nuint rackHandle, void* outInfo, nuint infoSize);
    /// <summary>Gets the handle of one voice in a rack.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2RackGetVoiceHandle(nuint rackHandle, uint voiceIndex, nuint* outHandle);

    // Voice

    /// <summary>Runs a batch of voice commands.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2VoiceRunCommands(nuint voiceHandle, void* @params, nuint numParams);
    /// <summary>Reads a voice information field.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2VoiceQueryInfo(nuint voiceHandle, uint queryId, void* outInfo, nuint infoSize);
    /// <summary>Applies a parameter list to a voice.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2VoiceControl(nuint voiceHandle, void* paramList);
    /// <summary>Reads a voice's state flags.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2VoiceGetStateFlags(nuint voiceHandle, uint* outStateFlags);
    /// <summary>Reads a voice's state block.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2VoiceGetState(nuint voiceHandle, void* outVoiceState, nuint voiceStateSize);
    /// <summary>Finds the rack and index that own a voice.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2VoiceGetOwner(nuint voiceHandle, nuint* outOwnerRack, uint* outVoiceIndex);
    /// <summary>Reads a voice's mixing-matrix information.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2VoiceGetMatrixInfo(nuint voiceHandle, uint matrixId, void* outInfo, nuint infoSize);
    /// <summary>Reads a voice's port information.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2VoiceGetPortInfo(nuint voiceHandle, uint portId, void* outInfo, nuint infoSize);

    // Stream

    /// <summary>Fills a stream option structure with its defaults.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2StreamResetOption(void* outOption);
    /// <summary>Reports the buffer a stream with these options needs.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2StreamQueryBufferSize(void* option, SceNgs2ContextBufferInfo* outBuffer);
    /// <summary>Creates a stream in a caller-provided buffer.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2StreamCreate(nuint systemHandle, void* option, SceNgs2ContextBufferInfo* buffer, nuint* outStreamHandle);
    /// <summary>Creates a stream, letting the engine allocate through the given handlers.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2StreamCreateWithAllocator(nuint systemHandle, void* option, SceNgs2BufferAllocator* allocator, nuint* outStreamHandle);
    /// <summary>Destroys a stream and returns its buffer.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2StreamDestroy(nuint streamHandle, SceNgs2ContextBufferInfo* outBuffer);
    /// <summary>Runs a batch of stream commands.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2StreamRunCommands(nuint streamHandle, void* @params, nuint numParams);
    /// <summary>Reads a stream information field.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2StreamQueryInfo(nuint streamHandle, uint queryId, void* outInfo, nuint infoSize);

    // Waveform utilities

    /// <summary>Reads waveform information from a block of memory.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2ParseWaveformData(void* data, nuint dataSize, void* outInfo);
    /// <summary>Reads waveform information from a file.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2ParseWaveformFile(byte* path, ulong offset, void* outInfo);
    /// <summary>Reads waveform information through a caller-supplied read handler.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2ParseWaveformUser(nint handler, nuint userData, void* outInfo);
    /// <summary>Computes the byte layout of a run of samples in a waveform format.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2CalcWaveformBlock(void* format, uint samplePos, uint numSamples, void* outBlock);
    /// <summary>Reports the frame layout of a waveform format.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2GetWaveformFrameInfo(void* format, uint* outFrameSize, uint* outNumFrameSamples, uint* outUnitsPerFrame, uint* outNumDelaySamples);

    /// <summary>Fills a job-scheduler option structure with its defaults.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2JobSchedulerResetOption(void* outOption);

    // Placing a sound in a scene. The geometry calls turn a listener and a source into the pitch and
    // the per-speaker levels a voice is then driven with; the panning calls do the same from an angle
    // and a distance, without a scene.

    /// <summary>Fills <paramref name="outListenerParam"/> with the listener defaults.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2GeomResetListenerParam(SceNgs2GeomListenerParam* outListenerParam);
    /// <summary>Fills <paramref name="outSourceParam"/> with the source defaults.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2GeomResetSourceParam(SceNgs2GeomSourceParam* outSourceParam);
    /// <summary>
    /// Turns a listener's placing into the working form <see cref="sceNgs2GeomApply"/> takes. Compute
    /// this once per frame and apply every source against it.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceNgs2GeomCalcListener(
        SceNgs2GeomListenerParam* param, SceNgs2GeomListenerWork* outWork, uint flags);
    /// <summary>
    /// Works out what one source sounds like to one listener: the pitch shift its movement causes and
    /// the level for each speaker.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceNgs2GeomApply(
        SceNgs2GeomListenerWork* listener, SceNgs2GeomSourceParam* source,
        SceNgs2GeomAttribute* outAttrib, uint flags);

    /// <summary>
    /// Prepares a panning workspace. <paramref name="speakerAngles"/> names where each speaker sits, or
    /// is null for the even spacing <paramref name="unitAngle"/> describes.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceNgs2PanInit(
        SceNgs2PanWork* work, float* speakerAngles, float unitAngle, uint numSpeakers);
    /// <summary>
    /// Turns one angle and distance per source into the level for each speaker, written to
    /// <paramref name="outVolumeMatrix"/> as <c>numParams</c> rows of <c>matrixFormat</c> levels.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceNgs2PanGetVolumeMatrix(
        SceNgs2PanWork* work, SceNgs2PanParam* @params, uint numParams, uint matrixFormat,
        float* outVolumeMatrix);

    /// <summary>
    /// Registers <paramref name="handler"/> for one kind of report. This is the only channel a rejected
    /// rack or voice parameter is described through: the call that made it answers a single number.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceNgs2ReportRegisterHandler(
        Ngs2ReportType reportType, nint handler, nuint userData, nuint* outHandle);
    /// <summary>Stops a handler registered above.</summary>
    [LibraryImport(Lib)] public static partial int sceNgs2ReportUnregisterHandler(nuint reportHandle);

    /// <summary>Reads back how one module of a custom rack is wired.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNgs2CustomRackGetModuleInfo(
        nuint rackHandle, uint moduleIndex, SceNgs2CustomModuleInfo* outInfo, nuint infoSize);
}
