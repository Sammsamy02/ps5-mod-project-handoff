// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.VideoOut;

namespace SharpProspero.Application;

/// <summary>Startup settings for a <see cref="ProsperoApp"/>.</summary>
public sealed class AppConfig
{
    /// <summary>Framebuffer width in pixels.</summary>
    public int Width { get; set; } = 1920;

    /// <summary>Framebuffer height in pixels.</summary>
    public int Height { get; set; } = 1080;

    /// <summary>Number of framebuffers in the swap chain.</summary>
    public int BufferCount { get; set; } = 2;

    /// <summary>
    /// The user the controller opens for. <see cref="SceUser.Invalid"/>, the default, means the user
    /// who started the application, read when the controller opens.
    /// </summary>
    /// <remarks>
    /// A standard controller belongs to a signed-in user, and the platform routes its samples to
    /// whoever the handle was opened for. Opening one for the system user is accepted and then never
    /// delivers anything: the application draws, the system hands it the controller, and every button
    /// reads as released. Only the remote control is opened for the system user.
    ///
    /// The display is a separate matter and is not opened for a user at all - an application of this
    /// kind owns the whole output, and the call that opens it takes only the system.
    /// </remarks>
    public int UserId { get; set; } = SceUser.Invalid;

    /// <summary>Flip timing used each frame.</summary>
    public VideoOutFlipMode FlipMode { get; set; } = VideoOutFlipMode.VSync;

    /// <summary>Remove the boot splash before the first frame.</summary>
    public bool HideSplashScreen { get; set; } = true;

    /// <summary>Open a controller for <see cref="UserId"/> at startup.</summary>
    public bool OpenGamePad { get; set; } = true;
}
