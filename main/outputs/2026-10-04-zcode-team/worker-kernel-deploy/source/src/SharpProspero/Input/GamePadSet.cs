// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Pad;
using SharpProspero.Interop.UserService;
using SharpProspero.Platform;
using System;
using System.Collections;
using System.Collections.Generic;

namespace SharpProspero.Input;

/// <summary>One player's controller: whose it is, the handle, and the last sample read from it.</summary>
public sealed class PlayerPad
{
    internal PlayerPad(int userId, GamePad pad)
    {
        UserId = userId;
        Pad = pad;
        State = GamePadState.Neutral;
    }

    /// <summary>The signed-in user this controller belongs to.</summary>
    public int UserId { get; }

    /// <summary>The controller itself, for vibration, the light bar and a direct read.</summary>
    public GamePad Pad { get; }

    /// <summary>What <see cref="GamePadSet.ReadAll"/> last read from it.</summary>
    public GamePadState State { get; internal set; }

    /// <summary>The player's display name, read once when the controller was opened.</summary>
    public string UserName { get; internal set; } = string.Empty;
}

/// <summary>
/// Every controller the machine has, one for each signed-in user, kept in step with players coming and
/// going. A single-player application opens one <see cref="GamePad"/>; anything with a second player -
/// a couch multiplayer game, an emulator front end with two joypads, a media application that any
/// household member may pick up - needs this.
/// </summary>
/// <remarks>
/// <para>
/// A controller belongs to a signed-in user, and the controller service routes each user's samples to
/// the handle opened for that user. There is no separate index for a second controller: the way to
/// reach every device is one handle per signed-in user, which is what this opens.
/// </para>
/// <para>
/// Call <see cref="Refresh"/> once a frame (or once a second - it is cheap but not free) to pick up a
/// player who has just signed in and to drop one who has signed out. Call <see cref="ReadAll"/> once a
/// frame to sample every controller.
/// </para>
/// </remarks>
/// <example>
/// <code>
/// using var pads = GamePadSet.OpenForSignedInUsers();
/// // each frame:
/// pads.Refresh();
/// pads.ReadAll();
/// foreach (PlayerPad player in pads)
///     if (player.State.IsPressed(ScePadButton.Cross))
///         Fire(player.UserId);
/// </code>
/// </example>
public sealed unsafe class GamePadSet : IReadOnlyCollection<PlayerPad>, IDisposable
{
    private readonly List<PlayerPad> _players = [];
    private readonly bool _readNames;
    private bool _disposed;

    private GamePadSet(bool readNames) => _readNames = readNames;

    /// <summary>
    /// Starts the controller service and opens a handle for every user signed in now.
    /// </summary>
    /// <param name="readUserNames">
    /// Whether to read each player's display name as their controller is opened. Turn this off where
    /// names are never shown, so signing in costs one call instead of two.
    /// </param>
    /// <exception cref="ProsperoException">
    /// The controller service could not be started, or the signed-in users could not be read.
    /// </exception>
    public static GamePadSet OpenForSignedInUsers(bool readUserNames = true)
    {
        SceResult.ThrowIfFailed(Pad.scePadInit(), nameof(Pad.scePadInit));
        var set = new GamePadSet(readUserNames);
        set.Refresh();
        return set;
    }

    /// <summary>How many controllers are open.</summary>
    public int Count => _players.Count;

    /// <summary>The open controllers, in the order their users were found.</summary>
    public IReadOnlyList<PlayerPad> Players => _players;

    /// <summary>
    /// The controller for <paramref name="userId"/>, or null when that user has none open.
    /// </summary>
    public PlayerPad? ForUser(int userId)
    {
        foreach (PlayerPad player in _players)
            if (player.UserId == userId)
                return player;
        return null;
    }

