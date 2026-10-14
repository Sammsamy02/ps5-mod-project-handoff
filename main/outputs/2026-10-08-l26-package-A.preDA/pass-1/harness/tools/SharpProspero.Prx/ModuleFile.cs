// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK

using System;
using System.IO;

namespace SharpProspero.Prx;

/// <summary>How a module or executable file is wrapped on disk.</summary>
public enum ModuleContainer
{
    /// <summary>A plain ELF: an unsigned <c>.elf</c> executable or <c>.prx</c> module.</summary>
    Elf,

    /// <summary>A signed container: a <c>.self</c> executable or <c>.sprx</c> module.</summary>
    Signed,
}

/// <summary>Which on-disk form a module or executable file takes.</summary>
public enum ModuleForm
{
    /// <summary>Not an ELF and not a container this reader recognizes.</summary>
    Unknown,

    /// <summary>A plain ELF: an unsigned <c>.elf</c> executable or <c>.prx</c> module.</summary>
    UnsignedElf,

    /// <summary>
    /// A signed container a development console accepts (<c>.self</c> / <c>.sprx</c>). Its contents
    /// are readable.
    /// </summary>
    SignedPlaintext,

    /// <summary>
    /// A signed and encrypted container for a retail console (<c>.self</c> / <c>.sprx</c>). Its
    /// contents cannot be read without its key.
    /// </summary>
    SignedEncrypted,
}

/// <summary>
/// A module or executable read from disk, with the ELF recovered whether the file was a plain ELF or
/// a signed container. The inspector, the stub generator and the version scanner read the ELF the
/// same way for either form.
/// </summary>
/// <param name="Elf">The plaintext ELF bytes.</param>
/// <param name="Container">Whether the file was a plain ELF or a signed container.</param>
public readonly record struct ModuleFile(byte[] Elf, ModuleContainer Container)
{
    /// <summary>Whether the file was a signed container.</summary>
    public bool IsSigned => Container == ModuleContainer.Signed;

    /// <summary>Reads and classifies the file at <paramref name="path"/>.</summary>
    public static ModuleFile Read(string path) => Parse(File.ReadAllBytes(path));

    /// <summary>
    /// Classifies bytes already in memory. A signed container is unwrapped to its embedded ELF; a
    /// plain ELF is returned as is.
    /// </summary>
    /// <exception cref="PrxFormatException">
    /// The bytes are neither an ELF nor a signed container, or the container is signed and encrypted
    /// for a retail console and cannot be read without its key.
    /// </exception>
    public static ModuleFile Parse(byte[] data)
    {
        ArgumentNullException.ThrowIfNull(data);
        return SelfContainer.Classify(data) switch
        {
            ModuleForm.SignedPlaintext => new ModuleFile(SelfContainer.ExtractElf(data), ModuleContainer.Signed),
            ModuleForm.UnsignedElf => new ModuleFile(data, ModuleContainer.Elf),
            ModuleForm.SignedEncrypted => throw new PrxFormatException(
                "This is a signed and encrypted module; its contents cannot be read without its key."),
            _ => throw new PrxFormatException("File is neither an ELF nor a signed container."),
        };
    }
}
