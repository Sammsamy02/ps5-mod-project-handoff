// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Pad;

/// <summary>Digital button bits reported in the leading word of a controller sample.</summary>
[Flags]
public enum ScePadButton : uint
{
    /// <summary>No buttons.</summary>
    None = 0,

    /// <summary>Left stick press.</summary>
    L3 = 0x00000002,

    /// <summary>Right stick press.</summary>
    R3 = 0x00000004,

    /// <summary>Options.</summary>
    Options = 0x00000008,

    /// <summary>D-pad up.</summary>
    Up = 0x00000010,

    /// <summary>D-pad right.</summary>
    Right = 0x00000020,

    /// <summary>D-pad down.</summary>
    Down = 0x00000040,

    /// <summary>D-pad left.</summary>
    Left = 0x00000080,

    /// <summary>Left trigger, digital threshold.</summary>
    L2 = 0x00000100,

    /// <summary>Right trigger, digital threshold.</summary>
    R2 = 0x00000200,

    /// <summary>Left bumper.</summary>
    L1 = 0x00000400,

    /// <summary>Right bumper.</summary>
    R1 = 0x00000800,

    /// <summary>Triangle.</summary>
    Triangle = 0x00001000,

    /// <summary>Circle.</summary>
    Circle = 0x00002000,

    /// <summary>Cross.</summary>
    Cross = 0x00004000,

    /// <summary>Square.</summary>
    Square = 0x00008000,

    /// <summary>Touch-pad press.</summary>
    TouchPad = 0x00100000,

    /// <summary>
    /// Set when the system has taken the controller for itself. The rest of the sample is then whatever
    /// the system was doing with it, not what the player is pressing, so a sample carrying this bit is
    /// discarded rather than read.
    /// </summary>
    Intercepted = 0x80000000,
}

/// <summary>Vibration motor levels: the large (left) and small (right) motors, 0 (stop) to 255.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct ScePadVibrationParam
{
    /// <summary>Large (left) motor level, 0 to 255.</summary>
    public byte LargeMotor;

    /// <summary>Small (right) motor level, 0 to 255.</summary>
    public byte SmallMotor;
}

/// <summary>How strongly the motors are driven.</summary>
public enum ScePadVibrationMode : int
{
    /// <summary>The default: the motors are driven at their full range.</summary>
    Advanced = 1,

    /// <summary>The motors are driven the way an earlier controller generation did.</summary>
    Compatible = 2,
}

/// <summary>What a trigger's motor arm does while the trigger is pulled.</summary>
public enum ScePadTriggerEffectMode : int
{
    /// <summary>The arm is released; the trigger moves freely.</summary>
    Off = 0,

    /// <summary>The arm resists from one position onwards.</summary>
    Feedback = 1,

    /// <summary>The arm resists over a span and then gives way, as a gun trigger does.</summary>
    Weapon = 2,

    /// <summary>The arm vibrates around a position.</summary>
    Vibration = 3,
}

/// <summary>Where a trigger currently is within its effect.</summary>
public enum ScePadTriggerEffectState : int
{
    /// <summary>No effect is set.</summary>
    Off = 0,

    /// <summary>A feedback effect is set and the trigger has not reached its position.</summary>
    FeedbackStandby = 1,

    /// <summary>The trigger is past the feedback position and the arm is resisting.</summary>
    FeedbackActive = 2,

    /// <summary>A weapon effect is set and the trigger has not reached its start position.</summary>
    WeaponStandby = 3,

    /// <summary>The trigger is inside the weapon span.</summary>
    WeaponPulling = 4,

    /// <summary>The trigger has passed the weapon end position.</summary>
    WeaponFired = 5,

    /// <summary>A vibration effect is set and the trigger has not reached its position.</summary>
    VibrationStandby = 6,

    /// <summary>The trigger is past the vibration position and the arm is vibrating.</summary>
    VibrationActive = 7,

    /// <summary>The system has taken the controller, so the state is not the application's.</summary>
    Intercepted = -1,
}

/// <summary>One trigger's effect: the mode and the fields that mode reads.</summary>
[StructLayout(LayoutKind.Sequential, Size = 56)]
public unsafe struct ScePadTriggerEffectCommand
{
    /// <summary>Which effect the fields below describe.</summary>
    public ScePadTriggerEffectMode Mode;

    private int _padding;

    /// <summary>
    /// The mode's own fields, laid out per mode. Build a command through one of the factories rather than
    /// writing these directly.
    /// </summary>
    public fixed byte CommandData[48];

