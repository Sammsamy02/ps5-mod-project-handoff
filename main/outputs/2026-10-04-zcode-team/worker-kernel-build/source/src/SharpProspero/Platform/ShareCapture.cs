// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using SharpProspero.Interop.Share;
using SharpProspero.Interop.Sysmodule;
using SharpProspero.Modules;
using System;

namespace SharpProspero.Platform;

/// <summary>
/// Captures a screenshot or a video clip of the system-composited output, the application together with
/// the system overlays, saved to the console's capture gallery. This is the capture the share
/// button drives; unlike encoding a drawing surface, it captures the whole finished screen. Start it
/// once, capture as needed, dispose it. Captures are asynchronous: a call returns a request id and the
/// image or clip is written in the background.
/// </summary>
/// <example>
/// <code>
/// using var share = ShareCapture.Start();
/// share.CaptureScreenshot(ScreenshotFormat.Png4K);
/// share.CaptureRecentClip(secondsBack: 30);
/// </code>
/// </example>
public sealed unsafe class ShareCapture : IDisposable
{
    private readonly SystemModule _module;
    private bool _disposed;

    private ShareCapture(SystemModule module) => _module = module;

    /// <summary>
    /// Loads the share module and starts the service. <paramref name="heapSize"/> is the service's
    /// internal heap, <paramref name="threadPriority"/> its helper-thread priority (256 highest to
    /// 767 lowest), and <paramref name="affinityMask"/> the processors its helper thread may run on.
    /// </summary>
    /// <remarks>
    /// The mask has to name at least one processor. An empty one is accepted everywhere it is stored
    /// and refused where the thread is finally created, so starting the service with nothing named
    /// fails with a deadlock error rather than anything naming the mask.
    /// </remarks>
    /// <exception cref="ProsperoException">The module could not be loaded or the service could not start.</exception>
    public static ShareCapture Start(
        int heapSize = Share.DefaultHeapSize,
        int threadPriority = Share.DefaultThreadPriority,
        ulong affinityMask = SceKernelCpumask.All)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(heapSize, Share.DefaultHeapSize);
        ArgumentOutOfRangeException.ThrowIfZero(affinityMask);

        SystemModule module = SystemModule.Load(SystemModuleId.Share);
        try
        {
            SceResult.ThrowIfFailed(
                Share.sceShareInitialize((nuint)heapSize, threadPriority, affinityMask),
                nameof(Share.sceShareInitialize));
            return new ShareCapture(module);
        }
        catch
        {
            module.Dispose();
            throw;
        }
    }

    /// <summary>
    /// Captures a screenshot of the finished screen. Returns the request id; the image is written to the
    /// gallery in the background.
    /// </summary>
    /// <param name="format">The size and file format, or <see cref="ScreenshotFormat.Auto"/>.</param>
    /// <param name="tapPoint">Which composited layer to capture, or <see cref="ScreenshotTapPoint.Auto"/>.</param>
    /// <exception cref="ProsperoException">The capture request failed.</exception>
    public int CaptureScreenshot(ScreenshotFormat format = ScreenshotFormat.Auto, ScreenshotTapPoint tapPoint = ScreenshotTapPoint.Auto)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var param = new SceShareCaptureScreenshotParam { Format = (int)format, TapPoint = (int)tapPoint };
        int reqId = Share.RequestIdInvalid;
        SceResult.ThrowIfFailed(Share.sceShareCaptureScreenshot(&param, &reqId), nameof(Share.sceShareCaptureScreenshot));
        return reqId;
    }

    /// <summary>
    /// Captures a video clip of the last <paramref name="secondsBack"/> seconds of output, the way the
    /// share button saves recent gameplay. Returns the request id; the clip is written in the background.
    /// </summary>
    /// <exception cref="ProsperoException">The capture request failed.</exception>
    public int CaptureRecentClip(int secondsBack)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(secondsBack);
        var param = new SceShareCaptureVideoClipParam
        {
            ThisSize = (nuint)sizeof(SceShareCaptureVideoClipParam),
            StartTime = -secondsBack,
            EndTime = 0,
            Mode = 0,
        };
        int reqId = Share.RequestIdInvalid;
        SceResult.ThrowIfFailed(Share.sceShareCaptureVideoClip(&param, &reqId), nameof(Share.sceShareCaptureVideoClip));
        return reqId;
    }

    /// <summary>The current video-clip recording status, for the 2K and 4K recorders.</summary>
    /// <exception cref="ProsperoException">The status could not be read.</exception>
    public (ShareRecordingStatus TwoK, ShareRecordingStatus FourK) RecordingStatus
    {
        get
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            SceShareCurrentStatus status = default;
            SceResult.ThrowIfFailed(
                Share.sceShareGetCurrentStatus((uint)ShareFeature.VideoRecording, &status),
                nameof(Share.sceShareGetCurrentStatus));
            return ((ShareRecordingStatus)status.Recording2kStatus, (ShareRecordingStatus)status.Recording4kStatus);
        }
    }

    /// <summary>
    /// Sets an image overlaid on captured screenshots, anchored by <paramref name="origin"/> with the
    /// given margins, for a watermark or frame.
    /// </summary>
    /// <exception cref="ProsperoException">The overlay could not be set.</exception>
    public void SetScreenshotOverlay(string filePath, int marginX, int marginY, ScreenshotOrigin origin = ScreenshotOrigin.LeftTop)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentException.ThrowIfNullOrEmpty(filePath);
        SceResult.ThrowIfFailed(
            Share.sceShareSetScreenshotOverlayImage(filePath, marginX, marginY, (int)origin),
            nameof(Share.sceShareSetScreenshotOverlayImage));
    }

    /// <summary>Allows the given share features to run.</summary>
    /// <exception cref="ProsperoException">The call failed.</exception>
    public void Allow(ShareFeature features)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        SceResult.ThrowIfFailed(Share.sceShareFeaturePermit((uint)features), nameof(Share.sceShareFeaturePermit));
    }

    /// <summary>
    /// Blocks the given share features, so a sensitive screen (a password entry, for example) is not
    /// captured or streamed while it is shown.
    /// </summary>
    /// <exception cref="ProsperoException">The call failed.</exception>
    public void Block(ShareFeature features)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        SceResult.ThrowIfFailed(Share.sceShareFeatureProhibit((uint)features), nameof(Share.sceShareFeatureProhibit));
    }

    /// <summary>Stops the service and unloads the module.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        Share.sceShareTerminate();
        _module.Dispose();
    }
}
