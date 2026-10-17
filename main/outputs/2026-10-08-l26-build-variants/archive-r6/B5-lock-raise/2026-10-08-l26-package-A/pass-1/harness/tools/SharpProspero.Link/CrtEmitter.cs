// SharpProspero.Link - a linker for module output.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Link;

/// <summary>
/// Writes the C-runtime start object: a relocatable object that defines the program entry point
/// <c>_start</c>. The entry reads the argument count and vector the loader places on the stack, calls
/// <c>main</c>, and hands the return value to <c>exit</c>. It is included ahead of the compiled object
/// so a module has an entry point without linking any outside start file.
/// </summary>
public static class CrtEmitter
{
    private const int ShtProgBits = 1;
    private const int ShtSymTab = 2;
    private const int ShtStrTab = 3;
    private const int ShtRela = 4;

    private const ulong ShfAlloc = 0x2;
    private const ulong ShfExec = 0x4;

    private const byte GlobalFunc = (1 << 4) | 2;   // STB_GLOBAL, STT_FUNC
    private const byte GlobalNoType = 1 << 4;       // STB_GLOBAL, STT_NOTYPE (undefined reference)

    private const uint RPlt32 = 4;                  // R_X86_64_PLT32
    private const uint RPc32 = 2;                   // R_X86_64_PC32 (an address taken, not called)

    /// <summary>The symbol the loader jumps to; the default entry for an application module.</summary>
    public const string StartSymbol = "_start";

    /// <summary>
    /// What the start object is called in the record of components a module was built from. A built
    /// module names its own start file there, so this one names its equivalent.
    /// </summary>
    public const string StartComponentName = "crt1";

    /// <summary>
    /// The C library's own setup, which the entry hands the loader's parameter block. Nothing the
    /// library provides works before it runs.
    /// </summary>
    public const string InitEnvSymbol = "_init_env";

    /// <summary>
    /// The module's constructors. The linker defines this, pointing it at the walker it builds over the
    /// constructor array, so the entry can run them without knowing where the array ended up.
    /// </summary>
    public const string InitSymbol = "_init";

    /// <summary>The module's teardown routine, which the entry registers to run when the process ends.</summary>
    public const string FiniSymbol = "_fini";

    /// <summary>
    /// The step the C library runs once <c>main</c> has returned, before the status reaches
    /// <c>exit</c>. It takes the status and returns; the entry keeps its own copy across the call.
    /// </summary>
    public const string CatchReturnSymbol = "catchReturnFromMain";

    // _start: the entry the loader jumps to.
    //
    // The loader does not put the arguments on the stack. It calls the entry with a parameter block in
    // the first argument register - the argument count at its start and the vector eight bytes in - and
    // a routine to run at teardown in the second. That block is also what starts the C library: it is
    // handed straight to the library's own setup, and until that runs nothing the library provides
    // works, which includes the allocator every later call reaches. So the order here is fixed: set the
    // library up, register the teardown routine, run the constructors, then call main.
    //
    //   push rbp / mov rbp,rsp / push r15,r14,rbx,rax   ; frame, and keep the call boundary aligned
    //   mov  r14d, [rdi]       ; argument count
    //   lea  r15, [rdi+8]      ; argument vector
    //   mov  rbx, rsi          ; the teardown routine
    //   call _init_env         ; rdi still holds the parameter block
    //   mov  rdi, rbx
    //   call atexit
    //   call _init             ; the constructors, which the linker points at its own walker
    //   mov  edi, r14d / mov rsi, r15 / xor edx, edx
    //   call main
    //   mov  edi, eax
    //   call exit
    //   ud2                    ; exit does not return
    private static ReadOnlySpan<byte> StartCode =>
    [
        0x55,                               // 0x00 push rbp
        0x48, 0x89, 0xE5,                   // 0x01 mov rbp, rsp
        0x41, 0x57,                         // 0x04 push r15
        0x41, 0x56,                         // 0x06 push r14
        0x53,                               // 0x08 push rbx
        0x50,                               // 0x09 push rax
        0x44, 0x8B, 0x37,                   // 0x0A mov r14d, [rdi]
        0x48, 0x89, 0xF3,                   // 0x0D mov rbx, rsi
        0x4C, 0x8D, 0x7F, 0x08,             // 0x10 lea r15, [rdi+8]
        0xE8, 0x00, 0x00, 0x00, 0x00,       // 0x14 call _init_env   (rel32 at 0x15)
        0x48, 0x89, 0xDF,                   // 0x19 mov rdi, rbx
        0xE8, 0x00, 0x00, 0x00, 0x00,       // 0x1C call atexit      (rel32 at 0x1D)
        0x48, 0x8D, 0x3D, 0, 0, 0, 0,       // 0x21 lea rdi, [rip+_fini]  (rel32 at 0x24)
        0xE8, 0x00, 0x00, 0x00, 0x00,       // 0x28 call atexit      (rel32 at 0x29)
        0xE8, 0x00, 0x00, 0x00, 0x00,       // 0x2D call _init       (rel32 at 0x2E)
        0x44, 0x89, 0xF7,                   // 0x32 mov edi, r14d
        0x4C, 0x89, 0xFE,                   // 0x35 mov rsi, r15
        0x31, 0xD2,                         // 0x38 xor edx, edx
        0xE8, 0x00, 0x00, 0x00, 0x00,       // 0x3A call main        (rel32 at 0x3B)
        0x89, 0xC7,                         // 0x3F mov edi, eax
        0x89, 0xC3,                         // 0x41 mov ebx, eax     (keep the status across the call)
        0xE8, 0x00, 0x00, 0x00, 0x00,       // 0x43 call catchReturnFromMain (rel32 at 0x44)
        0x89, 0xDF,                         // 0x48 mov edi, ebx
        0xE8, 0x00, 0x00, 0x00, 0x00,       // 0x4A call exit        (rel32 at 0x4B)
        0x0F, 0x0B,                         // 0x4F ud2
    ];

