// A SharpProspero save-data browser. Lists the signed-in user's saves, mounts the selected one, and
// keeps a counter in a file inside it: D-pad selects, Cross adds one, Options exits.

using System.Collections.Generic;
using System.Globalization;
using SharpProspero.Application;
using SharpProspero.Graphics;
using SharpProspero.Interop;
using SharpProspero.Interop.Pad;
using SharpProspero.Platform;
using SharpProspero.Storage;

namespace SampleApp;

internal sealed class Game : ProsperoApp
{
    private const string CounterFile = "counter.txt";

    private static readonly Color Background = Color.FromRgb(0x10, 0x14, 0x1A);
    private static readonly Color Text = Color.White;
    private static readonly Color Dim = Color.FromRgb(0x8A, 0x94, 0xA0);
    private static readonly Color Highlight = Color.FromRgb(0x4C, 0xC2, 0xFF);

    private SaveDataManager? _saves;
    private IReadOnlyList<SaveDataInfo> _list = [];
    private int _selected;
    private long _counter = -1;
    private string _status = "Opening save data...";

    protected override void OnFrame(FrameContext context)
    {
        EnsureLoaded();

        Surface surface = context.Surface;
        surface.Clear(Background);
        surface.DrawTextCentered("APP_TITLE", 80, 5, Text);
        surface.DrawTextCentered("D-pad selects a save, Cross adds one, Options exits.", 180, 2, Dim);

        if (_list.Count == 0)
        {
            surface.DrawTextCentered("No saves to browse.", 400, 3, Dim);
        }
        else
        {
            for (int i = 0; i < _list.Count; i++)
            {
                SaveDataInfo save = _list[i];
                bool active = i == _selected;
                string title = string.IsNullOrEmpty(save.Title) ? save.DirName : save.Title;
                surface.DrawText((active ? "> " : "  ") + save.DirName + "   " + title, 160, 300 + i * 44, 3,
                    active ? Highlight : Text);
            }

            if (_counter >= 0)
                surface.DrawText("Counter: " + _counter.ToString(CultureInfo.InvariantCulture), 160, 820, 3, Text);
        }

        surface.DrawText(_status, 160, 960, 2, Dim);

        HandleInput(context);
    }

    private void EnsureLoaded()
    {
        if (_saves is not null)
            return;

        try
        {
            _saves = SaveDataManager.Open();
            _list = _saves.Enumerate();
            if (_list.Count == 0)
                _status = "This user has no saves.";
            else
                LoadCounter();
        }
        catch (ProsperoException e)
        {
            _status = "Could not open save data: " + e.Message;
        }
    }

    // Mounts the selected save read-only and reads its counter, treating a missing file as zero.
    private void LoadCounter()
    {
        if (_saves is null || _list.Count == 0)
            return;

        try
        {
            using MountedSave mounted = _saves.Mount(_list[_selected].DirName);
            _counter = ReadCounter(mounted.MountPoint);
            _status = "Ready.";
        }
        catch (ProsperoException e)
        {
            _counter = -1;
            _status = "Mount failed: " + e.Message;
        }
    }

    // Mounts the selected save for writing, bumps the counter, and lets the using block commit it.
    private void Increment()
    {
        if (_saves is null || _list.Count == 0)
            return;

        try
        {
            long value;
            using (MountedSave mounted = _saves.Mount(_list[_selected].DirName, readOnly: false))
            {
                value = ReadCounter(mounted.MountPoint) + 1;
                FileSystem.WriteAllText(mounted.MountPoint + "/" + CounterFile,
                    value.ToString(CultureInfo.InvariantCulture));
            }

            _counter = value;
            _status = "Saved.";
        }
        catch (ProsperoException e)
        {
            _status = "Write failed: " + e.Message;
        }
    }

    private static long ReadCounter(string mountPoint)
    {
        string path = mountPoint + "/" + CounterFile;
        if (FileSystem.Exists(path) &&
            long.TryParse(FileSystem.ReadAllText(path).Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture, out long value))
            return value;
        return 0;
    }

    private void HandleInput(FrameContext context)
    {
        if (context.Pressed(ScePadButton.Options))
        {
            context.RequestExit();
            return;
        }

        if (_list.Count == 0)
            return;

        if (context.Pressed(ScePadButton.Up))
        {
            _selected = (_selected - 1 + _list.Count) % _list.Count;
            LoadCounter();
        }
        else if (context.Pressed(ScePadButton.Down))
        {
            _selected = (_selected + 1) % _list.Count;
            LoadCounter();
        }
        else if (context.Pressed(ScePadButton.Cross))
        {
            Increment();
        }
    }

    protected override void OnUnload() => _saves?.Dispose();
}

internal static class Program
{
    private static void Main()
    {
        using (var app = new Game())
            app.Run();

        // Returning from here is reported to the platform as a fault and the user is shown the
        // box that says the application closed unexpectedly, even when everything went as
        // intended. The process is ended through the C library instead.
        ProcessExit.Exit();
    }
}
