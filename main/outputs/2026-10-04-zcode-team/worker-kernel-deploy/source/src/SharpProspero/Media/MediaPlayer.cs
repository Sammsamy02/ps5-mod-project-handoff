// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Graphics;
using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using SharpProspero.Interop.Media;
using System;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace SharpProspero.Media;

/// <summary>One decoded audio frame: the samples, when they play, and how they are laid out.</summary>
public readonly ref struct AudioFrame
{
    /// <summary>The interleaved 16-bit samples.</summary>
    public ReadOnlySpan<short> Samples { get; init; }

    /// <summary>When the frame plays, in milliseconds.</summary>
    public ulong TimeStamp { get; init; }

    /// <summary>How many channels the samples interleave.</summary>
    public int ChannelCount { get; init; }

    /// <summary>Samples per second.</summary>
    public int SampleRate { get; init; }
}

/// <summary>
/// One decoded video frame in NV12 (a luma plane followed by an interleaved chroma plane). Its pixels
/// stay valid until the next call for a frame, so draw it to the display right away with
/// <see cref="RenderTo(Surface, int, int)"/>, which converts it to the surface's color format.
/// </summary>
/// <remarks>
/// The buffer the decoder hands back is wider and taller than the picture in it. Which part is the
/// picture is given as four insets from the buffer's edges, and the horizontal pair is measured from
/// the pitch, so the padding that makes the rows a convenient length is counted as crop. Drawing the
/// whole buffer therefore shows a band of whatever the decoder left in that padding.
/// </remarks>
public readonly ref struct VideoFrame
{
    private readonly unsafe byte* _data;

    internal unsafe VideoFrame(byte* data, int width, int height, int pitch, ulong timeStamp,
        int cropLeft, int cropRight, int cropTop, int cropBottom)
    {
        _data = data;
        Width = width;
        Height = height;
        Pitch = pitch;
        TimeStamp = timeStamp;
        CropLeft = cropLeft;
        CropRight = cropRight;
        CropTop = cropTop;
        CropBottom = cropBottom;
    }

    /// <summary>The width the stream declares, in pixels.</summary>
    public int Width { get; }

    /// <summary>The height of the buffer the decoder wrote, in pixels.</summary>
    public int Height { get; }

    /// <summary>Row pitch of the luma and chroma planes in bytes.</summary>
    public int Pitch { get; }

    /// <summary>Columns at the left of the buffer that are not part of the picture.</summary>
    public int CropLeft { get; }

    /// <summary>
    /// Columns at the right of the buffer that are not part of the picture, counted from the pitch.
    /// </summary>
    public int CropRight { get; }

    /// <summary>Rows at the top of the buffer that are not part of the picture.</summary>
    public int CropTop { get; }

    /// <summary>Rows at the bottom of the buffer that are not part of the picture.</summary>
    public int CropBottom { get; }

    /// <summary>The width of the picture inside the buffer, in pixels.</summary>
    public int VisibleWidth => Pitch - CropLeft - CropRight;

    /// <summary>The height of the picture inside the buffer, in pixels.</summary>
    public int VisibleHeight => Height - CropTop - CropBottom;

    /// <summary>When the frame plays, in milliseconds.</summary>
    public ulong TimeStamp { get; }

    /// <summary>Draws the picture at its own size with its top-left at (<paramref name="x"/>, <paramref name="y"/>).</summary>
    public void RenderTo(Surface destination, int x, int y) => RenderTo(destination, x, y, VisibleWidth, VisibleHeight);

    /// <summary>
    /// Draws the frame into the destination rectangle, scaling it to fit (nearest sampling) and
    /// converting from NV12 to the surface color. Use this to show video full-screen or in a window.
    /// </summary>
    public unsafe void RenderTo(Surface destination, int destX, int destY, int destWidth, int destHeight)
    {
        int sourceWidth = VisibleWidth, sourceHeight = VisibleHeight;
        if (_data == null || sourceWidth <= 0 || sourceHeight <= 0 || destWidth <= 0 || destHeight <= 0)
            return;

        // The chroma plane follows the whole luma buffer, so it starts past the buffer's height rather
        // than the picture's, and both planes are then read from the picture's corner inside it.
        byte* luma = _data;
        byte* chroma = _data + (long)Pitch * Height;
        int x0 = Math.Max(0, destX), y0 = Math.Max(0, destY);
        int x1 = Math.Min(destination.Width, destX + destWidth), y1 = Math.Min(destination.Height, destY + destHeight);
        uint* pixels = destination.Pixels;
        int stride = destination.Stride;

        for (int py = y0; py < y1; py++)
        {
            int sy = (py - destY) * sourceHeight / destHeight;
            if (sy >= sourceHeight) sy = sourceHeight - 1;
            sy += CropTop;
            byte* lumaRow = luma + (long)sy * Pitch;
            byte* chromaRow = chroma + (long)(sy >> 1) * Pitch;
            uint* destRow = pixels + (long)py * stride;
            for (int px = x0; px < x1; px++)
            {
                int sx = (px - destX) * sourceWidth / destWidth;
                if (sx >= sourceWidth) sx = sourceWidth - 1;
                sx += CropLeft;

                // Limited-range BT.601 conversion in integer arithmetic. The chroma is shared across a
                // 2x2 block, so the sample index drops the low bit and reads the U,V pair.
                int c = lumaRow[sx] - 16;
                int chromaIndex = sx & ~1;
                int d = chromaRow[chromaIndex] - 128;
                int e = chromaRow[chromaIndex + 1] - 128;
                int r = (298 * c + 409 * e + 128) >> 8;
                int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                int b = (298 * c + 516 * d + 128) >> 8;
                destRow[px] = 0xFF000000u | ((uint)Clamp(r) << 16) | ((uint)Clamp(g) << 8) | (uint)Clamp(b);
            }
        }
    }

    private static byte Clamp(int value) => (byte)(value < 0 ? 0 : value > 255 ? 255 : value);
}