    private const int CallInitEnvRel = 0x15;
    private const int CallAtexitRel = 0x1D;
    private const int LoadFiniRel = 0x24;
    private const int CallAtexitFiniRel = 0x29;
    private const int CallInitRel = 0x2E;
    private const int CallMainRel = 0x3B;
    private const int CallCatchReturnRel = 0x44;
    private const int CallExitRel = 0x4B;

    // How to walk back out of the entry routine, in the form the frame index and the unwinder read.
    //
    // Every other routine in a module carries this and the entry did not, so a walk up the stack that
    // reached the entry had nothing to go on there: it either stopped one frame early or carried on
    // into whatever the registers happened to hold. The entry is the frame every walk ends at, so it is
    // the one place a missing record is certain to be reached.
    //
    // The record describes a prologue of a frame pointer taken and four registers saved, which is this
    // routine's prologue exactly - and the length it covers is this routine's length. The two are
    // checked against each other rather than assumed: see <see cref="StartCode"/>.
    private static ReadOnlySpan<byte> StartFrame =>
    [
        // The common part: version 1, an augmentation naming a personality-less record whose addresses
        // are instruction-relative and signed four bytes, code aligned to one byte, data to minus
        // eight, and the return address in register sixteen.
        0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x7A, 0x52, 0x00, 0x01, 0x78, 0x10, 0x01,
        0x1B, 0x0C, 0x07, 0x08, 0x90, 0x01, 0x00, 0x00,
        // This routine: where it starts (filled in by the relocation below) and how far it reaches,
        // then where the frame address and each saved register are, as the prologue moves them.
        0x1C, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00,
        0x00, 0x41, 0x0E, 0x10, 0x86, 0x02, 0x43, 0x0D,
        0x06, 0x46, 0x83, 0x05, 0x8E, 0x04, 0x8F, 0x03,
    ];

    // Where in the record above the routine's own address is named, and the length it declares.
    private const int StartFrameAddressAt = 0x20;
    private const int StartFrameDeclaredLength = 0x51;

