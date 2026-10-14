// A SharpProspero toolbox application: a starting point for a system utility. It shows a few of the
// SDK's tool surfaces (a checksum, the console name, the network status) on screen. Press Options to
// exit. Swap the lines in Gather for whatever your tool does.

using System;
using System.Collections.Generic;
using System.Text;
using SharpProspero.Application;
using SharpProspero.Graphics;
using SharpProspero.Interop.Pad;
using SharpProspero.Platform;
using SharpProspero.Security;

namespace SampleApp;

internal sealed class Tool : ProsperoApp
{
    private List<string>? _lines;

    private List<string> Lines => _lines ??= Gather();

    private static List<string> Gather()
    {
        var lines = new List<string>();

        // A checksum, computed with no system module.
        string checksum = Sha256.HashHex(Encoding.ASCII.GetBytes("hello"));
        lines.Add("SHA-256(\"hello\") = " + checksum[..16] + "...");

        // The console's name, read from the system settings.
        lines.Add("Console: " + Try(() => SystemParameters.SystemName));

        // The network status. Opening the status service needs no socket pool.
        lines.Add("Network: " + Try(() =>
        {
            using var net = NetworkInfo.Open();
            return net.IsConnected ? net.IpAddress : "offline";
        }));

        return lines;
    }

    private static string Try(Func<string> read)
    {
        try { return read(); }
        catch (Exception e) { return "unavailable (" + e.Message + ")"; }
    }

    protected override void OnFrame(FrameContext context)
    {
        Surface surface = context.Surface;
        surface.Clear(Color.FromRgb(0x0E, 0x12, 0x18));
        surface.DrawText("APP_TITLE", 80, 80, 4, Color.White);

        int y = 200;
        foreach (string line in Lines)
        {
            surface.DrawText(line, 80, y, 2, Color.FromRgb(0xC8, 0xD0, 0xDC));
            y += 40;
        }

        surface.DrawText("Press Options to exit", 80, y + 40, 2, Color.FromRgb(0x8A, 0x94, 0xA0));

        if (context.Pressed(ScePadButton.Options))
            context.RequestExit();
    }
}

internal static class Program
{
    private static void Main()
    {
        using (var app = new Tool())
            app.Run();

        // Returning from here is reported to the platform as a fault and the user is shown the
        // box that says the application closed unexpectedly, even when everything went as
        // intended. The process is ended through the C library instead.
        ProcessExit.Exit();
    }
}
