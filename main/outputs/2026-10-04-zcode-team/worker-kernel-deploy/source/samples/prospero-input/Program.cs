// A SharpProspero input tester. Each frame it reads the controller, keyboard and mouse and draws
// their live state; press Options to exit.

using SharpProspero.Application;
using SharpProspero.Graphics;
using SharpProspero.Input;
using SharpProspero.Interop;
using SharpProspero.Interop.Pad;
using System;
using System.Text;

namespace SampleApp;

internal sealed class Game : ProsperoApp
{
    private static readonly Color Background = Color.FromRgb(0x10, 0x14, 0x1A);
    private static readonly Color Muted = Color.FromRgb(0x8A, 0x94, 0xA0);
    private static readonly Color Dim = Color.FromRgb(0x50, 0x5A, 0x66);
    private static readonly Color Panel = Color.FromRgb(0x2A, 0x33, 0x3E);
    private static readonly Color Active = Color.FromRgb(0x4C, 0xC2, 0xFF);

    private static readonly (ScePadButton Button, string Label)[] Buttons =
    [
        (ScePadButton.Cross, "Cross"), (ScePadButton.Circle, "Circle"),
        (ScePadButton.Square, "Square"), (ScePadButton.Triangle, "Triangle"),
        (ScePadButton.L1, "L1"), (ScePadButton.R1, "R1"),
        (ScePadButton.Up, "Up"), (ScePadButton.Down, "Down"),
        (ScePadButton.Left, "Left"), (ScePadButton.Right, "Right"),
        (ScePadButton.Options, "Options"), (ScePadButton.TouchPad, "TouchPad"),
    ];

    private readonly StringBuilder _line = new();
    private Keyboard? _keyboard;
    private Mouse? _mouse;
    private bool _cursorReady;
    private int _cursorX;
    private int _cursorY;

    protected override void OnLoad()
    {
        _keyboard = TryOpen(() => Keyboard.Open());
        _mouse = TryOpen(() => Mouse.Open());
    }

    protected override void OnFrame(FrameContext context)
    {
        Surface surface = context.Surface;
        GamePadState pad = context.Input;

        if (!_cursorReady)
        {
            _cursorX = surface.Width / 2;
            _cursorY = surface.Height / 2;
            _cursorReady = true;
        }

        surface.Clear(Background);
        surface.DrawTextCentered("APP_TITLE", 48, 5, Color.White);
        surface.DrawTextCentered("Press Options to exit", 128, 2, Muted);

        DrawButtons(surface, pad, 80, 200);
        DrawSticks(surface, pad, 80, 380);
        DrawTriggers(surface, pad, 560, 416);
        DrawTouch(surface, pad, 1040, 380);
        DrawKeyboard(surface, 80, 660);
        DrawMouse(surface, 80, 800);

        if (context.Input.IsPressed(ScePadButton.Options))
            context.RequestExit();
    }

    private static void DrawButtons(Surface surface, GamePadState pad, int x, int y)
    {
        surface.DrawText("BUTTONS", x, y, 2, Muted);
        const int cellW = 200, cellH = 40, gap = 12;
        for (int i = 0; i < Buttons.Length; i++)
        {
            (ScePadButton button, string label) = Buttons[i];
            int cx = x + i % 6 * (cellW + gap);
            int cy = y + 36 + i / 6 * (cellH + gap);
            bool held = pad.IsPressed(button);
            if (held)
                surface.FillRect(cx, cy, cellW, cellH, Active);
            else
                surface.DrawRect(cx, cy, cellW, cellH, Panel);
            surface.DrawText(label, cx + 12, cy + 12, 2, held ? Background : Muted);
        }
    }

    private static void DrawSticks(Surface surface, GamePadState pad, int x, int y)
    {
        surface.DrawText("STICKS", x, y, 2, Muted);
        DrawStick(surface, "L", x, y + 36, pad.LeftStick, pad.IsPressed(ScePadButton.L3));
        DrawStick(surface, "R", x + 220, y + 36, pad.RightStick, pad.IsPressed(ScePadButton.R3));
    }

