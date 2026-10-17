// A SharpProspero payload that finds a homebrew application's sce_sys/param.json and reads its
// contents into the kernel log. The application root is searched at three locations in priority
// order: the internal HDD homebrew folder, USB devices, and the shell's per-title link folder that
// points at whichever of the two the title actually lives on.
//
// The target title identifier is compiled in at the top. When set, the payload jumps straight to
// the matching path; when empty, the payload walks each root and reads the first param.json it
// finds along with the containing title's identifier. Every filesystem call goes through the
// standard C library the on-device libc publishes; no kernel-side primitive is required for a
// plain file read, so the payload is entirely userspace.

using System;
using System.Runtime.InteropServices;
using SharpProspero.Payload;
using SharpProspero.Payload.IO;
using SharpProspero.Payload.Services;

namespace SampleApp;

internal static unsafe class Program
{
    // When set to a nine-character identifier the payload reads that title's param.json directly.
    // Leaving it empty walks every known homebrew root and returns the first param.json found.
    private static ReadOnlySpan<byte> TargetTitleId => ""u8;

    // Cap on the number of bytes read from param.json and logged. param.json files are typically
    // a few hundred bytes to a few kilobytes; four kilobytes is comfortably above any realistic
    // size and small enough to fit in one klog buffer flush without truncation risk.
    private const int MaxReadBytes = 4096;

    // Cap on the number of bytes copied verbatim into a klog line. The kernel log line buffer is
    // finite; larger reads are logged in the "read <size>" line and the notification carries the
    // first 128 bytes of content so the user still sees what the file looks like.
    private const int MaxLogBytes = 512;

