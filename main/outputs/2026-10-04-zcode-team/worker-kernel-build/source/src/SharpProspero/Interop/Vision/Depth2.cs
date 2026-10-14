// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Vision;

/// <summary>The pixel form of the source images the depth library reads.</summary>
public enum SceDepth2PixelFormat : int
{
    /// <summary>8-bit luminance.</summary>
    Y8 = 0,
    /// <summary>Packed 4:2:2 luminance and chrominance.</summary>
    Yuv422 = 1,
}

/// <summary>How the depth library treats the source image.</summary>
public enum SceDepth2ExecutionMode : int
{
    /// <summary>Work on the caller's image in place rather than copying it.</summary>
    DoNotCopySourceImage = 1,
}

/// <summary>The depth-generation quality and cost profile.</summary>
public enum SceDepth2Profile : int
{
    /// <summary>Profile 1.5.</summary>
    Profile15 = 2,
    /// <summary>Profile 1.6.</summary>
    Profile16 = 4,
    /// <summary>Profile 2.0.</summary>
    Profile20 = 3,
}

/// <summary>The stereo camera the depth is generated from.</summary>
public enum SceDepth2StereoCameraType : int
{
    /// <summary>The HD stereo camera.</summary>
    HdCamera = 1,
}

/// <summary>Whether the library folds camera metadata into the depth generation.</summary>
public enum SceDepth2UseCameraInformation : int
{
    /// <summary>Do not use the camera metadata.</summary>
    Disable = 0,
    /// <summary>Use the camera metadata to improve the result.</summary>
    Enable = 1,
}

/// <summary>Which produced image <c>sceDepth2GetImage</c> returns.</summary>
public enum SceDepth2ImageType : int
{
    /// <summary>The 16-bit depth map. <c>0xFFFF</c> marks an invalid depth value.</summary>
    Depth16Bit = 0,
}

/// <summary>The size and alignment of the working memory the library needs.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceDepth2QueryMemoryResult
{
    /// <summary>Required working-memory size in bytes.</summary>
    public nuint MemorySize;
    /// <summary>Required working-memory alignment in bytes.</summary>
    public nuint MemoryAlignment;
}

/// <summary>The working memory the caller hands the library.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceDepth2MemoryInformation
{
    /// <summary>The working-memory block.</summary>
    public void* MemoryChunk;
    /// <summary>The block size in bytes.</summary>
    public nuint ChunkSize;
}

/// <summary>The source image's extent and form.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceDepth2InputImageInformation
{
    /// <summary>Source width in pixels.</summary>
    public int Width;
    /// <summary>Source height in pixels.</summary>
    public int Height;
    /// <summary>The source pixel form.</summary>
    public SceDepth2PixelFormat PixelFormat;
}

/// <summary>How the depth is processed: the region, mode, profile, and camera.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceDepth2ProcessingInformation
{
    /// <summary>Processing region width in pixels.</summary>
    public int Width;
    /// <summary>Processing region height in pixels.</summary>
    public int Height;
    /// <summary>The execution mode (see <see cref="SceDepth2ExecutionMode"/>).</summary>
    public int ExecutionMode;
    /// <summary>The depth profile.</summary>
    public SceDepth2Profile DepthProfile;
    private int _reserved0;
    private int _reserved1;
    /// <summary>The stereo camera type.</summary>
    public SceDepth2StereoCameraType CameraType;
}

/// <summary>Which GPU pipe and queue the library runs on.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceDepth2PlatformInformation
{
    /// <summary>The GPU pipe.</summary>
    public int Pipe;
    /// <summary>The GPU queue.</summary>
    public int Queue;
    /// <summary>Reserved context pointer.</summary>
    public void* Context;
}

/// <summary>The full parameter set for <c>sceDepth2Initialize</c> and <c>sceDepth2QueryMemory</c>.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceDepth2InitializeParameter
{
    /// <summary>Set to the size of this structure in bytes.</summary>
    public int SizeofInitializeParameter;
    /// <summary>The source image description.</summary>
    public SceDepth2InputImageInformation InputImageInformation;
    /// <summary>The processing description.</summary>
    public SceDepth2ProcessingInformation ProcessingInformation;
    /// <summary>The pipe and queue to run on.</summary>
    public SceDepth2PlatformInformation PlatformInformation;
}