    /// <summary>Releases the trigger.</summary>
    public static ScePadTriggerEffectCommand Off() => default;

    /// <summary>
    /// Resists from <paramref name="position"/> (0 to 9) onwards at <paramref name="strength"/>
    /// (0 to 8, where 0 is the same as no effect).
    /// </summary>
    public static ScePadTriggerEffectCommand Feedback(byte position, byte strength)
    {
        ArgumentOutOfRangeException.ThrowIfGreaterThan(position, (byte)9);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(strength, (byte)8);

        ScePadTriggerEffectCommand command = default;
        command.Mode = ScePadTriggerEffectMode.Feedback;
        command.CommandData[0] = position;
        command.CommandData[1] = strength;
        return command;
    }

    /// <summary>
    /// Resists from <paramref name="startPosition"/> (2 to 7) to <paramref name="endPosition"/>
    /// (above the start, up to 8) at <paramref name="strength"/> (0 to 8), then gives way.
    /// </summary>
    public static ScePadTriggerEffectCommand Weapon(byte startPosition, byte endPosition, byte strength)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(startPosition, (byte)2);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(startPosition, (byte)7);
        ArgumentOutOfRangeException.ThrowIfLessThanOrEqual(endPosition, startPosition);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(endPosition, (byte)8);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(strength, (byte)8);

        ScePadTriggerEffectCommand command = default;
        command.Mode = ScePadTriggerEffectMode.Weapon;
        command.CommandData[0] = startPosition;
        command.CommandData[1] = endPosition;
        command.CommandData[2] = strength;
        return command;
    }

    /// <summary>
    /// Vibrates around <paramref name="position"/> (0 to 9) at <paramref name="amplitude"/> (0 to 8) and
    /// <paramref name="frequency"/> hertz.
    /// </summary>
    public static ScePadTriggerEffectCommand Vibration(byte position, byte amplitude, byte frequency)
    {
        ArgumentOutOfRangeException.ThrowIfGreaterThan(position, (byte)9);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(amplitude, (byte)8);

        ScePadTriggerEffectCommand command = default;
        command.Mode = ScePadTriggerEffectMode.Vibration;
        command.CommandData[0] = position;
        command.CommandData[1] = amplitude;
        command.CommandData[2] = frequency;
        return command;
    }
}

/// <summary>
/// Both triggers' effects. A command is applied only while its bit is set in <see cref="TriggerMask"/>;
/// with the bit clear the command is ignored and the trigger keeps whatever it had.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 120)]
public unsafe struct ScePadTriggerEffectParam
{
    /// <summary>Which triggers this call applies to. See <see cref="Pad.TriggerMaskL2"/> and <see cref="Pad.TriggerMaskR2"/>.</summary>
    public byte TriggerMask;

    private fixed byte _padding[7];

    /// <summary>The left trigger's effect.</summary>
    public ScePadTriggerEffectCommand CommandL2;

    /// <summary>The right trigger's effect.</summary>
    public ScePadTriggerEffectCommand CommandR2;
}

/// <summary>Where both triggers are within their effects.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct ScePadTriggerEffectStateInformation
{
    /// <summary>The left trigger's state.</summary>
    public ScePadTriggerEffectState StateL2;

    /// <summary>The right trigger's state.</summary>
    public ScePadTriggerEffectState StateR2;
}

/// <summary>The kind of device behind a handle.</summary>
public enum ScePadDeviceClass : int
{
    /// <summary>Not a class the service recognizes.</summary>
    Invalid = -1,

    /// <summary>A standard controller.</summary>
    Standard = 0,

    /// <summary>A guitar.</summary>
    Guitar = 1,

    /// <summary>A drum kit.</summary>
    Drum = 2,

    /// <summary>A turntable.</summary>
    DjTurntable = 3,

    /// <summary>A dance mat.</summary>
    DanceMat = 4,

    /// <summary>A one-handed navigation controller.</summary>
    Navigation = 5,

    /// <summary>A steering wheel.</summary>
    SteeringWheel = 6,

    /// <summary>An arcade stick.</summary>
    Stick = 7,

    /// <summary>A flight stick.</summary>
    FlightStick = 8,

    /// <summary>A light gun.</summary>
    Gun = 9,
}

/// <summary>The touch pad's extent, which is what a touch sample's coordinates are relative to.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct ScePadTouchPadInformation
{
    /// <summary>Dots per millimetre.</summary>
    public float PixelDensity;

    /// <summary>Width in touch units.</summary>
    public ushort ResolutionX;

    /// <summary>Height in touch units.</summary>
    public ushort ResolutionY;
}