/// <summary>
/// Plays a media file. Open it for a path, start it, then pull decoded audio frames to push at an audio
/// port and video frames to draw to the display, while it stays active. The player decodes on its own
/// threads and calls back for memory, which this type supplies.
/// </summary>
/// <example>
/// <code>
/// using var player = MediaPlayer.Open("/app0/movie.mp4");
/// using var audio = AudioOutDevice.OpenStereo();
/// player.Start();
/// while (player.IsActive)
/// {
///     if (player.TryGetAudioFrame(out AudioFrame audioFrame))
///         audio.Output(audioFrame.Samples);
///     if (player.TryGetVideoFrame(out VideoFrame videoFrame))
///         videoFrame.RenderTo(display.BackBuffer, 0, 0, display.Width, display.Height);
/// }
/// </code>
/// </example>
public sealed unsafe class MediaPlayer : IDisposable
{
    private void* _handle;
    private bool _disposed;

    private MediaPlayer(void* handle) => _handle = handle;

    /// <summary>
    /// Starts a player for the file at <paramref name="path"/>. The player reads the file itself, so
    /// the path must be one it can reach, and its extension must be one the player recognises:
    /// <c>.mp4</c>, <c>.m4v</c>, <c>.m4a</c> or <c>.mov</c> for MPEG-4, or <c>.webm</c>.
    /// </summary>
    /// <remarks>
    /// Only a local file plays. A stream over the network is a separate source form that needs the
    /// player's second initialization call and a network context out of services this SDK does not yet
    /// bind, so there is no address-based counterpart to this method.
    /// </remarks>
    /// <param name="path">Absolute path of the media file.</param>
    /// <param name="basePriority">Player thread priority; zero takes the default.</param>
    /// <exception cref="ProsperoException">The player would not start or would not take the source.</exception>
    public static MediaPlayer Open(string path, uint basePriority = 0)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);

        AvPlayerInitData data;
        AvPlayer.InitializeData(&data);
        data.MemoryReplacement.Allocate = &AllocateGeneral;
        data.MemoryReplacement.Deallocate = &DeallocateGeneral;
        data.MemoryReplacement.AllocateTexture = &AllocateTexture;
        data.MemoryReplacement.DeallocateTexture = &DeallocateTexture;
        data.BasePriority = basePriority;
        data.AutoStart = 0;

        void* handle = AvPlayer.sceAvPlayerInit(&data);
        if (handle == null)
            throw new ProsperoException(nameof(AvPlayer.sceAvPlayerInit), -1);

        var player = new MediaPlayer(handle);
        try
        {
            int byteCount = Encoding.UTF8.GetByteCount(path);
            Span<byte> buffer = byteCount < 512 ? stackalloc byte[byteCount + 1] : new byte[byteCount + 1];
            int written = Encoding.UTF8.GetBytes(path, buffer);
            buffer[written] = 0;
            int rc;
            fixed (byte* p = buffer)
                rc = AvPlayer.sceAvPlayerAddSource(handle, p);
            SceResult.ThrowIfFailed(rc, nameof(AvPlayer.sceAvPlayerAddSource));
            return player;
        }
        catch
        {
            player.Dispose();
            throw;
        }
    }

    /// <summary>Whether the player still has something to play.</summary>
    public bool IsActive => !_disposed && AvPlayer.sceAvPlayerIsActive(_handle);

    /// <summary>The playback position in milliseconds.</summary>
    public ulong Position => AvPlayer.sceAvPlayerCurrentTime(Live());

    /// <summary>How long <see cref="Start"/> waits for a source to be read before giving up.</summary>
    public static readonly TimeSpan SourceReadTimeout = TimeSpan.FromSeconds(10);

    /// <summary>
    /// Begins playback, turning on the first video, audio and subtitle stream the source carries.
    /// Reading a source runs on the player's own thread, so this waits for that to finish before it
    /// can see what the source holds.
    /// </summary>
    /// <exception cref="ProsperoException">
    /// Nothing was readable within <see cref="SourceReadTimeout"/>, a stream would not turn on, or
    /// playback would not start.
    /// </exception>
    public void Start()
    {
        void* handle = Live();
        EnableTheStreamsToPlay(handle);
        SceResult.ThrowIfFailed(AvPlayer.sceAvPlayerStart(handle), nameof(AvPlayer.sceAvPlayerStart));
    }

    // Playback carries the streams that were turned on and no others, and turning none on is refused
    // rather than taken as "play everything". What a source holds is known only once the player's own
    // thread has read it, which is why the count is waited on rather than read once.
    private static void EnableTheStreamsToPlay(void* handle)
    {
        int count = AvPlayer.sceAvPlayerStreamCount(handle);
        long until = Environment.TickCount64 + (long)SourceReadTimeout.TotalMilliseconds;
        while (count <= 0 && Environment.TickCount64 < until)
        {
            Thread.Sleep(1);
            count = AvPlayer.sceAvPlayerStreamCount(handle);
        }
        if (count <= 0)
            throw new ProsperoException(nameof(AvPlayer.sceAvPlayerStreamCount), count);

        // The first stream of each kind, which is the choice a player with no way to ask the caller can
        // make. A second stream of a kind is another language or another angle, and turning that on as
        // well would play both at once.
        Span<bool> taken = stackalloc bool[4];
        for (uint i = 0; i < (uint)count; i++)
        {
            AvPlayerStreamInfo info;
            SceResult.ThrowIfFailed(
                AvPlayer.sceAvPlayerGetStreamInfo(handle, i, &info), nameof(AvPlayer.sceAvPlayerGetStreamInfo));
            int kind = (int)info.Type;
            if (kind <= 0 || kind >= taken.Length || taken[kind])
                continue;
            SceResult.ThrowIfFailed(
                AvPlayer.sceAvPlayerEnableStream(handle, i), nameof(AvPlayer.sceAvPlayerEnableStream));
            taken[kind] = true;
        }
    }

    /// <summary>Stops playback.</summary>
    public void Stop() => SceResult.ThrowIfFailed(AvPlayer.sceAvPlayerStop(Live()), nameof(AvPlayer.sceAvPlayerStop));

    /// <summary>Pauses playback.</summary>
    public void Pause() => SceResult.ThrowIfFailed(AvPlayer.sceAvPlayerPause(Live()), nameof(AvPlayer.sceAvPlayerPause));

    /// <summary>Resumes paused playback.</summary>
    public void Resume() => SceResult.ThrowIfFailed(AvPlayer.sceAvPlayerResume(Live()), nameof(AvPlayer.sceAvPlayerResume));

    /// <summary>Sets whether the source repeats.</summary>
    public void SetLooping(bool loop)
        => SceResult.ThrowIfFailed(AvPlayer.sceAvPlayerSetLooping(Live(), loop), nameof(AvPlayer.sceAvPlayerSetLooping));

    /// <summary>Moves playback to <paramref name="milliseconds"/>.</summary>
    public void JumpTo(ulong milliseconds)
        => SceResult.ThrowIfFailed(AvPlayer.sceAvPlayerJumpToTime(Live(), milliseconds), nameof(AvPlayer.sceAvPlayerJumpToTime));

    /// <summary>
    /// Takes the next decoded audio frame. Returns false when none is ready, which is normal while the
    /// player is still decoding. The samples stay valid until the next call.
    /// </summary>
    public bool TryGetAudioFrame(out AudioFrame frame)
    {
        AvPlayerFrameInfo info;
        if (!AvPlayer.sceAvPlayerGetAudioData(Live(), &info) || info.Data == null)
        {
            frame = default;
            return false;
        }

        // The payload size is in bytes; the samples are 16-bit.
        int sampleCount = (int)(info.Details.Audio.Size / sizeof(short));
        frame = new AudioFrame
        {
            Samples = new ReadOnlySpan<short>(info.Data, sampleCount),
            TimeStamp = info.TimeStamp,
            ChannelCount = info.Details.Audio.ChannelCount,
            SampleRate = (int)info.Details.Audio.SampleRate,
        };
        return true;
    }

    /// <summary>
    /// Takes the next decoded video frame. Returns false when none is ready, which is normal while the
    /// player is still decoding. Draw the frame to the display with <see cref="VideoFrame.RenderTo(Surface, int, int)"/>;
    /// it stays valid until the next call.
    /// </summary>
    public bool TryGetVideoFrame(out VideoFrame frame)
    {
        AvPlayerFrameInfoEx info = default;
        if (!AvPlayer.sceAvPlayerGetVideoDataEx(Live(), &info) || info.Data == null)
        {
            frame = default;
            return false;
        }
        frame = new VideoFrame(
            (byte*)info.Data, (int)info.VideoWidth, (int)info.VideoHeight, (int)info.VideoPitch, info.TimeStamp,
            (int)info.CropLeft, (int)info.CropRight, (int)info.CropTop, (int)info.CropBottom);
        return true;
    }

    private void* Live()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        return _handle;
    }

    /// <summary>Shuts the player down and releases it.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        if (_handle != null)
        {
            AvPlayer.sceAvPlayerClose(_handle);
            _handle = null;
        }
    }

    // The player has no allocator of its own and calls these from its own threads, so they must not
    // touch managed state. Aligned blocks come straight from the unmanaged heap. The player expects a
    // null return when a block cannot be given, and it runs on a thread with no managed frame above
    // it, so an exception must never leave this method: it would abort the process instead of failing
    // the allocation. Every path that could throw is turned into a null return.
    [UnmanagedCallersOnly]
    private static void* AllocateGeneral(void* context, uint alignment, uint size)
    {
        try
        {
            return NativeMemory.AlignedAlloc(size, RoundUpToPowerOfTwo(alignment));
        }
        catch
        {
            return null;
        }
    }

    // The aligned allocator requires a power-of-two alignment; the player passes a plain byte count.
    private static nuint RoundUpToPowerOfTwo(uint alignment)
    {
        if (alignment <= 1)
            return 1;
        return (nuint)BitOperations.RoundUpToPowerOf2(alignment);
    }

    [UnmanagedCallersOnly]
    private static void DeallocateGeneral(void* context, void* memory)
    {
        if (memory != null)
            NativeMemory.AlignedFree(memory);
    }

    // Video frame buffers are GPU memory: the decoder writes them and the caller reads them, so they
    // must be GPU-visible direct memory, not the plain heap the general allocator hands back. The
    // player asks for and releases them by pointer through these callbacks, so the reservation each
    // pointer belongs to is tracked in a small unmanaged table, keyed by the mapped address, that both the
    // player's threads reach without touching the managed heap.
    private struct TextureSlot
    {
        public void* Pointer;
        public long Offset;
        public nuint Size;
    }

    private const int TextureSlotCount = 128;
    private static readonly TextureSlot* TextureSlots =
        (TextureSlot*)NativeMemory.AllocZeroed((nuint)(TextureSlotCount * sizeof(TextureSlot)));
    private static int _textureLock;

    private static void EnterTextureLock()
    {
        while (Interlocked.CompareExchange(ref _textureLock, 1, 0) != 0)
        {
        }
    }

    private static void ExitTextureLock() => Interlocked.Exchange(ref _textureLock, 0);

    [UnmanagedCallersOnly]
    private static void* AllocateTexture(void* context, uint alignment, uint size)
    {
        try
        {
            // Direct-memory alignment must be a power of two and at least the page size.
            nuint align = alignment <= 0x4000 ? 0x4000 : (nuint)BitOperations.RoundUpToPowerOf2(alignment);
            nuint bytes = (size + align - 1) / align * align;

            long offset = 0;
            long pool = (long)KernelMemory.sceKernelGetDirectMemorySize();
            if (KernelMemory.sceKernelAllocateDirectMemory(0, pool, bytes, align, KernelMemory.MemoryTypeCachedShared, &offset) < 0)
                return null;

            void* address = null;
            if (KernelMemory.sceKernelMapDirectMemory(&address, bytes, KernelMemory.ProtCpuReadWrite | KernelMemory.ProtGpuAll, 0, offset, align) < 0)
            {
                KernelMemory.sceKernelReleaseDirectMemory(offset, bytes);
                return null;
            }

            EnterTextureLock();
            try
            {
                for (int i = 0; i < TextureSlotCount; i++)
                {
                    if (TextureSlots[i].Pointer == null)
                    {
                        TextureSlots[i].Pointer = address;
                        TextureSlots[i].Offset = offset;
                        TextureSlots[i].Size = bytes;
                        return address;
                    }
                }
            }
            finally
            {
                ExitTextureLock();
            }

            // The table is full; this many concurrent frame buffers is not expected, so give the
            // reservation and the addresses it was mapped at back and fail.
            KernelMemory.sceKernelMunmap(address, bytes);
            KernelMemory.sceKernelReleaseDirectMemory(offset, bytes);
            return null;
        }
        catch
        {
            return null;
        }
    }

    [UnmanagedCallersOnly]
    private static void DeallocateTexture(void* context, void* memory)
    {
        if (memory == null)
            return;

        long offset = 0;
        nuint size = 0;
        bool found = false;
        EnterTextureLock();
        try
        {
            for (int i = 0; i < TextureSlotCount; i++)
            {
                if (TextureSlots[i].Pointer == memory)
                {
                    offset = TextureSlots[i].Offset;
                    size = TextureSlots[i].Size;
                    TextureSlots[i] = default;
                    found = true;
                    break;
                }
            }
        }
        finally
        {
            ExitTextureLock();
        }

        // This callback both releases and unmaps: releasing alone gives the reservation back but leaves
        // the address range taken, so a player that cycles frame buffers exhausts the address space.
        if (found)
        {
            KernelMemory.sceKernelMunmap(memory, size);
            KernelMemory.sceKernelReleaseDirectMemory(offset, size);
        }
    }
}
