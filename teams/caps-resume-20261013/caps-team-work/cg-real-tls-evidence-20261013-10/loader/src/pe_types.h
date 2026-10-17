// pe_types.h -- minimal PE32+ structures for the cap13 native DLL loader.
// Only the fields this loader actually reads are declared; offsets are
// fixed by the Microsoft PE format spec and asserted below.
// CAP13 proposal-only. Integrator review required before any build.
#ifndef CAP13_PE_TYPES_H
#define CAP13_PE_TYPES_H

#include <stddef.h>
#include <stdint.h>

namespace cap13 {

#pragma pack(push, 1)

struct PeDosHeader
{
    uint16_t e_magic;    // 'MZ' = 0x5A4D
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    int32_t e_lfanew;    // file offset of the PE signature
};

struct PeFileHeader // COFF header
{
    uint16_t Machine;              // 0x8664 = x64 required
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};

struct PeDataDirectory
{
    uint32_t VirtualAddress;
    uint32_t Size;
};

enum PeDirIndex
{
    PE_DIR_EXPORT = 0,
    PE_DIR_IMPORT = 1,
    PE_DIR_EXCEPTION = 3,
    PE_DIR_BASERELOC = 5,
    PE_DIR_TLS = 9,
    PE_DIR_DELAYIMPORT = 13
};

struct PeOptionalHeader64
{
    uint16_t Magic;                // 0x020B = PE32+
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;  // RVA of _DllMainCRTStartup
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;     // expected 4096
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;  // expected >= 16
    PeDataDirectory DataDirectory[16];
};

struct PeSectionHeader
{
    char Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;       // RVA
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
};

// Section Characteristics bits used for the permission partition.
static const uint32_t PE_SCN_CNT_CODE = 0x00000020u;
static const uint32_t PE_SCN_CNT_INITIALIZED_DATA = 0x00000040u;
static const uint32_t PE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080u;
static const uint32_t PE_SCN_MEM_DISCARDABLE = 0x02000000u;
static const uint32_t PE_SCN_MEM_EXECUTE = 0x20000000u;
static const uint32_t PE_SCN_MEM_READ = 0x40000000u;
static const uint32_t PE_SCN_MEM_WRITE = 0x80000000u;

// Base relocation block + entry (entries follow the header in place).
struct PeRelocBlock
{
    uint32_t PageRva;
    uint32_t BlockSize; // includes this header
};
struct PeRelocEntry
{
    uint16_t Offset : 12;
    uint16_t Type : 4;
};
enum PeRelocType
{
    PE_REL_ABSOLUTE = 0, // skip (padding)
    PE_REL_HIGH = 1,
    PE_REL_LOW = 2,
    PE_REL_HIGHLOW = 3,
    PE_REL_HIGHADJ = 4,
    PE_REL_DIR64 = 10
};

// Import directory entry (array, null-terminated).
struct PeImportDescriptor
{
    uint32_t OriginalFirstThunk; // RVA of the Import Name Table (INT); may be 0
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;               // RVA of ASCII dll name
    uint32_t FirstThunk;         // RVA of the Import Address Table (IAT)
};
static const uint64_t PE_IMPORT_BY_ORDINAL = 0x8000000000000000ull;

// Export directory.
struct PeExportDirectory
{
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;
    uint32_t Base;               // ordinal base
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;       // RVA array
    uint32_t AddressOfNames;           // RVA array (ASCII names)
    uint32_t AddressOfNameOrdinals;    // uint16 index array into functions
};

// TLS directory. Real template data or callbacks are refused; an empty
// directory can be admitted only if every other preflight check passes.
struct PeTlsDirectory64
{
    uint64_t StartAddressOfRawData;
    uint64_t EndAddressOfRawData;
    uint64_t AddressOfIndex;
    uint64_t AddressOfCallBacks; // PIMAGE_TLS_CALLBACK array
    uint32_t SizeOfZeroFill;
    uint32_t Characteristics;
};

#pragma pack(pop)

static_assert(sizeof(PeDosHeader) == 64, "DOS header must be 64 bytes");
static_assert(sizeof(PeFileHeader) == 20, "COFF header must be 20 bytes");
static_assert(sizeof(PeOptionalHeader64) == 240, "PE32+ optional header must be 240 bytes");
static_assert(sizeof(PeSectionHeader) == 40, "Section header must be 40 bytes");
static_assert(sizeof(PeImportDescriptor) == 20, "Import descriptor must be 20 bytes");
static_assert(sizeof(PeExportDirectory) == 40, "Export directory must be 40 bytes");
static_assert(sizeof(PeTlsDirectory64) == 40, "TLS directory64 must be 40 bytes");

} // namespace cap13

#endif // CAP13_PE_TYPES_H
