// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.AvCapture;
using System;

namespace SharpProspero.Platform;

/// <summary>
/// Captures the system-composited video the console produces — the finished screen the whole
/// system draws — for a recorder or a stream. Open it, open a video channel,
/// start it, and read frames; each read hands back a descriptor into the capture buffers.
/// </summary>
/// <remarks>
/// <para>
/// This is an advanced, privileged surface. The capture service runs behind a system message channel,
/// and the process must hold the authority to reach it; a plain application sandbox does not, so opening
/// it there fails with a permission error. Treat every call as best-effort and handle the failure. For
/// capturing only what the application itself drew, encode a drawing surface instead (see the image
/// encoders); for saving a clip of the finished screen to the gallery, use <see cref="ShareCapture"/>,
/// which needs no elevated privilege.
/// </para>
/// <para>
/// The service is not part of the module set a title links against, so it is loaded at run time and its
/// entry points are resolved by name, the same way as the other run-time services.
/// </para>
/// </remarks>
/// <example>
/// <code>
/// using var capture = SystemAvCapture.Open();
/// capture.OpenVideo(Avcap2VideoConfig.Create());
/// capture.Start();
/// while (recording)
/// {
///     if (capture.TryReadVideo(out Avcap2VideoFrameInfo frame) &amp;&amp; frame.IsValid)
///         Encode(frame);          // a privileged consumer reads the frame planes
/// }
/// capture.Stop();
/// </code>
/// </example>
public sealed unsafe class SystemAvCapture : IDisposable
{
    /// <summary>The module that carries the AV-capture service.</summary>
    public const string ModulePath = "/system/common/lib/libSceAvcap2.sprx";

    private readonly SystemLibrary _library;
    private readonly delegate* unmanaged<int> _terminate;
    private readonly delegate* unmanaged<nint*, void*, byte, int> _openVideo;
    private readonly delegate* unmanaged<nint, int> _close;
    private readonly delegate* unmanaged<nint, int> _start;
    private readonly delegate* unmanaged<nint, int> _stop;
    private readonly delegate* unmanaged<nint, nint, nint, void*, uint, int> _readVideo;
    private readonly delegate* unmanaged<nint, int*, int> _getFramePitch;

    private nint _videoHandle;
    private bool _started;
    private bool _disposed;

    private SystemAvCapture(
        SystemLibrary library,
        delegate* unmanaged<int> terminate,
        delegate* unmanaged<nint*, void*, byte, int> openVideo,
        delegate* unmanaged<nint, int> close,
        delegate* unmanaged<nint, int> start,
        delegate* unmanaged<nint, int> stop,
        delegate* unmanaged<nint, nint, nint, void*, uint, int> readVideo,
        delegate* unmanaged<nint, int*, int> getFramePitch)
    {
        _library = library;
        _terminate = terminate;
        _openVideo = openVideo;
        _close = close;
        _start = start;
        _stop = stop;
        _readVideo = readVideo;
        _getFramePitch = getFramePitch;
    }

    /// <summary>Loads the AV-capture service and initializes it.</summary>
    /// <exception cref="ProsperoException">
    /// The module could not be loaded, or the service could not be initialized — including when the
    /// process lacks the privilege to reach it.
    /// </exception>
    public static SystemAvCapture Open()
    {
        SystemLibrary library = SystemLibrary.Open(ModulePath);
        try
        {
            var initialize = (delegate* unmanaged<int>)library.GetFunction("sceAvcap2Initialize");
            var terminate = (delegate* unmanaged<int>)library.GetFunction("sceAvcap2Terminate");
            var openVideo = (delegate* unmanaged<nint*, void*, byte, int>)library.GetFunction("sceAvcap2OpenVideo");
            var close = (delegate* unmanaged<nint, int>)library.GetFunction("sceAvcap2Close");
            var start = (delegate* unmanaged<nint, int>)library.GetFunction("sceAvcap2Start");
            var stop = (delegate* unmanaged<nint, int>)library.GetFunction("sceAvcap2Stop");
            var readVideo = (delegate* unmanaged<nint, nint, nint, void*, uint, int>)library.GetFunction("sceAvcap2ReadVideo");
            var getFramePitch = (delegate* unmanaged<nint, int*, int>)library.GetFunction("sceAvcap2GetFramePitch");

            SceResult.ThrowIfFailed(initialize(), "sceAvcap2Initialize");
            return new SystemAvCapture(library, terminate, openVideo, close, start, stop, readVideo, getFramePitch);
        }
        catch
        {
            library.Dispose();
            throw;
        }
    }

