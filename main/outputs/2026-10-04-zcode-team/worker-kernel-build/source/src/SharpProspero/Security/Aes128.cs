// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Security;

/// <summary>
/// AES-128 block cipher with key expansion, encrypt-block, and decrypt-block operations.
/// Implements the full 10-round Rijndael algorithm with precomputed T-tables for performance.
/// </summary>
public sealed class Aes128
{
    private readonly uint[] _encKey = new uint[44];
    private readonly uint[] _decKey = new uint[44];

    /// <summary>
    /// Initialises the cipher with a 16-byte key. Both encryption and decryption round
    /// keys are expanded.
    /// </summary>
    public Aes128(ReadOnlySpan<byte> key)
    {
        if (key.Length != 16)
            throw new ArgumentException("AES-128 requires a 16-byte key.");
        ExpandKey(key);
    }

    /// <summary>
    /// Encrypts a single 16-byte block in place.
    /// </summary>
    public void EncryptBlock(Span<byte> block)
    {
        if (block.Length < 16) throw new ArgumentException("Block must be at least 16 bytes.");
        uint s0 = Be(block, 0) ^ _encKey[0];
        uint s1 = Be(block, 4) ^ _encKey[1];
        uint s2 = Be(block, 8) ^ _encKey[2];
        uint s3 = Be(block, 12) ^ _encKey[3];

        for (int r = 1; r < 10; r++)
        {
            uint t0 = Te0[s0 >> 24] ^ Te1[(s1 >> 16) & 0xFF] ^ Te2[(s2 >> 8) & 0xFF] ^ Te3[s3 & 0xFF] ^ _encKey[r * 4];
            uint t1 = Te0[s1 >> 24] ^ Te1[(s2 >> 16) & 0xFF] ^ Te2[(s3 >> 8) & 0xFF] ^ Te3[s0 & 0xFF] ^ _encKey[r * 4 + 1];
            uint t2 = Te0[s2 >> 24] ^ Te1[(s3 >> 16) & 0xFF] ^ Te2[(s0 >> 8) & 0xFF] ^ Te3[s1 & 0xFF] ^ _encKey[r * 4 + 2];
            uint t3 = Te0[s3 >> 24] ^ Te1[(s0 >> 16) & 0xFF] ^ Te2[(s1 >> 8) & 0xFF] ^ Te3[s2 & 0xFF] ^ _encKey[r * 4 + 3];
            s0 = t0; s1 = t1; s2 = t2; s3 = t3;
        }

        // Final round (no MixColumns).
        uint f0 = ((uint)Sbox[s0 >> 24] << 24) | ((uint)Sbox[(s1 >> 16) & 0xFF] << 16) | ((uint)Sbox[(s2 >> 8) & 0xFF] << 8) | Sbox[s3 & 0xFF];
        uint f1 = ((uint)Sbox[s1 >> 24] << 24) | ((uint)Sbox[(s2 >> 16) & 0xFF] << 16) | ((uint)Sbox[(s3 >> 8) & 0xFF] << 8) | Sbox[s0 & 0xFF];
        uint f2 = ((uint)Sbox[s2 >> 24] << 24) | ((uint)Sbox[(s3 >> 16) & 0xFF] << 16) | ((uint)Sbox[(s0 >> 8) & 0xFF] << 8) | Sbox[s1 & 0xFF];
        uint f3 = ((uint)Sbox[s3 >> 24] << 24) | ((uint)Sbox[(s0 >> 16) & 0xFF] << 16) | ((uint)Sbox[(s1 >> 8) & 0xFF] << 8) | Sbox[s2 & 0xFF];
        PutBe(block, 0, f0 ^ _encKey[40]);
        PutBe(block, 4, f1 ^ _encKey[41]);
        PutBe(block, 8, f2 ^ _encKey[42]);
        PutBe(block, 12, f3 ^ _encKey[43]);
    }

