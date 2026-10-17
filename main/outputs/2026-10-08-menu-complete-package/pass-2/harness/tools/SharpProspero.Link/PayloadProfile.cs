// SharpProspero.Link - a linker for module output.
// Copyright (C) 2026 SvenGDK
//
// Per-payload dependency model: each sample declares which SPRX modules it loads.
// Every payload gets the FULL CRT unconditionally; there are no profile tiers.
// The CRT emitter always emits every subsystem. Linker gc-sections prunes unreachable code.

using System;
using System.Collections.Generic;

namespace SharpProspero.Link;

/// <summary>
/// Individual CRT subsystem flags. These are architectural documentation of the CRT's
/// subsystem structure, identifying each independently compilable piece. The emitter
/// always emits the full CRT; gc-sections prunes unreachable functions.
/// </summary>
[Flags]
public enum CrtSubsystem
{
    None = 0,

    /// <summary><c>__crt_syscall</c> + <c>__crt_syscall_init</c></summary>
    Syscall = 1 << 0,

    /// <summary><c>__kernel_init</c> + kernel pipe primitives</summary>
    KernelInit = 1 << 1,

    /// <summary><c>kernel_copyin</c> + <c>kernel_copyout</c> + <c>kernel_write</c></summary>
    KernelRw = 1 << 2,

    /// <summary><c>nid_encode</c> + <c>SHA1Transform</c> + <c>kernel_dynlib_dlsym/resolve/obj</c> + <c>kernel_get_proc</c></summary>
    Resolver = 1 << 3,

    /// <summary><c>__klog_init</c> + <c>klog_printf/puts/perror</c></summary>
    Klog = 1 << 4,

    /// <summary><c>__patch_init</c></summary>
    Patch = 1 << 5,

    /// <summary><c>__rtld_init</c> + <c>__rtld_sprx_init</c> + <c>__rtld_so_init</c> + <c>__rtld_payload_init</c></summary>
    Rtld = 1 << 6,

    /// <summary><c>__rtld_dlfcn_init</c> + <c>__dlopen/__dlsym/__dlclose</c></summary>
    Dlfcn = 1 << 7,

    /// <summary>FreeBSD syscall stub table (one 7-byte stub per syscall number)</summary>
    SyscallStubs = 1 << 8,

    /// <summary><c>payload_exit</c> + <c>payload_get_args</c></summary>
    PayloadExit = 1 << 9,
}

/// <summary>
/// Declares the SPRX dependency set for a payload sample. Each sample has a fixed set
/// of DT_NEEDED SPRX modules. Every payload gets the full CRT unconditionally; there are
/// no profile tiers. The only per-payload variation is which SPRX modules are linked.
/// </summary>
public sealed class PayloadProfile
{
    /// <summary>The DT_NEEDED SPRX modules this sample requires at load time.</summary>
    public required string[] NeededSprx { get; init; }

    /// <summary>The kernel module SPRX to link against. Defaults to libkernel_sys.sprx (the
    /// kernel wrapper the payload host process actually loads; libkernel_web.sprx is not present
    /// in the hijacked host context and would fail sceKernelLoadStartModule).</summary>
    public string KernelSprx { get; init; } = "libkernel_sys.sprx";

    /// <summary>SPRX modules loaded at runtime via dlopen (NOT in DT_NEEDED).</summary>
    public string[]? RuntimeSprx { get; init; }

    // ---- Known SPRX module names ----
    //
    // Every known SPRX module available for payload samples is catalogued here.
    // The
    // constants are the canonical sonames the firmware publishes. Samples declare their
    // extra SPRX dependencies through ProsperoSprx MSBuild items; these names are the valid
    // values.

    /// <summary>Kernel module for application-level payloads (default). Provides
    /// sceKernel* APIs, POSIX thread wrappers, and the dlsym resolver.</summary>
    public const string SprxKernelWeb = "libkernel_web.sprx";

    /// <summary>Kernel module for process-level payloads. Provides additional low-level
    /// kernel access (mount enumeration, hardware info). Used by the hwinfo and mntinfo
    /// samples.</summary>
    public const string SprxKernelSys = "libkernel_sys.sprx";

    /// <summary>C library internals (libc, POSIX pthread, string, memory). Every payload
    /// links this as a default.</summary>
    public const string SprxLibcInternal = "libSceLibcInternal.sprx";

    /// <summary>Network library. Every payload links this as a default for socket-based
    /// communication with the host.</summary>
    public const string SprxNet = "libSceNet.sprx";

    /// <summary>Network control library. Used by payload samples needing network interface
    /// management.</summary>
    public const string SprxNetCtl = "libSceNetCtl.sprx";

