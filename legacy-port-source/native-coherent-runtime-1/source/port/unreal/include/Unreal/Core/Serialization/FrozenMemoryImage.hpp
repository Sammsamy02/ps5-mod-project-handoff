#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <Unreal/Core/CoreTypes.hpp>

namespace RC::Unreal
{
    /**
     * Frozen memory image primitives: pointer decoding, bounds-checked reading and
     * image building for UE's freeze/unfreeze serialization (FMemoryImage).
     *
     * Pointer packing differs by engine generation:
     *  - UE4 (<= 4.27, and FF7 Rebirth's custom 4.26): bit 0 = frozen flag,
     *    bits 1-63 = signed self-relative byte offset (arithmetic >> 1).
     *  - UE5: bit 0 = frozen flag, bits 1-23 = pointer-table type index (+1 biased),
     *    bits 24-63 = signed self-relative byte offset (arithmetic >> 24).
     * When the frozen flag is clear, the packed value is a real (unfrozen) pointer.
     */
    enum class EFrozenPointerFormat : uint8
    {
        UE4,
        UE5,
    };

    struct FFrozenMemoryImagePtrGeneric
    {
        uint64 Packed = 0;
        EFrozenPointerFormat Format = EFrozenPointerFormat::UE4;

        bool IsFrozen() const { return (Packed & 1) != 0; }

        int64 GetOffsetFromThis() const
        {
            if (Format == EFrozenPointerFormat::UE5)
            {
                return static_cast<int64>(Packed) >> 24;
            }
            return static_cast<int64>(Packed) >> 1;
        }

        // UE5 only; -1 (INDEX_NONE) otherwise
        int32 GetTypeIndex() const
        {
            if (Format == EFrozenPointerFormat::UE5)
            {
                constexpr uint64 TypeIndexMask = ((1ull << 23) - 1ull) << 1;
                return static_cast<int32>((Packed & TypeIndexMask) >> 1) - 1;
            }
            return -1;
        }

        void* GetUnfrozenPtr() const { return reinterpret_cast<void*>(Packed); }

        static uint64 PackFrozen(int64 offsetFromThis, EFrozenPointerFormat format)
        {
            if (format == EFrozenPointerFormat::UE5)
            {
                return (static_cast<uint64>(offsetFromThis) << 24) | 1;
            }
            return (static_cast<uint64>(offsetFromThis) << 1) | 1;
        }
    };

    class FFrozenImageParseError : public std::runtime_error
    {
    public:
        explicit FFrozenImageParseError(const std::string& msg) : std::runtime_error(msg) {}
    };

    struct FFrozenArrayHeader
    {
        uint64 DataOffset = 0; // absolute offset of element data within the image
        int32 Num = 0;
        int32 Max = 0;
        bool Frozen = false;
    };

    /**
     * Bounds-checked reader over a frozen image. Positions are absolute offsets
     * within the image; reads are unaligned unless the caller aligns first, and
     * frozen pointers resolve relative to their own position.
     */
    class FFrozenImageReader
    {
    public:
        FFrozenImageReader(const void* data, size_t size, EFrozenPointerFormat format = EFrozenPointerFormat::UE4)
            : Format(format), m_data(static_cast<const uint8*>(data)), m_size(size)
        {
        }

        uint64 Pos = 0;
        EFrozenPointerFormat Format;
        std::vector<std::string>* Warnings = nullptr;

        const uint8* Base() const { return m_data; }
        size_t Size() const { return m_size; }

        void Align(uint64 alignment) { Pos = (Pos + alignment - 1) & ~(alignment - 1); }

        void Require(uint64 pos, uint64 bytes) const
        {
            if (pos + bytes > m_size || pos + bytes < pos)
            {
                throw FFrozenImageParseError("read of " + std::to_string(bytes) + " bytes at offset " +
                                             std::to_string(pos) + " exceeds image size " + std::to_string(m_size));
            }
        }

        void Warn(const std::string& msg) const
        {
            if (Warnings)
            {
                Warnings->push_back(msg);
            }
        }

        template <typename T>
        T ReadAt(uint64 pos) const
        {
            Require(pos, sizeof(T));
            T value;
            std::memcpy(&value, m_data + pos, sizeof(T));
            return value;
        }

        template <typename T>
        T Read()
        {
            T value = ReadAt<T>(Pos);
            Pos += sizeof(T);
            return value;
        }

        // Reads an 8-byte frozen pointer at Pos and resolves it to an absolute image
        // offset. Unfrozen (real) pointers are rejected.
        uint64 ReadFrozenPtrTarget()
        {
            uint64 ptrPos = Pos;
            FFrozenMemoryImagePtrGeneric ptr{Read<uint64>(), Format};
            if (!ptr.IsFrozen() && ptr.Packed != 0)
            {
                throw FFrozenImageParseError("unfrozen pointer at offset " + std::to_string(ptrPos));
            }
            int64 target = static_cast<int64>(ptrPos) + ptr.GetOffsetFromThis();
            if (target < 0 || static_cast<uint64>(target) > m_size)
            {
                throw FFrozenImageParseError("frozen pointer at offset " + std::to_string(ptrPos) +
                                             " resolves outside image (target " + std::to_string(target) + ")");
            }
            return static_cast<uint64>(target);
        }