    /// <summary>
    /// Decrypts a single 16-byte block in place.
    /// </summary>
    public void DecryptBlock(Span<byte> block)
    {
        if (block.Length < 16) throw new ArgumentException("Block must be at least 16 bytes.");
        uint s0 = Be(block, 0) ^ _decKey[0];
        uint s1 = Be(block, 4) ^ _decKey[1];
        uint s2 = Be(block, 8) ^ _decKey[2];
        uint s3 = Be(block, 12) ^ _decKey[3];

        for (int r = 1; r < 10; r++)
        {
            uint t0 = Td0[s0 >> 24] ^ Td1[(s3 >> 16) & 0xFF] ^ Td2[(s2 >> 8) & 0xFF] ^ Td3[s1 & 0xFF] ^ _decKey[r * 4];
            uint t1 = Td0[s1 >> 24] ^ Td1[(s0 >> 16) & 0xFF] ^ Td2[(s3 >> 8) & 0xFF] ^ Td3[s2 & 0xFF] ^ _decKey[r * 4 + 1];
            uint t2 = Td0[s2 >> 24] ^ Td1[(s1 >> 16) & 0xFF] ^ Td2[(s0 >> 8) & 0xFF] ^ Td3[s3 & 0xFF] ^ _decKey[r * 4 + 2];
            uint t3 = Td0[s3 >> 24] ^ Td1[(s2 >> 16) & 0xFF] ^ Td2[(s1 >> 8) & 0xFF] ^ Td3[s0 & 0xFF] ^ _decKey[r * 4 + 3];
            s0 = t0; s1 = t1; s2 = t2; s3 = t3;
        }

        uint f0 = ((uint)InvSbox[s0 >> 24] << 24) | ((uint)InvSbox[(s3 >> 16) & 0xFF] << 16) | ((uint)InvSbox[(s2 >> 8) & 0xFF] << 8) | InvSbox[s1 & 0xFF];
        uint f1 = ((uint)InvSbox[s1 >> 24] << 24) | ((uint)InvSbox[(s0 >> 16) & 0xFF] << 16) | ((uint)InvSbox[(s3 >> 8) & 0xFF] << 8) | InvSbox[s2 & 0xFF];
        uint f2 = ((uint)InvSbox[s2 >> 24] << 24) | ((uint)InvSbox[(s1 >> 16) & 0xFF] << 16) | ((uint)InvSbox[(s0 >> 8) & 0xFF] << 8) | InvSbox[s3 & 0xFF];
        uint f3 = ((uint)InvSbox[s3 >> 24] << 24) | ((uint)InvSbox[(s2 >> 16) & 0xFF] << 16) | ((uint)InvSbox[(s1 >> 8) & 0xFF] << 8) | InvSbox[s0 & 0xFF];
        PutBe(block, 0, f0 ^ _decKey[40]);
        PutBe(block, 4, f1 ^ _decKey[41]);
        PutBe(block, 8, f2 ^ _decKey[42]);
        PutBe(block, 12, f3 ^ _decKey[43]);
    }

    /// <summary>
    /// Encrypts data in CBC mode. <paramref name="iv"/> is the 16-byte initialization vector
    /// (modified in place to the last ciphertext block). <paramref name="data"/> length must
    /// be a multiple of 16.
    /// </summary>
    public void EncryptCbc(Span<byte> data, Span<byte> iv)
    {
        for (int i = 0; i < data.Length; i += 16)
        {
            for (int j = 0; j < 16; j++) data[i + j] ^= iv[j];
            EncryptBlock(data.Slice(i, 16));
            data.Slice(i, 16).CopyTo(iv);
        }
    }

    /// <summary>
    /// Decrypts data in CBC mode. <paramref name="iv"/> is the 16-byte initialization vector
    /// (modified in place). <paramref name="data"/> length must be a multiple of 16.
    /// </summary>
    public void DecryptCbc(Span<byte> data, Span<byte> iv)
    {
        Span<byte> prev = stackalloc byte[16];
        for (int i = 0; i < data.Length; i += 16)
        {
            data.Slice(i, 16).CopyTo(prev);
            DecryptBlock(data.Slice(i, 16));
            for (int j = 0; j < 16; j++) data[i + j] ^= iv[j];
            prev.CopyTo(iv);
        }
    }

    /// <summary>
    /// Encrypts data in XTS mode with the given 16-byte tweak. Requires a second
    /// <see cref="Aes128"/> instance for the tweak cipher. <paramref name="data"/> length
    /// must be a multiple of 16.
    /// </summary>
    public void EncryptXts(Span<byte> data, ReadOnlySpan<byte> tweak, Aes128 tweakCipher)
    {
        Span<byte> t = stackalloc byte[16];
        tweak.Slice(0, 16).CopyTo(t);
        tweakCipher.EncryptBlock(t);

        for (int i = 0; i < data.Length; i += 16)
        {
            for (int j = 0; j < 16; j++) data[i + j] ^= t[j];
            EncryptBlock(data.Slice(i, 16));
            for (int j = 0; j < 16; j++) data[i + j] ^= t[j];
            GfMul(t);
        }
    }

