// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Elf;
using SharpProspero.Payload.IO;
using SharpProspero.Payload.Process;
using System;

namespace SharpProspero.Payload.Kernel;

/// <summary>
/// Remote process ELF loader. Maps and executes an ELF binary in a target process
/// using ptrace-based memory operations and JIT shared memory.
/// </summary>
public static unsafe class PayloadElfLoader
{
    /// <summary>
    /// Loads a PIE ELF binary into a target process. Allocates JIT shared memory,
    /// copies the ELF segments, applies relocations, and sets the instruction pointer
    /// to the entry point.
    /// </summary>
    /// <param name="pid">Target process identifier (must be ptrace-attached).</param>
    /// <param name="elfData">Pointer to the ELF binary data.</param>
    /// <param name="elfSize">Size of the ELF data in bytes.</param>
    /// <returns>The entry point address in the target process, or zero on failure.</returns>
    public static ulong Load(int pid, byte* elfData, int elfSize)
    {
        if (!PayloadElfReader.IsValidElf(elfData, elfSize)) return 0;

        Elf64Ehdr* ehdr = PayloadElfReader.GetHeader(elfData);
        ulong baseAddr = PayloadElfReader.GetBaseAddress(elfData, ehdr);
        ulong totalSize = PayloadElfReader.GetTotalMemorySize(elfData, ehdr);
        if (totalSize == 0) return 0;

        // Allocate JIT shared memory in the target process.
        int jitFd = -1;
        int rc = PayloadProcessControl.sceKernelJitCreateSharedMemory(
            0, (nuint)totalSize, 0x02 | 0x04, &jitFd); // RW+EXEC
        if (rc != 0 || jitFd < 0) return 0;

        int aliasFd = PayloadProcessControl.sceKernelJitCreateAliasOfSharedMemory(jitFd, 0x01); // Write
        if (aliasFd < 0) { PayloadIo.close(jitFd); return 0; }

        // Map the writable alias in our process for writing.
        void* writableMap = PayloadIo.mmap(null, (nuint)totalSize,
            PayloadIo.ProtRead | PayloadIo.ProtWrite,
            PayloadIo.MapShared, aliasFd, 0);
        if (writableMap == (void*)-1) { PayloadIo.close(aliasFd); PayloadIo.close(jitFd); return 0; }

        byte* loadBase = (byte*)writableMap;

        // Copy PT_LOAD segments.
        for (int i = 0; i < ehdr->Phnum; i++)
        {
            Elf64Phdr* phdr = PayloadElfReader.GetProgramHeader(elfData, ehdr, i);
            if (phdr == null || phdr->Type != ElfConstants.PtLoad) continue;

            ulong destOff = phdr->Vaddr - baseAddr;
            ulong srcOff = phdr->Offset;
            ulong copyLen = phdr->Filesz < phdr->Memsz ? phdr->Filesz : phdr->Memsz;

            Buffer.MemoryCopy(elfData + srcOff, loadBase + destOff, (long)totalSize - (long)destOff, (long)copyLen);
        }

        // Apply R_X86_64_RELATIVE relocations.
        int dynCount;
        Elf64Dyn* dynTable = PayloadElfReader.GetDynamicTable(elfData, ehdr, out dynCount);
        if (dynTable != null)
        {
            Elf64Dyn* relaDyn = PayloadElfReader.FindDynamic(dynTable, dynCount, ElfConstants.DtRela);
            Elf64Dyn* relaSz = PayloadElfReader.FindDynamic(dynTable, dynCount, ElfConstants.DtRelasz);
            if (relaDyn != null && relaSz != null)
            {
                int relaCount = (int)(relaSz->Val / (ulong)sizeof(Elf64Rela));
                Elf64Rela* relas = (Elf64Rela*)(loadBase + relaDyn->Val - baseAddr);
                PayloadElfReader.ApplyRelativeRelocations(loadBase, relas, relaCount, baseAddr);
            }
        }

        PayloadIo.munmap(writableMap, (nuint)totalSize);
        PayloadIo.close(aliasFd);
        PayloadIo.close(jitFd);

        return ehdr->Entry - baseAddr;
    }
}
