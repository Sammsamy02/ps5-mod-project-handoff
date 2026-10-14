// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Random;

/// <summary>Random-number bindings backed by the system's entropy source.</summary>
public static unsafe partial class SceRandom
{
    private const string Lib = "libSceRandom";

    /// <summary>The most bytes a single call may request.</summary>
    public const int MaxSize = 64;

    /// <summary>
    /// Fills <paramref name="buffer"/> with <paramref name="size"/> random bytes, at most
    /// <see cref="MaxSize"/> per call.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceRandomGetRandomNumber(void* buffer, nuint size);
}
