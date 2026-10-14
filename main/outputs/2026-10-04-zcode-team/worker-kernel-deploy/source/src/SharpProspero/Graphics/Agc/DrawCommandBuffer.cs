// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Agc;
using SharpProspero.Interop.Kernel;
using SharpProspero.Memory;
using System;
using System.Runtime.InteropServices;

namespace SharpProspero.Graphics.Agc;

/// <summary>
/// A graphics command buffer. It records GPU commands - draws, register writes, and synchronization -
/// into a caller-provided buffer, then hands them to the GPU through the driver. The recorder is a
/// two-sided allocator over a block of 32-bit words: commands are appended from the low end as the write
/// cursor rises, so <see cref="SubmitSizeDwords"/> is how much has been recorded.
/// </summary>
/// <remarks>
/// The word buffer must be memory the GPU can read - direct memory mapped with GPU access, the same kind
/// the display path uses - because the GPU reads the recorded commands straight from it. This type does
/// not allocate that memory; the caller provides it and keeps it alive for as long as the buffer records
/// and until the GPU has finished the submission. Only a small book-keeping block is owned here and freed
/// by <see cref="Dispose"/>. Each recording call wraps a command builder from
/// <see cref="SharpProspero.Interop.Agc.SceAgc"/>: the builder writes its packet and advances the cursor,
/// and returns the address of the packet it wrote (useful for the indirect-argument patch calls).
/// If the buffer fills, every builder calls the routine below, which answers that no more room can be
/// had; the builder then returns a null packet address rather than overrunning. Size the buffer for the
/// frame and check <see cref="RemainingDwords"/> when in doubt.
/// </remarks>
public sealed unsafe class DrawCommandBuffer : IDisposable
{
    private State* _state;
    private readonly uint* _buffer;
    private readonly uint _capacityDwords;
    private DirectMemoryRegion? _ownedRegion;

    // The state block, guarded: every record call and query goes through here so a use after Dispose
    // throws ObjectDisposedException rather than dereferencing a freed (null) pointer.
    private State* St
    {
        get
        {
            ObjectDisposedException.ThrowIf(_state is null, this);
            return _state;
        }
    }

    /// <summary>
    /// Records into <paramref name="buffer"/>, a block of <paramref name="sizeInBytes"/> bytes of
    /// GPU-readable memory the caller owns. The size is rounded down to a whole number of 32-bit words.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="buffer"/> is null.</exception>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="sizeInBytes"/> is under four bytes.</exception>
    public DrawCommandBuffer(void* buffer, uint sizeInBytes)
    {
        if (buffer is null)
            throw new ArgumentNullException(nameof(buffer));
        if (sizeInBytes < sizeof(uint))
            throw new ArgumentOutOfRangeException(nameof(sizeInBytes));

        _buffer = (uint*)buffer;
        _capacityDwords = sizeInBytes / sizeof(uint);
        _state = (State*)NativeMemory.AllocZeroed((nuint)sizeof(State));
        Reset();
    }

    /// <summary>
    /// Creates a command buffer backed by a freshly allocated block of GPU-readable direct memory of
    /// <paramref name="sizeInBytes"/> bytes, which the buffer owns and releases on <see cref="Dispose"/>.
    /// The simplest way to get a ready-to-record buffer.
    /// </summary>
    /// <exception cref="ProsperoException">The memory could not be reserved or mapped.</exception>
    public static DrawCommandBuffer Allocate(uint sizeInBytes)
    {
        DirectMemoryRegion region = DirectMemoryRegion.Allocate(sizeInBytes, KernelMemory.PageSize);
        try
        {
            return new DrawCommandBuffer(region.Pointer, sizeInBytes) { _ownedRegion = region };
        }
        catch
        {
            region.Dispose();
            throw;
        }
    }

    /// <summary>The book-keeping block. The packet builders read and advance the cursors in it.</summary>
    public void* Handle => St;

    /// <summary>The recorded words themselves, which is what a submit call describes to the driver.</summary>
    public void* BufferAddress => St->Bottom;

    /// <summary>The total capacity, in 32-bit words.</summary>
    public uint CapacityDwords => _capacityDwords;

    /// <summary>The number of words recorded so far - what a submit call sends.</summary>
    public uint SubmitSizeDwords => (uint)(St->UpCursor - St->Bottom);

    /// <summary>The number of bytes recorded so far.</summary>
    public uint SubmitSizeBytes => SubmitSizeDwords * sizeof(uint);

