// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Posix;

/// <summary>
/// Runtime dynamic linking from a payload context. Wraps the CRT's dlfcn subsystem
/// (<c>__dlopen</c>, <c>__dlsym</c>, <c>__dlclose</c>, <c>__dlerror</c>). These functions
/// are provided by the CRT's <c>rtld_dlfcn</c> subsystem and resolve at link time through
/// the CRT archive, not through a DT_NEEDED SPRX.
/// </summary>
/// <remarks>
/// <para>The CRT publishes these under <c>__dl*</c> names (double underscore prefix). The
/// <c>RTLD_LAZY</c> and <c>RTLD_DEFAULT</c> constants match the FreeBSD values.</para>
/// <para>Unlike the POSIX dlopen that returns <c>void*</c>, the CRT's version uses the same
/// convention. The handle is opaque and must be passed back to <see cref="Dlsym"/> and
/// <see cref="Dlclose"/>.</para>
/// </remarks>
public static unsafe partial class PayloadDlfcn
{
    // The dlfcn functions live in the CRT, which is linked into the payload as a static
    // archive. They are exposed through the libScePosix interop library name because the
    // NativeAOT DirectPInvoke mechanism maps them to the same GOT entries the CRT populates.
    private const string Lib = "libScePosix";

    /// <summary>Load the shared object immediately.</summary>
    public const int RtldNow = 0x0002;

    /// <summary>Resolve symbols lazily on first reference.</summary>
    public const int RtldLazy = 0x0001;

    /// <summary>Search all loaded modules (the payload itself and every loaded SPRX).</summary>
    public static readonly nint RtldDefault = (nint)0;

    /// <summary>
    /// Opens a shared object at runtime, returning a handle for symbol lookup.
    /// </summary>
    /// <param name="path">A NUL-terminated UTF-8 soname or path (e.g. "libSceRandom.sprx\0").</param>
    /// <param name="mode">One of <see cref="RtldLazy"/> or <see cref="RtldNow"/>.</param>
    /// <returns>A handle on success, or null on failure. Call <see cref="Dlerror"/> for the
    /// error message.</returns>
    [LibraryImport(Lib, EntryPoint = "__dlopen")]
    public static partial void* Dlopen(byte* path, int mode);

    /// <summary>
    /// Looks up a symbol by name in a loaded shared object.
    /// </summary>
    /// <param name="handle">A handle from <see cref="Dlopen"/>, or <see cref="RtldDefault"/>
    /// to search all loaded modules.</param>
    /// <param name="symbol">A NUL-terminated UTF-8 symbol name.</param>
    /// <returns>The symbol's address on success, or null on failure. Call <see cref="Dlerror"/>
    /// for the error message.</returns>
    [LibraryImport(Lib, EntryPoint = "__dlsym")]
    public static partial void* Dlsym(void* handle, byte* symbol);

    /// <summary>
    /// Closes a shared object handle, releasing the module if no other references remain.
    /// </summary>
    /// <param name="handle">A handle from <see cref="Dlopen"/>.</param>
    /// <returns>Zero on success, or non-zero on failure.</returns>
    [LibraryImport(Lib, EntryPoint = "__dlclose")]
    public static partial int Dlclose(void* handle);

    /// <summary>
    /// Returns a NUL-terminated error message describing the last dlfcn failure, or null if
    /// no error has occurred since the last call to <see cref="Dlerror"/>.
    /// </summary>
    [LibraryImport(Lib, EntryPoint = "__dlerror")]
    public static partial byte* Dlerror();

    /// <summary>
    /// Convenience: opens a shared object by soname and returns the handle. Throws on failure.
    /// </summary>
    /// <param name="soname">A NUL-terminated UTF-8 soname (e.g. "libSceRandom.sprx\0").</param>
    /// <param name="mode">Load mode (default: <see cref="RtldLazy"/>).</param>
    /// <returns>The opaque handle.</returns>
    /// <exception cref="InvalidOperationException">The module could not be loaded.</exception>
    public static void* Open(ReadOnlySpan<byte> soname, int mode = RtldLazy)
    {
        fixed (byte* p = soname)
        {
            void* handle = Dlopen(p, mode);
            if (handle == null)
            {
                byte* err = Dlerror();
                string msg = err != null ? Marshal.PtrToStringUTF8((nint)err) ?? "dlopen failed" : "dlopen failed";
                throw new InvalidOperationException(msg);
            }
            return handle;
        }
    }

    /// <summary>
    /// Convenience: looks up a symbol by name. Throws on failure.
    /// </summary>
    /// <param name="handle">A handle from <see cref="Open"/> or <see cref="Dlopen"/>.</param>
    /// <param name="symbol">A NUL-terminated UTF-8 symbol name.</param>
    /// <returns>The symbol's address.</returns>
    /// <exception cref="InvalidOperationException">The symbol was not found.</exception>
    public static void* Sym(void* handle, ReadOnlySpan<byte> symbol)
    {
        fixed (byte* p = symbol)
        {
            void* addr = Dlsym(handle, p);
            if (addr == null)
            {
                byte* err = Dlerror();
                string msg = err != null ? Marshal.PtrToStringUTF8((nint)err) ?? "dlsym failed" : "dlsym failed";
                throw new InvalidOperationException(msg);
            }
            return addr;
        }
    }
}
