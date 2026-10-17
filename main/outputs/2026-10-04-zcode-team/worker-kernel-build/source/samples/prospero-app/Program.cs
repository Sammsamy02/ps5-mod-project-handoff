// A SharpProspero application. Draw each frame in OnFrame; press Options to exit.

using SharpProspero.Application;
using SharpProspero.Graphics;
using SharpProspero.Interop.Pad;

namespace SampleApp;

internal sealed class Game : ProsperoApp
{
    private static readonly Color Background = Color.FromRgb(0x10, 0x14, 0x1A);
    private static readonly Color Text = Color.White;

    protected override void OnFrame(FrameContext context)
    {
        Surface surface = context.Surface;
        surface.Clear(Background);
        surface.DrawTextCentered("APP_TITLE", 480, 6, Text);
        surface.DrawTextCentered("Built with SharpProspero", 620, 3, Color.FromRgb(0x8A, 0x94, 0xA0));

        if (context.Input.IsPressed(ScePadButton.Options))
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