    /// <summary>
    /// Decrypts data in XTS mode with the given 16-byte tweak. Requires a second
    /// <see cref="Aes128"/> instance for the tweak cipher. <paramref name="data"/> length
    /// must be a multiple of 16.
    /// </summary>
    public void DecryptXts(Span<byte> data, ReadOnlySpan<byte> tweak, Aes128 tweakCipher)
    {
        Span<byte> t = stackalloc byte[16];
        tweak.Slice(0, 16).CopyTo(t);
        tweakCipher.EncryptBlock(t);

        for (int i = 0; i < data.Length; i += 16)
        {
            for (int j = 0; j < 16; j++) data[i + j] ^= t[j];
            DecryptBlock(data.Slice(i, 16));
            for (int j = 0; j < 16; j++) data[i + j] ^= t[j];
            GfMul(t);
        }
    }

    // GF(2^128) multiplication by x for XTS tweak advancement.
    private static void GfMul(Span<byte> block)
    {
        int carry = 0;
        for (int i = 0; i < 16; i++)
        {
            int next = (block[i] >> 7) & 1;
            block[i] = (byte)((block[i] << 1) | carry);
            carry = next;
        }
        if (carry != 0)
            block[0] ^= 0x87;
    }

    private void ExpandKey(ReadOnlySpan<byte> key)
    {
        for (int i = 0; i < 4; i++)
            _encKey[i] = Be(key, i * 4);

        for (int i = 4; i < 44; i++)
        {
            uint t = _encKey[i - 1];
            if ((i & 3) == 0)
            {
                t = ((uint)Sbox[(t >> 16) & 0xFF] << 24) | ((uint)Sbox[(t >> 8) & 0xFF] << 16) |
                    ((uint)Sbox[t & 0xFF] << 8) | Sbox[t >> 24];
                t ^= Rcon[i / 4 - 1];
            }
            _encKey[i] = _encKey[i - 4] ^ t;
        }

        // Inverse key schedule for decryption: reverse all round keys, then apply
        // InvMixColumns to the middle rounds (1 through 9).
        for (int r = 0; r <= 10; r++)
        {
            int src = (10 - r) * 4;
            _decKey[r * 4] = _encKey[src];
            _decKey[r * 4 + 1] = _encKey[src + 1];
            _decKey[r * 4 + 2] = _encKey[src + 2];
            _decKey[r * 4 + 3] = _encKey[src + 3];
        }
        for (int r = 1; r < 10; r++)
        {
            for (int j = 0; j < 4; j++)
            {
                uint w = _decKey[r * 4 + j];
                _decKey[r * 4 + j] =
                    Td0[Sbox[w >> 24]] ^ Td1[Sbox[(w >> 16) & 0xFF]] ^
                    Td2[Sbox[(w >> 8) & 0xFF]] ^ Td3[Sbox[w & 0xFF]];
            }
        }
    }

    private static uint Be(ReadOnlySpan<byte> b, int i) =>
        (uint)(b[i] << 24 | b[i + 1] << 16 | b[i + 2] << 8 | b[i + 3]);

    private static void PutBe(Span<byte> b, int i, uint v)
    {
        b[i] = (byte)(v >> 24); b[i + 1] = (byte)(v >> 16);
        b[i + 2] = (byte)(v >> 8); b[i + 3] = (byte)v;
    }

    // Round constants.
    private static readonly uint[] Rcon =
    [
        0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000,
        0x20000000, 0x40000000, 0x80000000, 0x1B000000, 0x36000000,
    ];