    /// <summary>Whether a video channel is open.</summary>
    public bool IsVideoOpen => _videoHandle != 0;

    /// <summary>Whether capture is running.</summary>
    public bool IsStarted => _started;

    /// <summary>
    /// Opens a video-capture channel with <paramref name="config"/>. Only one channel is open at a time;
    /// close the current one first. <paramref name="options"/> is an advanced open flag, zero for the
    /// default.
    /// </summary>
    /// <exception cref="ProsperoException">The channel could not be opened.</exception>
    public void OpenVideo(Avcap2VideoConfig config, byte options = 0)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (_videoHandle != 0)
            throw new InvalidOperationException("A video channel is already open.");

        nint handle = 0;
        SceResult.ThrowIfFailed(_openVideo(&handle, &config, options), "sceAvcap2OpenVideo");
        _videoHandle = handle;
    }

    /// <summary>Starts capture on the open video channel.</summary>
    /// <exception cref="ProsperoException">Capture could not start.</exception>
    public void Start()
    {
        RequireVideo();
        SceResult.ThrowIfFailed(_start(_videoHandle), "sceAvcap2Start");
        _started = true;
    }

    /// <summary>Stops capture on the open video channel.</summary>
    /// <exception cref="ProsperoException">Capture could not stop.</exception>
    public void Stop()
    {
        RequireVideo();
        SceResult.ThrowIfFailed(_stop(_videoHandle), "sceAvcap2Stop");
        _started = false;
    }

    /// <summary>
    /// Reads the next captured video frame into <paramref name="frame"/>. Returns false when no valid
    /// frame is available this call (a non-negative service result that carries no delivered frame);
    /// throws when the read itself fails.
    /// </summary>
    /// <exception cref="ProsperoException">The read failed.</exception>
    public bool TryReadVideo(out Avcap2VideoFrameInfo frame)
    {
        RequireVideo();
        frame = default;
        fixed (Avcap2VideoFrameInfo* p = &frame)
        {
            int rc = _readVideo(_videoHandle, 0, 0, p, 0);
            SceResult.ThrowIfFailed(rc, "sceAvcap2ReadVideo");
        }
        return frame.IsValid;
    }

    /// <summary>Reads the row pitch, in bytes, of the captured video frames.</summary>
    /// <exception cref="ProsperoException">The pitch could not be read.</exception>
    public int GetFramePitch()
    {
        RequireVideo();
        int pitch = 0;
        SceResult.ThrowIfFailed(_getFramePitch(_videoHandle, &pitch), "sceAvcap2GetFramePitch");
        return pitch;
    }

    /// <summary>Closes the open video channel, if any. Stops capture first when it is running.</summary>
    public void CloseVideo()
    {
        if (_videoHandle == 0)
            return;
        if (_started)
        {
            _stop(_videoHandle);
            _started = false;
        }
        _close(_videoHandle);
        _videoHandle = 0;
    }

    /// <summary>Closes any open channel, terminates the service, and unloads the module.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        CloseVideo();
        _terminate();
        _library.Dispose();
    }

    private void RequireVideo()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (_videoHandle == 0)
            throw new InvalidOperationException("No video channel is open; call OpenVideo first.");
    }
}
