// A SharpProspero file manager. Browses the on-device file system with the controller: D-pad moves,
// Cross opens a folder, Circle goes up a level (or exits at the root). Point StartPath at a folder the
// application can read (the package mounts at /app0; save data and other mounts have their own paths).

using SharpProspero.Application;
using SharpProspero.Graphics;
using SharpProspero.Storage;
using SharpProspero.Ui;
using System;
using System.Collections.Generic;

namespace SampleApp;

internal sealed class Game : ProsperoApp
{
    // The folder the browser opens at. /app0 is the read-only package mount.
    private const string StartPath = "/app0";

    private readonly ListView _list = new() { VisibleRows = 12 };
    private readonly List<string> _paths = []; // the full path for each visible row, parallel to the list
    private string _path = StartPath;
    private UiScreen? _screen;
    private Label? _pathLabel;
    private bool _exit;

    private UiScreen Screen => _screen ??= BuildScreen();

    private UiScreen BuildScreen()
    {
        _list.Activated = OnActivate;
        _pathLabel = new Label(_path) { TextColor = Color.FromRgb(0x8A, 0x94, 0xA0) };
        Refresh();
        var root = new StackPanel()
            .Add(new Label("APP_TITLE") { Scale = 3 })
            .Add(_pathLabel)
            .Add(_list);
        // Circle goes up a level; at the root it exits.
        return new UiScreen(root) { Cancelled = GoUp };
    }

    // Fills the list from the current folder: a way up first, then folders, then files.
    private void Refresh()
    {
        _list.Clear();
        _paths.Clear();
        if (_pathLabel is not null)
            _pathLabel.Text = _path;
        if (_path != "/")
        {
            _list.Add(".. (up)");
            _paths.Add(Parent(_path));
        }
        try
        {
            var entries = new List<DirectoryEntry>(FileSystem.EnumerateDirectory(_path));
            entries.Sort((a, b) => a.IsDirectory != b.IsDirectory ? (a.IsDirectory ? -1 : 1) : string.CompareOrdinal(a.Name, b.Name));
            foreach (DirectoryEntry entry in entries)
            {
                _list.Add(entry.IsDirectory ? entry.Name + "/" : entry.Name);
                _paths.Add(Combine(_path, entry.Name));
            }
        }
        catch (Exception)
        {
            _list.Add("(cannot read this folder)");
            _paths.Add(_path);
        }
        _list.SelectedIndex = 0;
    }

    private void OnActivate(int index)
    {
        if (index < 0 || index >= _paths.Count)
            return;
        // The way-up row and any folder row open; a file selection is left for the application to handle.
        bool isUp = index == 0 && _path != "/";
        if (isUp || (index < _list.Items.Count && _list.Items[index].EndsWith('/')))
        {
            _path = _paths[index];
            Refresh();
        }
    }

    private void GoUp()
    {
        if (_path == "/" || _path == StartPath)
        {
            _exit = true;
            return;
        }
        _path = Parent(_path);
        Refresh();
    }

    private static string Parent(string path)
    {
        int slash = path.TrimEnd('/').LastIndexOf('/');
        return slash <= 0 ? "/" : path[..slash];
    }

    private static string Combine(string dir, string name)
        => dir.EndsWith('/') ? dir + name : dir + "/" + name;

    protected override void OnFrame(FrameContext context)
    {
        context.Surface.Clear(Screen.Theme.Background);
        Screen.Update(UiInput.From(context.Input, context.PreviousInput));
        Screen.Render(context.Surface, margin: 60);

        if (_exit)
            context.RequestExit();
    }
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
