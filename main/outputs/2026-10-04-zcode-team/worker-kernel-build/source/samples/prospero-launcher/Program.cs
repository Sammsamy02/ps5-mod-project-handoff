// A SharpProspero app launcher. Browse apps with the d-pad and launch the one in the middle with Cross;
// press Circle to exit. Replace the Apps list with your own titles and their nine-character title ids.

using SharpProspero.Application;
using SharpProspero.Graphics;
using SharpProspero.Platform;
using SharpProspero.Ui;

namespace SampleApp;

internal sealed class Game : ProsperoApp
{
    // The apps to offer: a display name and its title id. Replace these with your own.
    private static readonly (string Name, string TitleId)[] Apps =
    [
        ("First App", "PPSA99001"),
        ("Second App", "PPSA99002"),
        ("Third App", "PPSA99003"),
    ];

    private UiScreen? _screen;
    private bool _exit;
    private string _status = "";

    private UiScreen Screen => _screen ??= BuildScreen();

    private UiScreen BuildScreen()
    {
        var names = new string[Apps.Length];
        for (int i = 0; i < Apps.Length; i++)
            names[i] = Apps[i].Name;

        var root = new StackPanel()
            .Add(new Label("APP_TITLE") { Scale = 4 })
            .Add(new Label("D-pad browses, Cross launches, Circle exits.") { TextColor = Color.FromRgb(0x8A, 0x94, 0xA0) })
            .Add(new Carousel(names, activated: Launch));

        return new UiScreen(root) { Cancelled = () => _exit = true };
    }

    private void Launch(int index)
    {
        try
        {
            AppLauncher.Launch(Apps[index].TitleId);
            _status = "Launching " + Apps[index].Name + "...";
        }
        catch (System.Exception e)
        {
            _status = "Could not launch: " + e.Message;
        }
    }

    protected override void OnFrame(FrameContext context)
    {
        context.Surface.Clear(Screen.Theme.Background);
        Screen.Update(UiInput.From(context.Input, context.PreviousInput));
        Screen.Render(context.Surface, margin: 60);
        if (_status.Length > 0)
            context.Surface.DrawTextCentered(_status, 960, 2, Color.FromRgb(0x8A, 0x94, 0xA0));

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