    /// <summary>Builds the start object bytes.</summary>
    public static byte[] BuildStartObject()
    {
        byte[] text = StartCode.ToArray();
        if (text.Length != StartFrameDeclaredLength)
            throw new ElfLinkException(
                $"The entry routine is {text.Length} bytes and the record describing how to walk out of " +
                $"it covers {StartFrameDeclaredLength}. A record that stops short of the routine it " +
                "describes leaves the frames past that point unreadable.");

        // .strtab: symbol names.
        var strtab = new StringTable();
        int nStart = strtab.Add(StartSymbol);
        int nInitEnv = strtab.Add(InitEnvSymbol);
        int nAtexit = strtab.Add("atexit");
        int nInit = strtab.Add(InitSymbol);
        int nFini = strtab.Add(FiniSymbol);
        int nMain = strtab.Add("main");
        int nExit = strtab.Add("exit");
        int nCatch = strtab.Add(CatchReturnSymbol);
        byte[] strtabBytes = strtab.ToBytes();

        // .symtab: null, _start (defined in .text), then every name it calls as an undefined reference.
        // Locals come first; only the null entry is local, so sh_info is 1.
        const int symStart = 1, symInitEnv = 2, symAtexit = 3, symInit = 4, symFini = 5, symMain = 6,
                  symExit = 7, symCatch = 8;
        byte[] symtab = new byte[24 * 9];
        WriteSym(symtab, symStart, nStart, GlobalFunc, sectionIndex: 1, value: 0, size: (ulong)text.Length);
        WriteSym(symtab, symInitEnv, nInitEnv, GlobalNoType, sectionIndex: 0, value: 0, size: 0);
        WriteSym(symtab, symAtexit, nAtexit, GlobalNoType, sectionIndex: 0, value: 0, size: 0);
        WriteSym(symtab, symInit, nInit, GlobalNoType, sectionIndex: 0, value: 0, size: 0);
        WriteSym(symtab, symFini, nFini, GlobalNoType, sectionIndex: 0, value: 0, size: 0);
        WriteSym(symtab, symMain, nMain, GlobalNoType, sectionIndex: 0, value: 0, size: 0);
        WriteSym(symtab, symExit, nExit, GlobalNoType, sectionIndex: 0, value: 0, size: 0);
        WriteSym(symtab, symCatch, nCatch, GlobalNoType, sectionIndex: 0, value: 0, size: 0);

        // .rela.text: each call, a PLT32 with addend -4 (the displacement is measured from the end of
        // the four-byte field).
        byte[] rela = new byte[24 * 8];
        WriteRela(rela, 0, CallInitEnvRel, symInitEnv, RPlt32, -4);
        WriteRela(rela, 1, CallAtexitRel, symAtexit, RPlt32, -4);
        WriteRela(rela, 2, LoadFiniRel, symFini, RPc32, -4);
        WriteRela(rela, 3, CallAtexitFiniRel, symAtexit, RPlt32, -4);
        WriteRela(rela, 4, CallInitRel, symInit, RPlt32, -4);
        WriteRela(rela, 5, CallMainRel, symMain, RPlt32, -4);
        WriteRela(rela, 6, CallCatchReturnRel, symCatch, RPlt32, -4);
        WriteRela(rela, 7, CallExitRel, symExit, RPlt32, -4);

        // The record naming this routine's own start, which the linker fills in once the routine has an
        // address. It is measured from the field itself, like every other instruction-relative one here.
        byte[] frame = StartFrame.ToArray();
        byte[] frameRela = new byte[24];
        WriteRela(frameRela, 0, StartFrameAddressAt, symStart, RPc32, 0);

        // .shstrtab: section names.
        var shstr = new StringTable();
        int nText = shstr.Add(".text");
        int nRela = shstr.Add(".rela.text");
        int nFrame = shstr.Add(".eh_frame");
        int nFrameRela = shstr.Add(".rela.eh_frame");
        int nSym = shstr.Add(".symtab");
        int nStr = shstr.Add(".strtab");
        int nShStr = shstr.Add(".shstrtab");
        byte[] shstrBytes = shstr.ToBytes();

        // Section header indices: [0]null [1].text [2].rela.text [3].eh_frame [4].rela.eh_frame
        // [5].symtab [6].strtab [7].shstrtab.
        const int shText = 1, shRela = 2, shFrame = 3, shFrameRela = 4, shSym = 5, shStr = 6, shShStr = 7;

        var body = new List<byte>();
        long textOff = Place(body, text);
        long relaOff = Place(body, rela);
        Align(body, 8);
        long frameOff = Place(body, frame);
        long frameRelaOff = Place(body, frameRela);
        long symOff = Place(body, symtab);
        long strOff = Place(body, strtabBytes);
        long shstrOff = Place(body, shstrBytes);
        Align(body, 8);
        long shdrOff = 64 + body.Count;

        byte[] shdr = new byte[64 * 8];
        WriteShdr(shdr, shText, nText, ShtProgBits, ShfAlloc | ShfExec, textOff, text.Length, 0, 0, 16, 0);
        WriteShdr(shdr, shRela, nRela, ShtRela, 0, relaOff, rela.Length, shSym, shText, 8, 24);
        // Held as ordinary contents rather than under the type the toolchain gives it: the link picks
        // the frame sections out by name and by asking for something read-only that reserves memory,
        // and ordinary contents answers that. It reserves memory and is never written to or run.
        WriteShdr(shdr, shFrame, nFrame, ShtProgBits, ShfAlloc, frameOff, frame.Length, 0, 0, 8, 0);
        WriteShdr(shdr, shFrameRela, nFrameRela, ShtRela, 0, frameRelaOff, frameRela.Length, shSym, shFrame, 8, 24);
        WriteShdr(shdr, shSym, nSym, ShtSymTab, 0, symOff, symtab.Length, shStr, 1, 8, 24);
        WriteShdr(shdr, shStr, nStr, ShtStrTab, 0, strOff, strtabBytes.Length, 0, 0, 1, 0);
        WriteShdr(shdr, shShStr, nShStr, ShtStrTab, 0, shstrOff, shstrBytes.Length, 0, 0, 1, 0);

        var output = new List<byte>(64 + body.Count + shdr.Length);
        output.AddRange(BuildHeader(shdrOff, shShStr, sectionCount: 8));
        output.AddRange(body);
        output.AddRange(shdr);
        return [.. output];
    }

