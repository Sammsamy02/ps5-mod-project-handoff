// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Np;

/// <summary>A trophy's grade.</summary>
public enum SceNpTrophy2Grade
{
    /// <summary>Unknown grade.</summary>
    Unknown = 0,

    /// <summary>The platinum trophy, awarded for earning every other trophy.</summary>
    Platinum = 1,

    /// <summary>A gold trophy.</summary>
    Gold = 2,

    /// <summary>A silver trophy.</summary>
    Silver = 3,

    /// <summary>A bronze trophy.</summary>
    Bronze = 4,
}

/// <summary>Progress toward a trophy, as defined by the trophy set.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNpTrophy2Progress
{
    /// <summary>0 for none, 1 for a 64-bit target value.</summary>
    public int Type;

    private uint _reserved;

    /// <summary>The target value when <see cref="Type"/> is 1.</summary>
    public ulong ValueUInt64;
}

/// <summary>The fixed facts about a trophy set.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceNpTrophy2GameDetails
{
    /// <summary>How many trophy groups the set has.</summary>
    public uint NumGroups;

    /// <summary>The total number of trophies.</summary>
    public uint NumTrophies;

    /// <summary>How many platinum trophies (0 or 1).</summary>
    public uint NumPlatinum;

    /// <summary>How many gold trophies.</summary>
    public uint NumGold;

    /// <summary>How many silver trophies.</summary>
    public uint NumSilver;

    /// <summary>How many bronze trophies.</summary>
    public uint NumBronze;

    /// <summary>The title, a NUL-terminated UTF-8 string.</summary>
    public fixed byte Title[128];
}

/// <summary>The player's progress across a whole trophy set.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNpTrophy2GameData
{
    /// <summary>How many trophies the player has unlocked.</summary>
    public uint UnlockedTrophies;

    /// <summary>How many platinum trophies unlocked.</summary>
    public uint UnlockedPlatinum;

    /// <summary>How many gold trophies unlocked.</summary>
    public uint UnlockedGold;

    /// <summary>How many silver trophies unlocked.</summary>
    public uint UnlockedSilver;

    /// <summary>How many bronze trophies unlocked.</summary>
    public uint UnlockedBronze;

    /// <summary>Overall completion, 0 to 100.</summary>
    public uint ProgressPercentage;
}

/// <summary>The fixed facts about a trophy group.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceNpTrophy2GroupDetails
{
    /// <summary>The group id.</summary>
    public int GroupId;

    /// <summary>How many trophies are in the group.</summary>
    public uint NumTrophies;

    /// <summary>How many platinum trophies.</summary>
    public uint NumPlatinum;

    /// <summary>How many gold trophies.</summary>
    public uint NumGold;

    /// <summary>How many silver trophies.</summary>
    public uint NumSilver;

    /// <summary>How many bronze trophies.</summary>
    public uint NumBronze;

    /// <summary>The group title, a NUL-terminated UTF-8 string.</summary>
    public fixed byte Title[128];
}

/// <summary>The player's progress within a trophy group.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNpTrophy2GroupData
{
    /// <summary>The group id.</summary>
    public int GroupId;

    /// <summary>How many trophies unlocked in the group.</summary>
    public uint UnlockedTrophies;

    /// <summary>How many platinum trophies unlocked.</summary>
    public uint UnlockedPlatinum;

    /// <summary>How many gold trophies unlocked.</summary>
    public uint UnlockedGold;

    /// <summary>How many silver trophies unlocked.</summary>
    public uint UnlockedSilver;

    /// <summary>How many bronze trophies unlocked.</summary>
    public uint UnlockedBronze;

    /// <summary>Group completion, 0 to 100.</summary>
    public uint ProgressPercentage;

    private uint _reserved;
}

