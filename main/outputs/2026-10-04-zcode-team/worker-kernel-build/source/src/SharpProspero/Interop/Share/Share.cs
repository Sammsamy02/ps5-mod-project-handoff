// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Share;

/// <summary>The share features a request or a permission applies to. Combine as flags.</summary>
[System.Flags]
public enum ShareFeature : uint
{
    /// <summary>No feature.</summary>
    None = 0x00000000,

    /// <summary>Video recording (the recent-gameplay clip capture).</summary>
    VideoRecording = 0x00000001,

    /// <summary>Screenshots.</summary>
    Screenshot = 0x00000002,

    /// <summary>Broadcasting.</summary>
    Broadcast = 0x00000004,

    /// <summary>Remote play.</summary>
    RemotePlay = 0x00000008,

    /// <summary>Share play.</summary>
    SharePlay = 0x00000010,

    /// <summary>Screen sharing.</summary>
    ScreenShare = 0x00000020,

    /// <summary>Every feature.</summary>
    All = 0xFFFFFFFF,
}

/// <summary>The size and format of a captured screenshot.</summary>
public enum ScreenshotFormat
{
    /// <summary>The system picks the size and format.</summary>
    Auto = 0,

    /// <summary>2K JPEG.</summary>
    Jpeg2K = 1,

    /// <summary>4K JPEG.</summary>
    Jpeg4K = 2,

    /// <summary>2K PNG.</summary>
    Png2K = 11,

    /// <summary>4K PNG.</summary>
    Png4K = 12,
}

/// <summary>Which composited layer a screenshot is taken from.</summary>
public enum ScreenshotTapPoint
{
    /// <summary>The system picks the source.</summary>
    Auto = 0,

    /// <summary>The full television output, including system overlays.</summary>
    Tv = 1,

    /// <summary>An extended source.</summary>
    Extended = 2,

    /// <summary>The application's own output only, without system overlays.</summary>
    MainOnly = 3,
}

/// <summary>Where an overlay image is anchored on a screenshot.</summary>
public enum ScreenshotOrigin
{
    /// <summary>Top-left.</summary>
    LeftTop = 1,

    /// <summary>Middle-left.</summary>
    LeftCenter = 2,

    /// <summary>Bottom-left.</summary>
    LeftBottom = 3,

    /// <summary>Top-center.</summary>
    CenterTop = 4,

    /// <summary>Center.</summary>
    CenterCenter = 5,

    /// <summary>Bottom-center.</summary>
    CenterBottom = 6,

    /// <summary>Top-right.</summary>
    RightTop = 7,

    /// <summary>Middle-right.</summary>
    RightCenter = 8,

    /// <summary>Bottom-right.</summary>
    RightBottom = 9,
}

/// <summary>Whether a video-clip recording is running, paused, or stopped.</summary>
public enum ShareRecordingStatus : uint
{
    /// <summary>Not recording.</summary>
    NotRecording = 0,

    /// <summary>Paused.</summary>
    Paused = 1,

    /// <summary>Recording.</summary>
    Running = 2,
}

/// <summary>Screenshot capture settings.</summary>
[StructLayout(LayoutKind.Sequential, Size = 32)]
public struct SceShareCaptureScreenshotParam
{
    /// <summary><see cref="ScreenshotFormat"/>.</summary>
    public int Format;

    /// <summary><see cref="ScreenshotTapPoint"/>.</summary>
    public int TapPoint;
}

/// <summary>Video-clip capture settings. The relative times are seconds around the present moment.</summary>
[StructLayout(LayoutKind.Sequential, Size = 32)]
public struct SceShareCaptureVideoClipParam
{
    /// <summary>Size of this structure in bytes.</summary>
    public nuint ThisSize;

    /// <summary>The start of the clip, in seconds relative to now (a negative value is in the past).</summary>
    public int StartTime;

    /// <summary>The end of the clip, in seconds relative to now.</summary>
    public int EndTime;

    private ulong _timeReserved;

    /// <summary>The time mode: zero for the relative times above.</summary>
    public uint Mode;
}

/// <summary>The current recording status the system reports.</summary>
[StructLayout(LayoutKind.Sequential, Size = 16)]
public struct SceShareCurrentStatus
{
    /// <summary>The 2K recording status, a <see cref="ShareRecordingStatus"/>.</summary>
    public uint Recording2kStatus;

    /// <summary>The 4K recording status, a <see cref="ShareRecordingStatus"/>.</summary>
    public uint Recording4kStatus;
}

/// <summary>
/// Share bindings: capture a screenshot or a video clip of the system-composited output (the
/// application together with the system overlays), which is saved to the capture gallery. The module
/// must be loaded (<c>SystemModuleId.Share</c>) and the service initialized before capturing. Capture
/// requests are asynchronous: the call returns a request id and the capture completes in the background.
/// </summary>
public static unsafe partial class Share
{
    private const string Lib = "libSceShare";

    /// <summary>The default service heap size.</summary>
    public const int DefaultHeapSize = 128 * 1024;

    /// <summary>The default helper-thread priority.</summary>
    public const int DefaultThreadPriority = 700;

    /// <summary>An invalid request id.</summary>
    public const int RequestIdInvalid = -1;

    /// <summary>Starts the share service with an internal heap and a helper thread.</summary>
    [LibraryImport(Lib)]
    public static partial int sceShareInitialize(nuint heapSize, int threadPriority, ulong affinityMask);

    /// <summary>Stops the share service.</summary>
    [LibraryImport(Lib)]
    public static partial int sceShareTerminate();

    /// <summary>Requests a screenshot; the request id is written to <paramref name="reqId"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceShareCaptureScreenshot(SceShareCaptureScreenshotParam* param, int* reqId);

    /// <summary>Requests a video clip; the request id is written to <paramref name="reqId"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceShareCaptureVideoClip(SceShareCaptureVideoClipParam* param, int* reqId);

    /// <summary>Reads the current status for <paramref name="featureFlag"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceShareGetCurrentStatus(uint featureFlag, SceShareCurrentStatus* status);

    /// <summary>Sets an image overlaid on captured screenshots.</summary>
    [LibraryImport(Lib, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int sceShareSetScreenshotOverlayImage(string filePath, int marginX, int marginY, int origin);

    /// <summary>Allows the given share features.</summary>
    [LibraryImport(Lib)]
    public static partial int sceShareFeaturePermit(uint featureFlags);

    /// <summary>Blocks the given share features, so a sensitive screen is not captured.</summary>
    [LibraryImport(Lib)]
    public static partial int sceShareFeatureProhibit(uint featureFlags);
}