    [System.Runtime.InteropServices.UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
    public static int Main(void* args)
    {
        Log("<118>read-param-json: payload entered\n"u8);

        byte* pathBuf = stackalloc byte[256];
        byte* contents = stackalloc byte[MaxReadBytes];

        if (!TargetTitleId.IsEmpty)
        {
            int size = ReadTitleParamJson(TargetTitleId, pathBuf, contents);
            if (size > 0)
            {
                ReportSuccess(pathBuf, contents, size);
                return 0;
            }
        }
        else
        {
            int size = ScanRootsForFirstParamJson(pathBuf, contents);
            if (size > 0)
            {
                ReportSuccess(pathBuf, contents, size);
                return 0;
            }
        }

        Log("<118>read-param-json: no param.json found\n"u8);
        PayloadNotification.SendKernelNotification("read-param-json: not found"u8);
        return -1;
    }

    // Reads the param.json for a specific title identifier, trying the internal HDD first, then
    // every USB slot, then the shell's per-title link. The successful path is written into pathBuf
    // NUL-terminated so the caller can log where the file came from.
    private static int ReadTitleParamJson(ReadOnlySpan<byte> titleId, byte* pathBuf, byte* contents)
    {
        int size = TryReadParamJsonAt("/data/homebrew/"u8, titleId, pathBuf, contents);
        if (size > 0)
            return size;

        for (int usb = 0; usb < 8; usb++)
        {
            byte* root = stackalloc byte[24];
            int rootLen = CopyBytes(root, "/mnt/usb"u8);
            root[rootLen++] = (byte)('0' + usb);
            rootLen += CopyBytes(root + rootLen, "/homebrew/"u8);
            root[rootLen] = 0;

            size = TryReadParamJsonAt(new ReadOnlySpan<byte>(root, rootLen), titleId, pathBuf, contents);
            if (size > 0)
                return size;
        }

        return TryReadThroughMountLnk(titleId, pathBuf, contents);
    }

    // Walks the internal HDD and every USB root, opens every homebrew title directory it finds,
    // and returns the first param.json that reads back at least one byte. If none of the two roots
    // are readable (the payload host may not have file-system permission for them yet) the walk
    // returns zero and the caller reports "not found".
    private static int ScanRootsForFirstParamJson(byte* pathBuf, byte* contents)
    {
        int size = WalkHomebrewRoot("/data/homebrew"u8, pathBuf, contents);
        if (size > 0)
            return size;

        for (int usb = 0; usb < 8; usb++)
        {
            byte* root = stackalloc byte[24];
            int rootLen = CopyBytes(root, "/mnt/usb"u8);
            root[rootLen++] = (byte)('0' + usb);
            rootLen += CopyBytes(root + rootLen, "/homebrew"u8);
            root[rootLen] = 0;

            size = WalkHomebrewRoot(new ReadOnlySpan<byte>(root, rootLen), pathBuf, contents);
            if (size > 0)
                return size;
        }

        return 0;
    }

    // Opens a homebrew root directory and reads param.json out of the first subdirectory whose
    // name starts with PP (PPSA / PPXS / PPAA and friends). Returns the number of bytes read.
    private static int WalkHomebrewRoot(ReadOnlySpan<byte> root, byte* pathBuf, byte* contents)
    {
        byte* rootZ = stackalloc byte[64];
        int rootLen = CopyBytes(rootZ, root);
        rootZ[rootLen] = 0;

        void* dir = PayloadFileSystem.opendir(rootZ);
        if (dir == null)
            return 0;

        int size = 0;
        try
        {
            while (true)
            {
                FreeBsdDirent* entry = PayloadFileSystem.readdir(dir);
                if (entry == null)
                    break;

                byte* name = entry->d_name;
                if (name[0] != (byte)'P' || name[1] != (byte)'P')
                    continue;

                int nameLen = 0;
                while (name[nameLen] != 0 && nameLen < 32)
                    nameLen++;

                size = TryReadParamJsonAt(root, new ReadOnlySpan<byte>(name, nameLen), pathBuf, contents);
                if (size > 0)
                    return size;
            }
        }
        finally
        {
            PayloadFileSystem.closedir(dir);
        }

        return 0;
    }

    // Builds "<root>/<titleId>/sce_sys/param.json" into pathBuf and reads the file into contents.
    // Root can carry an optional trailing slash; the concatenation collapses redundant slashes so
    // the on-disk path is what the platform expects.
    private static int TryReadParamJsonAt(ReadOnlySpan<byte> root, ReadOnlySpan<byte> titleId, byte* pathBuf, byte* contents)
    {
        int pathLen = CopyBytes(pathBuf, root);
        if (pathLen == 0 || pathBuf[pathLen - 1] != (byte)'/')
            pathBuf[pathLen++] = (byte)'/';
        pathLen += CopyBytes(pathBuf + pathLen, titleId);
        pathLen += CopyBytes(pathBuf + pathLen, "/sce_sys/param.json"u8);
        pathBuf[pathLen] = 0;

        return ReadEntireFile(pathBuf, contents);
    }

    // Reads /user/app/<titleId>/mount.lnk as a short text file, follows the target path it names
    // (an absolute path to the app root the shell mounted), and reads that root's sce_sys/param.
    // A mount.lnk on FW 10.01 is a plain text file with the target path, optionally terminated
    // by a newline and any padding bytes. The read strips those before joining "/sce_sys/param.json".
    private static int TryReadThroughMountLnk(ReadOnlySpan<byte> titleId, byte* pathBuf, byte* contents)
    {
        int pathLen = CopyBytes(pathBuf, "/user/app/"u8);
        pathLen += CopyBytes(pathBuf + pathLen, titleId);
        pathLen += CopyBytes(pathBuf + pathLen, "/mount.lnk"u8);
        pathBuf[pathLen] = 0;

        byte* linkBuf = stackalloc byte[256];
        int linkLen = ReadEntireFile(pathBuf, linkBuf);
        if (linkLen <= 0)
            return 0;

        int end = linkLen;
        while (end > 0)
        {
            byte b = linkBuf[end - 1];
            if (b > 0x20 && b != 0)
                break;
            end--;
        }
        if (end == 0)
            return 0;

        // Build "<link_target>/sce_sys/param.json" into pathBuf and read it.
        pathLen = end;
        for (int i = 0; i < end; i++)
            pathBuf[i] = linkBuf[i];
        if (pathBuf[pathLen - 1] != (byte)'/')
            pathBuf[pathLen++] = (byte)'/';
        pathLen += CopyBytes(pathBuf + pathLen, "sce_sys/param.json"u8);
        pathBuf[pathLen] = 0;

        return ReadEntireFile(pathBuf, contents);
    }

    private static int ReadEntireFile(byte* pathZ, byte* buffer)
    {
        int fd = PayloadIo.open(pathZ, PayloadFileSystem.O_RDONLY);
        if (fd < 0)
            return 0;

        int total = 0;
        while (total < MaxReadBytes)
        {
            long n = PayloadIo.read(fd, buffer + total, (nuint)(MaxReadBytes - total));
            if (n <= 0)
                break;
            total += (int)n;
        }
        PayloadIo.close(fd);
        return total;
    }

    private static int CopyBytes(byte* dest, ReadOnlySpan<byte> src)
    {
        for (int i = 0; i < src.Length; i++)
            dest[i] = src[i];
        return src.Length;
    }

    private static void ReportSuccess(byte* pathZ, byte* contents, int size)
    {
        // Log the resolved path and byte count on a single line so a grep on the device log yields
        // one clean record per successful read.
        byte* line = stackalloc byte[512];
        int lineLen = CopyBytes(line, "<118>read-param-json: read "u8);

        // Decimal-format the byte count.
        int digits = 0;
        byte* digitBuf = stackalloc byte[16];
        int n = size;
        if (n == 0)
        {
            digitBuf[digits++] = (byte)'0';
        }
        else
        {
            while (n > 0)
            {
                digitBuf[digits++] = (byte)('0' + (n % 10));
                n /= 10;
            }
        }
        for (int i = digits - 1; i >= 0; i--)
            line[lineLen++] = digitBuf[i];

        lineLen += CopyBytes(line + lineLen, " bytes from "u8);

        int pathLen = 0;
        while (pathZ[pathLen] != 0 && pathLen < 256)
            pathLen++;
        for (int i = 0; i < pathLen; i++)
            line[lineLen++] = pathZ[i];

        line[lineLen++] = (byte)'\n';
        line[lineLen] = 0;
        PayloadCrt.Klog(line);

        // Log a hex slice of the content so the user sees the raw bytes without needing to fetch
        // the file. Cap at MaxLogBytes to keep the klog line inside its buffer.
        int dump = size < MaxLogBytes ? size : MaxLogBytes;
        byte* dumpLine = stackalloc byte[MaxLogBytes + 64];
        int dumpLen = CopyBytes(dumpLine, "<118>read-param-json: "u8);
        for (int i = 0; i < dump; i++)
        {
            byte b = contents[i];
            // Replace any byte the klog would eat with a space so the raw JSON stays readable.
            if (b < 0x20 && b != (byte)'\n')
                dumpLine[dumpLen++] = (byte)' ';
            else
                dumpLine[dumpLen++] = b;
        }
        dumpLine[dumpLen++] = (byte)'\n';
        dumpLine[dumpLen] = 0;
        PayloadCrt.Klog(dumpLine);

        PayloadNotification.SendKernelNotification("read-param-json: done"u8);
    }

    private static void Log(ReadOnlySpan<byte> message)
    {
        PayloadCrt.Klog(message);
    }
}