/// <summary>The fixed facts about a single trophy.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceNpTrophy2Details
{
    /// <summary>The trophy id.</summary>
    public int TrophyId;

    /// <summary>The grade, as <see cref="SceNpTrophy2Grade"/>.</summary>
    public int TrophyGrade;

    /// <summary>The group the trophy belongs to.</summary>
    public int GroupId;

    /// <summary>Non-zero when the trophy is hidden until unlocked.</summary>
    public byte Hidden;

    /// <summary>Non-zero when the trophy grants a reward.</summary>
    public byte HasReward;

    private ushort _reserved2;

    /// <summary>The progress target for the trophy.</summary>
    public SceNpTrophy2Progress Target;

    /// <summary>The trophy name, a NUL-terminated UTF-8 string.</summary>
    public fixed byte Name[128];

    /// <summary>The trophy description, a NUL-terminated UTF-8 string.</summary>
    public fixed byte Description[1024];

    /// <summary>The reward description, a NUL-terminated UTF-8 string.</summary>
    public fixed byte Reward[128];
}

/// <summary>The player's state for a single trophy.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNpTrophy2Data
{
    /// <summary>The trophy id.</summary>
    public int TrophyId;

    /// <summary>Non-zero when the player has unlocked the trophy.</summary>
    public byte Unlocked;

    private byte _reserved0;
    private byte _reserved1;
    private byte _reserved2;

    /// <summary>The player's progress toward the trophy.</summary>
    public SceNpTrophy2Progress Progress;

    /// <summary>When the trophy was unlocked, as a real-time-clock tick.</summary>
    public ulong Timestamp;
}

/// <summary>
/// Trophy bindings (libSceNpTrophy2). The library reads a title's trophy set and the signed-in player's
/// progress, fetches the trophy and reward icons, and shows the system trophy list. Unlocking a trophy is
/// done through the universal-data-system events, not here.
/// </summary>
public static unsafe partial class NpTrophy2
{
    private const string Lib = "libSceNpTrophy2";

    /// <summary>Creates a work handle used across the info calls.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2CreateHandle(int* handle);

    /// <summary>Destroys a work handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2DestroyHandle(int handle);

    /// <summary>Aborts the operation running on a handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2AbortHandle(int handle);

    /// <summary>Creates a trophy context for a user and service label.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2CreateContext(int* context, int userId, uint serviceLabel, ulong options);

    /// <summary>Destroys a trophy context.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2DestroyContext(int context);

    /// <summary>Registers the trophy context, loading the title's trophy set.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2RegisterContext(int context, int handle, ulong options);

    /// <summary>Registers a callback invoked when a trophy unlocks.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2RegisterUnlockCallback(delegate* unmanaged[Cdecl]<int, int, void*, void> callback, void* userdata);

    /// <summary>Removes the unlock callback.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2UnregisterUnlockCallback();

    /// <summary>Reads the trophy set's fixed facts and the player's overall progress.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2GetGameInfo(int context, int handle, SceNpTrophy2GameDetails* details, SceNpTrophy2GameData* data);

    /// <summary>Reads one trophy group's facts and the player's progress in it.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2GetGroupInfo(int context, int handle, int groupId, SceNpTrophy2GroupDetails* details, SceNpTrophy2GroupData* data);

    /// <summary>Reads a range of trophy groups.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2GetGroupInfoArray(int context, int handle, uint offset, uint limit, SceNpTrophy2GroupDetails* detailsArray, SceNpTrophy2GroupData* dataArray, uint* count);

    /// <summary>Reads one trophy's facts and the player's state.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2GetTrophyInfo(int context, int handle, int trophyId, SceNpTrophy2Details* details, SceNpTrophy2Data* data);

    /// <summary>Reads a range of trophies.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2GetTrophyInfoArray(int context, int handle, uint offset, uint limit, SceNpTrophy2Details* detailsArray, SceNpTrophy2Data* dataArray, uint* count);

    /// <summary>Reads the trophy-set icon into <paramref name="buffer"/>, or the required size when it is null.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2GetGameIcon(int context, int handle, void* buffer, nuint* size);

    /// <summary>Reads a group icon.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2GetGroupIcon(int context, int handle, int groupId, void* buffer, nuint* size);

    /// <summary>Reads a trophy icon.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2GetTrophyIcon(int context, int handle, int trophyId, void* buffer, nuint* size);

    /// <summary>Reads a reward icon.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2GetRewardIcon(int context, int handle, int trophyId, void* buffer, nuint* size);

    /// <summary>Shows the system trophy list for the context.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpTrophy2ShowTrophyList(int context);
}