    /// <summary>
    /// Brings the set into line with who is signed in: opens a controller for a user who has arrived and
    /// closes one whose user has gone.
    /// </summary>
    /// <returns>True when the set changed, so a caller can rebuild a player list only when it must.</returns>
    /// <exception cref="ProsperoException">The signed-in users could not be read.</exception>
    public bool Refresh()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        // The service keeps a list of who has come and gone that grows for as long as nothing drains it.
        // Draining it here is what keeps that list from growing without end, whether or not the answer
        // below has changed; the list of who is signed in is what the set is actually built from, because
        // it is right even if an entry was missed.
        DrainUserEvents();

        int* ids = stackalloc int[UserService.MaxLoginUsers];
        SceResult.ThrowIfFailed(
            UserService.sceUserServiceGetLoginUserIdList(ids),
            nameof(UserService.sceUserServiceGetLoginUserIdList));

        bool changed = false;

        // Close the controllers of users who are no longer signed in.
        for (int i = _players.Count - 1; i >= 0; i--)
        {
            bool stillSignedIn = false;
            for (int slot = 0; slot < UserService.MaxLoginUsers; slot++)
                if (ids[slot] == _players[i].UserId)
                    stillSignedIn = true;
            if (stillSignedIn)
                continue;
            _players[i].Pad.Dispose();
            _players.RemoveAt(i);
            changed = true;
        }

        // Open a controller for each user who has none.
        for (int slot = 0; slot < UserService.MaxLoginUsers; slot++)
        {
            int userId = ids[slot];
            if (userId == SceUser.Invalid || ForUser(userId) is not null)
                continue;
            PlayerPad? opened = OpenFor(userId);
            if (opened is null)
                continue;
            _players.Add(opened);
            changed = true;
        }

        return changed;
    }

    /// <summary>
    /// Reads a sample from every open controller into <see cref="PlayerPad.State"/>. A controller that
    /// is asleep or out of charge reads as a resting sample rather than failing the whole call.
    /// </summary>
    public void ReadAll()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        foreach (PlayerPad player in _players)
            player.State = player.Pad.Read();
    }

    /// <summary>Walks the open controllers.</summary>
    public IEnumerator<PlayerPad> GetEnumerator() => _players.GetEnumerator();

    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

    /// <summary>Closes every controller the set opened.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        foreach (PlayerPad player in _players)
            player.Pad.Dispose();
        _players.Clear();
    }

    // Opens a controller for one user, or returns null when the service will not give one. A user can be
    // signed in with no controller paired to them, which is refused rather than exceptional.
    private PlayerPad? OpenFor(int userId)
    {
        int handle = Pad.scePadOpen(userId, Pad.PortTypeStandard, 0, null);
        bool owned = true;
        if (handle == Pad.ErrorAlreadyOpened)
        {
            // Something else in the module already holds this user's controller - the single-pad API,
            // most likely. Reach the open handle instead of failing, and leave closing it to whoever
            // opened it.
            handle = Pad.scePadGetHandle(userId, Pad.PortTypeStandard, 0);
            owned = false;
        }
        if (SceResult.Failed(handle))
            return null;

        var player = new PlayerPad(userId, GamePad.FromHandle(handle, owned));
        if (_readNames)
        {
            // A name is a nicety; a user whose name cannot be read still gets a controller.
            try { player.UserName = Users.GetUserName(userId); }
            catch (ProsperoException) { }
        }
        return player;
    }

    // The most entries one drain takes. The list is emptied to stop it growing, not to act on it, so a
    // service that kept answering would otherwise hold the frame here; the rest are taken next time.
    private const int MaxEventsPerDrain = 32;

    // Empties the service's sign-in list. The entries themselves are not acted on: the list of who is
    // signed in settles the set, and an entry can be missed if two arrive between refreshes.
    private static void DrainUserEvents()
    {
        SceUserServiceEvent entry;
        for (int i = 0; i < MaxEventsPerDrain; i++)
            if (SceResult.Failed(UserService.sceUserServiceGetEvent(&entry)))
                return;
    }
}
