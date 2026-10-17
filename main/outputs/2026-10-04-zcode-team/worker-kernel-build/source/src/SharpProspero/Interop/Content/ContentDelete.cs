// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Content;

/// <summary>The parameters the content-delete service is initialized with.</summary>
[StructLayout(LayoutKind.Sequential, Size = 48)]
public unsafe struct SceContentDeleteInitParam
{
    private fixed byte _reserved1[4];

    /// <summary>The size of the heap the service works with, in bytes.</summary>
    public nuint HeapSize;

    private fixed byte _reserved2[32];
}

/// <summary>Content-delete bindings: remove a capture or a piece of content by path or id.</summary>
public static unsafe partial class ContentDelete
{
    private const string Lib = "libSceContentDelete";

    /// <summary>A permission the process does not hold is required to delete this content.</summary>
    public const int NoPermission = unchecked((int)0x809D5006);

    /// <summary>Starts the content-delete service.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentDeleteInitialize(SceContentDeleteInitParam* initParam);

    /// <summary>Stops the content-delete service.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentDeleteTerminate();

    /// <summary>Deletes the content with <paramref name="contentId"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentDeleteById(long contentId);

    /// <summary>Deletes the content at <paramref name="contentPath"/> (UTF-8).</summary>
    [LibraryImport(Lib)]
    public static partial int sceContentDeleteByPath(byte* contentPath);
}