    // Rijndael S-box.
    private static readonly byte[] Sbox =
    [
        0x63,0x7C,0x77,0x7B,0xF2,0x6B,0x6F,0xC5,0x30,0x01,0x67,0x2B,0xFE,0xD7,0xAB,0x76,
        0xCA,0x82,0xC9,0x7D,0xFA,0x59,0x47,0xF0,0xAD,0xD4,0xA2,0xAF,0x9C,0xA4,0x72,0xC0,
        0xB7,0xFD,0x93,0x26,0x36,0x3F,0xF7,0xCC,0x34,0xA5,0xE5,0xF1,0x71,0xD8,0x31,0x15,
        0x04,0xC7,0x23,0xC3,0x18,0x96,0x05,0x9A,0x07,0x12,0x80,0xE2,0xEB,0x27,0xB2,0x75,
        0x09,0x83,0x2C,0x1A,0x1B,0x6E,0x5A,0xA0,0x52,0x3B,0xD6,0xB3,0x29,0xE3,0x2F,0x84,
        0x53,0xD1,0x00,0xED,0x20,0xFC,0xB1,0x5B,0x6A,0xCB,0xBE,0x39,0x4A,0x4C,0x58,0xCF,
        0xD0,0xEF,0xAA,0xFB,0x43,0x4D,0x33,0x85,0x45,0xF9,0x02,0x7F,0x50,0x3C,0x9F,0xA8,
        0x51,0xA3,0x40,0x8F,0x92,0x9D,0x38,0xF5,0xBC,0xB6,0xDA,0x21,0x10,0xFF,0xF3,0xD2,
        0xCD,0x0C,0x13,0xEC,0x5F,0x97,0x44,0x17,0xC4,0xA7,0x7E,0x3D,0x64,0x5D,0x19,0x73,
        0x60,0x81,0x4F,0xDC,0x22,0x2A,0x90,0x88,0x46,0xEE,0xB8,0x14,0xDE,0x5E,0x0B,0xDB,
        0xE0,0x32,0x3A,0x0A,0x49,0x06,0x24,0x5C,0xC2,0xD3,0xAC,0x62,0x91,0x95,0xE4,0x79,
        0xE7,0xC8,0x37,0x6D,0x8D,0xD5,0x4E,0xA9,0x6C,0x56,0xF4,0xEA,0x65,0x7A,0xAE,0x08,
        0xBA,0x78,0x25,0x2E,0x1C,0xA6,0xB4,0xC6,0xE8,0xDD,0x74,0x1F,0x4B,0xBD,0x8B,0x8A,
        0x70,0x3E,0xB5,0x66,0x48,0x03,0xF6,0x0E,0x61,0x35,0x57,0xB9,0x86,0xC1,0x1D,0x9E,
        0xE1,0xF8,0x98,0x11,0x69,0xD9,0x8E,0x94,0x9B,0x1E,0x87,0xE9,0xCE,0x55,0x28,0xDF,
        0x8C,0xA1,0x89,0x0D,0xBF,0xE6,0x42,0x68,0x41,0x99,0x2D,0x0F,0xB0,0x54,0xBB,0x16,
    ];

    // Inverse S-box.
    private static readonly byte[] InvSbox =
    [
        0x52,0x09,0x6A,0xD5,0x30,0x36,0xA5,0x38,0xBF,0x40,0xA3,0x9E,0x81,0xF3,0xD7,0xFB,
        0x7C,0xE3,0x39,0x82,0x9B,0x2F,0xFF,0x87,0x34,0x8E,0x43,0x44,0xC4,0xDE,0xE9,0xCB,
        0x54,0x7B,0x94,0x32,0xA6,0xC2,0x23,0x3D,0xEE,0x4C,0x95,0x0B,0x42,0xFA,0xC3,0x4E,
        0x08,0x2E,0xA1,0x66,0x28,0xD9,0x24,0xB2,0x76,0x5B,0xA2,0x49,0x6D,0x8B,0xD1,0x25,
        0x72,0xF8,0xF6,0x64,0x86,0x68,0x98,0x16,0xD4,0xA4,0x5C,0xCC,0x5D,0x65,0xB6,0x92,
        0x6C,0x70,0x48,0x50,0xFD,0xED,0xB9,0xDA,0x5E,0x15,0x46,0x57,0xA7,0x8D,0x9D,0x84,
        0x90,0xD8,0xAB,0x00,0x8C,0xBC,0xD3,0x0A,0xF7,0xE4,0x58,0x05,0xB8,0xB3,0x45,0x06,
        0xD0,0x2C,0x1E,0x8F,0xCA,0x3F,0x0F,0x02,0xC1,0xAF,0xBD,0x03,0x01,0x13,0x8A,0x6B,
        0x3A,0x91,0x11,0x41,0x4F,0x67,0xDC,0xEA,0x97,0xF2,0xCF,0xCE,0xF0,0xB4,0xE6,0x73,
        0x96,0xAC,0x74,0x22,0xE7,0xAD,0x35,0x85,0xE2,0xF9,0x37,0xE8,0x1C,0x75,0xDF,0x6E,
        0x47,0xF1,0x1A,0x71,0x1D,0x29,0xC5,0x89,0x6F,0xB7,0x62,0x0E,0xAA,0x18,0xBE,0x1B,
        0xFC,0x56,0x3E,0x4B,0xC6,0xD2,0x79,0x20,0x9A,0xDB,0xC0,0xFE,0x78,0xCD,0x5A,0xF4,
        0x1F,0xDD,0xA8,0x33,0x88,0x07,0xC7,0x31,0xB1,0x12,0x10,0x59,0x27,0x80,0xEC,0x5F,
        0x60,0x51,0x7F,0xA9,0x19,0xB5,0x4A,0x0D,0x2D,0xE5,0x7A,0x9F,0x93,0xC9,0x9C,0xEF,
        0xA0,0xE0,0x3B,0x4D,0xAE,0x2A,0xF5,0xB0,0xC8,0xEB,0xBB,0x3C,0x83,0x53,0x99,0x61,
        0x17,0x2B,0x04,0x7E,0xBA,0x77,0xD6,0x26,0xE1,0x69,0x14,0x63,0x55,0x21,0x0C,0x7D,
    ];