/// <summary>How far a stick moves before the service reports it as moved at all.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct ScePadStickInformation
{
    /// <summary>The left stick's dead zone.</summary>
    public byte DeadZoneLeft;

    /// <summary>The right stick's dead zone.</summary>
    public byte DeadZoneRight;
}

/// <summary>What a handle's device is and how it is attached.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct ScePadControllerInformation
{
    /// <summary>The touch pad's extent.</summary>
    public ScePadTouchPadInformation TouchPadInfo;

    /// <summary>The stick dead zones.</summary>
    public ScePadStickInformation StickInfo;

    /// <summary>0 for a local device, 2 for one reached over a remote session.</summary>
    public byte ConnectionType;

    /// <summary>How many times a device has been attached to this handle. A change means a different device.</summary>
    public byte ConnectedCount;

    /// <summary>Whether a device is attached right now.</summary>
    public byte Connected;

    private byte _pad0;
    private byte _pad1;
    private byte _pad2;

    /// <summary>The kind of device.</summary>
    public ScePadDeviceClass DeviceClass;

    private fixed byte _reserved[8];
}

/// <summary>The codes the controller service returns when it refuses a request.</summary>
public static class ScePadError
{
    /// <summary>An argument was outside what the call accepts, or a required pointer was null.</summary>
    public const int InvalidArg = unchecked((int)0x80920001);

    /// <summary>The port does not exist.</summary>
    public const int InvalidPort = unchecked((int)0x80920002);

    /// <summary>The handle is not one the service issued, or it has been closed.</summary>
    public const int InvalidHandle = unchecked((int)0x80920003);

    /// <summary>The port is already open.</summary>
    public const int AlreadyOpened = unchecked((int)0x80920004);

    /// <summary>The subsystem has not been initialized.</summary>
    public const int NotInitialized = unchecked((int)0x80920005);

    /// <summary>The light-bar color is not one the service accepts.</summary>
    public const int InvalidLightBarSetting = unchecked((int)0x80920006);

    /// <summary>No device is attached to the handle.</summary>
    public const int DeviceNotConnected = unchecked((int)0x80920007);

    /// <summary>No handle exists for the request.</summary>
    public const int NoHandle = unchecked((int)0x80920008);

    /// <summary>The service failed in a way it does not describe further.</summary>
    public const int Fatal = unchecked((int)0x809200FF);
}

/// <summary>An 8-bit-per-channel color for the controller light bar.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct ScePadColor
{
    /// <summary>Red.</summary>
    public byte R;

    /// <summary>Green.</summary>
    public byte G;

    /// <summary>Blue.</summary>
    public byte B;

    /// <summary>Reserved. Leave zero.</summary>
    public byte Reserved;
}

/// <summary>
/// Controller bindings. Initialize the subsystem, open a handle for a user, then read samples. The
/// sample buffer is treated as raw bytes; <see cref="SharpProspero.Input.GamePad"/> decodes the
/// buttons, stick axes, triggers, motion and touch fields from it.
/// </summary>
public static unsafe partial class Pad
{
    private const string Lib = "libScePad";

    /// <summary>Size, in bytes, of the buffer passed to <see cref="scePadReadState"/>. Exceeds the
    /// sample structure so the service never writes past the buffer.</summary>
    public const int SampleBufferSize = 1024;

    /// <summary>The distance, in bytes, from one sample to the next in a <see cref="scePadRead"/> buffer.</summary>
    public const int SampleStride = 120;

    /// <summary>The most samples one <see cref="scePadRead"/> can return.</summary>
    public const int MaxDataNum = 64;

    /// <summary>Apply the effect to the left trigger.</summary>
    public const byte TriggerMaskL2 = 0x01;

    /// <summary>Apply the effect to the right trigger.</summary>
    public const byte TriggerMaskR2 = 0x02;

    /// <summary>A standard controller.</summary>
    public const int PortTypeStandard = 0;

    /// <summary>A special controller such as a wheel or a stick.</summary>
    public const int PortTypeSpecial = 2;

    /// <summary>The remote control. Only the system user may open one.</summary>
    public const int PortTypeRemoteControl = 16;

    /// <summary>
    /// A handle for this user, type and index is already open. Ask <see cref="scePadGetHandle"/> for it
    /// rather than opening a second one.
    /// </summary>
    public const int ErrorAlreadyOpened = unchecked((int)0x80920004);

    /// <summary>No device is attached to the handle.</summary>
    public const int ErrorDeviceNotConnected = unchecked((int)0x80920007);

