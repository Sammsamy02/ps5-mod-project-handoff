// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using SharpProspero.Interop.Video;
using SharpProspero.Memory;
using System;
using System.Runtime.InteropServices;

namespace SharpProspero.Media;

/// <summary>A decoded picture: where it is and how it is laid out.</summary>
/// <param name="Width">Picture width in pixels.</param>
/// <param name="Height">Picture height in pixels.</param>
/// <param name="PitchInBytes">Bytes from the start of one row to the next.</param>
/// <param name="Buffer">Where the picture is; this is the buffer that was offered for it.</param>
/// <param name="BufferSize">How large the picture is, in bytes.</param>
/// <param name="IsErrorFrame">Whether it came from damaged input.</param>
public readonly record struct DecodedPicture(
    int Width, int Height, int PitchInBytes, nint Buffer, nuint BufferSize, bool IsErrorFrame)
{
    /// <summary>The picture's bytes, as laid out by <see cref="PitchInBytes"/>.</summary>
    public unsafe ReadOnlySpan<byte> AsSpan() => new((void*)Buffer, checked((int)BufferSize));
}

/// <summary>
/// Decodes compressed video a unit at a time, handing back each picture as it is produced. Where
/// <see cref="MediaPlayer"/> plays a whole file, this gives an application the pictures themselves —
/// for a stream it receives, a format the player does not open, or anything that needs the frames
/// rather than playback.
/// </summary>
/// <remarks>
/// The service provides nothing itself: it is asked how much memory it needs and every region is
/// supplied by this type, each of the kind the service expects. The picture buffer is supplied per
/// call and the picture is written into it, so a caller keeps as many buffers as it needs frames in
/// flight. Dispose the decoder when finished; it is not thread-safe.
/// </remarks>
public sealed unsafe class VideoDecoder : IDisposable
{
    private readonly void* _computeQueue;
    private readonly void* _decoder;
    private readonly DirectMemoryRegion _computeMemory;
    private readonly DirectMemoryRegion _sharedMemory;
    private readonly DirectMemoryRegion _graphicsMemory;
    private readonly void* _ordinaryMemory;
    private bool _disposed;

    private VideoDecoder(
        void* computeQueue, void* decoder, DirectMemoryRegion computeMemory,
        DirectMemoryRegion sharedMemory, DirectMemoryRegion graphicsMemory, void* ordinaryMemory,
        nuint frameBufferSize, uint frameBufferAlignment)
    {
        _computeQueue = computeQueue;
        _decoder = decoder;
        _computeMemory = computeMemory;
        _sharedMemory = sharedMemory;
        _graphicsMemory = graphicsMemory;
        _ordinaryMemory = ordinaryMemory;
        FrameBufferSize = frameBufferSize;
        FrameBufferAlignment = frameBufferAlignment;
    }

    /// <summary>The largest picture buffer the decoder will ask for, in bytes.</summary>
    public nuint FrameBufferSize { get; }

    /// <summary>The alignment a picture buffer must be made on.</summary>
    public uint FrameBufferAlignment { get; }

    /// <summary>
    /// Creates a decoder for H.264 at up to <paramref name="maxWidth"/> by <paramref name="maxHeight"/>.
    /// The defaults suit a 1080p stream; the height is the coded height, which is a multiple of sixteen.
    /// </summary>
    /// <exception cref="ProsperoException">The queue, the memory or the decoder could not be created.</exception>
    public static VideoDecoder CreateAvc(
        int maxWidth = 1920,
        int maxHeight = 1088,
        Videodec2AvcProfile profile = Videodec2AvcProfile.High,
        uint maxLevel = 42)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(maxWidth);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(maxHeight);

        // The compute queue is asked what it needs, given memory both sides reach, and taken.
        SceVideodec2ComputeMemoryInfo computeInfo = default;
        computeInfo.ThisSize = (nuint)sizeof(SceVideodec2ComputeMemoryInfo);
        SceResult.ThrowIfFailed(
            Videodec2.sceVideodec2QueryComputeMemoryInfo(&computeInfo),
            nameof(Videodec2.sceVideodec2QueryComputeMemoryInfo));

