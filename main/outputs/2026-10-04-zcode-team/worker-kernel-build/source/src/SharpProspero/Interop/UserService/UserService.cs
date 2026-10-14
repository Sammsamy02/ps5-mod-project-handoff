// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.UserService;

/// <summary>What happened to a user.</summary>
public enum UserServiceEventType
{
    /// <summary>The user signed in.</summary>
    Login = 0,

    /// <summary>The user signed out.</summary>
    Logout = 1,
}

/// <summary>One entry from the service's notification list.</summary>
[StructLayout(LayoutKind.Sequential, Size = 8)]
public struct SceUserServiceEvent
{
    public UserServiceEventType EventType;
    public int UserId;
}

/// <summary>
/// The preferences a user set for every game at once. Set <see cref="ThisSize"/> to the size of this
/// block before the call so the service knows which fields it may fill.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 40)]
public struct SceUserServiceGamePresets
{
    public nuint ThisSize;
    public uint Difficulty;
    public uint Priority;
    public uint InvertVerticalViewFor1stPersonView;
    public uint InvertHorizontalViewFor1stPersonView;
    public uint InvertVerticalViewFor3rdPersonView;
    public uint InvertHorizontalViewFor3rdPersonView;
    public uint DisplaySubTitles;
    public uint AudioLanguage;
}

/// <summary>
/// User-service bindings. The service resolves the signed-in profile that owns input and display
/// resources. Initialize it once at startup before opening a controller.
/// </summary>
public static unsafe partial class UserService
{
    private const string Lib = "libSceUserService";

    /// <summary>Starts the service with default parameters.</summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceInitialize(void* initParams);

    /// <summary>Stops the service.</summary>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceTerminate();

    /// <summary>Writes the initial (first signed-in) user id to <paramref name="userId"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceGetInitialUser(int* userId);

    /// <summary>The most users signed in at once.</summary>
    public const int MaxLoginUsers = 4;

    /// <summary>The longest a user name is, in characters.</summary>
    public const int MaxUserNameLength = 16;

    /// <summary>
    /// Writes the ids of the signed-in users into <paramref name="userIdList"/>, a buffer of
    /// <see cref="MaxLoginUsers"/> ints. Unused slots hold <c>SceUser.Invalid</c>.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceGetLoginUserIdList(int* userIdList);

    /// <summary>
    /// Writes <paramref name="userId"/>'s display name into <paramref name="userName"/> as a
    /// null-terminated UTF-8 string, up to <paramref name="size"/> bytes.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceGetUserName(int userId, byte* userName, nuint size);

    /// <summary>Writes the number the system assigned <paramref name="userId"/> to <paramref name="number"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceGetUserNumber(int userId, int* number);

    /// <summary>
    /// Starts the service and pins its notification thread to <paramref name="cpuAffinityMask"/>. Use
    /// this instead of <see cref="sceUserServiceInitialize"/> when the application places its own threads.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceInitialize2(int threadPriority, ulong cpuAffinityMask);

    /// <summary>
    /// Removes the oldest sign-in or sign-out entry from the service's list and writes it to
    /// <paramref name="event"/>. The list grows for as long as nothing drains it, so an application that
    /// wants to react to a user coming or going calls this until it stops returning zero.
    /// </summary>
    /// <returns>
    /// Zero when an entry was written, or a negative error code; the empty-list code is returned once the
    /// list runs out.
    /// </returns>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceGetEvent(SceUserServiceEvent* @event);

    /// <summary>
    /// The list held nothing more. This is what <see cref="sceUserServiceGetEvent"/> returns once it has
    /// been drained, and it is the ordinary end of a drain rather than a fault.
    /// </summary>
    public const int ErrorNoEvent = unchecked((int)0x80960007);

    /// <summary>Reads <paramref name="userId"/>'s cross-game preferences.</summary>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceGetGamePresets(int userId, SceUserServiceGamePresets* presets);

    /// <summary>
    /// Reads the age restriction that applies to <paramref name="userId"/>. <c>0xFFFFFFFF</c> means
    /// unrestricted.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceGetAgeLevel(int userId, uint* ageLevel);

    /// <summary>Vibration is switched off for this user.</summary>
    public const int VibrationIntensityOff = 0;

    /// <summary>Vibration runs at the standard strength.</summary>
    public const int VibrationIntensityStrong = 1;

    /// <summary>Vibration runs at half strength.</summary>
    public const int VibrationIntensityMedium = 2;

    /// <summary>Vibration runs at the lowest strength.</summary>
    public const int VibrationIntensityWeak = 3;

    /// <summary>Trigger resistance is switched off for this user.</summary>
    public const int TriggerEffectIntensityOff = 0;

    /// <summary>Trigger resistance runs at the standard strength.</summary>
    public const int TriggerEffectIntensityStrong = 1;

    /// <summary>Trigger resistance runs at half strength.</summary>
    public const int TriggerEffectIntensityMedium = 2;

    /// <summary>Trigger resistance runs at the lowest strength.</summary>
    public const int TriggerEffectIntensityWeak = 3;

    /// <summary>
    /// Reads the vibration strength <paramref name="userId"/> chose, as one of the
    /// <c>VibrationIntensity</c> values. A pad rumble is meant to be scaled by this.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceGetAccessibilityVibration(int userId, int* vibration);

    /// <summary>
    /// Reads the trigger-resistance strength <paramref name="userId"/> chose, as one of the
    /// <c>TriggerEffectIntensity</c> values.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceGetAccessibilityTriggerEffect(int userId, int* triggerEffect);

    /// <summary>Reads how long <paramref name="userId"/> wants a press-and-hold to take.</summary>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceGetAccessibilityPressAndHoldDelay(int userId, int* pressAndHoldDelay);

    /// <summary>Reads whether <paramref name="userId"/> wants voice chat transcribed.</summary>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceGetAccessibilityChatTranscription(int userId, int* chatTranscription);

    /// <summary>
    /// Writes the user id of the foreground user (the profile whose controller is active) to
    /// <paramref name="userId"/>.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceUserServiceGetForegroundUser(int* userId);
}
