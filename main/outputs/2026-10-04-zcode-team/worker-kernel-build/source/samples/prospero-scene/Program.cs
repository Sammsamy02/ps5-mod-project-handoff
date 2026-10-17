// A SharpProspero 2D scene. A camera follows the player across a scrolling tile map drawn over a
// gradient sky; move with the left stick or d-pad, press Cross for a particle burst, Options to exit.

using SharpProspero.Application;
using SharpProspero.Graphics;
using SharpProspero.Interop.Pad;
using SharpProspero.Numerics;
using System;

namespace SampleApp;

internal sealed class Game : ProsperoApp
{
    private const int TileSize = 32;
    private const int PlayerSize = 24;
    private const int WalkFrames = 4;
    private const int MapColumns = 80;
    private const int MapRows = 50;
    private const int GrassTile = 0;
    private const int DirtTile = 1;
    private const float Speed = 260f;

    private static readonly Color HintColor = Color.FromRgb(0xE6, 0xEC, 0xF2);

    private readonly Gradient _sky = new(
        new GradientStop(0f, Color.FromRgb(0x22, 0x33, 0x5C)),
        new GradientStop(0.55f, Color.FromRgb(0x51, 0x76, 0xB4)),
        new GradientStop(1f, Color.FromRgb(0xA6, 0xCB, 0xE6)));

    private PixelBuffer? _tileBuffer;
    private PixelBuffer? _playerBuffer;
    private SpriteSheet _tileSheet;
    private SpriteSheet _playerSheet;
    private TileMap _map = null!;
    private AnimatedSprite _walk = null!;
    private Camera2D _camera = null!;
    private ParticleSystem _sparks = null!;
    private Vector2 _position;

    protected override void OnLoad()
    {
        _tileBuffer = new PixelBuffer(TileSize * 2, TileSize);
        _tileSheet = new SpriteSheet(_tileBuffer.AsSurface(), TileSize, TileSize);
        PaintGrass(_tileSheet.Frame(GrassTile));
        PaintDirt(_tileSheet.Frame(DirtTile));

        _playerBuffer = new PixelBuffer(PlayerSize * WalkFrames, PlayerSize);
        _playerSheet = new SpriteSheet(_playerBuffer.AsSurface(), PlayerSize, PlayerSize);
        for (int frame = 0; frame < WalkFrames; frame++)
            PaintWalkFrame(_playerSheet.Frame(frame), frame);
        _walk = new AnimatedSprite(_playerSheet, framesPerSecond: 10f);

        _map = BuildMap();
        _position = new Vector2(34 * TileSize, (MapRows - 9) * TileSize);
        _sparks = new ParticleSystem(512) { Gravity = new Vector2(0f, 240f) };

        Surface back = Display.BackBuffer;
        _camera = new Camera2D(back.Width, back.Height);
    }

    protected override void OnFrame(FrameContext context)
    {
        float dt = (float)context.DeltaSeconds;
        Move(context, dt);

        if (context.Pressed(ScePadButton.Cross))
            _sparks.Emit(_position, 48, EmitParams.Burst(Color.FromRgb(0xFF, 0xCE, 0x7A)));
        _sparks.Update(dt);

        _camera.MoveTo(_position);
        _camera.ClampToBounds(_map.WorldBounds);

        Surface surface = context.Surface;
        for (int y = 0; y < surface.Height; y++)
            surface.HLine(0, y, surface.Width, _sky.Sample((float)y / surface.Height));

        _map.Draw(surface, _tileSheet, _camera);
        _sparks.Draw(surface, _camera);

        Vector2 screen = _camera.WorldToScreen(_position);
        _walk.Draw(surface, (int)screen.X - PlayerSize / 2, (int)screen.Y - PlayerSize / 2);

        surface.DrawTextCentered("APP_TITLE", 32, 4, Color.White);
        surface.DrawText("Left stick or d-pad to move   Cross for a burst   Options to exit",
            40, surface.Height - 44, 2, HintColor);

        if (context.Pressed(ScePadButton.Options))
            context.RequestExit();
    }

    protected override void OnUnload()
    {
        _playerBuffer?.Dispose();
        _tileBuffer?.Dispose();
    }

