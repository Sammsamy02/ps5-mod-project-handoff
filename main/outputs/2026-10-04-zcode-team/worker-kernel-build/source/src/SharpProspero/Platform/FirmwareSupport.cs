// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System.Collections.Generic;

namespace SharpProspero.Platform;

/// <summary>
/// The outcome of checking a run-time library against the system it is running on: which of its
/// required exports resolved, and which did not. A valid result means the service can be used; an
/// invalid one names exactly what the system is missing, so a feature can be refused with a specific
/// reason instead of failing partway through.
/// </summary>
/// <param name="Library">The library that was checked.</param>
/// <param name="Firmware">The system version it was checked against.</param>
/// <param name="MissingExports">The required exports the system does not provide, in the order listed.</param>
public sealed record FirmwareValidation(
    SystemLibraryDescriptor Library,
    FirmwareVersion Firmware,
    IReadOnlyList<string> MissingExports)
{
    /// <summary>True when every required export resolved, so the service can be used.</summary>
    public bool IsValid => MissingExports.Count == 0;

    /// <summary>A one-line summary a diagnostic can show the user.</summary>
    public override string ToString()
        => IsValid
            ? $"{Library.Name}: all required exports present on firmware {Firmware}."
            : $"{Library.Name}: missing {string.Join(", ", MissingExports)} on firmware {Firmware}.";
}

/// <summary>
/// Tells a module what system it is running on and whether the SDK supports it, and checks that a
/// run-time service actually resolves before it is used. Call <see cref="EnsureSupported"/> at startup
/// to fail early on an out-of-range system, and <see cref="Validate(SystemLibraryDescriptor)"/> before
/// a feature that depends on a resolved-by-name service.
/// </summary>
public static unsafe class FirmwareSupport
{
    /// <summary>The system software version this module is running on.</summary>
    /// <exception cref="ProsperoException">The version could not be read.</exception>
    public static FirmwareVersion Current => FirmwareVersion.Current;

    /// <summary>The range of system versions the SDK supports.</summary>
    public static FirmwareRange SupportedRange => FirmwareRegistry.SupportedRange;

    /// <summary>True when the running system falls within <see cref="SupportedRange"/>.</summary>
    /// <exception cref="ProsperoException">The version could not be read.</exception>
    public static bool IsSupported => IsSupportedOn(Current);

    /// <summary>Reports whether <paramref name="version"/> falls within <see cref="SupportedRange"/>.</summary>
    public static bool IsSupportedOn(FirmwareVersion version) => SupportedRange.Contains(version);

    /// <summary>
    /// The highest SDK version the running system will accept a module built against. A module built
    /// against more than this is rejected when it loads, so this is the ceiling a build can target for
    /// the system it is on.
    /// </summary>
    /// <exception cref="ProsperoException">The value could not be read.</exception>
    public static FirmwareVersion AllowedSdkVersion
    {
        get
        {
            uint value = 0;
            SceResult.ThrowIfFailed(
                KernelSystem.sceKernelGetAllowedSdkVersionOnSystem(&value),
                nameof(KernelSystem.sceKernelGetAllowedSdkVersionOnSystem));
            return FirmwareVersion.FromSystemValue(value);
        }
    }

    /// <summary>
    /// Throws when the running system is outside <see cref="SupportedRange"/>. Call this once at
    /// startup so an unsupported system fails with a clear message rather than an obscure error later.
    /// </summary>
    /// <exception cref="ProsperoException">The system is outside the supported range.</exception>
    public static void EnsureSupported()
    {
        FirmwareVersion current = Current;
        if (!IsSupportedOn(current))
            throw new ProsperoException(
                $"This SDK supports firmware {SupportedRange}, but the system is {current}.", -1);
    }

    /// <summary>
    /// Loads a run-time service and checks that every export it needs resolves on this system. The
    /// service is loaded and unloaded by the check, so it has no lasting effect. Use the result to
    /// refuse a feature whose service the system does not fully provide.
    /// </summary>
    /// <exception cref="ProsperoException">The library itself could not be loaded.</exception>
    public static FirmwareValidation Validate(SystemLibraryDescriptor descriptor)
    {
        System.ArgumentNullException.ThrowIfNull(descriptor);
        FirmwareVersion.TryGetCurrent(out FirmwareVersion firmware);
        using SystemLibrary library = SystemLibrary.Open(descriptor.Path);
        IReadOnlyList<string> missing = library.FindMissingExports(descriptor.RequiredExports);
        return new FirmwareValidation(descriptor, firmware, missing);
    }

    /// <summary>
    /// Checks the service named <paramref name="name"/> from the registry. Returns null when no service
    /// by that name is registered.
    /// </summary>
    /// <exception cref="ProsperoException">The library itself could not be loaded.</exception>
    public static FirmwareValidation? Validate(string name)
    {
        SystemLibraryDescriptor? descriptor = FirmwareRegistry.FindLibrary(name);
        return descriptor is null ? null : Validate(descriptor);
    }
}
