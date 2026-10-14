// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace SharpProspero.Platform;

/// <summary>
/// A reading of a transfer's progress, taken from the service's record. It covers four of the record's
/// fields and leaves the rest of the 88-byte record out. Read the whole record with
/// <see cref="DownloadService.TryGetProgressRecord"/>.
/// </summary>
/// <param name="State">
/// The status word. Its low two bits are the transfer's sub-state (three means finished), and the next
/// bits carry a phase; the named helpers read the parts that are established.
/// </param>
/// <param name="ErrorCode">The transfer's result code: zero while healthy, negative on a failure.</param>
/// <param name="TotalBytes">The transfer's total size in bytes.</param>
/// <param name="TransferredBytes">How many bytes have transferred so far.</param>
public readonly record struct TransferProgress(uint State, int ErrorCode, ulong TotalBytes, ulong TransferredBytes)
{
    // The low two bits of the status word carry the sub-state; the value three is the finished state.
    private const uint SubStateMask = 0x3;
    private const uint SubStateFinished = 0x3;

    /// <summary>True once the transfer has finished.</summary>
    public bool IsComplete => (State & SubStateMask) == SubStateFinished;

    /// <summary>True when the transfer's result code reports a failure.</summary>
    public bool HasError => ErrorCode < 0;

    /// <summary>
    /// How far the transfer has gone, from 0 to 100. Zero when the total is not yet known, so a caller
    /// can show it before the size arrives.
    /// </summary>
    public int PercentComplete => TotalBytes == 0 ? 0 : (int)(TransferredBytes * 100 / TotalBytes);
}

/// <summary>
/// Controls the system's background transfers: find the task carrying a piece of content, then stop,
/// pause or resume it. A tool that reports what the console is downloading, or that holds a transfer
/// back while something else runs, works through this.
/// </summary>
/// <remarks>
/// The service is loaded at run time and needs a block of memory from the caller, which this type
/// reserves and releases. Reaching it depends on what the running build is permitted to do, so
/// <see cref="TryOpen"/> reports a refusal rather than raising. Tasks are addressed by the identifier
/// the service assigns; <see cref="TryFindTaskByContentId"/> turns a content identifier into one.
/// <para>
/// Creating a transfer is not offered: the call that registers one from storage reports "not
/// supported" on a retail system, and the call that takes a network address needs a parameter block
/// this SDK does not describe. This type controls transfers that already exist.
/// </para>
/// </remarks>
public sealed unsafe class DownloadService : IDisposable
{
    /// <summary>Where the transfer service is loaded from.</summary>
    public const string ModulePath = "/system/common/lib/libSceBgft.sprx";

    /// <summary>
    /// The transfer kinds <see cref="TryFindTaskByContentId"/> accepts. The service takes exactly the
    /// values 6, 7 and 8; which kind each selects is not established, so a caller passes the value that
    /// matches what it is looking for rather than a named one.
    /// </summary>
    public static ReadOnlySpan<int> FindKinds => [6, 7, 8];

    /// <summary>The smallest block of memory the service accepts, in bytes.</summary>
    public const int MinimumMemorySize = 1024 * 1024;

    /// <summary>The identifier used when no task matches.</summary>
    public const int NoTask = -1;

    // The block of memory the service is started with: a pointer, its size, and a word that must be
    // zero. The service rejects anything else.
    [StructLayout(LayoutKind.Sequential)]
    private struct InitParams
    {
        public void* Memory;
        public uint MemorySize;
        public int Reserved;
    }

    private readonly SystemLibrary _library;
    private readonly void* _memory;
    private readonly delegate* unmanaged<int, int> _startTask;
    private readonly delegate* unmanaged<int, int> _stopTask;
    private readonly delegate* unmanaged<int, int> _pauseTask;
    private readonly delegate* unmanaged<int, int> _resumeTask;
    private readonly delegate* unmanaged<int, byte*, int> _getProgress;
    private readonly delegate* unmanaged<byte*, int, int*, int> _findByContentId;
    private readonly delegate* unmanaged<int> _term;
    private bool _disposed;