/// <summary>
/// The stereo-camera depth-generation library. Query the working memory a configuration needs, initialize
/// the library into a caller-provided block to get a handle, then per frame set the source images with a
/// command, submit, wait, and read back the 16-bit depth map. This is the flat interface exactly as the
/// vision header declares it. The per-frame command parameter embeds camera metadata (from the camera
/// library), so it is passed as a pointer the caller builds.
/// </summary>
public static unsafe partial class Depth2
{
    private const string Lib = "libSceDepth2";

    /// <summary>The depth value that marks an invalid (unknown) pixel.</summary>
    public const ushort InvalidDepthValue = 0xffff;

    /// <summary>Reports the working memory the given configuration needs.</summary>
    [LibraryImport(Lib)]
    public static partial int sceDepth2QueryMemory(SceDepth2InitializeParameter* initializeParameter, SceDepth2QueryMemoryResult* memoryQueryResult);

    /// <summary>Initializes the library into the caller's working memory. Returns a non-negative handle, or a negative error.</summary>
    [LibraryImport(Lib)]
    public static partial int sceDepth2Initialize(SceDepth2InitializeParameter* initializeParameter, SceDepth2MemoryInformation* memoryInformation);

    /// <summary>Tears the library down.</summary>
    [LibraryImport(Lib)]
    public static partial int sceDepth2Terminate(int handle);

    /// <summary>Sets the source images and options for the next depth pass. The parameter is a <c>SceDepth2SetCommandParameter</c>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceDepth2SetCommand(int handle, void* parameter);

    /// <summary>Submits the depth pass to the GPU.</summary>
    [LibraryImport(Lib)]
    public static partial int sceDepth2Submit(int handle);

    /// <summary>Waits for the pass to finish and runs its post-processing.</summary>
    [LibraryImport(Lib)]
    public static partial int sceDepth2WaitAndExecutePostProcess(int handle);

    /// <summary>Sets the region of interest, in normalized [0, 1] coordinates.</summary>
    [LibraryImport(Lib)]
    public static partial int sceDepth2SetRoi(int handle, float sx, float sy, float ex, float ey);

    /// <summary>Copies a produced image into the caller's buffer.</summary>
    [LibraryImport(Lib)]
    public static partial int sceDepth2GetImage(int handle, SceDepth2ImageType imageType, void* destinationBuffer, int destinationBufferSize);

}

/// <summary>The pixel form the sensor delivers at the full-size level.</summary>
public enum SceCamera2BaseFormat : int
{
    /// <summary>Packed 4:2:2 luminance and chrominance.</summary>
    Yuv422 = 0x00,

    /// <summary>The level is not produced.</summary>
    NoUse = 0x10,

    /// <summary>Not a form the service recognizes.</summary>
    Unknown = 0xFF,
}

/// <summary>The pixel form a scaled-down level is delivered in.</summary>
public enum SceCamera2ScaleFormat : int
{
    /// <summary>Packed 4:2:2 luminance and chrominance.</summary>
    Yuv422 = 0x00,

    /// <summary>16-bit luminance. Scaled levels only.</summary>
    Y16 = 0x03,

    /// <summary>8-bit luminance. Scaled levels only, and what the depth library reads.</summary>
    Y8 = 0x04,

    /// <summary>The level is not produced.</summary>
    NoUse = 0x10,

    /// <summary>Not a form the service recognizes.</summary>
    Unknown = 0xFF,
}

/// <summary>The extent of the full-size level.</summary>
public enum SceCamera2Resolution : int
{
    /// <summary>1920 by 1080.</summary>
    Res1920x1080 = 0x10,

    /// <summary>960 by 520.</summary>
    Res960x520 = 0x11,

    /// <summary>448 by 256.</summary>
    Res448x256 = 0x12,

    /// <summary>240 by 135. Declared, but the service does not deliver it.</summary>
    Res240x135 = 0x13,

