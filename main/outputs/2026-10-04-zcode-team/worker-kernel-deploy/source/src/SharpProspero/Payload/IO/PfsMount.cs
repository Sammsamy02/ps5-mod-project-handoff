// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.IO;

/// <summary>
/// Options for <see cref="PayloadPfsMount.sceFsMountSaveData"/>.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct MountSaveDataOpt
{
    /// <summary>Reserved byte; leave zero.</summary>
    public byte Reserved;

    private fixed byte _pad[7];

    /// <summary>Budget identifier string (e.g. "system\0"). Must remain pinned for the
    /// duration of the mount call.</summary>
    public byte* BudgetId;
}

/// <summary>
/// Options for <see cref="PayloadPfsMount.sceFsUmountSaveData"/>.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct UmountSaveDataOpt
{
    /// <summary>Reserved byte; leave zero.</summary>
    public byte Dummy;
}

/// <summary>
/// PFS image mounting for a payload context. Wraps the four <c>sceFs*SaveData</c> functions
/// from <c>libSceFsInternalForVsh</c> for mounting and unmounting PFS volumes.
/// </summary>
/// <remarks>
/// Requires <c>libSceFsInternalForVsh</c> in the payload's DT_NEEDED list and elevated
/// credentials (the calling process must be unjailed with root uid).
/// </remarks>
public static unsafe partial class PayloadPfsMount
{
    private const string Lib = "libSceFsInternalForVsh";

    /// <summary>
    /// Initialises a <see cref="MountSaveDataOpt"/> to its default state.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceFsInitMountSaveDataOpt(MountSaveDataOpt* opt);

    /// <summary>
    /// Mounts a PFS volume at <paramref name="mountPath"/>.
    /// </summary>
    /// <param name="opt">Mount options (initialised via <see cref="sceFsInitMountSaveDataOpt"/>).</param>
    /// <param name="volumePath">A NUL-terminated UTF-8 path to the PFS image file.</param>
    /// <param name="mountPath">A NUL-terminated UTF-8 mount point path.</param>
    /// <param name="key">A 32-byte encryption key, or a zeroed buffer for unencrypted volumes.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceFsMountSaveData(MountSaveDataOpt* opt, byte* volumePath,
        byte* mountPath, byte* key);

    /// <summary>
    /// Initialises an <see cref="UmountSaveDataOpt"/> to its default state.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceFsInitUmountSaveDataOpt(UmountSaveDataOpt* opt);

    /// <summary>
    /// Unmounts a PFS volume at <paramref name="mountPath"/>.
    /// </summary>
    /// <param name="opt">Unmount options (initialised via <see cref="sceFsInitUmountSaveDataOpt"/>).</param>
    /// <param name="mountPath">A NUL-terminated UTF-8 mount point path.</param>
    /// <param name="handle">A mount handle, or -1 for the default.</param>
    /// <param name="ignoreErrors">Non-zero to ignore unmount errors.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceFsUmountSaveData(UmountSaveDataOpt* opt, byte* mountPath,
        int handle, int ignoreErrors);
}
