// A SharpProspero interface application. Builds a screen from widgets and drives it with the
// controller; press Circle to exit. Replace the widgets in BuildScreen with your own.

using SharpProspero.Application;
using SharpProspero.Graphics;
using SharpProspero.Ui;

namespace SampleApp;

internal sealed class Game : ProsperoApp
{
    private UiScreen? _screen;
    private bool _exit;

    private UiScreen Screen => _screen ??= BuildScreen();

    private UiScreen BuildScreen()
    {
        var root = new StackPanel()
            .Add(new Label("APP_TITLE") { Scale = 4 })
            .Add(new Label("D-pad moves, Cross selects, Circle exits.") { TextColor = Color.FromRgb(0x8A, 0x94, 0xA0) })
            .Add(new Carousel(["Forest", "Desert", "Ocean", "Volcano"], activated: i => { /* pick a level */ }))
            .Add(new Button("Play", () => { /* start something */ }))
            .Add(new Checkbox("Fullscreen", true))
            .Add(new Slider("Volume", 0, 100, 80, step: 5))
            .Add(new Stepper("Lives", 3, 1, 9, format: n => "x" + n))
            .Add(new OptionSelector("Difficulty", ["Easy", "Normal", "Hard"], selected: 1));

        return new UiScreen(root) { Cancelled = () => _exit = true };
    }

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
