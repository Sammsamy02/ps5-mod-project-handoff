// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.PlayGo;

/// <summary>Where a chunk of an application is.</summary>
public enum PlayGoLocus : sbyte
{
    /// <summary>Not downloaded.</summary>
    NotDownloaded = 0,

    /// <summary>Local, on the slower storage.</summary>
    LocalSlow = 2,

    /// <summary>Local, on the faster storage.</summary>
    LocalFast = 3,
}

/// <summary>The parameters PlayGo is initialized with.</summary>
[StructLayout(LayoutKind.Sequential, Size = 16)]
public unsafe struct ScePlayGoInitParams
{
    /// <summary>The work buffer.</summary>
    public void* BufAddr;

    /// <summary>The work buffer size, in bytes.</summary>
    public uint BufSize;

    private uint _reserved;
}

/// <summary>How much of a set of chunks has downloaded.</summary>
[StructLayout(LayoutKind.Sequential, Size = 16)]
public struct ScePlayGoProgress
{
    /// <summary>The bytes downloaded so far.</summary>
    public ulong ProgressSize;

    /// <summary>The total bytes.</summary>
    public ulong TotalSize;
}

/// <summary>Install and download progress bindings.</summary>
public static unsafe partial class PlayGo
{
    private const string Lib = "libScePlayGo";

    /// <summary>The work buffer size PlayGo requires.</summary>
    public const int HeapSize = 2 * 1024 * 1024;

    /// <summary>Starts PlayGo with a work buffer.</summary>
    [LibraryImport(Lib)]
    public static partial int scePlayGoInitialize(ScePlayGoInitParams* initParam);

    /// <summary>Stops PlayGo.</summary>
    [LibraryImport(Lib)]
    public static partial int scePlayGoTerminate();

    /// <summary>Opens a handle for the running application.</summary>
    [LibraryImport(Lib)]
    public static partial int scePlayGoOpen(int* outHandle, void* param);

    /// <summary>Closes a handle.</summary>
    [LibraryImport(Lib)]
    public static partial int scePlayGoClose(int handle);

    /// <summary>Reads where each of <paramref name="chunkIds"/> is into <paramref name="outLoci"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int scePlayGoGetLocus(int handle, ushort* chunkIds, uint numberOfEntries, sbyte* outLoci);

    /// <summary>Reads the download progress of <paramref name="chunkIds"/> into <paramref name="outProgress"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int scePlayGoGetProgress(int handle, ushort* chunkIds, uint numberOfEntries, ScePlayGoProgress* outProgress);
}
