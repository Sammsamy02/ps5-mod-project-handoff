// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Dialog;

/// <summary>
/// The parameters the error dialog is opened with. Fill it through
/// <see cref="ErrorDialog.InitializeParam"/> so the size is set, then set the error code and user.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 16)]
public struct SceErrorDialogParam
{
    /// <summary>The size of this block, in bytes. Set before the call.</summary>
    public int Size;

    /// <summary>The error code to show a message for.</summary>
    public int ErrorCode;

    /// <summary>The user the dialog belongs to.</summary>
    public int UserId;

    private int _reserved;
}

/// <summary>
/// The system error dialog: it shows the standard message for an error code. A package installer or
/// any utility uses it to report a failure to the user in the console's own style.
/// </summary>
public static unsafe partial class ErrorDialog
{
    private const string Lib = "libSceErrorDialog";

    /// <summary>
    /// Clears <paramref name="param"/>, sets its size, and names the system profile as the user.
    /// </summary>
    /// <remarks>
    /// The user has to be named. Clearing the block leaves it at nought, and the dialog refuses that
    /// outright - it takes an identifier from one to two hundred and fifty-five, or one of the larger
    /// range the platform assigns, and nothing else. Set <see cref="SceErrorDialogParam.UserId"/> to a
    /// real signed-in user where the message belongs to one.
    /// </remarks>
    public static void InitializeParam(SceErrorDialogParam* param)
    {
        *param = default;
        param->Size = sizeof(SceErrorDialogParam);
        param->UserId = SceUser.System;
    }

    /// <summary>Brings the error dialog up. Zero on success.</summary>
    [LibraryImport(Lib)]
    public static partial int sceErrorDialogInitialize();

    /// <summary>Shuts the error dialog down.</summary>
    [LibraryImport(Lib)]
    public static partial int sceErrorDialogTerminate();

    /// <summary>Opens the dialog for the error in <paramref name="param"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceErrorDialogOpen(SceErrorDialogParam* param);

    /// <summary>Closes an open dialog.</summary>
    [LibraryImport(Lib)]
    public static partial int sceErrorDialogClose();

    /// <summary>Advances the dialog and reports where it is.</summary>
    [LibraryImport(Lib)]
    public static partial CommonDialogStatus sceErrorDialogUpdateStatus();

    /// <summary>Reports where the dialog is without advancing it.</summary>
    [LibraryImport(Lib)]
    public static partial CommonDialogStatus sceErrorDialogGetStatus();
}