    /// <summary>The number of words still free to record into.</summary>
    public uint RemainingDwords => (uint)(St->DownCursor - St->UpCursor);

    /// <summary>Clears the recording so the buffer can be written again from the start.</summary>
    public void Reset()
    {
        State* st = St;
        st->Bottom = _buffer;
        st->Top = _buffer + _capacityDwords;
        st->UpCursor = _buffer;
        st->DownCursor = _buffer + _capacityDwords;
        st->Callback = (nint)(delegate* unmanaged<State*, uint, void*, byte>)&OutOfSpace;
        st->UserData = null;
        st->ReservedDwords = 0;
    }

    // What a builder calls when the recording will not fit. Answering zero means no more room can be
    // had, which is what makes the builder give back a null packet address. There is no choice about
    // installing it: every builder calls through this slot without looking at it first, so leaving it
    // empty is a call to address zero rather than a recording that stops politely.
    [UnmanagedCallersOnly]
    private static byte OutOfSpace(State* allocator, uint sizeInDwords, void* userData) => 0;

    /// <summary>Packs a register offset and value the way the direct register-write packet expects.</summary>
    internal static ulong Pack(uint offset, uint value) => (offset & 0xffffu) | ((ulong)value << 32);

    // --- Register writes. Cx = context, Sh = shader, Uc = user config: the three register spaces. ---

    /// <summary>Writes one context register (<paramref name="offset"/> = <paramref name="value"/>).</summary>
    public nint SetContextRegister(uint offset, uint value) => (nint)SceAgc.sceAgcDcbSetCxRegisterDirect(St, Pack(offset, value));

    /// <summary>Writes one shader register (<paramref name="offset"/> = <paramref name="value"/>).</summary>
    public nint SetShaderRegister(uint offset, uint value) => (nint)SceAgc.sceAgcDcbSetShRegisterDirect(St, Pack(offset, value));

    /// <summary>Writes one user-config register (<paramref name="offset"/> = <paramref name="value"/>).</summary>
    public nint SetUserConfigRegister(uint offset, uint value) => (nint)SceAgc.sceAgcDcbSetUcRegisterDirect(St, Pack(offset, value));

    // --- Index state and draws. ---

    /// <summary>Sets the index buffer base address for indexed draws.</summary>
    public nint SetIndexBuffer(void* indexAddr) => (nint)SceAgc.sceAgcDcbSetIndexBuffer(St, indexAddr);

    /// <summary>Sets the number of indices for a following draw.</summary>
    public nint SetIndexCount(uint indexCount) => (nint)SceAgc.sceAgcDcbSetIndexCount(St, indexCount);

    /// <summary>Sets the index element size (<paramref name="indexSize"/>) and its cache policy.</summary>
    public nint SetIndexSize(byte indexSize, byte cachePolicy = 0) => (nint)SceAgc.sceAgcDcbSetIndexSize(St, indexSize, cachePolicy);

    /// <summary>Sets the instance count for following draws.</summary>
    public nint SetNumInstances(uint numInstances) => (nint)SceAgc.sceAgcDcbSetNumInstances(St, numInstances);

    /// <summary>Records an indexed draw of <paramref name="indexCount"/> indices from <paramref name="indexAddr"/>.</summary>
    public nint DrawIndex(uint indexCount, void* indexAddr, ulong modifier = 0) => (nint)SceAgc.sceAgcDcbDrawIndex(St, indexCount, indexAddr, modifier);

    /// <summary>Records a non-indexed draw of <paramref name="indexCount"/> vertices.</summary>
    public nint DrawIndexAuto(uint indexCount, ulong modifier = 0) => (nint)SceAgc.sceAgcDcbDrawIndexAuto(St, indexCount, modifier);

    /// <summary>Records an indexed draw starting at <paramref name="indexOffset"/> in the bound index buffer.</summary>
    public nint DrawIndexOffset(uint indexOffset, uint indexCount, ulong modifier = 0) => (nint)SceAgc.sceAgcDcbDrawIndexOffset(St, indexOffset, indexCount, modifier);

    // --- Synchronization. ---