    /// <summary>Random number generation. Used by the hello_sprx and hello_so
    /// samples.</summary>
    public const string SprxRandom = "libSceRandom.sprx";

    /// <summary>On-screen notification toasts. Used by the notify sample.</summary>
    public const string SprxNotification = "libSceNotification.sprx";

    /// <summary>TLS/SSL library. Used by payload samples needing secure connections (TLS/SSL for HTTP/2 and web server
    /// samples.</summary>
    public const string SprxSsl = "libSceSsl.sprx";

    /// <summary>HTTP/1.x client library. Used by payload samples needing HTTP client functionality.</summary>
    public const string SprxHttp = "libSceHttp.sprx";

    /// <summary>HTTP/2 client library. Used by the http2_get sample.</summary>
    public const string SprxHttp2 = "libSceHttp2.sprx";

    /// <summary>System service library (browser launch, app management). Used by the browser
    /// and install_app samples.</summary>
    public const string SprxSystemService = "libSceSystemService.sprx";

    /// <summary>User service library (user session management). Used by browser and
    /// install_app samples.</summary>
    public const string SprxUserService = "libSceUserService.sprx";

    /// <summary>Application installation utility. Used by payload samples needing app installation
    /// samples.</summary>
    public const string SprxAppInstUtil = "libSceAppInstUtil.sprx";

    /// <summary>Inter-process messaging interface. Used by the install_app sample.</summary>
    public const string SprxIpmi = "libSceIpmi.sprx";

    /// <summary>Internal filesystem operations. Used by payload samples needing filesystem access.</summary>
    public const string SprxFsInternalForVsh = "libSceFsInternalForVsh.sprx";

    /// <summary>System core services. Used by payload samples needing system core services.</summary>
    public const string SprxSysCore = "libSceSysCore.sprx";

    /// <summary>Registry manager. Available for payload declaration.</summary>
    public const string SprxRegMgr = "libSceRegMgr.sprx";

    /// <summary>Remote play service. Available for payload declaration.</summary>
    public const string SprxRemoteplay = "libSceRemoteplay.sprx";

    /// <summary>Keyboard input. Available for payload declaration.</summary>
    public const string SprxKeyboard = "libSceKeyboard.sprx";

    /// <summary>Gamepad input. Available for payload declaration.</summary>
    public const string SprxPad = "libScePad.sprx";

    /// <summary>Audio output. Available for payload declaration.</summary>
    public const string SprxAudioOut = "libSceAudioOut.sprx";

    /// <summary>Video output. Available for payload declaration.</summary>
    public const string SprxVideoOut = "libSceVideoOut.sprx";

    /// <summary>GNM graphics driver. Available for payload declaration.</summary>
    public const string SprxGnmDriver = "libSceGnmDriver.sprx";

    /// <summary>GNM graphics driver for backward-compatibility mode. Available for payload
    /// declaration.</summary>
    public const string SprxGnmDriverForNeoMode = "libSceGnmDriverForNeoMode.sprx";

    /// <summary>Slim OpenGL ES driver for system UI rendering. Available for payload
    /// declaration.</summary>
    public const string SprxGLSlimVSH = "libSceGLSlimVSH.sprx";

    /// <summary>System module loader. Available for payload declaration.</summary>
    public const string SprxSysmodule = "libSceSysmodule.sprx";

    /// <summary>IME dialog (on-screen keyboard). Available for payload declaration.</summary>
    public const string SprxImeDialog = "libSceImeDialog.sprx";

    /// <summary>POSIX compatibility layer for WebKit. Available for payload
    /// declaration.</summary>
    public const string SprxPosixForWebKit = "libScePosixForWebKit.sprx";

    /// <summary>
    /// The complete set of known SPRX module sonames. Every payload sample
    /// SPRX dependency is in this set. The <see cref="IsKnownSprx"/> method checks
    /// membership; the set is useful for validation and diagnostics.
    /// </summary>
    public static IReadOnlyCollection<string> AllKnownSprx { get; } = new HashSet<string>(StringComparer.Ordinal)
    {
        SprxKernelWeb, SprxKernelSys,
        SprxLibcInternal, SprxNet, SprxNetCtl,
        SprxRandom, SprxNotification,
        SprxSsl, SprxHttp, SprxHttp2,
        SprxSystemService, SprxUserService, SprxAppInstUtil, SprxIpmi,
        SprxFsInternalForVsh, SprxSysCore,
        SprxRegMgr, SprxRemoteplay, SprxKeyboard, SprxPad,
        SprxAudioOut, SprxVideoOut, SprxGnmDriver, SprxGnmDriverForNeoMode, SprxGLSlimVSH,
        SprxSysmodule, SprxImeDialog, SprxPosixForWebKit,
    };

