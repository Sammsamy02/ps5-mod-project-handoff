// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.Kernel;
using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Posix;

/// <summary>
/// FreeBSD <c>struct kevent</c> for the <c>kqueue</c>/<c>kevent</c> event notification interface.
/// Layout matches the FreeBSD 12.x kernel structure.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct FreeBsdKevent
{
    /// <summary>Identifier for this event (descriptor, process id, signal number, etc.).</summary>
    public nuint ident;

    /// <summary>Filter for the event (<c>EVFILT_*</c>).</summary>
    public short filter;

    /// <summary>Action flags (<c>EV_ADD</c>, <c>EV_DELETE</c>, etc.).</summary>
    public ushort flags;

    /// <summary>Filter-specific flags (<c>NOTE_*</c>).</summary>
    public uint fflags;

    /// <summary>Filter-specific data.</summary>
    public nint data;

    /// <summary>Opaque user data.</summary>
    public void* udata;
}

/// <summary>
/// Raw <c>kqueue</c>/<c>kevent</c> event notification for a payload context. Wraps the FreeBSD
/// <c>kqueue(2)</c> and <c>kevent(2)</c> system calls from <c>libkernel</c>.
/// </summary>
/// <remarks>
/// <para>
/// Application modules should use the <c>sceKernelCreateEqueue</c> family in
/// <see cref="KernelEqueue"/> instead. This type exposes the raw FreeBSD interface for payloads
/// that need <c>EVFILT_PROC</c> (process exit/fork/exec monitoring) or other filters the SCE
/// wrapper does not surface.
/// </para>
/// </remarks>
public static unsafe partial class PayloadEvent
{
    private const string Lib = "libkernel";

    // ---- Filters ----

    /// <summary>Descriptor became readable.</summary>
    public const short EvfiltRead = -1;

    /// <summary>Descriptor became writable.</summary>
    public const short EvfiltWrite = -2;

    /// <summary>AIO completion.</summary>
    public const short EvfiltAio = -3;

    /// <summary>Vnode changes (file events).</summary>
    public const short EvfiltVnode = -4;

    /// <summary>Process events (exit, fork, exec).</summary>
    public const short EvfiltProc = -5;

    /// <summary>Signal delivery.</summary>
    public const short EvfiltSignal = -6;

    /// <summary>Timer expiration.</summary>
    public const short EvfiltTimer = -7;

    /// <summary>Process descriptor events.</summary>
    public const short EvfiltProcdesc = -8;

    /// <summary>Filesystem events.</summary>
    public const short EvfiltFs = -9;

    /// <summary>User-defined event.</summary>
    public const short EvfiltUser = -11;

    // ---- Action flags ----

    /// <summary>Add the event to the queue.</summary>
    public const ushort EvAdd = 0x0001;

    /// <summary>Delete the event from the queue.</summary>
    public const ushort EvDelete = 0x0002;

    /// <summary>Enable the event.</summary>
    public const ushort EvEnable = 0x0004;

    /// <summary>Disable the event (do not deliver).</summary>
    public const ushort EvDisable = 0x0008;

    /// <summary>Remove the event after the first delivery.</summary>
    public const ushort EvOneshot = 0x0010;

    /// <summary>Clear the event state after delivery.</summary>
    public const ushort EvClear = 0x0020;

    /// <summary>Return the event in the changelist instead of delivering it.</summary>
    public const ushort EvReceipt = 0x0040;

    /// <summary>Disable the event after delivery (combine with <see cref="EvClear"/>).</summary>
    public const ushort EvDispatch = 0x0080;

    /// <summary>End-of-file condition on a descriptor.</summary>
    public const ushort EvEof = 0x8000;

    /// <summary>Error condition; <see cref="FreeBsdKevent.data"/> holds the error number.</summary>
    public const ushort EvError = 0x4000;

    // ---- Process notes ----

    /// <summary>The process exited.</summary>
    public const uint NoteExit = 0x80000000;

    /// <summary>The process forked.</summary>
    public const uint NoteFork = 0x40000000;

    /// <summary>The process called exec.</summary>
    public const uint NoteExec = 0x20000000;

    /// <summary>Follow the process across fork.</summary>
    public const uint NoteTrack = 0x00000001;

    /// <summary>A tracking error occurred.</summary>
    public const uint NoteTrackerr = 0x00000002;

    /// <summary>The event is for a child process.</summary>
    public const uint NoteChild = 0x00000004;

    // ---- Vnode notes ----

    /// <summary>The file was deleted.</summary>
    public const uint NoteDelete = 0x0001;

    /// <summary>The file contents changed.</summary>
    public const uint NoteWrite = 0x0002;

    /// <summary>The file was extended.</summary>
    public const uint NoteExtend = 0x0004;

    /// <summary>The file attributes changed.</summary>
    public const uint NoteAttrib = 0x0008;

    /// <summary>The link count changed.</summary>
    public const uint NoteLink = 0x0010;

    /// <summary>The file was renamed.</summary>
    public const uint NoteRename = 0x0020;

    /// <summary>Access to the file was revoked.</summary>
    public const uint NoteRevoke = 0x0040;

    // ---- Timer notes ----

    /// <summary>Timer data is in seconds.</summary>
    public const uint NoteSeconds = 0x01;

    /// <summary>Timer data is in milliseconds.</summary>
    public const uint NoteMseconds = 0x02;

    /// <summary>Timer data is in microseconds.</summary>
    public const uint NoteUseconds = 0x04;

    /// <summary>Timer data is in nanoseconds.</summary>
    public const uint NoteNseconds = 0x08;

    /// <summary>
    /// Creates a new kernel event queue and returns its descriptor.
    /// </summary>
    /// <returns>A non-negative queue descriptor, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial int kqueue();

    /// <summary>
    /// Registers and/or retrieves events on a kernel event queue.
    /// </summary>
    /// <param name="kq">The queue descriptor from <see cref="kqueue"/>.</param>
    /// <param name="changelist">Events to register, or null.</param>
    /// <param name="nchanges">Number of entries in <paramref name="changelist"/>.</param>
    /// <param name="eventlist">Buffer for returned events, or null.</param>
    /// <param name="nevents">Capacity of <paramref name="eventlist"/>.</param>
    /// <param name="timeout">Maximum wait time, or null to wait indefinitely.</param>
    /// <returns>The number of events placed in <paramref name="eventlist"/>, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial int kevent(
        int kq,
        FreeBsdKevent* changelist, int nchanges,
        FreeBsdKevent* eventlist, int nevents,
        KernelTimespec* timeout);

    /// <summary>
    /// Fills a <see cref="FreeBsdKevent"/> structure, matching the <c>EV_SET</c> macro from
    /// <c>&lt;sys/event.h&gt;</c>.
    /// </summary>
    public static void EvSet(
        FreeBsdKevent* ev, nuint ident, short filter, ushort flags, uint fflags, nint data, void* udata)
    {
        ev->ident = ident;
        ev->filter = filter;
        ev->flags = flags;
        ev->fflags = fflags;
        ev->data = data;
        ev->udata = udata;
    }
}