    // Te0–Te3 and Td0–Td3 T-tables (precomputed SubBytes + ShiftRows + MixColumns).
    private static readonly uint[] Te0 = BuildTe0();
    private static readonly uint[] Te1 = BuildTe1();
    private static readonly uint[] Te2 = BuildTe2();
    private static readonly uint[] Te3 = BuildTe3();
    private static readonly uint[] Td0 = BuildTd0();
    private static readonly uint[] Td1 = BuildTd1();
    private static readonly uint[] Td2 = BuildTd2();
    private static readonly uint[] Td3 = BuildTd3();

    private static uint[] BuildTe0()
    {
        var t = new uint[256];
        for (int i = 0; i < 256; i++)
        {
            byte s = Sbox[i];
            byte x2 = Xtime(s);
            byte x3 = (byte)(x2 ^ s);
            t[i] = (uint)(x2 << 24 | s << 16 | s << 8 | x3);
        }
        return t;
    }

    private static uint[] BuildTe1() { var t = BuildTe0(); var r = new uint[256]; for (int i = 0; i < 256; i++) r[i] = Ror(t[i], 8); return r; }
    private static uint[] BuildTe2() { var t = BuildTe0(); var r = new uint[256]; for (int i = 0; i < 256; i++) r[i] = Ror(t[i], 16); return r; }
    private static uint[] BuildTe3() { var t = BuildTe0(); var r = new uint[256]; for (int i = 0; i < 256; i++) r[i] = Ror(t[i], 24); return r; }

    private static uint[] BuildTd0()
    {
        var t = new uint[256];
        for (int i = 0; i < 256; i++)
        {
            byte s = InvSbox[i];
            byte x2 = Xtime(s); byte x4 = Xtime(x2); byte x8 = Xtime(x4);
            byte xe = (byte)(x8 ^ x4 ^ x2); byte xb = (byte)(x8 ^ x2 ^ s);
            byte xd = (byte)(x8 ^ x4 ^ s); byte x9 = (byte)(x8 ^ s);
            t[i] = (uint)(xe << 24 | x9 << 16 | xd << 8 | xb);
        }
        return t;
    }

    private static uint[] BuildTd1() { var t = BuildTd0(); var r = new uint[256]; for (int i = 0; i < 256; i++) r[i] = Ror(t[i], 8); return r; }
    private static uint[] BuildTd2() { var t = BuildTd0(); var r = new uint[256]; for (int i = 0; i < 256; i++) r[i] = Ror(t[i], 16); return r; }
    private static uint[] BuildTd3() { var t = BuildTd0(); var r = new uint[256]; for (int i = 0; i < 256; i++) r[i] = Ror(t[i], 24); return r; }

    private static byte Xtime(byte x) => (byte)((x << 1) ^ ((x >> 7) * 0x1B));
    private static uint Ror(uint v, int n) => (v >> n) | (v << (32 - n));
}