    /// <summary>
    /// Records a wait until the display has released buffer <paramref name="displayBufferIndex"/> of
    /// video-out handle <paramref name="videoOutHandle"/>, so the buffer may be rendered into again.
    /// Both have to name a real display buffer: neither is a mode, and a handle of zero matches no
    /// display, which leaves the wait out of the recording without saying so.
    /// </summary>
    public nint WaitUntilSafeForRendering(uint videoOutHandle, int displayBufferIndex)
        => (nint)SceAgc.sceAgcDcbWaitUntilSafeForRendering(St, videoOutHandle, displayBufferIndex);

    /// <summary>The queue a draw buffer records into.</summary>
    private const uint DrawQueue = 0;

    /// <summary>Records an end-of-pipe event of <paramref name="eventType"/>.</summary>
    public nint EventWrite(uint eventType, ulong eventControl = 0) => (nint)SceAgc.sceAgcDcbEventWrite(St, eventType, eventControl);

    // --- Present: wait for the display to release a buffer, then queue the flip. ---

    /// <summary>
    /// Records a wait until the display has released buffer <paramref name="bufferIndex"/> of video-out
    /// handle <paramref name="videoOutHandle"/>, so the GPU may render into it. Returns the number of
    /// words the packet took.
    /// </summary>
    /// <remarks>
    /// The routine underneath takes the slot holding this buffer's write cursor rather than the buffer:
    /// it reads the cursor, appends the packet, and writes the advanced cursor back. Handing it the
    /// book-keeping block instead put the packet wherever the block's first word pointed and then
    /// overwrote that word - the buffer's own base - on the first frame. It also checks nothing about
    /// space, so the room is checked here.
    /// </remarks>
    /// <exception cref="InvalidOperationException">The buffer has no room for the packet.</exception>
    public uint WaitUntilSafeForDisplay(int videoOutHandle, uint bufferIndex)
    {
        uint size = SceAgcDriver.sceAgcDriverGetWaitRenderingPacketSizeInDwords();
        if (RemainingDwords < St->ReservedDwords + size)
            throw new InvalidOperationException("The command buffer has no room for the wait packet.");
        return SceAgcDriver.sceAgcDriverWaitUntilSafeForRendering(
            &St->UpCursor, size, DrawQueue, (uint)videoOutHandle, (int)bufferIndex);
    }

    /// <summary>
    /// Records a flip so the GPU displays buffer <paramref name="bufferIndex"/> of video-out handle
    /// <paramref name="videoOutHandle"/> once it reaches this point. <paramref name="flipMode"/> is the
    /// video-out flip mode (vertical sync by default). The flip flushes the render caches to memory first.
    /// </summary>
    public nint SetFlip(int videoOutHandle, int bufferIndex, uint flipMode = 1, long flipArg = 0)
        => (nint)SceAgc.sceAgcDcbSetFlip(St, (uint)videoOutHandle, bufferIndex, flipMode, flipArg);

    /// <summary>Appends <paramref name="dwordCount"/> no-op words (padding). The generic packet works on a draw buffer.</summary>
    public nint Nop(uint dwordCount) => (nint)SceAgc.sceAgcCbNop(St, dwordCount);

    /// <summary>Releases the book-keeping block. The recording buffer is the caller's and is not freed.</summary>
    public void Dispose()
    {
        if (_state is not null)
        {
            NativeMemory.Free(_state);
            _state = null;
        }
        _ownedRegion?.Dispose();
        _ownedRegion = null;
        GC.SuppressFinalize(this);
    }

    /// <summary>Reclaims the book-keeping block if the buffer was dropped without a <see cref="Dispose"/> call.</summary>
    ~DrawCommandBuffer()
    {
        // Free only the unmanaged block here; the owned region has its own finalizer.
        if (_state is not null)
        {
            NativeMemory.Free(_state);
            _state = null;
        }
    }

    // The command-buffer state the flat-C builders read and advance: a two-sided allocator over the word
    // buffer. The builders read the cursors and the callback at these exact offsets, so the layout is fixed.
    [StructLayout(LayoutKind.Sequential)]
    private struct State
    {
        public uint* Bottom;        // 0x00 start of the buffer (lowest address)
        public uint* Top;           // 0x08 end of the buffer (highest address)
        public uint* UpCursor;      // 0x10 lowest free word; the write cursor, advanced by each builder
        public uint* DownCursor;    // 0x18 highest free word (top-down allocations)
        public nint Callback;       // 0x20 out-of-room callback; every builder calls through it unconditionally
        public void* UserData;      // 0x28 callback argument
        public uint ReservedDwords; // 0x30 keep this many words free
        private uint _pad;          // 0x34 pad to eight-byte alignment
    }
}
