// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Dialog;

/// <summary>Which display the message dialog is opened in.</summary>
public enum MsgDialogMode
{
    Invalid = 0,

    /// <summary>An application message with a set of buttons.</summary>
    UserMessage = 1,

    /// <summary>A progress bar the application drives.</summary>
    ProgressBar = 2,

    /// <summary>A system-defined message.</summary>
    SystemMessage = 3,
}

/// <summary>The set of buttons a user message shows.</summary>
public enum MsgDialogButtonType
{
    Ok = 0,
    YesNo = 1,
    None = 2,
    OkCancel = 3,
    Wait = 5,
    WaitCancel = 6,
    YesNoFocusNo = 7,
    OkCancelFocusCancel = 8,
    TwoButtons = 9,
}

/// <summary>The button the user chose. OK, Yes, and the first button share value 1; No and the second share 2.</summary>
public enum MsgDialogButtonId
{
    Invalid = 0,

    /// <summary>OK, Yes, or the first button.</summary>
    Ok = 1,

    /// <summary>No or the second button.</summary>
    No = 2,
}

/// <summary>Whether the progress bar shows a cancel button.</summary>
public enum MsgDialogProgressBarType
{
    Percentage = 0,
    PercentageWithCancel = 1,
}

/// <summary>Which progress bar a call targets. There is one.</summary>
public enum MsgDialogProgressBarTarget
{
    Default = 0,
}

/// <summary>The buttons of an application message.</summary>
[StructLayout(LayoutKind.Sequential, Size = 48)]
public unsafe struct SceMsgDialogButtonsParam
{
    /// <summary>The first button's label (UTF-8), or null.</summary>
    public byte* Msg1;

    /// <summary>The second button's label (UTF-8), or null.</summary>
    public byte* Msg2;

    private fixed byte _reserved[32];
}

/// <summary>An application message: a string and a set of buttons.</summary>
[StructLayout(LayoutKind.Sequential, Size = 48)]
public unsafe struct SceMsgDialogUserMessageParam
{
    /// <summary>The button set.</summary>
    public MsgDialogButtonType ButtonType;

    private int _pad0;

    /// <summary>The message (UTF-8, NUL-terminated).</summary>
    public byte* Msg;

    /// <summary>Custom button labels, for <see cref="MsgDialogButtonType.TwoButtons"/> only.</summary>
    public SceMsgDialogButtonsParam* ButtonsParam;

    private fixed byte _reserved[24];
}

/// <summary>A system-defined message.</summary>
[StructLayout(LayoutKind.Sequential, Size = 36)]
public unsafe struct SceMsgDialogSystemMessageParam
{
    /// <summary>Which system message to show.</summary>
    public int SysMsgType;

    private fixed byte _reserved[32];
}

/// <summary>A progress bar and its caption.</summary>
[StructLayout(LayoutKind.Sequential, Size = 80)]
public unsafe struct SceMsgDialogProgressBarParam
{
    /// <summary>Whether the bar shows a cancel button.</summary>
    public MsgDialogProgressBarType BarType;

    private int _pad0;

    /// <summary>The caption shown with the bar (UTF-8, NUL-terminated).</summary>
    public byte* Msg;

    private fixed byte _reserved[64];
}

/// <summary>
/// The message dialog's parameters. Fill it through <see cref="MessageDialog.InitializeParam"/> so the
/// sizes and the check value are set, then set the mode and its matching parameter block.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 136)]
public unsafe struct SceMsgDialogParam
{
    /// <summary>The shared dialog block.</summary>
    public CommonDialogBaseParam BaseParam;

    /// <summary>The size of this block, in bytes.</summary>
    public ulong Size;

    /// <summary>Which display to open.</summary>
    public MsgDialogMode Mode;

    private int _pad0;

    /// <summary>The parameter block for <see cref="MsgDialogMode.UserMessage"/>, or null.</summary>
    public SceMsgDialogUserMessageParam* UserMsgParam;

    /// <summary>The parameter block for <see cref="MsgDialogMode.ProgressBar"/>, or null.</summary>
    public SceMsgDialogProgressBarParam* ProgBarParam;

    /// <summary>The parameter block for <see cref="MsgDialogMode.SystemMessage"/>, or null.</summary>
    public SceMsgDialogSystemMessageParam* SysMsgParam;

    /// <summary>The user the dialog belongs to.</summary>
    public int UserId;

    private fixed byte _reserved[40];
    private int _pad1;
}

/// <summary>The result of a message dialog that has closed.</summary>
[StructLayout(LayoutKind.Sequential, Size = 44)]
public unsafe struct SceMsgDialogResult
{
    /// <summary>The display that ran.</summary>
    public MsgDialogMode Mode;

    /// <summary>The result code.</summary>
    public int Result;

    /// <summary>The button the user chose.</summary>
    public MsgDialogButtonId ButtonId;

    private fixed byte _reserved[32];
}

/// <summary>
/// The system message dialog: an application message with buttons, a progress bar the application
/// drives, or a system-defined message. A package installer uses the progress bar; any utility uses
/// the message for a yes/no question. The higher-level <see cref="Platform.MessageDialog"/> drives it.
/// </summary>
public static unsafe partial class MessageDialog
{
    private const string Lib = "libSceMsgDialog";
    private const ulong MagicNumber = 0xC0D1A109;

    /// <summary>Zeroes <paramref name="param"/> and fills the sizes and the check value.</summary>
    public static void InitializeParam(SceMsgDialogParam* param)
    {
        new System.Span<byte>(param, sizeof(SceMsgDialogParam)).Clear();
        param->BaseParam.Size = (ulong)sizeof(CommonDialogBaseParam);
        param->BaseParam.Magic = unchecked((uint)(MagicNumber + (ulong)&param->BaseParam));
        param->Size = (ulong)sizeof(SceMsgDialogParam);
    }

    /// <summary>Brings the message dialog up.</summary>
    [LibraryImport(Lib)]
    public static partial int sceMsgDialogInitialize();

    /// <summary>Shuts the message dialog down.</summary>
    [LibraryImport(Lib)]
    public static partial int sceMsgDialogTerminate();

    /// <summary>Opens the dialog described by <paramref name="param"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceMsgDialogOpen(SceMsgDialogParam* param);

    /// <summary>Advances the dialog and returns its status.</summary>
    [LibraryImport(Lib)]
    public static partial CommonDialogStatus sceMsgDialogUpdateStatus();

    /// <summary>Returns the dialog's status without advancing it.</summary>
    [LibraryImport(Lib)]
    public static partial CommonDialogStatus sceMsgDialogGetStatus();

    /// <summary>Reads the result of a finished dialog.</summary>
    [LibraryImport(Lib)]
    public static partial int sceMsgDialogGetResult(SceMsgDialogResult* result);

    /// <summary>Closes an open dialog.</summary>
    [LibraryImport(Lib)]
    public static partial int sceMsgDialogClose();

    /// <summary>Adds <paramref name="delta"/> percent to the progress bar.</summary>
    [LibraryImport(Lib)]
    public static partial int sceMsgDialogProgressBarInc(int target, uint delta);

    /// <summary>Sets the progress bar to <paramref name="rate"/> percent.</summary>
    [LibraryImport(Lib)]
    public static partial int sceMsgDialogProgressBarSetValue(int target, uint rate);

    /// <summary>Sets the caption shown with the progress bar (UTF-8).</summary>
    [LibraryImport(Lib)]
    public static partial int sceMsgDialogProgressBarSetMsg(int target, byte* barMsg);
}