    private static long Place(List<byte> body, byte[] data)
    {
        Align(body, 8);
        long offset = 64 + body.Count;
        body.AddRange(data);
        return offset;
    }

    private static void Align(List<byte> body, int alignment)
    {
        while ((64 + body.Count) % alignment != 0)
            body.Add(0);
    }

    private static byte[] BuildHeader(long shoff, int shstrndx, int sectionCount)
    {
        byte[] e = new byte[64];
        e[0] = 0x7F; e[1] = (byte)'E'; e[2] = (byte)'L'; e[3] = (byte)'F';
        e[4] = 2;    // ELFCLASS64
        e[5] = 1;    // ELFDATA2LSB
        e[6] = 1;    // EV_CURRENT
        e[7] = 9;    // OS/ABI
        BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(0x10), 1);     // ET_REL
        BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(0x12), 0x3E);  // x86-64
        BinaryPrimitives.WriteUInt32LittleEndian(e.AsSpan(0x14), 1);     // e_version
        BinaryPrimitives.WriteUInt64LittleEndian(e.AsSpan(0x28), (ulong)shoff); // e_shoff
        BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(0x34), 64);    // e_ehsize
        BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(0x3A), 64);    // e_shentsize
        BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(0x3C), (ushort)sectionCount);
        BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(0x3E), (ushort)shstrndx);
        return e;
    }

    private static void WriteSym(byte[] table, int index, int nameOff, byte info, int sectionIndex, ulong value, ulong size)
    {
        int b = index * 24;
        BinaryPrimitives.WriteUInt32LittleEndian(table.AsSpan(b), (uint)nameOff);
        table[b + 4] = info;
        table[b + 5] = 0; // st_other
        BinaryPrimitives.WriteUInt16LittleEndian(table.AsSpan(b + 6), (ushort)sectionIndex);
        BinaryPrimitives.WriteUInt64LittleEndian(table.AsSpan(b + 8), value);
        BinaryPrimitives.WriteUInt64LittleEndian(table.AsSpan(b + 16), size);
    }

    private static void WriteRela(byte[] table, int index, int offset, int symbol, uint type, long addend)
    {
        int b = index * 24;
        BinaryPrimitives.WriteUInt64LittleEndian(table.AsSpan(b), (ulong)offset);
        BinaryPrimitives.WriteUInt64LittleEndian(table.AsSpan(b + 8), ((ulong)(uint)symbol << 32) | type);
        BinaryPrimitives.WriteInt64LittleEndian(table.AsSpan(b + 16), addend);
    }

    private static void WriteShdr(byte[] shdr, int index, int nameOff, uint type, ulong flags, long offset, long size,
        int link, int info, int align, int entsize)
    {
        int b = index * 64;
        BinaryPrimitives.WriteUInt32LittleEndian(shdr.AsSpan(b), (uint)nameOff);
        BinaryPrimitives.WriteUInt32LittleEndian(shdr.AsSpan(b + 4), type);
        BinaryPrimitives.WriteUInt64LittleEndian(shdr.AsSpan(b + 8), flags);
        BinaryPrimitives.WriteUInt64LittleEndian(shdr.AsSpan(b + 16), 0); // addr
        BinaryPrimitives.WriteUInt64LittleEndian(shdr.AsSpan(b + 24), (ulong)offset);
        BinaryPrimitives.WriteUInt64LittleEndian(shdr.AsSpan(b + 32), (ulong)size);
        BinaryPrimitives.WriteUInt32LittleEndian(shdr.AsSpan(b + 40), (uint)link);
        BinaryPrimitives.WriteUInt32LittleEndian(shdr.AsSpan(b + 44), (uint)info);
        BinaryPrimitives.WriteUInt64LittleEndian(shdr.AsSpan(b + 48), (ulong)align);
        BinaryPrimitives.WriteUInt64LittleEndian(shdr.AsSpan(b + 56), (ulong)entsize);
    }

    private sealed class StringTable
    {
        private readonly List<byte> _bytes = [0];
        private readonly Dictionary<string, int> _off = new(StringComparer.Ordinal);

        public int Add(string value)
        {
            if (_off.TryGetValue(value, out int existing))
                return existing;
            int offset = _bytes.Count;
            _bytes.AddRange(Encoding.ASCII.GetBytes(value));
            _bytes.Add(0);
            _off[value] = offset;
            return offset;
        }

        public byte[] ToBytes() => [.. _bytes];
    }
}
