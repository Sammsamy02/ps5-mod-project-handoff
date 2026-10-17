// A SharpProspero audio synthesizer. A ToneGenerator renders a scale of notes, an AudioMixer layers
// them, and an AudioOutDevice streams the mix on its own thread. The d-pad changes the note; the
// mixer overlaps a new note over the one still ringing. Press Options to exit.

using SharpProspero.Application;
using SharpProspero.Audio;
using SharpProspero.Graphics;
using SharpProspero.Interop.Pad;
using System.Threading;

namespace SampleApp;

internal sealed class Game : ProsperoApp
{
    private static readonly Color Background = Color.FromRgb(0x10, 0x14, 0x1A);
    private static readonly Color Text = Color.White;
    private static readonly Color Accent = Color.FromRgb(0x6C, 0xE0, 0xB0);
    private static readonly Color Dim = Color.FromRgb(0x8A, 0x94, 0xA0);

    private static readonly string[] NoteNames = ["C4", "D4", "E4", "G4", "A4", "C5"];
    private static readonly double[] NoteHertz = [261.63, 293.66, 329.63, 392.00, 440.00, 523.25];

    private AudioOutDevice? _audio;
    private Thread? _mixThread;
    private volatile bool _running;
    private volatile int _note;

    protected override void OnLoad()
    {
        _audio = AudioOutDevice.OpenStereo();
        _running = true;
        _mixThread = new Thread(MixLoop) { IsBackground = true, Name = "Synth" };
        _mixThread.Start();
    }

    // Renders each note once, then mixes and streams blocks until exit. Only this thread touches the
    // mixer; the frame thread hands it a note index through the volatile field.
    private void MixLoop()
    {
        AudioOutDevice audio = _audio!;
        var mixer = new AudioMixer();
        var tone = new ToneGenerator { Waveform = Waveform.Square, Amplitude = 0.35f };

        PcmAudio[] notes = new PcmAudio[NoteHertz.Length];
        for (int i = 0; i < notes.Length; i++)
        {
            tone.Frequency = NoteHertz[i];
            tone.Reset();
            notes[i] = tone.RenderClip(0.4);
        }

        short[] block = new short[audio.SamplesPerBlock];
        int playing = _note;
        while (_running)
        {
            int note = _note;
            if (note != playing)
            {
                playing = note;
                mixer.Play(notes[note], volume: 0.8f);
            }

            mixer.Mix(block);
            audio.Output(block);
        }
    }

    protected override void OnFrame(FrameContext context)
    {
        Surface surface = context.Surface;
        surface.Clear(Background);
        surface.DrawTextCentered("APP_TITLE", 420, 6, Text);
        surface.DrawTextCentered("Note " + NoteNames[_note], 560, 5, Accent);
        surface.DrawTextCentered("D-pad changes the note, Options exits.", 700, 3, Dim);

        if (context.Pressed(ScePadButton.Left) || context.Pressed(ScePadButton.Down))
            _note = (_note + NoteNames.Length - 1) % NoteNames.Length;
        if (context.Pressed(ScePadButton.Right) || context.Pressed(ScePadButton.Up))
            _note = (_note + 1) % NoteNames.Length;

        if (context.Pressed(ScePadButton.Options))
            context.RequestExit();
    }

    protected override void OnUnload()
    {
        _running = false;
        _mixThread?.Join();
        _audio?.Dispose();
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
