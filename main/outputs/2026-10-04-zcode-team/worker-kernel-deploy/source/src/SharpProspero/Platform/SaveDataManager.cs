// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.SaveData;
using SharpProspero.Interop.UserService;
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using Native = SharpProspero.Interop.SaveData.SaveData;

namespace SharpProspero.Platform;

/// <summary>One save, as a save-data manager lists it.</summary>
/// <param name="DirName">The save's directory name, its identity within the title.</param>
/// <param name="Title">The save's title.</param>
/// <param name="SubTitle">The save's subtitle.</param>
/// <param name="Detail">The save's detail text.</param>
/// <param name="UserParam">An integer the title stored with the save.</param>
/// <param name="ModifiedTime">When the save was last written.</param>
public readonly record struct SaveDataInfo(
    string DirName, string Title, string SubTitle, string Detail, uint UserParam, DateTimeOffset ModifiedTime);

/// <summary>A mounted save. The files live under <see cref="MountPoint"/>; dispose to unmount.</summary>
public sealed unsafe class MountedSave : IDisposable
{
    private SceSaveDataMountPoint _mountPoint;
    private bool _disposed;

    internal MountedSave(SceSaveDataMountPoint mountPoint, string path)
    {
        _mountPoint = mountPoint;
        MountPoint = path;
    }

    /// <summary>The path the save's files live under, for example <c>/savedata0</c>.</summary>
    public string MountPoint { get; }

    /// <summary>Unmounts the save.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        fixed (SceSaveDataMountPoint* mp = &_mountPoint)
            Native.sceSaveDataUmount2((uint)SaveDataUmountMode.Commit, mp);
    }
}

/// <summary>
/// Reads and manages the console's save data for the running user: list the saves, read their
/// details, mount one to reach its files, or delete one. This is the surface a save-data manager or a
/// backup tool is built on.
/// </summary>
/// <example>
/// <code>
/// using var saves = SaveDataManager.Open();
/// foreach (SaveDataInfo save in saves.Enumerate("CUSA00000"))
///     Show(save.Title, save.ModifiedTime);
/// </code>
/// </example>
public sealed unsafe class SaveDataManager : IDisposable
{
    private readonly int _userId;
    private bool _disposed;

    private SaveDataManager(int userId) => _userId = userId;

    /// <summary>
    /// Starts the save-data service for <paramref name="userId"/> (the signed-in user by default).
    /// </summary>
    /// <exception cref="ProsperoException">The service could not be started.</exception>
    public static SaveDataManager Open(int userId = int.MinValue)
    {
        int user = userId;
        if (user == int.MinValue)
        {
            int initial;
            SceResult.ThrowIfFailed(UserService.sceUserServiceGetInitialUser(&initial),
                nameof(UserService.sceUserServiceGetInitialUser));
            user = initial;
        }

        SceResult.ThrowIfFailed(Native.sceSaveDataInitialize3(null), nameof(Native.sceSaveDataInitialize3));
        return new SaveDataManager(user);
    }

    /// <summary>
    /// Lists the saves. Without a <paramref name="titleId"/> it lists the running application's own,
    /// not every save the user has: leaving it out does not widen the search, it is filled in with the
    /// calling title. Another title's saves cannot be listed through here at all.
    /// </summary>
    /// <exception cref="ProsperoException">The search failed.</exception>
    public IReadOnlyList<SaveDataInfo> Enumerate(string? titleId = null)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);

        SceSaveDataTitleId tid = default;
        bool hasTitle = !string.IsNullOrEmpty(titleId);
        if (hasTitle)
            WriteFixed(tid.Data, 10, titleId!);

        SceSaveDataDirNameSearchCond cond = default;
        cond.UserId = _userId;
        cond.Key = SaveDataSortKey.DirName;
        cond.Order = SaveDataSortOrder.Ascending;
        cond.TitleId = hasTitle ? &tid : null;

        // First pass counts the matches; the second reads that many directories and their parameters.
        SceSaveDataDirNameSearchResult count = default;
        SceResult.ThrowIfFailed(Native.sceSaveDataDirNameSearch(&cond, &count), nameof(Native.sceSaveDataDirNameSearch));
        int n = (int)count.HitNum;
        if (n == 0)
            return [];

        var dirNames = (SceSaveDataDirName*)NativeMemory.AllocZeroed((nuint)n, (nuint)sizeof(SceSaveDataDirName));
        var parms = (SceSaveDataParam*)NativeMemory.AllocZeroed((nuint)n, (nuint)sizeof(SceSaveDataParam));
        try
        {
            SceSaveDataDirNameSearchResult result = default;
            result.DirNames = dirNames;
            result.Params = parms;
            result.DirNamesNum = (uint)n;
            SceResult.ThrowIfFailed(Native.sceSaveDataDirNameSearch(&cond, &result), nameof(Native.sceSaveDataDirNameSearch));

            int set = (int)result.SetNum;
            var list = new List<SaveDataInfo>(set);
            for (int i = 0; i < set; i++)
            {
                SceSaveDataParam* p = &parms[i];
                list.Add(new SaveDataInfo(
                    ReadFixed(dirNames[i].Data, 32),
                    ReadFixed(p->Title, 128),
                    ReadFixed(p->SubTitle, 128),
                    ReadFixed(p->Detail, 1024),
                    p->UserParam,
                    DateTimeOffset.FromUnixTimeSeconds(p->Mtime)));
            }
            return list;
        }
        finally
        {
            NativeMemory.Free(dirNames);
            NativeMemory.Free(parms);
        }
    }

    /// <summary>
    /// Mounts the save named <paramref name="dirName"/> so its files can be read under the returned
    /// mount point. Read-only by default.
    /// </summary>
    /// <exception cref="ProsperoException">The save could not be mounted.</exception>
    public MountedSave Mount(string dirName, bool readOnly = true)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentException.ThrowIfNullOrEmpty(dirName);

        SceSaveDataDirName dir = default;
        WriteFixed(dir.Data, 32, dirName);

        SceSaveDataMount3 mount = default;
        mount.UserId = _userId;
        mount.DirName = &dir;
        mount.MountMode = readOnly ? SaveDataMountMode.ReadOnly : SaveDataMountMode.ReadWrite;

        SceSaveDataMountResult result = default;
        SceResult.ThrowIfFailed(Native.sceSaveDataMount3(&mount, &result), nameof(Native.sceSaveDataMount3));
        return new MountedSave(result.MountPoint, ReadFixed(result.MountPoint.Data, 16));
    }

    /// <summary>Deletes the save named <paramref name="dirName"/>.</summary>
    /// <exception cref="ProsperoException">The save could not be deleted.</exception>
    public void Delete(string dirName)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentException.ThrowIfNullOrEmpty(dirName);

        SceSaveDataDirName dir = default;
        WriteFixed(dir.Data, 32, dirName);

        SceSaveDataDelete del = default;
        del.UserId = _userId;
        del.DirName = &dir;
        SceResult.ThrowIfFailed(Native.sceSaveDataDelete(&del), nameof(Native.sceSaveDataDelete));
    }

    /// <summary>Stops the save-data service.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        Native.sceSaveDataTerminate();
    }

    private static void WriteFixed(byte* destination, int capacity, string value)
    {
        int n = Encoding.UTF8.GetBytes(value, new Span<byte>(destination, capacity - 1));
        destination[n] = 0;
    }

    private static string ReadFixed(byte* source, int capacity)
    {
        int length = 0;
        while (length < capacity && source[length] != 0)
            length++;
        return Encoding.UTF8.GetString(source, length);
    }
}