        // TArray/FMemoryImageString header: ptr(8) + num(4) + max(4)
        FFrozenArrayHeader ReadArrayHeader(bool requireNumEqualsMax, const char* what)
        {
            FFrozenArrayHeader header;
            uint64 ptrPos = Pos;
            FFrozenMemoryImagePtrGeneric ptr{Read<uint64>(), Format};
            header.Num = Read<int32>();
            header.Max = Read<int32>();
            header.Frozen = ptr.IsFrozen();
            if (header.Num < 0)
            {
                throw FFrozenImageParseError(std::string(what) + " has negative Num at offset " + std::to_string(ptrPos));
            }
            if (requireNumEqualsMax && header.Num != header.Max)
            {
                Warn(std::string(what) + " Num (" + std::to_string(header.Num) + ") != Max (" +
                     std::to_string(header.Max) + ") at offset " + std::to_string(ptrPos));
            }
            if (header.Num > 0)
            {
                if (!ptr.IsFrozen())
                {
                    throw FFrozenImageParseError(std::string(what) + " has unfrozen data pointer at offset " +
                                                 std::to_string(ptrPos));
                }
                int64 target = static_cast<int64>(ptrPos) + ptr.GetOffsetFromThis();
                if (target < 0 || static_cast<uint64>(target) > m_size)
                {
                    throw FFrozenImageParseError(std::string(what) + " data resolves outside image (target " +
                                                 std::to_string(target) + ")");
                }
                header.DataOffset = static_cast<uint64>(target);
            }
            return header;
        }

        // FMemoryImageString: TArray<TCHAR> header, Num includes the null terminator
        std::wstring ReadString()
        {
            FFrozenArrayHeader header = ReadArrayHeader(true, "FString");
            if (header.Num <= 1)
            {
                return {};
            }
            if (header.Num > 1000000)
            {
                throw FFrozenImageParseError("FString length " + std::to_string(header.Num) + " is implausible");
            }
            Require(header.DataOffset, static_cast<uint64>(header.Num) * 2);
            const char16_t* chars = reinterpret_cast<const char16_t*>(m_data + header.DataOffset);
            if (chars[header.Num - 1] != 0)
            {
                Warn("FString at offset " + std::to_string(header.DataOffset) + " is not null terminated");
            }
            static_assert(sizeof(wchar_t) == 2, "expected 16-bit wchar_t");
            return std::wstring(reinterpret_cast<const wchar_t*>(chars), header.Num - 1);
        }

        // TBitArray: ptr(8) + numBits(4) + maxBits(4), int32 words, LSB first
        std::vector<bool> ReadBitArray()
        {
            uint64 ptrPos = Pos;
            FFrozenMemoryImagePtrGeneric ptr{Read<uint64>(), Format};
            int32 numBits = Read<int32>();
            Read<int32>(); // MaxBits
            std::vector<bool> result;
            if (numBits <= 0)
            {
                return result;
            }
            if (!ptr.IsFrozen())
            {
                throw FFrozenImageParseError("TBitArray has unfrozen data pointer at offset " + std::to_string(ptrPos));
            }
            int64 target = static_cast<int64>(ptrPos) + ptr.GetOffsetFromThis();
            int32 numWords = (numBits + 31) / 32;
            Require(static_cast<uint64>(target), static_cast<uint64>(numWords) * 4);
            const uint32* words = reinterpret_cast<const uint32*>(m_data + target);
            result.resize(numBits);
            for (int32 i = 0; i < numBits; ++i)
            {
                result[i] = (words[i / 32] & (1u << (i % 32))) != 0;
            }
            return result;
        }

      private:
        const uint8* m_data;
        size_t m_size;
    };

    /**
     * Append-only builder for frozen images: aligned writes plus self-relative
     * frozen pointer patching. Callers lay out their own structures.
     */
    class FFrozenImageWriter
    {
    public:
        explicit FFrozenImageWriter(EFrozenPointerFormat format = EFrozenPointerFormat::UE4) : Format(format) {}

        EFrozenPointerFormat Format;
        std::vector<uint8> Data;

        uint64 Pos() const { return Data.size(); }

        void Align(uint64 alignment)
        {
            size_t target = (Data.size() + alignment - 1) & ~(alignment - 1);
            Data.resize(target, 0);
        }

        template <typename T>
        void Write(T value)
        {
            size_t at = Data.size();
            Data.resize(at + sizeof(T));
            std::memcpy(Data.data() + at, &value, sizeof(T));
        }

        void WriteBytes(const void* bytes, size_t size)
        {
            size_t at = Data.size();
            Data.resize(at + size);
            std::memcpy(Data.data() + at, bytes, size);
        }

        template <typename T>
        void Patch(uint64 pos, T value)
        {
            std::memcpy(Data.data() + pos, &value, sizeof(T));
        }

        // Frozen self-relative pointer at pos pointing to target
        void PatchFrozenPtr(uint64 pos, uint64 target)
        {
            Patch(pos, FFrozenMemoryImagePtrGeneric::PackFrozen(
                           static_cast<int64>(target) - static_cast<int64>(pos), Format));
        }
    };
} // namespace RC::Unreal
