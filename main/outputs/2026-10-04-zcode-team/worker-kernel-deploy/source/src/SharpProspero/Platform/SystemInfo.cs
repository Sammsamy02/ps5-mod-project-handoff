// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System.Text;

namespace SharpProspero.Platform;

/// <summary>
/// Facts about the system the application runs on. A system-information or diagnostics utility reads
/// these to show the user what the console is.
/// </summary>
public static unsafe class SystemInfo
{
    /// <summary>
    /// The system software version, as the console displays it (for example "11.020.000"). A system
    /// utility shows this as the firmware version.
    /// </summary>
    /// <exception cref="ProsperoException">The version could not be read.</exception>
    public static string SystemSoftwareVersion
    {
        get
        {
            SceKernelSwVersion version = ReadVersion();
            int length = 0;
            while (length < 28 && version.VersionString[length] != 0)
                length++;
            return Encoding.ASCII.GetString(version.VersionString, length);
        }
    }

    /// <summary>
    /// The system software version packed into a word, major byte then minor byte, as it reads: 11.20
    /// is <c>0x1120</c> in the high half. This is the form a package's requirement is compared in.
    /// </summary>
    /// <exception cref="ProsperoException">The version could not be read.</exception>
    public static uint SystemSoftwareVersionValue => ReadVersion().Version;

    /// <summary>
    /// The console's open identifier, as a 32-character hex string. This is a stable per-console value
    /// a diagnostics tool shows.
    /// </summary>
    /// <exception cref="ProsperoException">The identifier could not be read.</exception>
    public static string ConsoleId
    {
        get
        {
            byte* id = stackalloc byte[16];
            SceResult.ThrowIfFailed(KernelSystem.sceKernelGetOpenPsId(id), nameof(KernelSystem.sceKernelGetOpenPsId));
            var sb = new StringBuilder(32);
            for (int i = 0; i < 16; i++)
                sb.Append(id[i].ToString("x2"));
            return sb.ToString();
        }
    }

    /// <summary>
    /// The number of processor cores available to the application, read through <c>hw.ncpu</c>.
    /// </summary>
    /// <exception cref="ProsperoException">The value could not be read.</exception>
    public static int ProcessorCount
    {
        get
        {
            int value = 0;
            nuint size = sizeof(int);
            SceResult.ThrowIfFailed(
                KernelSystem.sysctlbyname("hw.ncpu", &value, &size, null, 0),
                nameof(KernelSystem.sysctlbyname));
            return value;
        }
    }

    private static SceKernelSwVersion ReadVersion()
    {
        SceKernelSwVersion version = default;
        version.Size = (ulong)sizeof(SceKernelSwVersion);
        SceResult.ThrowIfFailed(
            KernelSystem.sceKernelGetProsperoSystemSwVersion(&version),
            nameof(KernelSystem.sceKernelGetProsperoSystemSwVersion));
        return version;
    }
}
