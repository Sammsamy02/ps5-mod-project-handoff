// A SharpProspero media player. Plays a media file bundled with the application: it pulls decoded audio
// and feeds it to the audio port (which paces playback), draws the video when a frame is ready, and
// shows the elapsed time over it. Put your file at /app0/media.mp4, or change MediaPath. Cross pauses
// and resumes, Options exits.

using System;
using System.Collections.Generic;
using SharpProspero.Application;
using SharpProspero.Audio;
using SharpProspero.Graphics;
using SharpProspero.Interop.Pad;
using SharpProspero.Media;

namespace SampleApp;

internal sealed class Player : ProsperoApp
{
    // The rate the main output is opened at. It takes this and one other and refuses everything else.
    private const uint PortRate = 48000;

    private const string MediaPath = "/app0/media.mp4";

    private readonly Queue<short> _pending = new();
    private MediaPlayer? _media;
    private AudioOutDevice? _audio;
    private bool _paused;
    private bool _failed;
    private string _message = "opening...";

    protected override void OnLoad()
    {
        try
        {
            _media = MediaPlayer.Open(MediaPath);
            _media.SetLooping(true);
            _media.Start();
        }
        catch (Exception e)
        {
            _failed = true;
            _message = "Cannot open " + MediaPath + " (" + e.Message + ")";
        }
    }

    protected override void OnFrame(FrameContext context)
    {
        Surface surface = context.Surface;

        if (_media is null || _failed)
        {
            DrawMessage(surface);
        }
        else
        {
            DrawVideo(surface);
            PumpAudio();
            DrawOverlay(surface);
            HandleInput(context);
        }

        if (context.Pressed(ScePadButton.Options))
            context.RequestExit();
    }

    private void DrawVideo(Surface surface)
    {
        // The player returns a video frame when one is ready; otherwise leave a plain background so an
        // audio-only file still shows the time.
        if (_media!.TryGetVideoFrame(out VideoFrame frame) && frame.VisibleWidth > 0 && frame.VisibleHeight > 0)
        {
            surface.Clear(Color.Black);
            (int x, int y, int w, int h) = Fit(frame.VisibleWidth, frame.VisibleHeight, surface.Width, surface.Height);
            frame.RenderTo(surface, x, y, w, h);
        }
        else
        {
            surface.Clear(Color.FromRgb(0x0E, 0x12, 0x18));
        }
    }

    private void PumpAudio()
    {
        // Gather decoded audio, spreading a mono track across both channels and stepping through the
        // clip's samples at the ratio between its rate and the port's.
        //
        // The port is opened at one fixed rate. The main output takes that rate and one other and
        // refuses everything else, so opening it at whatever rate the clip happens to carry failed on
        // most clips and ended the application on the first frame of sound. Anything else is stepped
        // through here instead, which is coarse - it picks the nearest sample rather than blending
        // between them - and is enough for a template.
        while (_media!.TryGetAudioFrame(out AudioFrame frame))
        {
            _audio ??= AudioOutDevice.OpenStereo(grain: 1024, sampleRate: PortRate);

            int channels = frame.ChannelCount == 2 ? 2 : 1;
            int available = frame.Samples.Length / channels;
            if (available == 0)
                continue;

            // How many of the clip's samples one of the port's is worth.
            double step = frame.SampleRate <= 0 ? 1.0 : (double)frame.SampleRate / PortRate;
            for (double at = 0; at < available; at += step)
            {
                int i = (int)at * channels;
                if (channels == 2)
                {
                    _pending.Enqueue(frame.Samples[i]);
                    _pending.Enqueue(frame.Samples[i + 1]);
                }
                else
                {
                    _pending.Enqueue(frame.Samples[i]);
                    _pending.Enqueue(frame.Samples[i]);
                }
            }
        }

        // Push whole blocks; each push blocks until it plays, which paces the whole loop to real time.
        if (_audio is null)
            return;
        int block = _audio.SamplesPerBlock;
        if (_pending.Count < block)
            return;

        short[] buffer = new short[block];
        while (_pending.Count >= block)
        {
            for (int i = 0; i < block; i++)
                buffer[i] = _pending.Dequeue();
            _audio.Output(buffer);
        }
    }

    private void DrawOverlay(Surface surface)
    {
        ulong ms = _media!.Position;
        string time = $"{ms / 60000:00}:{(ms / 1000) % 60:00}";
        surface.DrawTextOutlined(time, 40, 40, 3, Color.White, Color.Black);

        string state = _paused ? "paused" : "playing";
        surface.DrawTextOutlined(state + "   Cross: pause   Options: exit", 40, surface.Height - 56, 2, Color.White, Color.Black);
    }

    private void HandleInput(FrameContext context)
    {
        if (!context.Pressed(ScePadButton.Cross))
            return;
        if (_paused)
            _media!.Resume();
        else
            _media!.Pause();
        _paused = !_paused;
    }

    private void DrawMessage(Surface surface)
    {
        surface.Clear(Color.FromRgb(0x0E, 0x12, 0x18));
        surface.DrawTextCentered("APP_TITLE", (surface.Height / 2) - 40, 4, Color.White);
        surface.DrawTextCentered(_message, (surface.Height / 2) + 24, 2, Color.FromRgb(0xC8, 0xD0, 0xDC));
    }

    // The largest fit of a source of the given size inside the destination, keeping the shape, centred.
    private static (int X, int Y, int Width, int Height) Fit(int sourceWidth, int sourceHeight, int destWidth, int destHeight)
    {
        float scale = MathF.Min((float)destWidth / sourceWidth, (float)destHeight / sourceHeight);
        int width = (int)(sourceWidth * scale);
        int height = (int)(sourceHeight * scale);
        return ((destWidth - width) / 2, (destHeight - height) / 2, width, height);
    }

    protected override void OnUnload()
    {
        _audio?.Dispose();
        _media?.Dispose();
    }
}

internal static class Program
{
    private static void Main()
    {
        using (var app = new Player())
            app.Run();

        // Returning from here is reported to the platform as a fault and the user is shown the
        // box that says the application closed unexpectedly, even when everything went as
        // intended. The process is ended through the C library instead.
        ProcessExit.Exit();
    }
}