    /// <summary>Not a resolution the service recognizes.</summary>
    Unknown = 0xFF,
}

/// <summary>The part of the sensor a lens geometry query refers to.</summary>
public enum SceCamera2ImageArea : int
{
    /// <summary>The 1920 by 1080 area.</summary>
    Area1920x1080 = 0x10,

    /// <summary>The 960 by 520 area.</summary>
    Area960x520 = 0x11,

    /// <summary>The 448 by 256 area.</summary>
    Area448x256 = 0x12,

    /// <summary>Not an area the service recognizes.</summary>
    Unknown = 0xFF,
}

/// <summary>Frames per second.</summary>
public enum SceCamera2Framerate : int
{
    /// <summary>Not a rate the service recognizes.</summary>
    Unknown = 0,

    /// <summary>7.5 frames per second.</summary>
    Fps7_5 = 7,

    /// <summary>15 frames per second.</summary>
    Fps15 = 15,

    /// <summary>30 frames per second.</summary>
    Fps30 = 30,

    /// <summary>60 frames per second.</summary>
    Fps60 = 60,

    /// <summary>120 frames per second. Declared, but the service does not deliver it.</summary>
    Fps120 = 120,
}

/// <summary>Which of the two sensors a call applies to.</summary>
public enum SceCamera2Channel : int
{
    /// <summary>The left sensor.</summary>
    Channel0 = 1,

    /// <summary>The right sensor.</summary>
    Channel1 = 2,

    /// <summary>Both sensors.</summary>
    Both = 3,
}

/// <summary>Which shape of configuration is being supplied.</summary>
public enum SceCamera2ConfigType : int
{
    /// <summary>A preset pairing of format and resolution.</summary>
    Type21 = 0x21,

    /// <summary>A second preset pairing.</summary>
    Type22 = 0x22,

    /// <summary>The caller states the format, resolution and rate itself.</summary>
    Extention = 0x30,
}

/// <summary>The pixel form of each of the four size levels a sensor produces.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceCamera2Format
{
    /// <summary>The full-size level.</summary>
    public SceCamera2BaseFormat FormatLevel0;

    /// <summary>The quarter-area level.</summary>
    public SceCamera2ScaleFormat FormatLevel1;

    /// <summary>The sixteenth-area level.</summary>
    public SceCamera2ScaleFormat FormatLevel2;

    /// <summary>The sixty-fourth-area level.</summary>
    public SceCamera2ScaleFormat FormatLevel3;
}

/// <summary>One sensor's format, resolution and rate.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceCamera2ConfigExtention
{
    /// <summary>The pixel form of each level.</summary>
    public SceCamera2Format Format;

    /// <summary>The full-size extent.</summary>
    public SceCamera2Resolution Resolution;

    /// <summary>The frame rate.</summary>
    public SceCamera2Framerate Framerate;

    /// <summary>The full-size width in pixels.</summary>
    public uint Width;

    /// <summary>The full-size height in pixels.</summary>
    public uint Height;

    /// <summary>Reserved. Leave zero.</summary>
    public uint Reserved1;

    /// <summary>Reserved. Leave null.</summary>
    public void* BaseOption;
}

/// <summary>Reserved parameters for opening the camera.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceCamera2OpenParameter
{
    /// <summary>Set to the size of this structure in bytes.</summary>
    public uint SizeThis;

    /// <summary>Reserved. Leave zero.</summary>
    public uint Reserved1;

    /// <summary>Reserved. Leave zero.</summary>
    public uint Reserved2;

    /// <summary>Reserved. Leave zero.</summary>
    public uint Reserved3;
}

/// <summary>Both sensors' configuration.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceCamera2Config
{
    /// <summary>Set to the size of this structure in bytes.</summary>
    public uint SizeThis;

    /// <summary>Which shape the two entries below carry.</summary>
    public SceCamera2ConfigType ConfigType;

    /// <summary>The left sensor.</summary>
    public SceCamera2ConfigExtention ConfigLeft;

    /// <summary>The right sensor.</summary>
    public SceCamera2ConfigExtention ConfigRight;
}

