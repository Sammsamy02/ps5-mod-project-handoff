// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Security;
using System;

namespace SharpProspero.Payload.Bypass;

/// <summary>
/// NPDRM (Network Pass DRM) / RIF (Rights Information File) license bypass. Handles
/// license key decryption and verification bypass for content that requires a license.
/// </summary>
public static class PayloadNpdrmBypass
{
    /// <summary>RIF file magic.</summary>
    public const uint RifMagic = 0x52494600; // "RIF\0"

    /// <summary>Size of a RIF file content ID field.</summary>
    public const int ContentIdSize = 48;

    /// <summary>Size of a RIF encryption key.</summary>
    public const int RifKeySize = 16;

    /// <summary>
    /// Decrypts a RIF debug key using AES-CBC-128 with the debug RIF key.
    /// </summary>
    /// <param name="encryptedKey">The 16-byte encrypted key from the RIF.</param>
    /// <param name="debugRifKey">The 16-byte debug RIF decryption key.</param>
    /// <param name="iv">The 16-byte IV (typically zero for debug RIFs).</param>
    /// <param name="decryptedKey">Buffer to receive the 16-byte decrypted key.</param>
    public static void DecryptDebugRifKey(ReadOnlySpan<byte> encryptedKey,
        ReadOnlySpan<byte> debugRifKey, ReadOnlySpan<byte> iv, Span<byte> decryptedKey)
    {
        encryptedKey.CopyTo(decryptedKey);
        Span<byte> ivCopy = stackalloc byte[16];
        iv.CopyTo(ivCopy);
        var aes = new Aes128(debugRifKey);
        aes.DecryptCbc(decryptedKey, ivCopy);
    }

    /// <summary>
    /// Checks whether a RIF buffer contains a valid RIF header.
    /// </summary>
    public static bool IsValidRif(ReadOnlySpan<byte> data)
    {
        if (data.Length < 0x400) return false;
        return data[0] == (byte)'R' && data[1] == (byte)'I' && data[2] == (byte)'F';
    }

    /// <summary>
    /// Extracts the content identifier from a RIF buffer.
    /// </summary>
    public static ReadOnlySpan<byte> GetContentId(ReadOnlySpan<byte> rif)
    {
        return rif.Slice(0x30, ContentIdSize);
    }
}