    private static void DrawStick(Surface surface, string label, int x, int y, (float X, float Y) stick, bool pressed)
    {
        const int size = 160, radius = 10;
        surface.DrawRect(x, y, size, size, Panel);
        surface.DrawLine(x + size / 2, y, x + size / 2, y + size, Panel);
        surface.DrawLine(x, y + size / 2, x + size, y + size / 2, Panel);
        int half = size / 2 - radius;
        int dotX = x + size / 2 + (int)(stick.X * half);
        int dotY = y + size / 2 + (int)(stick.Y * half);
        surface.FillCircle(dotX, dotY, radius, pressed ? Active : Color.White);
        surface.DrawText(label, x, y + size + 10, 2, Muted);
    }

    private static void DrawTriggers(Surface surface, GamePadState pad, int x, int y)
    {
        surface.DrawText("TRIGGERS", x, y, 2, Muted);
        DrawBar(surface, "L2", x, y + 36, pad.LeftTrigger);
        DrawBar(surface, "R2", x, y + 80, pad.RightTrigger);
    }

    private static void DrawBar(Surface surface, string label, int x, int y, byte value)
    {
        const int width = 300, height = 28;
        surface.DrawText(label, x, y + 4, 2, Muted);
        int barX = x + 60;
        surface.DrawRect(barX, y, width, height, Panel);
        surface.FillRect(barX, y, value * width / 255, height, Active);
    }

    private static void DrawTouch(Surface surface, GamePadState pad, int x, int y)
    {
        surface.DrawText("TOUCH", x, y, 2, Muted);
        surface.DrawText(pad.TouchCount == 0 ? "no contacts" : pad.TouchCount + " active", x, y + 36, 2, Muted);
        DrawTouchPoint(surface, "1", pad.Touch1, x, y + 76);
        DrawTouchPoint(surface, "2", pad.Touch2, x, y + 112);
    }

    private static void DrawTouchPoint(Surface surface, string slot, TouchPoint point, int x, int y)
    {
        if (!point.IsActive)
        {
            surface.DrawText(slot + ": -", x, y, 2, Dim);
            return;
        }
        surface.DrawText($"{slot}: id {point.Id}  {point.X},{point.Y}", x, y, 2, Color.White);
    }

    private void DrawKeyboard(Surface surface, int x, int y)
    {
        surface.DrawText("KEYBOARD", x, y, 2, Muted);
        if (_keyboard is null)
        {
            surface.DrawText("unavailable", x, y + 36, 2, Dim);
            return;
        }

        KeyboardState keys = _keyboard.Read();
        if (!keys.Connected)
        {
            surface.DrawText("not connected", x, y + 36, 2, Dim);
            return;
        }

        surface.DrawText("mods: " + keys.Modifiers, x, y + 36, 2, Color.White);

        _line.Clear();
        foreach (ushort code in keys.Keys)
            _line.Append(code).Append(' ');
        surface.DrawText("keys: " + (_line.Length == 0 ? "-" : _line.ToString()), x, y + 72, 2, Color.White);
    }

    private void DrawMouse(Surface surface, int x, int y)
    {
        surface.DrawText("MOUSE", x, y, 2, Muted);
        if (_mouse is null)
        {
            surface.DrawText("unavailable", x, y + 36, 2, Dim);
            return;
        }

        MouseState m = _mouse.Read();
        _cursorX = Math.Clamp(_cursorX + m.DeltaX, 0, surface.Width);
        _cursorY = Math.Clamp(_cursorY + m.DeltaY, 0, surface.Height);
        if (!m.Connected)
        {
            surface.DrawText("not connected", x, y + 36, 2, Dim);
            return;
        }

        surface.DrawText($"cursor {_cursorX},{_cursorY}  buttons {m.Buttons}", x, y + 36, 2, Color.White);
        surface.FillCircle(_cursorX, _cursorY, 6, Active);
    }

    protected override void OnUnload()
    {
        _mouse?.Dispose();
        _keyboard?.Dispose();
    }

    private static T? TryOpen<T>(Func<T> open) where T : class
    {
        try
        {
            return open();
        }
        catch (ProsperoException)
        {
            return null;
        }
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