    /// <summary>Nothing has opened a handle for this user, type and index.</summary>
    public const int ErrorNoHandle = unchecked((int)0x80920008);

    /// <summary>Initializes the controller subsystem. Call once before opening a handle.</summary>
    [LibraryImport(Lib)]
    public static partial int scePadInit();

    /// <summary>Opens a controller handle for <paramref name="userId"/>.</summary>
    /// <returns>A non-negative handle on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int scePadOpen(int userId, int type, int index, void* param);

    /// <summary>
    /// Returns the handle an earlier <see cref="scePadOpen"/> produced for the same user, type and index,
    /// so a second consumer reaches the open device instead of opening it again.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int scePadGetHandle(int userId, int type, int index);

    /// <summary>Closes a controller handle.</summary>
    [LibraryImport(Lib)]
    public static partial int scePadClose(int handle);

    /// <summary>Reads the latest sample into <paramref name="data"/> (at least
    /// <see cref="SampleBufferSize"/> bytes).</summary>
    [LibraryImport(Lib)]
    public static partial int scePadReadState(int handle, void* data);

    /// <summary>
    /// Drains the samples buffered since the last read into <paramref name="data"/>, oldest first, which
    /// is how an input that lasted less than a frame is still seen. <paramref name="data"/> holds
    /// <paramref name="num"/> samples of <see cref="SampleStride"/> bytes each, and <paramref name="num"/>
    /// runs from 1 to <see cref="MaxDataNum"/>.
    /// </summary>
    /// <returns>How many samples were written, 0 when nothing arrived, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int scePadRead(int handle, void* data, int num);

    /// <summary>Reads what the device is and how it is attached.</summary>
    [LibraryImport(Lib)]
    public static partial int scePadGetControllerInformation(int handle, ScePadControllerInformation* info);

    /// <summary>Sets the vibration motors.</summary>
    [LibraryImport(Lib)]
    public static partial int scePadSetVibration(int handle, ScePadVibrationParam* param);

    /// <summary>Sets the light-bar color.</summary>
    [LibraryImport(Lib)]
    public static partial int scePadSetLightBar(int handle, ScePadColor* param);

    /// <summary>Chooses how strongly the motors are driven.</summary>
    [LibraryImport(Lib)]
    public static partial int scePadSetVibrationMode(int handle, ScePadVibrationMode mode);

    /// <summary>
    /// Weakens vibration and trigger effects while the built-in microphone is in use, so the motors are
    /// not what the microphone picks up. Applies to every handle, not one.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(
        [MarshalAs(UnmanagedType.U1)] bool enable);

    /// <summary>
    /// Sets both triggers' effects. Only the triggers named in
    /// <see cref="ScePadTriggerEffectParam.TriggerMask"/> are touched.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int scePadSetTriggerEffect(int handle, ScePadTriggerEffectParam* param);

    /// <summary>Reads where both triggers are within their effects.</summary>
    [LibraryImport(Lib)]
    public static partial int scePadGetTriggerEffectState(int handle, ScePadTriggerEffectStateInformation* info);

    /// <summary>
    /// Turns the motion sensors on or off. They are on by default; turning them off stops the
    /// orientation, acceleration and angular-velocity fields of a sample from being updated.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int scePadSetMotionSensorState(int handle, [MarshalAs(UnmanagedType.U1)] bool enable);

    /// <summary>Corrects the reported orientation for sensor drift. Off by default.</summary>
    [LibraryImport(Lib)]
    public static partial int scePadSetTiltCorrectionState(int handle, [MarshalAs(UnmanagedType.U1)] bool enable);

    /// <summary>
    /// Zeroes small angular-velocity readings so a resting controller reports no rotation. Off by
    /// default.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int scePadSetAngularVelocityDeadbandState(int handle, [MarshalAs(UnmanagedType.U1)] bool enable);

    /// <summary>
    /// Makes the controller's current attitude the identity orientation. The orientation a sample carries
    /// accumulates from when the device attached, so this is what re-centres it.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int scePadResetOrientation(int handle);

    /// <summary>Restores the light bar to its default color.</summary>
    [LibraryImport(Lib)]
    public static partial int scePadResetLightBar(int handle);

    /// <summary>
    /// Grants or revokes pad input privilege for the calling process. A background process
    /// (such as a daemon) calls this with 1 to receive controller input without being the
    /// foreground application.
    /// </summary>
    /// <param name="value">1 to grant privilege, 0 to revoke.</param>
    [LibraryImport(Lib)]
    public static partial int scePadSetProcessPrivilege(int value);
}