/// <summary>Which size levels each sensor delivers once started.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceCamera2StartParameter
{
    /// <summary>Set to the size of this structure in bytes.</summary>
    public uint SizeThis;

    /// <summary>The left sensor's levels, one bit per level starting at bit 0.</summary>
    public uint FormatLevelLeft;

    /// <summary>The right sensor's levels, one bit per level starting at bit 0.</summary>
    public uint FormatLevelRight;

    /// <summary>Reserved. Leave null.</summary>
    public void* StartOption;
}

/// <summary>Whether capture is locked to the display's refresh.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceCamera2VideoSyncParameter
{
    /// <summary>Set to the size of this structure in bytes.</summary>
    public uint SizeThis;

    /// <summary>1 to lock capture to the display, 0 to leave it free-running.</summary>
    public uint VideoSyncMode;

    /// <summary>Reserved. Leave null.</summary>
    public void* ModeOption;
}

/// <summary>One level's extent.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceCamera2FrameLength
{
    /// <summary>Width in pixels.</summary>
    public uint Width;

    /// <summary>Height in pixels.</summary>
    public uint Height;
}

/// <summary>How long the sensor integrates and how much it amplifies.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceCamera2ExposureGain
{
    /// <summary>Whether the fields below are applied.</summary>
    public uint ExposureControl;

    /// <summary>Integration time.</summary>
    public uint Exposure;

    /// <summary>Amplification.</summary>
    public uint Gain;

    /// <summary>Which of the two gain curves is used.</summary>
    public uint Mode;
}

/// <summary>The per-channel gains that set the white point.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceCamera2WhiteBalance
{
    /// <summary>Whether the gains below are applied.</summary>
    public uint WhiteBalanceControl;

    /// <summary>Red gain.</summary>
    public uint GainRed;

    /// <summary>Blue gain.</summary>
    public uint GainBlue;

    /// <summary>Green gain.</summary>
    public uint GainGreen;
}

/// <summary>The transfer curve applied to the sensor output.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceCamera2Gamma
{
    /// <summary>Whether the value below is applied.</summary>
    public uint GammaControl;

    /// <summary>The curve value.</summary>
    public uint Value;
}

/// <summary>
/// What the sensors reported alongside a frame. The paired arrays are indexed by device, left first.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceCamera2Meta
{
    /// <summary>Which metadata the service filled in.</summary>
    public uint MetaMode;

    /// <summary>The pixel form of each level, indexed as device * 4 + level.</summary>
    public fixed uint Format[8];

    /// <summary>The frame counter of each device.</summary>
    public fixed ulong Frame[2];

    /// <summary>The capture time of each device, in microseconds.</summary>
    public fixed ulong Timestamp[2];

    /// <summary>The sensor's own capture time for each device.</summary>
    public fixed uint DeviceTimestamp[2];

    /// <summary>The left sensor's exposure and gain.</summary>
    public SceCamera2ExposureGain ExposureGainLeft;

    /// <summary>The right sensor's exposure and gain.</summary>
    public SceCamera2ExposureGain ExposureGainRight;

    /// <summary>The left sensor's white balance.</summary>
    public SceCamera2WhiteBalance WhiteBalanceLeft;

    /// <summary>The right sensor's white balance.</summary>
    public SceCamera2WhiteBalance WhiteBalanceRight;

    /// <summary>The left sensor's transfer curve.</summary>
    public SceCamera2Gamma GammaLeft;

    /// <summary>The right sensor's transfer curve.</summary>
    public SceCamera2Gamma GammaRight;

    /// <summary>The measured brightness of each device's frame.</summary>
    public fixed uint Luminance[2];

    /// <summary>Camera acceleration, x.</summary>
    public float AccelerationX;

    /// <summary>Camera acceleration, y.</summary>
    public float AccelerationY;

    /// <summary>Camera acceleration, z.</summary>
    public float AccelerationZ;

    /// <summary>The display counter the frame was captured against.</summary>
    public ulong VCounter;

    private fixed uint _reserved[14];
}

