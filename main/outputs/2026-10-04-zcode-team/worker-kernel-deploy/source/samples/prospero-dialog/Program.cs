// A SharpProspero system-dialog sampler. A menu opens a message box, the on-screen keyboard, and the
// system error dialog; each dialog is pumped to completion in the frame loop. Press Options to exit.

using SharpProspero.Application;
using SharpProspero.Graphics;
using SharpProspero.Interop.Pad;
using SharpProspero.Platform;
using MsgDialogButtonId = SharpProspero.Interop.Dialog.MsgDialogButtonId;
using ImeDialogEndStatus = SharpProspero.Interop.Dialog.ImeDialogEndStatus;

namespace SampleApp;

internal sealed class Game : ProsperoApp
{
    private static readonly Color Background = Color.FromRgb(0x10, 0x14, 0x1A);
    private static readonly Color Muted = Color.FromRgb(0x8A, 0x94, 0xA0);
    private static readonly Color Accent = Color.FromRgb(0x4A, 0xA8, 0xFF);

    private static readonly string[] Items =
    [
        "Message dialog (Yes / No)",
        "Text input keyboard",
        "System error dialog",
    ];

    private int _selected;
    private string _status = "Cross opens the selection. D-pad moves, Options exits.";

    protected override void OnFrame(FrameContext context)
    {
        Surface surface = context.Surface;
        surface.Clear(Background);
        surface.DrawTextCentered("APP_TITLE", 120, 6, Color.White);

        for (int i = 0; i < Items.Length; i++)
        {
            bool active = i == _selected;
            surface.DrawText((active ? "> " : "  ") + Items[i], 260, 320 + i * 70, 4, active ? Accent : Color.White);
        }

        surface.DrawTextCentered(_status, 780, 3, Muted);

        if (context.Pressed(ScePadButton.Up))
            _selected = (_selected + Items.Length - 1) % Items.Length;
        if (context.Pressed(ScePadButton.Down))
            _selected = (_selected + 1) % Items.Length;
        if (context.Pressed(ScePadButton.Cross))
            _status = OpenSelected();
        if (context.Pressed(ScePadButton.Options))
            context.RequestExit();
    }

    private string OpenSelected() => _selected switch
    {
        0 => AskQuestion(),
        1 => ReadText(),
        _ => ShowError(),
    };

    // A message with Yes and No. Pump it once per frame, presenting so the overlay stays live, then read
    // which button the user pressed.
    private string AskQuestion()
    {
        using var dialog = MessageDialog.ShowMessage("Apply the changes?", MessageDialogButtons.YesNo);
        while (dialog.Update() == MessageDialogState.Running)
            Display.Present();
        return dialog.ChosenButton == MsgDialogButtonId.Ok ? "Message dialog: Yes" : "Message dialog: No";
    }

    // The on-screen keyboard hands back the typed text once the user accepts it.
    private string ReadText()
    {
        using var input = TextInputDialog.Open("Enter a name", maxLength: 64);
        while (input.Update() == TextInputState.Running)
            Display.Present();
        return input.EndStatus == ImeDialogEndStatus.Ok ? "You typed: " + input.Text : "Text input cancelled";
    }

    // The system error box shows the console's own wording for a code.
    private string ShowError()
    {
        using var dialog = ErrorDialog.Show(unchecked((int)0x80B4010A));
        while (dialog.Update() != ErrorDialogState.Closed)
            Display.Present();
        return "Error dialog closed";
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