    /// <summary>Returns <c>true</c> when <paramref name="soname"/> is in the catalogue of
    /// known SPRX module sonames. Unknown sonames are still valid DT_NEEDED entries (the
    /// firmware may ship modules not in our catalogue), but a mismatch is a useful diagnostic
    /// for typos.</summary>
    public static bool IsKnownSprx(string soname) =>
        AllKnownSprx is HashSet<string> hs
            ? hs.Contains(soname)
            : ((ICollection<string>)AllKnownSprx).Contains(soname);

    /// <summary>Returns <c>true</c> when <paramref name="soname"/> is a kernel module
    /// (libkernel_web.sprx or libkernel_sys.sprx). Only kernel modules may appear in the
    /// <see cref="KernelSprx"/> property; other SPRX modules go in <see cref="NeededSprx"/>
    /// as extras.</summary>
    public static bool IsKernelSprx(string soname) =>
        string.Equals(soname, SprxKernelWeb, StringComparison.Ordinal) ||
        string.Equals(soname, SprxKernelSys, StringComparison.Ordinal);

    // ---- Default SPRX sets ----

    /// <summary>The three base DT_NEEDED modules every payload links against. The kernel module
    /// comes first (the loader must initialise it before resolving symbols from the others),
    /// followed by the C library and the network library. Samples that declare extra SPRX
    /// modules via <c>&lt;ProsperoSprx&gt;</c> in their csproj get these appended automatically
    /// unless the sample already lists them.</summary>
    public static readonly string[] DefaultNeeded =
    [
        SprxKernelSys,
        SprxLibcInternal,
        SprxNet,
    ];

    /// <summary>
    /// Computes the DT_NEEDED SPRX list for a payload by merging sample-declared extras with
    /// the three default modules. The result preserves declaration order for extras, then appends
    /// any defaults the sample did not already declare. An optional <paramref name="kernelOverride"/>
    /// replaces the default <c>libkernel_web.sprx</c> with a different kernel module (e.g.
    /// <c>libkernel_sys.sprx</c> for process-level payloads).
    /// <para>When <paramref name="extraSprx"/> is empty or null, the result is exactly
    /// <see cref="DefaultNeeded"/>.</para>
    /// </summary>
    /// <param name="extraSprx">Template-declared SPRX modules, in link order. May be empty.</param>
    /// <param name="kernelOverride">When non-null, replaces the default kernel module name in the
    /// defaults. Has no effect if the sample already declares a kernel module.</param>
    /// <returns>The complete DT_NEEDED list: extras first, then defaults that were not already
    /// declared.</returns>
    public static string[] BuildNeededSprx(string[] extraSprx, string? kernelOverride = null)
    {
        string kernelSprx = kernelOverride ?? DefaultNeeded[0];
        var result = new List<string>();
        var seen = new HashSet<string>(StringComparer.Ordinal);

        // Template-declared extras first, preserving declaration order.
        if (extraSprx is not null)
            foreach (string s in extraSprx)
                if (s.Length > 0 && seen.Add(s))
                    result.Add(s);

        // Defaults appended in canonical order if not already present.
        if (seen.Add(kernelSprx)) result.Add(kernelSprx);
        if (seen.Add(SprxLibcInternal)) result.Add(SprxLibcInternal);
        if (seen.Add(SprxNet)) result.Add(SprxNet);

        return result.ToArray();
    }

    // ---- Profile creation ----

    /// <summary>
    /// Creates a profile by composing sample-specific SPRX declarations with an optional
    /// kernel module override. This is the primary composition entry point: the build script
    /// reads the <c>ProsperoSprx</c> and <c>ProsperoKernelSprx</c> MSBuild properties and
    /// passes them here. The method validates the kernel override and builds the complete
    /// DT_NEEDED list. Every payload gets the full CRT; there are no profile tiers.
    /// </summary>
    /// <param name="extraSprx">Additional SPRX modules the sample declares via
    /// <c>&lt;ProsperoSprx&gt;</c> items. May be null or empty.</param>
    /// <param name="kernelOverride">When non-null, replaces the default kernel module. Must
    /// be a kernel SPRX (<see cref="IsKernelSprx"/>); other values are ignored.</param>
    /// <returns>A new <see cref="PayloadProfile"/> with the composed SPRX list.</returns>
    public static PayloadProfile Create(string[]? extraSprx = null, string? kernelOverride = null)
    {
        string effectiveKernel = SprxKernelSys;
        if (kernelOverride is not null && IsKernelSprx(kernelOverride))
            effectiveKernel = kernelOverride;

        string[] needed = BuildNeededSprx(extraSprx ?? [], effectiveKernel);

        return new PayloadProfile
        {
            NeededSprx = needed,
            KernelSprx = effectiveKernel,
        };
    }
}