/// <summary>
/// A frame request and its result. The caller fills the size, the read mode and the extent it wants each
/// level at; the service fills the pointers, the sizes, the per-device status and the metadata. The
/// pixels stay owned by the service and are valid until the next request.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceCamera2FrameData
{
    /// <summary>Set to the size of this structure in bytes.</summary>
    public uint SizeThis;

    /// <summary>Where the pixels come from and whether the call waits for the next frame.</summary>
    public uint ReadMode;

    /// <summary>The pixels of each level, indexed as device * 4 + level.</summary>
    public fixed ulong FramePointerList[8];

    /// <summary>The byte count of each level, indexed as device * 4 + level.</summary>
    public fixed uint FrameSize[8];

    /// <summary>Width then height of each level, indexed as (device * 4 + level) * 2.</summary>
    public fixed uint FrameLength[16];

    /// <summary>Each device's frame status.</summary>
    public fixed int Status[2];

    /// <summary>What the sensors reported alongside the frame.</summary>
    public SceCamera2Meta Meta;

    /// <summary>Whether the camera is attached.</summary>
    public int Connected;
}

/// <summary>
/// The image controls, read or written in one call. A field set to
/// <see cref="Camera2.AttributeIgnore"/> is left alone.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceCamera2Attribute
{
    /// <summary>Set to the size of this structure in bytes.</summary>
    public uint SizeThis;

    /// <summary>Which sensor the call applies to.</summary>
    public SceCamera2Channel Channel;

    /// <summary>Exposure and gain.</summary>
    public SceCamera2ExposureGain ExposureGain;

    /// <summary>White balance.</summary>
    public SceCamera2WhiteBalance WhiteBalance;

    /// <summary>Transfer curve.</summary>
    public SceCamera2Gamma Gamma;

    /// <summary>Colour saturation.</summary>
    public uint Saturation;

    /// <summary>Contrast.</summary>
    public uint Contrast;

    /// <summary>Edge enhancement.</summary>
    public uint Sharpness;

    /// <summary>Hue rotation.</summary>
    public int Hue;

    /// <summary>Reserved. Leave zero.</summary>
    public uint Reserved1;

    /// <summary>Reserved. Leave zero.</summary>
    public uint Reserved2;

    /// <summary>Reserved. Leave zero.</summary>
    public uint Reserved3;

    /// <summary>Reserved. Leave zero.</summary>
    public uint Reserved4;
}

/// <summary>The lens geometry, which is what turns a pixel into a direction.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceCamera2FieldOfView
{
    /// <summary>Set to the size of this structure in bytes.</summary>
    public uint SizeThis;

    /// <summary>The distance between the two sensors, in millimetres.</summary>
    public float Baseline;

    /// <summary>Diagonal field of view, in degrees.</summary>
    public double DiagonalFov;

    /// <summary>Horizontal field of view, in degrees.</summary>
    public double HorizontalFov;

    /// <summary>Vertical field of view, in degrees.</summary>
    public double VerticalFov;
}

/// <summary>
/// The stereo camera capture bindings, which produce the colour frames the depth library consumes.
/// Initialize the service, open the camera, describe the levels wanted, start, then request a frame per
/// pass and stop when done. The pixels a frame request returns belong to the service, so anything kept
/// beyond the next request is copied out.
/// </summary>
public static unsafe partial class Camera2
{
    private const string Lib = "libSceCamera";

    /// <summary>
    /// The only user id the camera accepts. It is not the logged-in user: the service rejects every other
    /// value, and rejects a non-zero type or index with it.
    /// </summary>
    public const int SystemUserId = 0xFF;

    /// <summary>Deliver the full-size level.</summary>
    public const uint FrameFormatLevel0 = 0x01;

    /// <summary>Deliver the quarter-area level.</summary>
    public const uint FrameFormatLevel1 = 0x02;

    /// <summary>Deliver the sixteenth-area level.</summary>
    public const uint FrameFormatLevel2 = 0x04;

    /// <summary>Deliver the sixty-fourth-area level.</summary>
    public const uint FrameFormatLevel3 = 0x08;

