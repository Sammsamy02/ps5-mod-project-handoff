// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Audio;

/// <summary>Which compressed form the decoder reads.</summary>
public enum AudiodecCodecType : uint
{
    /// <summary>The device's own compressed audio form.</summary>
    At9 = 0x0001,

    /// <summary>MPEG-1/2 Audio Layer III.</summary>
    Mp3 = 0x0002,

    /// <summary>MPEG-4 Advanced Audio Coding.</summary>
    M4Aac = 0x0003,
}

/// <summary>The sample form the decoder writes. There are two; a third was offered and never existed.</summary>
public enum AudiodecWordSize
{
    /// <summary>Signed 16-bit samples; the form the audio port takes.</summary>
    Signed16 = 1,

    /// <summary>32-bit floating-point samples.</summary>
    Float = 2,
}

/// <summary>Where the compressed input is and how much of it there is.</summary>
/// <remarks>
/// <c>Size</c> is the size of this structure. <c>Length</c> is set to how many bytes are available
/// before a decode and holds how many were consumed after it, so a stream is walked by adding it to
/// the read position.
/// </remarks>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceAudiodecAuInfo
{
    /// <summary>The size of this structure in bytes.</summary>
    public uint Size;

    /// <summary>The compressed bytes to read.</summary>
    public void* Address;

    /// <summary>Bytes available going in; bytes consumed coming out.</summary>
    public uint Length;
}

/// <summary>Where the decoded samples go and how much room there is.</summary>
/// <remarks>
/// <c>Length</c> is set to the room available before a decode and holds how many bytes were written
/// after it.
/// </remarks>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceAudiodecPcmItem
{
    /// <summary>The size of this structure in bytes.</summary>
    public uint Size;

    /// <summary>Where to write the decoded samples.</summary>
    public void* Address;

    /// <summary>Room available going in; bytes written coming out.</summary>
    public uint Length;
}

/// <summary>The four pieces a decode call works from.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceAudiodecCtrl
{
    /// <summary>The codec's own settings, such as <see cref="SceAudiodecParamMp3"/>.</summary>
    public void* Param;

    /// <summary>Where the codec reports what it found in the stream.</summary>
    public void* StreamInfo;

    /// <summary>The compressed input.</summary>
    public SceAudiodecAuInfo* AuInfo;

    /// <summary>The decoded output.</summary>
    public SceAudiodecPcmItem* PcmItem;
}

/// <summary>Settings for the MPEG Audio Layer III decoder.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceAudiodecParamMp3
{
    /// <summary>The size of this structure in bytes.</summary>
    public uint Size;

    /// <summary>The sample form to write, from <see cref="AudiodecWordSize"/>.</summary>
    public int WordSize;
}

/// <summary>What the Layer III decoder found in the stream.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceAudiodecMp3Info
{
    /// <summary>The size of this structure in bytes.</summary>
    public uint Size;

    /// <summary>The frame header word.</summary>
    public uint Header;

    /// <summary>Whether the frame carries a checksum.</summary>
    public byte Crc;

    /// <summary>The channel mode.</summary>
    public byte Mode;

    /// <summary>Extra detail for the joint-stereo mode.</summary>
    public byte ModeExtension;

    /// <summary>The copyright flag.</summary>
    public byte Copyright;

    /// <summary>The original-media flag.</summary>
    public byte Original;

    /// <summary>The emphasis setting.</summary>
    public byte Emphasis;

    private fixed byte _reserved[2];

    /// <summary>Zero when the frame was read, or a non-zero code when its header was rejected.</summary>
    public int Result;
}

/// <summary>Settings for the Advanced Audio Coding decoder.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceAudiodecParamM4aac
{
    /// <summary>The size of this structure in bytes.</summary>
    public uint Size;

    /// <summary>The sample form to write, from <see cref="AudiodecWordSize"/>.</summary>
    public int WordSize;

    /// <summary>1 when each frame carries its own header, 2 when the stream is raw blocks.</summary>
    public uint ConfigNumber;

    /// <summary>The sample-rate index used when the stream is raw blocks.</summary>
    public uint SamplingFrequencyIndex;

    /// <summary>The most channels to decode.</summary>
    public uint MaxChannels;

    /// <summary>Non-zero to decode the high-efficiency form.</summary>
    public uint EnableHeAac;
}

/// <summary>What the Advanced Audio Coding decoder found in the stream.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceAudiodecM4aacInfo
{
    /// <summary>The size of this structure in bytes.</summary>
    public uint Size;

    /// <summary>Samples per second.</summary>
    public uint SamplingFrequency;

    /// <summary>How many channels the stream carries.</summary>
    public uint ChannelCount;

    /// <summary>Non-zero when the high-efficiency form was found.</summary>
    public uint HeAac;

    /// <summary>Zero when the frame was read, or a non-zero code when it was rejected.</summary>
    public int Result;
}

/// <summary>
/// Compressed audio decoding. A library is started once per form, a decoder is created from the
/// settings for that form, and each call decodes from a compressed stream into samples. Frame
/// boundaries are found by the decoder, so a caller hands it a run of bytes and is told how many were
/// consumed.
/// </summary>
public static unsafe partial class Audiodec
{
    private const string Lib = "libSceAudiodec";

    /// <summary>The most channels the Layer III decoder writes.</summary>
    public const int Mp3MaxChannels = 2;

    /// <summary>The most samples one Layer III frame produces per channel.</summary>
    public const int Mp3MaxFrameSamples = 1152;

    /// <summary>The largest Layer III frame in bytes.</summary>
    public const int Mp3MaxFrameSize = 1441;

    /// <summary>The most channels the Advanced Audio Coding decoder writes.</summary>
    public const int AacMaxChannels = 6;

    /// <summary>The most samples one Advanced Audio Coding frame produces per channel.</summary>
    public const int AacMaxFrameSamples = 2048;

    /// <summary>The largest Advanced Audio Coding frame in bytes.</summary>
    public const int AacMaxFrameSize = 4608;

    /// <summary>Starts the library for one compressed form.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAudiodecInitLibrary(AudiodecCodecType codecType);

    /// <summary>Stops the library for one compressed form.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAudiodecTermLibrary(AudiodecCodecType codecType);

    /// <summary>Creates a decoder from the settings in <paramref name="ctrl"/>.</summary>
    /// <returns>A non-negative handle on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceAudiodecCreateDecoder(SceAudiodecCtrl* ctrl, AudiodecCodecType codecType);

    /// <summary>Destroys a decoder.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAudiodecDeleteDecoder(int handle);

    /// <summary>Decodes from the compressed input into the samples described by <paramref name="ctrl"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAudiodecDecode(int handle, SceAudiodecCtrl* ctrl);

    /// <summary>Drops what the decoder was carrying between frames, for a seek.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAudiodecClearContext(int handle);
}