    private DownloadService(
        SystemLibrary library, void* memory,
        delegate* unmanaged<int, int> startTask, delegate* unmanaged<int, int> stopTask,
        delegate* unmanaged<int, int> pauseTask, delegate* unmanaged<int, int> resumeTask,
        delegate* unmanaged<int, byte*, int> getProgress,
        delegate* unmanaged<byte*, int, int*, int> findByContentId,
        delegate* unmanaged<int> term)
    {
        _library = library;
        _memory = memory;
        _startTask = startTask;
        _stopTask = stopTask;
        _pauseTask = pauseTask;
        _resumeTask = resumeTask;
        _getProgress = getProgress;
        _findByContentId = findByContentId;
        _term = term;
    }

    /// <summary>How many bytes a progress reading occupies.</summary>
    public const int ProgressSize = 88;

    /// <summary>Loads the transfer service and starts it with a block of memory.</summary>
    /// <param name="memorySize">Bytes to give the service; at least <see cref="MinimumMemorySize"/>.</param>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="memorySize"/> is below the minimum.</exception>
    /// <exception cref="ProsperoException">The service could not be loaded or started.</exception>
    public static DownloadService Open(int memorySize = MinimumMemorySize)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(memorySize, MinimumMemorySize);

        SystemLibrary library = SystemLibrary.Open(ModulePath);
        void* memory = null;
        try
        {
            var init = (delegate* unmanaged<InitParams*, int>)library.GetFunction("sceBgftServiceInit");
            var term = (delegate* unmanaged<int>)library.GetFunction("sceBgftServiceTerm");
            var startTask = (delegate* unmanaged<int, int>)library.GetFunction("sceBgftServiceDownloadStartTask");
            var stopTask = (delegate* unmanaged<int, int>)library.GetFunction("sceBgftServiceDownloadStopTask");
            var pauseTask = (delegate* unmanaged<int, int>)library.GetFunction("sceBgftServiceDownloadPauseTask");
            var resumeTask = (delegate* unmanaged<int, int>)library.GetFunction("sceBgftServiceDownloadResumeTask");
            var getProgress = (delegate* unmanaged<int, byte*, int>)library.GetFunction("sceBgftServiceDownloadGetProgress");
            var findByContentId = (delegate* unmanaged<byte*, int, int*, int>)library.GetFunction("sceBgftServiceDownloadFindTaskByContentId");

            memory = NativeMemory.AllocZeroed((nuint)memorySize);
            InitParams parameters = default;
            parameters.Memory = memory;
            parameters.MemorySize = (uint)memorySize;
            parameters.Reserved = 0;

            SceResult.ThrowIfFailed(init(&parameters), "sceBgftServiceInit");
            return new DownloadService(
                library, memory, startTask, stopTask, pauseTask, resumeTask, getProgress, findByContentId, term);
        }
        catch
        {
            if (memory is not null)
                NativeMemory.Free(memory);
            library.Dispose();
            throw;
        }
    }

    /// <summary>
    /// Loads and starts the transfer service, reporting whether it could be reached rather than
    /// raising. Use this where the running build may not be permitted to reach it.
    /// </summary>
    public static bool TryOpen(out DownloadService? service, int memorySize = MinimumMemorySize)
    {
        try
        {
            service = Open(memorySize);
            return true;
        }
        catch (ProsperoException)
        {
            service = null;
            return false;
        }
    }

    /// <summary>
    /// Finds the transfer carrying <paramref name="contentId"/>, if there is one.
    /// <paramref name="kind"/> selects which sort of transfer to look for; the service defines its
    /// values.
    /// </summary>
    /// <returns>True when a task was found, with its identifier in <paramref name="taskId"/>.</returns>
    public bool TryFindTaskByContentId(string contentId, int kind, out int taskId)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentException.ThrowIfNullOrEmpty(contentId);

        int found = NoTask;
        byte[] owned = ToNullTerminated(contentId);
        bool ok;
        fixed (byte* p = owned)
            ok = SceResult.Succeeded(_findByContentId(p, kind, &found));

        taskId = ok ? found : NoTask;
        return ok && found != NoTask;
    }

    /// <summary>Starts the transfer <paramref name="taskId"/>.</summary>
    /// <exception cref="ProsperoException">The transfer could not be started.</exception>
    public void Start(int taskId) => Control(_startTask, taskId, "sceBgftServiceDownloadStartTask");

    /// <summary>Stops the transfer <paramref name="taskId"/>.</summary>
    /// <exception cref="ProsperoException">The transfer could not be stopped.</exception>
    public void Stop(int taskId) => Control(_stopTask, taskId, "sceBgftServiceDownloadStopTask");

    /// <summary>Holds the transfer <paramref name="taskId"/> back.</summary>
    /// <exception cref="ProsperoException">The transfer could not be paused.</exception>
    public void Pause(int taskId) => Control(_pauseTask, taskId, "sceBgftServiceDownloadPauseTask");

    /// <summary>Lets the transfer <paramref name="taskId"/> carry on.</summary>
    /// <exception cref="ProsperoException">The transfer could not be resumed.</exception>
    public void Resume(int taskId) => Control(_resumeTask, taskId, "sceBgftServiceDownloadResumeTask");

    /// <summary>
    /// Reads how far the transfer <paramref name="taskId"/> has gone: its state, result code, and the
    /// bytes transferred against the total. These are the fields of the service's record whose meaning
    /// is established; the rest of the record is not read here.
    /// </summary>
    /// <returns>True when the reading was taken.</returns>
    public bool TryGetProgress(int taskId, out TransferProgress progress)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        byte* record = stackalloc byte[ProgressSize];
        new Span<byte>(record, ProgressSize).Clear();
        if (!SceResult.Succeeded(_getProgress(taskId, record)))
        {
            progress = default;
            return false;
        }

        progress = new TransferProgress(
            *(uint*)(record + 0x00),
            *(int*)(record + 0x04),
            *(ulong*)(record + 0x18),
            *(ulong*)(record + 0x20));
        return true;
    }

    /// <summary>
    /// Reads the service's whole progress record for <paramref name="taskId"/> into
    /// <paramref name="destination"/>, which must be at least <see cref="ProgressSize"/> bytes. Use
    /// <see cref="TryGetProgress"/> for the fields that are named; this is for the bytes that are not.
    /// </summary>
    /// <returns>True when the record was read.</returns>
    /// <exception cref="ArgumentException"><paramref name="destination"/> is too small.</exception>
    public bool TryGetProgressRecord(int taskId, Span<byte> destination)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (destination.Length < ProgressSize)
            throw new ArgumentException($"The record needs at least {ProgressSize} bytes.", nameof(destination));

        destination[..ProgressSize].Clear();
        fixed (byte* p = destination)
            return SceResult.Succeeded(_getProgress(taskId, p));
    }

    private void Control(delegate* unmanaged<int, int> call, int taskId, string name)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (taskId == NoTask)
            throw new ArgumentOutOfRangeException(nameof(taskId), "The service rejects this identifier.");
        SceResult.ThrowIfFailed(call(taskId), name);
    }

    private static byte[] ToNullTerminated(string value)
    {
        int count = Encoding.UTF8.GetByteCount(value);
        byte[] buffer = new byte[count + 1];
        Encoding.UTF8.GetBytes(value, buffer);
        return buffer;
    }

    /// <summary>Stops the service, releases its memory and unloads it.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;

        _term();
        if (_memory is not null)
            NativeMemory.Free(_memory);
        _library.Dispose();
    }
}
