// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Dialog;

/// <summary>
/// The shared dialog subsystem every common dialog rides on: the browser, the message dialog, the
/// on-screen keyboard, and the rest. It is brought up once for the application before any dialog
/// opens, and each dialog then initializes on top of it.
/// </summary>
public static partial class CommonDialog
{
    private const string Lib = "libSceCommonDialog";

    /// <summary>The subsystem was already up. A second bring-up returns this rather than failing.</summary>
    public const int AlreadyInitialized = unchecked((int)0x80B80002);

    /// <summary>A dialog was used before the subsystem was brought up.</summary>
    public const int NotInitialized = unchecked((int)0x80B80001);

    /// <summary>
    /// Brings the dialog subsystem up for the application. Zero on success; a second call returns
    /// <see cref="AlreadyInitialized"/>, which is not a failure.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceCommonDialogInitialize();

    /// <summary>Reports whether any common dialog is currently in use.</summary>
    [LibraryImport(Lib)]
    [return: MarshalAs(UnmanagedType.U1)]
    public static partial bool sceCommonDialogIsUsed();

    /// <summary>
    /// Brings the subsystem up once, treating an already-up subsystem as success. A dialog can call
    /// this on open without tracking whether some other dialog got there first.
    /// </summary>
    public static void EnsureInitialized()
    {
        int result = sceCommonDialogInitialize();
        if (result < 0 && result != AlreadyInitialized)
            SceResult.ThrowIfFailed(result, nameof(sceCommonDialogInitialize));
    }
}
