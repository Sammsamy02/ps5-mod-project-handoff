// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Agc;
using System;
using System.Runtime.InteropServices;

namespace SharpProspero.Graphics.Agc;

/// <summary>
/// The graphics device: one-time setup, command-buffer submission, and the frame boundary. A frame is
/// recorded into a <see cref="DrawCommandBuffer"/> (wait for the target, set state and draw, set the
/// flip), then handed to the GPU with <see cref="Submit"/> and closed with <see cref="SuspendPoint"/>.
/// </summary>
public static unsafe class AgcDevice
{
    // The revision of the register defaults the library starts from. Distinct revisions select distinct
    // default tables, and the library skips its whole setup when the revision it is given matches the
    // one already recorded - which on a fresh process is zero, so asking for zero asks for nothing to
    // happen. The reference startup code asks for this revision, and so does the table this SDK reads
    // its own defaults from.
    private const uint DefaultsRevision = 8;

    private static void* _state;

    /// <summary>
    /// Prepares the graphics API. Call once before any other graphics work; calling again does nothing.
    /// </summary>
    /// <exception cref="ProsperoException">The graphics library refused to start.</exception>
    public static void Initialize()
    {
        if (_state is not null)
            return;
        // The library keeps a word of its own here and the caller owns it for the life of the process.
        void* state = NativeMemory.AllocZeroed(sizeof(ulong));
        int rc = SceAgc.sceAgcInit(state, DefaultsRevision);
        if (rc != 0)
        {
            NativeMemory.Free(state);
            SceResult.ThrowIfFailed(rc, nameof(SceAgc.sceAgcInit));
        }
        _state = state;
    }

    /// <summary>
    /// What the driver reads to find a recorded buffer: where the words are, how many there are, and a
    /// flag. It is a description of the buffer rather than the buffer itself, which is the distinction
    /// that matters - handing over the book-keeping block instead had the driver read the word count
    /// out of whatever happened to sit twelve bytes into it.
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Size = 16)]
    private struct SubmitDescription
    {
        public void* Words;
        public uint WordCount;
        public byte Flag;
    }

    // The largest submission the graphics library accepts, in 32-bit words, and the status it answers
    // with above it. The limit is not a diagnostic nicety: the driver folds the word count into a
    // twenty-bit field of the packet it builds, without checking it, so a larger recording would run as
    // its low twenty bits - a truncated command stream with nothing reported.
    private const uint MaximumSubmitDwords = 0xFFFFF;
    private const int SubmitSizeTooLarge = unchecked((int)0x8A6C0020);

    /// <summary>
    /// Submits a recorded command buffer to the graphics queue. The buffer's recorded contents run on
    /// the GPU; keep the buffer and everything it references alive until the GPU has finished. A buffer
    /// with nothing recorded is not submitted.
    /// </summary>
    /// <exception cref="ProsperoException">
    /// The recording is larger than the graphics library accepts, or the driver refused the submission.
    /// </exception>
    public static void Submit(DrawCommandBuffer commandBuffer)
    {
        ArgumentNullException.ThrowIfNull(commandBuffer);
        uint words = commandBuffer.SubmitSizeDwords;
        if (words == 0)
            return;
        if (words > MaximumSubmitDwords)
            throw new ProsperoException(nameof(SceAgcDriver.sceAgcDriverSubmitDcb), SubmitSizeTooLarge);

        var description = new SubmitDescription
        {
            Words = commandBuffer.BufferAddress,
            WordCount = words,
            Flag = 0,
        };
        SceResult.ThrowIfFailed(
            SceAgcDriver.sceAgcDriverSubmitDcb(&description), nameof(SceAgcDriver.sceAgcDriverSubmitDcb));
    }

    /// <summary>
    /// Marks the end of a frame's submission. If the application is being suspended, it happens here.
    /// Returns the API status.
    /// </summary>
    public static int SuspendPoint() => SceAgc.sceAgcSuspendPoint();
}
