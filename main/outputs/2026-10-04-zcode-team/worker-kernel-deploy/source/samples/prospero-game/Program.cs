// A SharpProspero game: a real-time loop paced by the frame time. A paddle the player moves catches a
// ball that speeds up on every hit; miss it and the round ends. Press Cross to start, Options to exit.
// The pieces to reuse are the delta-timed update, the controller reads, and the frame-rate overlay.

using System;
using SharpProspero.Application;
using SharpProspero.Diagnostics;
using SharpProspero.Graphics;
using SharpProspero.Interop.Pad;

namespace SampleApp;

internal sealed class Game : ProsperoApp
{
    private const float PaddleSpeed = 900f; // pixels a second
    private const int PaddleWidth = 240;
    private const int PaddleHeight = 24;
    private const int BallRadius = 16;
    private const int PaddleMargin = 90;

    private readonly FrameStats _stats = new();
    private bool _ready;
    private bool _running;
    private float _paddleX;
    private float _ballX, _ballY, _ballVelocityX, _ballVelocityY;
    private int _score, _best;

    protected override void OnFrame(FrameContext context)
    {
        Surface surface = context.Surface;
        int width = surface.Width, height = surface.Height;
        float delta = (float)context.DeltaSeconds;
        _stats.Record(delta);

        if (!_ready)
        {
            Reset(width, height);
            _ready = true;
        }

        if (!_running && context.Pressed(ScePadButton.Cross))
        {
            Reset(width, height);
            _running = true;
        }

        if (_running)
            Update(context, width, height, delta);

        Draw(surface, width, height);

        if (context.Pressed(ScePadButton.Options))
            context.RequestExit();
    }

    private void Reset(int width, int height)
    {
        _paddleX = (width - PaddleWidth) / 2f;
        _ballX = width / 2f;
        _ballY = height / 3f;
        _ballVelocityX = 340f;
        _ballVelocityY = 380f;
        _score = 0;
    }

    private void Update(FrameContext context, int width, int height, float delta)
    {
        int paddleY = height - PaddleMargin - PaddleHeight;

        // Move the paddle with the d-pad, clamped to the screen.
        float direction = 0f;
        if (context.Held(ScePadButton.Left))
            direction -= 1f;
        if (context.Held(ScePadButton.Right))
            direction += 1f;
        _paddleX = Math.Clamp(_paddleX + (direction * PaddleSpeed * delta), 0f, width - PaddleWidth);

        _ballX += _ballVelocityX * delta;
        _ballY += _ballVelocityY * delta;

        // Bounce off the side and top walls.
        if (_ballX < BallRadius)
        {
            _ballX = BallRadius;
            _ballVelocityX = Math.Abs(_ballVelocityX);
        }
        else if (_ballX > width - BallRadius)
        {
            _ballX = width - BallRadius;
            _ballVelocityX = -Math.Abs(_ballVelocityX);
        }
        if (_ballY < BallRadius)
        {
            _ballY = BallRadius;
            _ballVelocityY = Math.Abs(_ballVelocityY);
        }

        // Bounce off the paddle, angling by where it struck and speeding up a little each time.
        bool overPaddle = _ballX >= _paddleX && _ballX <= _paddleX + PaddleWidth;
        if (_ballVelocityY > 0f && _ballY + BallRadius >= paddleY && _ballY < paddleY + PaddleHeight && overPaddle)
        {
            _ballY = paddleY - BallRadius;
            _ballVelocityY = -Math.Abs(_ballVelocityY) * 1.03f;
            float offset = ((_ballX - _paddleX) / PaddleWidth) - 0.5f; // -0.5 at the left edge, +0.5 at the right
            _ballVelocityX += offset * 420f;
            _score++;
            _best = Math.Max(_best, _score);
        }

        // The ball fell past the bottom: the round is over.
        if (_ballY - BallRadius > height)
            _running = false;
    }

    private void Draw(Surface surface, int width, int height)
    {
        surface.FillVerticalGradient(0, 0, width, height, Color.FromRgb(0x10, 0x16, 0x24), Color.FromRgb(0x06, 0x08, 0x0E));

        int paddleY = height - PaddleMargin - PaddleHeight;
        surface.FillRoundedRect((int)_paddleX, paddleY, PaddleWidth, PaddleHeight, PaddleHeight / 2, Color.FromRgb(0x5A, 0xA0, 0xFF));
        surface.FillCircle((int)_ballX, (int)_ballY, BallRadius, Color.White);

        surface.DrawText("SCORE " + _score, 60, 50, 3, Color.White);
        string best = "BEST " + _best;
        surface.DrawText(best, width - 60 - Surface.MeasureText(best, 3), 50, 3, Color.FromRgb(0x8A, 0x94, 0xA0));

        if (!_running)
        {
            surface.DrawTextCentered("APP_TITLE", (height / 2) - 60, 6, Color.White);
            string prompt = _score > 0 || _best > 0 ? "Press Cross to play again" : "Press Cross to start";
            surface.DrawTextCentered(prompt, (height / 2) + 20, 3, Color.FromRgb(0xC8, 0xD0, 0xDC));
        }

        _stats.Draw(surface, 60, height - 44, 2, Color.FromRgb(0x60, 0x70, 0x80));
        string exit = "Options: exit";
        surface.DrawText(exit, width - 60 - Surface.MeasureText(exit, 2), height - 44, 2, Color.FromRgb(0x60, 0x70, 0x80));
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