    private void Move(FrameContext context, float dt)
    {
        (float stickX, float stickY) = context.Input.LeftStick;
        float x = MathF.Abs(stickX) > 0.25f ? stickX : 0f;
        float y = MathF.Abs(stickY) > 0.25f ? stickY : 0f;
        if (context.Held(ScePadButton.Left)) x -= 1f;
        if (context.Held(ScePadButton.Right)) x += 1f;
        if (context.Held(ScePadButton.Up)) y -= 1f;
        if (context.Held(ScePadButton.Down)) y += 1f;

        var direction = new Vector2(x, y);
        if (direction.LengthSquared <= 0.0001f)
        {
            _walk.Reset();
            return;
        }

        Vector2 next = _position + (direction.ClampLength(1f) * (Speed * dt));
        var bounds = new RectF(next.X - (PlayerSize / 2f), next.Y - (PlayerSize / 2f), PlayerSize, PlayerSize);
        if (!_map.Collides(bounds, tile => tile >= GrassTile))
            _position = next;
        _walk.Update(dt);
    }

    private static TileMap BuildMap()
    {
        var map = new TileMap(MapColumns, MapRows, TileSize, TileSize);

        // A ground band along the bottom, so the sky shows above it as the view scrolls.
        int groundTop = MapRows - 6;
        for (int row = groundTop; row < MapRows; row++)
            for (int column = 0; column < MapColumns; column++)
                map.SetTile(column, row, row == groundTop ? GrassTile : DirtTile);

        // Side walls for the full height of the level.
        for (int row = 0; row < MapRows; row++)
        {
            map.SetTile(0, row, DirtTile);
            map.SetTile(MapColumns - 1, row, DirtTile);
        }

        // A handful of platforms to scroll past and bump into.
        Platform(map, 10, groundTop - 6, 6);
        Platform(map, 24, groundTop - 10, 5);
        Platform(map, 46, groundTop - 4, 7);
        Platform(map, 58, groundTop - 11, 6);
        Platform(map, 68, groundTop - 7, 5);
        return map;
    }

    private static void Platform(TileMap map, int column, int row, int length)
    {
        for (int i = 0; i < length; i++)
            map.SetTile(column + i, row, GrassTile);
    }

    private static void PaintGrass(Surface frame)
    {
        frame.Clear(Color.FromRgb(0x4C, 0x8C, 0x3A));
        frame.FillRect(0, 0, TileSize, 6, Color.FromRgb(0x63, 0xA8, 0x4A));
        frame.FillRect(0, 6, TileSize, 2, Color.FromRgb(0x36, 0x66, 0x28));
        frame.FillRect(7, 16, 3, 3, Color.FromRgb(0x3C, 0x74, 0x2E));
        frame.FillRect(21, 23, 3, 3, Color.FromRgb(0x3C, 0x74, 0x2E));
    }

    private static void PaintDirt(Surface frame)
    {
        frame.Clear(Color.FromRgb(0x7A, 0x53, 0x36));
        frame.DrawRect(0, 0, TileSize, TileSize, Color.FromRgb(0x4E, 0x35, 0x22));
        frame.FillRect(0, TileSize / 2, TileSize, 2, Color.FromRgb(0x4E, 0x35, 0x22));
        frame.FillRect(TileSize / 2, 0, 2, TileSize / 2, Color.FromRgb(0x4E, 0x35, 0x22));
        frame.FillRect(TileSize / 2, TileSize / 2, 2, TileSize / 2, Color.FromRgb(0x4E, 0x35, 0x22));
    }

    private static void PaintWalkFrame(Surface frame, int index)
    {
        // The frame starts transparent, so only the body pixels blend onto the scene.
        var skin = Color.FromRgb(0xF0, 0xC0, 0x9A);
        var shirt = Color.FromRgb(0xD8, 0x50, 0x40);
        var pants = Color.FromRgb(0x2E, 0x3E, 0x60);

        int step = index % 2 == 0 ? 3 : -3;
        frame.FillRect(9 - step, 17, 3, 6, pants);
        frame.FillRect(12 + step, 17, 3, 6, pants);
        frame.FillRect(8, 9, 8, 9, shirt);
        frame.FillCircle(12, 6, 4, skin);
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
