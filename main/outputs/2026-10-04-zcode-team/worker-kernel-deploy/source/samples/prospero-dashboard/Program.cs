// A SharpProspero system dashboard. Four tabs of live console facts, rebuilt from the platform and
// memory services each frame; Left and Right change tab, Circle exits.

using SharpProspero.Application;
using SharpProspero.Graphics;
using SharpProspero.Interop;
using SharpProspero.Interop.Net;
using SharpProspero.Memory;
using SharpProspero.Platform;
using SharpProspero.Ui;
using System;
using System.Collections.Generic;

namespace SampleApp;

internal sealed class Game : ProsperoApp
{
    private static readonly Color Muted = Color.FromRgb(0x8A, 0x94, 0xA0);

    private UiScreen? _screen;
    private NetworkInfo? _net;
    private bool _exit;

    // Console facts.
    private readonly KeyValueRow _firmware = new("Firmware");
    private readonly KeyValueRow _sdkCeiling = new("Allowed SDK");
    private readonly KeyValueRow _consoleId = new("Console ID");
    private readonly KeyValueRow _cores = new("CPU cores");
    private readonly KeyValueRow _systemName = new("System name");
    private readonly KeyValueRow _language = new("Language");
    private readonly KeyValueRow _timeZone = new("UTC offset");

    // Signed-in users: the launching user, then one row per occupied slot.
    private readonly KeyValueRow _signedInAs = new("Signed in as");
    private readonly KeyValueRow[] _userRows = [new(""), new(""), new(""), new("")];

    // Network connection.
    private readonly KeyValueRow _netState = new("Status");
    private readonly KeyValueRow _netDevice = new("Link");
    private readonly KeyValueRow _ip = new("IP address");
    private readonly KeyValueRow _ssid = new("Network");
    private readonly KeyValueRow _mac = new("MAC address");
    private readonly KeyValueRow _signal = new("Signal");

    // Managed heap.
    private readonly KeyValueRow _heap = new("Heap in use");
    private readonly KeyValueRow _allocated = new("Allocated total");
    private readonly KeyValueRow _limit = new("Ceiling");
    private readonly KeyValueRow _pressure = new("Pressure");
    private readonly KeyValueRow _collections = new("Collections");
    private readonly KeyValueRow _supported = new("Supported build");

    private UiScreen Screen => _screen ??= BuildScreen();

    protected override void OnLoad()
    {
        try { _net = NetworkInfo.Open(); }
        catch (ProsperoException) { _net = null; }
    }

    protected override void OnUnload() => _net?.Dispose();

    private UiScreen BuildScreen()
    {
        var console = new StackPanel()
            .Add(_firmware).Add(_sdkCeiling).Add(_consoleId).Add(_cores)
            .Add(_systemName).Add(_language).Add(_timeZone);

        var users = new StackPanel().Add(_signedInAs);
        foreach (KeyValueRow row in _userRows)
            users.Add(row);

        var network = new StackPanel()
            .Add(_netState).Add(_netDevice).Add(_ip).Add(_ssid).Add(_mac).Add(_signal);

        var memory = new StackPanel()
            .Add(_heap).Add(_allocated).Add(_limit).Add(_pressure).Add(_collections).Add(_supported);

        var tabs = new TabView();
        tabs.Add("Console", console);
        tabs.Add("Users", users);
        tabs.Add("Network", network);
        tabs.Add("Memory", memory);

        var root = new StackPanel()
            .Add(new Label("APP_TITLE") { Scale = 4 })
            .Add(new Label("Left/Right change tab, Circle exits.") { TextColor = Muted })
            .Add(tabs);

        return new UiScreen(root) { Cancelled = () => _exit = true };
    }

    protected override void OnFrame(FrameContext context)
    {
        Refresh();

        context.Surface.Clear(Screen.Theme.Background);
        Screen.Update(UiInput.From(context.Input, context.PreviousInput));
        Screen.Render(context.Surface, margin: 60);

        if (_exit)
            context.RequestExit();
    }

    private void Refresh()
    {
        _firmware.Value = Read(() => SystemInfo.SystemSoftwareVersion);
        _sdkCeiling.Value = Read(() => FirmwareSupport.AllowedSdkVersion.ToString());
        _consoleId.Value = Read(() => SystemInfo.ConsoleId);
        _cores.Value = Read(() => SystemInfo.ProcessorCount.ToString());
        _systemName.Value = Read(() => SystemParameters.SystemName);
        _language.Value = Read(() => SystemParameters.Language.ToString());
        _timeZone.Value = Read(() => FormatOffset(SystemParameters.TimeZoneMinutes));

        _signedInAs.Value = Read(() => Users.InitialUserName);
        IReadOnlyList<UserProfile> profiles = ReadUsers();
        for (int i = 0; i < _userRows.Length; i++)
        {
            bool present = i < profiles.Count;
            _userRows[i].Visible = present;
            if (present)
            {
                _userRows[i].Name = "User " + profiles[i].Id;
                _userRows[i].Value = profiles[i].Name;
            }
        }

        RefreshNetwork();

        HeapSnapshot heap = HeapMonitor.Capture();
        _heap.Value = FormatBytes(heap.HeapSizeBytes);
        _allocated.Value = FormatBytes(heap.TotalAllocatedBytes);
        _limit.Value = heap.HardLimitBytes > 0 ? FormatBytes(heap.HardLimitBytes) : "unset";
        _pressure.Value = (heap.Pressure * 100).ToString("F0") + "%";
        _collections.Value = heap.CollectionCount.ToString();
        _supported.Value = Read(() => FirmwareSupport.IsSupported ? "yes" : "no");
    }

    private void RefreshNetwork()
    {
        if (_net is null)
        {
            _netState.Value = "unavailable";
            _netDevice.Value = _ip.Value = _ssid.Value = _mac.Value = _signal.Value = "";
            return;
        }

        try
        {
            bool connected = _net.IsConnected;
            bool wireless = _net.Device == NetCtlDevice.Wireless;
            _netState.Value = _net.State.ToString();
            _netDevice.Value = connected ? (wireless ? "Wireless" : "Wired") : "";
            _ip.Value = _net.IpAddress;
            _ssid.Value = wireless ? _net.Ssid : "";
            _mac.Value = _net.MacAddress;
            _signal.Value = wireless ? _net.SignalStrength + "%" : "";
        }
        catch (ProsperoException)
        {
            _netState.Value = "unavailable";
        }
    }

    private static string Read(Func<string> read)
    {
        try { return read(); }
        catch (ProsperoException) { return "unavailable"; }
    }

    private static IReadOnlyList<UserProfile> ReadUsers()
    {
        try { return Users.LoggedInUsers; }
        catch (ProsperoException) { return Array.Empty<UserProfile>(); }
    }

    private static string FormatBytes(long bytes) => (bytes / (1024.0 * 1024.0)).ToString("F1") + " MB";

    private static string FormatOffset(int minutes)
    {
        int hours = Math.Abs(minutes) / 60;
        int rest = Math.Abs(minutes) % 60;
        return $"UTC{(minutes < 0 ? '-' : '+')}{hours:D2}:{rest:D2}";
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