        DirectMemoryRegion computeMemory = AllocateShared(computeInfo.CpuGpuMemorySize);
        DirectMemoryRegion? sharedMemory = null;
        DirectMemoryRegion? graphicsMemory = null;
        void* ordinaryMemory = null;
        void* computeQueue = null;
        try
        {
            computeInfo.CpuGpuMemory = computeMemory.Pointer;
            computeInfo.CpuGpuMemorySize = computeMemory.Size;

            SceVideodec2ComputeConfigInfo computeConfig = default;
            computeConfig.ThisSize = (nuint)sizeof(SceVideodec2ComputeConfigInfo);
            computeConfig.ComputePipeId = 0;
            computeConfig.ComputeQueueId = 0;
            computeConfig.CheckMemoryType = false;

            SceResult.ThrowIfFailed(
                Videodec2.sceVideodec2AllocateComputeQueue(&computeConfig, &computeInfo, &computeQueue),
                nameof(Videodec2.sceVideodec2AllocateComputeQueue));

            // The settings below mirror the arrangement the service documents for this form; only the
            // picture size and level change between presets.
            SceVideodec2DecoderConfigInfo config = default;
            config.ThisSize = (nuint)sizeof(SceVideodec2DecoderConfigInfo);
            config.ResourceType = (uint)Videodec2ResourceType.Compute;
            config.CodecType = (uint)Videodec2CodecType.Avc;
            config.Profile = (uint)profile;
            config.MaxLevel = maxLevel;
            config.MaxFrameWidth = maxWidth;
            config.MaxFrameHeight = maxHeight;
            config.MaxDpbFrameCount = Videodec2.AutoFrameSetting;
            config.DecodeInputQueueDepth = 4;
            config.ComputeQueue = computeQueue;
            config.CpuAffinityMask = Videodec2.InheritAffinityMask;
            config.CpuThreadPriority = Videodec2.InheritThreadPriority;
            config.OptimizeProgressiveVideo = true;
            config.CheckMemoryType = false;
            config.ExtraConfigInfo = null;

            SceVideodec2DecoderMemoryInfo memory = default;
            memory.ThisSize = (nuint)sizeof(SceVideodec2DecoderMemoryInfo);
            SceResult.ThrowIfFailed(
                Videodec2.sceVideodec2QueryDecoderMemoryInfo(&config, &memory),
                nameof(Videodec2.sceVideodec2QueryDecoderMemoryInfo));

            // Each region is of the kind the service expects: ordinary memory for its own bookkeeping,
            // memory both sides reach, and memory the graphics side works in.
            ordinaryMemory = NativeMemory.AlignedAlloc(memory.CpuMemorySize, Videodec2.MemoryAlignment);
            sharedMemory = AllocateShared(memory.CpuGpuMemorySize);
            graphicsMemory = AllocateGraphics(memory.GpuMemorySize);

            memory.CpuMemory = ordinaryMemory;
            memory.CpuGpuMemory = sharedMemory.Pointer;
            memory.CpuGpuMemorySize = sharedMemory.Size;
            memory.GpuMemory = graphicsMemory.Pointer;
            memory.GpuMemorySize = graphicsMemory.Size;

            void* decoder = null;
            SceResult.ThrowIfFailed(
                Videodec2.sceVideodec2CreateDecoder(&config, &memory, &decoder),
                nameof(Videodec2.sceVideodec2CreateDecoder));

            return new VideoDecoder(
                computeQueue, decoder, computeMemory, sharedMemory, graphicsMemory, ordinaryMemory,
                memory.MaxFrameBufferSize, memory.FrameBufferAlignment);
        }
        catch
        {
            if (ordinaryMemory is not null)
                NativeMemory.AlignedFree(ordinaryMemory);
            graphicsMemory?.Dispose();
            sharedMemory?.Dispose();
            if (computeQueue is not null)
                Videodec2.sceVideodec2ReleaseComputeQueue(computeQueue);
            computeMemory.Dispose();
            throw;
        }
    }

    /// <summary>
    /// Reserves a picture buffer of the size and alignment this decoder asks for. Keep one per picture
    /// in flight and give it back when the picture has been used.
    /// </summary>
    /// <exception cref="ProsperoException">The buffer could not be reserved.</exception>
    public DirectMemoryRegion AllocateFrameBuffer()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        nuint alignment = FrameBufferAlignment == 0 ? Videodec2.MemoryAlignment : FrameBufferAlignment;
        return AllocateShared(FrameBufferSize, alignment);
    }

    /// <summary>
    /// Decodes one compressed unit, writing any picture into <paramref name="frameBuffer"/>. Returns
    /// null when the unit produced no picture yet, which is normal while the decoder fills up.
    /// <paramref name="attachedData"/> is carried with the unit for the service's own use; reading it
    /// back needs the picture-detail call, which is not bound here.
    /// </summary>
    /// <exception cref="ProsperoException">The unit could not be decoded.</exception>
    public DecodedPicture? Decode(ReadOnlySpan<byte> unit, DirectMemoryRegion frameBuffer, ulong attachedData = 0)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentNullException.ThrowIfNull(frameBuffer);
        if (unit.IsEmpty)
            return null;

        SceVideodec2OutputInfo output = default;
        output.ThisSize = (nuint)sizeof(SceVideodec2OutputInfo);

        SceVideodec2FrameBuffer buffer = default;
        buffer.ThisSize = (nuint)sizeof(SceVideodec2FrameBuffer);
        buffer.FrameBuffer = frameBuffer.Pointer;
        buffer.FrameBufferSize = frameBuffer.Size;

        fixed (byte* p = unit)
        {
            SceVideodec2InputData input = default;
            input.ThisSize = (nuint)sizeof(SceVideodec2InputData);
            input.AuData = p;
            input.AuSize = (nuint)unit.Length;
            input.AttachedData = attachedData;

            SceResult.ThrowIfFailed(
                Videodec2.sceVideodec2Decode(_decoder, &input, &buffer, &output),
                nameof(Videodec2.sceVideodec2Decode));
        }

        return Describe(&output);
    }

    /// <summary>
    /// Pushes out a picture the decoder was still holding, writing it into
    /// <paramref name="frameBuffer"/>. Call this until it returns null once the input has run out.
    /// </summary>
    /// <exception cref="ProsperoException">The decoder could not be flushed.</exception>
    public DecodedPicture? Flush(DirectMemoryRegion frameBuffer)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentNullException.ThrowIfNull(frameBuffer);

        SceVideodec2OutputInfo output = default;
        output.ThisSize = (nuint)sizeof(SceVideodec2OutputInfo);

        SceVideodec2FrameBuffer buffer = default;
        buffer.ThisSize = (nuint)sizeof(SceVideodec2FrameBuffer);
        buffer.FrameBuffer = frameBuffer.Pointer;
        buffer.FrameBufferSize = frameBuffer.Size;

        SceResult.ThrowIfFailed(
            Videodec2.sceVideodec2Flush(_decoder, &buffer, &output), nameof(Videodec2.sceVideodec2Flush));
        return Describe(&output);
    }

    /// <summary>Drops what the decoder was carrying, so the next unit starts fresh after a seek.</summary>
    /// <exception cref="ProsperoException">The decoder could not be reset.</exception>
    public void Reset()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        SceResult.ThrowIfFailed(Videodec2.sceVideodec2Reset(_decoder), nameof(Videodec2.sceVideodec2Reset));
    }

    private static DecodedPicture? Describe(SceVideodec2OutputInfo* output)
    {
        if (!output->IsValid || output->PictureCount == 0 || output->IsDiscardedFrame)
            return null;
        return new DecodedPicture(
            (int)output->FrameWidth,
            (int)output->FrameHeight,
            (int)output->FramePitchInBytes,
            (nint)output->FrameBuffer,
            output->FrameBufferSize,
            output->IsErrorFrame);
    }

    // Memory both sides reach, kept as its own mapping because the service inspects what backs it.
    private static DirectMemoryRegion AllocateShared(nuint bytes, nuint alignment = Videodec2.MemoryAlignment)
        => DirectMemoryRegion.Allocate(
            bytes, alignment, KernelMemory.MemoryTypeCachedShared,
            KernelMemory.ProtCpuReadWrite | KernelMemory.ProtGpuReadWrite, KernelMemory.MapNoCoalesce);

    // Memory the graphics side works in, which is a different kind from the shared regions.
    private static DirectMemoryRegion AllocateGraphics(nuint bytes)
        => DirectMemoryRegion.Allocate(
            bytes, Videodec2.MemoryAlignment, KernelMemory.MemoryTypeCached,
            KernelMemory.ProtCpuReadWrite | KernelMemory.ProtGpuReadWrite, KernelMemory.MapNoCoalesce);

    /// <summary>Destroys the decoder, gives the compute queue back and releases every region.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;

        Videodec2.sceVideodec2DeleteDecoder(_decoder);
        Videodec2.sceVideodec2ReleaseComputeQueue(_computeQueue);
        if (_ordinaryMemory is not null)
            NativeMemory.AlignedFree(_ordinaryMemory);
        _graphicsMemory.Dispose();
        _sharedMemory.Dispose();
        _computeMemory.Dispose();
    }
}
