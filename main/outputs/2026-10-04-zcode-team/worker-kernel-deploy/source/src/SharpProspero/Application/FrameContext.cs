// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Graphics;
using SharpProspero.Input;
using SharpProspero.Interop.Pad;
using SharpProspero.Threading;

namespace SharpProspero.Application;

/// <summary>
/// The per-frame state handed to <see cref="ProsperoApp.OnFrame"/>. One instance is reused across
/// frames so the loop allocates nothing steady-state; read its fields but do not hold a reference
/// past the current frame.
/// </summary>
public sealed class FrameContext
{
    internal FrameContext()
    {
    }

    /// <summary>The framebuffer to draw this frame into.</summary>
    public Surface Surface { get; internal set; }

    /// <summary>Zero-based index of the current frame since <see cref="ProsperoApp.Run"/> began.</summary>
    public long FrameIndex { get; internal set; }

    /// <summary>Seconds elapsed since the previous frame, measured at the vertical blank.</summary>
    public double DeltaSeconds { get; internal set; }

    /// <summary>Seconds elapsed since <see cref="ProsperoApp.Run"/> began, summed at each vertical blank.</summary>
    public double TotalSeconds { get; internal set; }

    /// <summary>The latest controller sample, or the resting sample when no controller is open.</summary>
    public GamePadState Input { get; internal set; }

    /// <summary>The controller sample from the previous frame, for detecting button edges.</summary>
    public GamePadState PreviousInput { get; internal set; }

    /// <summary>
    /// The hand-off point back to the frame thread. Post work here from a worker thread to run it on the
    /// next frame; the run loop drains it once per frame before <see cref="ProsperoApp.OnFrame"/>.
    /// </summary>
    public Dispatcher Dispatcher { get; internal set; } = new();

    /// <summary>True while every button in <paramref name="button"/> is held this frame.</summary>
    public bool Held(ScePadButton button) => Input.IsPressed(button);

    /// <summary>True on the frame every button in <paramref name="button"/> becomes pressed.</summary>
    public bool Pressed(ScePadButton button) => Input.IsPressed(button) && !PreviousInput.IsPressed(button);

    /// <summary>True on the frame <paramref name="button"/> stops being fully pressed.</summary>
    public bool Released(ScePadButton button) => !Input.IsPressed(button) && PreviousInput.IsPressed(button);

    /// <summary>True once <see cref="RequestExit"/> has been called.</summary>
    public bool ExitRequested { get; private set; }

    /// <summary>Signals the run loop to leave after the current frame is presented.</summary>
    public void RequestExit() => ExitRequested = true;
}
