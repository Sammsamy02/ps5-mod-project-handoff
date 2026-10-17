// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Np;
using System;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Platform;

/// <summary>The signed-in player's progress across a title's whole trophy set.</summary>
/// <param name="Title">The trophy set's title.</param>
/// <param name="TotalTrophies">How many trophies the set has.</param>
/// <param name="UnlockedTrophies">How many the player has unlocked.</param>
/// <param name="ProgressPercentage">Overall completion, 0 to 100.</param>
public readonly record struct TrophyProgress(string Title, int TotalTrophies, int UnlockedTrophies, int ProgressPercentage);

/// <summary>One trophy and whether the player has unlocked it.</summary>
/// <param name="Id">The trophy id.</param>
/// <param name="Grade">The trophy's grade.</param>
/// <param name="Unlocked">Whether the player has unlocked it.</param>
/// <param name="Hidden">Whether the trophy is hidden until unlocked.</param>
/// <param name="Name">The trophy name.</param>
/// <param name="Description">The trophy description.</param>
public readonly record struct TrophyInfo(int Id, SceNpTrophy2Grade Grade, bool Unlocked, bool Hidden, string Name, string Description);

/// <summary>
/// Reads a title's trophies and the signed-in player's progress, and shows the system trophy list.
/// Open it for a user, read the set-wide progress or the individual trophies, and dispose it. Unlocking a
/// trophy is a separate system event, not part of reading the set here.
/// </summary>
/// <example>
/// <code>
/// using var trophies = TrophySet.Open(userId);
/// TrophyProgress progress = trophies.GetProgress();  // "12 of 34 unlocked"
/// foreach (TrophyInfo t in trophies.GetTrophies())
///     Draw(t.Name, t.Unlocked);
/// </code>
/// </example>
public sealed unsafe class TrophySet : IDisposable
{
    private int _context;
    private int _handle;

    private TrophySet(int context, int handle)
    {
        _context = context;
        _handle = handle;
    }

    /// <summary>
    /// Opens the trophy set for <paramref name="userId"/> (creating a context and handle and registering
    /// the title's trophies). <paramref name="serviceLabel"/> selects a trophy set for a title that ships
    /// more than one; the default is the first.
    /// </summary>
    /// <exception cref="ProsperoException">The context could not be created or registered.</exception>
    public static TrophySet Open(int userId, uint serviceLabel = 0)
    {
        int handle;
        SceResult.ThrowIfFailed(NpTrophy2.sceNpTrophy2CreateHandle(&handle), nameof(NpTrophy2.sceNpTrophy2CreateHandle));

        int context;
        try
        {
            SceResult.ThrowIfFailed(
                NpTrophy2.sceNpTrophy2CreateContext(&context, userId, serviceLabel, 0),
                nameof(NpTrophy2.sceNpTrophy2CreateContext));
        }
        catch
        {
            NpTrophy2.sceNpTrophy2DestroyHandle(handle);
            throw;
        }

        try
        {
            SceResult.ThrowIfFailed(
                NpTrophy2.sceNpTrophy2RegisterContext(context, handle, 0),
                nameof(NpTrophy2.sceNpTrophy2RegisterContext));
        }
        catch
        {
            NpTrophy2.sceNpTrophy2DestroyContext(context);
            NpTrophy2.sceNpTrophy2DestroyHandle(handle);
            throw;
        }

        return new TrophySet(context, handle);
    }

    /// <summary>Reads the set's title and the player's overall progress.</summary>
    /// <exception cref="ProsperoException">The information could not be read.</exception>
    public TrophyProgress GetProgress()
    {
        SceNpTrophy2GameDetails details = default;
        SceNpTrophy2GameData data = default;
        SceResult.ThrowIfFailed(
            NpTrophy2.sceNpTrophy2GetGameInfo(_context, _handle, &details, &data),
            nameof(NpTrophy2.sceNpTrophy2GetGameInfo));

        return new TrophyProgress(
            ReadUtf8(details.Title, 128),
            (int)details.NumTrophies,
            (int)data.UnlockedTrophies,
            (int)data.ProgressPercentage);
    }

    /// <summary>Reads every trophy in the set with the player's unlock state.</summary>
    /// <exception cref="ProsperoException">The trophies could not be read.</exception>
    public List<TrophyInfo> GetTrophies()
    {
        SceNpTrophy2GameDetails game = default;
        SceNpTrophy2GameData gameData = default;
        SceResult.ThrowIfFailed(
            NpTrophy2.sceNpTrophy2GetGameInfo(_context, _handle, &game, &gameData),
            nameof(NpTrophy2.sceNpTrophy2GetGameInfo));

        int total = (int)game.NumTrophies;
        var trophies = new List<TrophyInfo>(total);
        if (total == 0)
            return trophies;

        var details = new SceNpTrophy2Details[total];
        var data = new SceNpTrophy2Data[total];
        uint count;
        fixed (SceNpTrophy2Details* detailsPtr = details)
        fixed (SceNpTrophy2Data* dataPtr = data)
        {
            SceResult.ThrowIfFailed(
                NpTrophy2.sceNpTrophy2GetTrophyInfoArray(_context, _handle, 0, (uint)total, detailsPtr, dataPtr, &count),
                nameof(NpTrophy2.sceNpTrophy2GetTrophyInfoArray));

            for (int i = 0; i < (int)count; i++)
            {
                trophies.Add(new TrophyInfo(
                    detailsPtr[i].TrophyId,
                    (SceNpTrophy2Grade)detailsPtr[i].TrophyGrade,
                    dataPtr[i].Unlocked != 0,
                    detailsPtr[i].Hidden != 0,
                    ReadUtf8(detailsPtr[i].Name, 128),
                    ReadUtf8(detailsPtr[i].Description, 1024)));
            }
        }

        return trophies;
    }

    /// <summary>Shows the system trophy list for this set.</summary>
    /// <exception cref="ProsperoException">The list could not be shown.</exception>
    public void ShowList() =>
        SceResult.ThrowIfFailed(NpTrophy2.sceNpTrophy2ShowTrophyList(_context), nameof(NpTrophy2.sceNpTrophy2ShowTrophyList));

    /// <summary>Destroys the handle and context.</summary>
    public void Dispose()
    {
        if (_handle >= 0)
        {
            NpTrophy2.sceNpTrophy2DestroyHandle(_handle);
            _handle = -1;
        }

        if (_context >= 0)
        {
            NpTrophy2.sceNpTrophy2DestroyContext(_context);
            _context = -1;
        }

        GC.SuppressFinalize(this);
    }

    ~TrophySet() => Dispose();

    private static string ReadUtf8(byte* buffer, int maxLength)
    {
        int length = 0;
        while (length < maxLength && buffer[length] != 0)
            length++;
        return Encoding.UTF8.GetString(buffer, length);
    }
}