    /// <summary>Wait for the next frame rather than returning the one already captured.</summary>
    public const uint FrameWaitNextFrameOn = 0x0000;

    /// <summary>Return the frame already captured, even if it was read before.</summary>
    public const uint FrameWaitNextFrameOff = 0x0010;

    /// <summary>The frame is live.</summary>
    public const int StatusIsActive = 1;

    /// <summary>The camera is not delivering frames.</summary>
    public const int StatusIsNotActive = 0;

    /// <summary>The same frame was already handed out.</summary>
    public const int StatusIsAlreadyRead = 2;

    /// <summary>The sensor is still settling; the pixels are not yet worth using.</summary>
    public const int StatusIsNotStable = 3;

    /// <summary>Leave an attribute field as it is.</summary>
    public const uint AttributeIgnore = 0xFFFFFFFF;

    /// <summary>Starts the camera service. Call once before opening.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2Initialize();

    /// <summary>Stops the camera service.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2Finalize();

    /// <summary>
    /// Opens the camera, returning a handle. <paramref name="userId"/> must be
    /// <see cref="SystemUserId"/>, and <paramref name="type"/> and <paramref name="index"/> must both be
    /// zero; anything else is refused.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2Open(int userId, int type, int index, SceCamera2OpenParameter* param);

    /// <summary>Closes a camera handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2Close(int handle);

    /// <summary>Sets both sensors' format, resolution and rate. Do this before starting.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2SetConfig(int handle, SceCamera2Config* config);

    /// <summary>Reads back both sensors' configuration.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2GetConfig(int handle, SceCamera2Config* config);

    /// <summary>Begins capture, delivering the levels named in <paramref name="param"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2Start(int handle, SceCamera2StartParameter* param);

    /// <summary>Ends capture.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2Stop(int handle);

    /// <summary>Requests a frame. The pixels are borrowed, not owned.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2GetFrameData(int handle, SceCamera2FrameData* frameData);

    /// <summary>Reports whether a frame already fetched is still the current one.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2IsValidFrameData(int handle, SceCamera2FrameData* frameData);

    /// <summary>Reports whether a camera is physically attached.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2IsAttached(int index);

    /// <summary>Reads the image controls.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2GetAttribute(int handle, SceCamera2Attribute* attribute);

    /// <summary>Writes the image controls.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2SetAttribute(int handle, SceCamera2Attribute* attribute);

    /// <summary>
    /// Turns automatic exposure and gain on or off. It has to be off before a manual exposure or gain is
    /// accepted.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2SetAutoExposureGain(int handle, SceCamera2Channel channel, uint enable, void* option);

    /// <summary>Reads whether automatic exposure and gain is on.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2GetAutoExposureGain(int handle, SceCamera2Channel channel, uint* enable, void* option);

    /// <summary>Turns automatic white balance on or off.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2SetAutoWhiteBalance(int handle, SceCamera2Channel channel, uint enable, void* option);

    /// <summary>Reads whether automatic white balance is on.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2GetAutoWhiteBalance(int handle, SceCamera2Channel channel, uint* enable, void* option);

    /// <summary>Sets exposure and gain directly.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2SetExposureGain(int handle, SceCamera2Channel channel, SceCamera2ExposureGain* exposureGain, void* option);

    /// <summary>Reads exposure and gain.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2GetExposureGain(int handle, SceCamera2Channel channel, SceCamera2ExposureGain* exposureGain, void* option);

    /// <summary>Sets the white point directly.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2SetWhiteBalance(int handle, SceCamera2Channel channel, SceCamera2WhiteBalance* whiteBalance, void* option);

    /// <summary>Reads the white point.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2GetWhiteBalance(int handle, SceCamera2Channel channel, SceCamera2WhiteBalance* whiteBalance, void* option);

    /// <summary>Locks capture to the display's refresh, or lets it run free.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2SetVideoSync(int handle, SceCamera2VideoSyncParameter* videoSync);

    /// <summary>Reads the lens geometry for one image area.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCamera2GetFieldOfView(int handle, SceCamera2ImageArea imageArea, SceCamera2FieldOfView* fieldOfView);
}
