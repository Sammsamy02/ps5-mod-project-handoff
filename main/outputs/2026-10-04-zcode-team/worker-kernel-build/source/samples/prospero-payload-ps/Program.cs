// Uses sysctl(KERN_PROC) to enumerate all processes, then
// for each process calls sceKernelGetAppInfo and reads ucred_authid through the kernel
// pipe primitive. Outputs a process table via klog.

using System;
using System.Runtime.InteropServices;
using SharpProspero.Payload;
using SharpProspero.Payload.Kernel;
using SharpProspero.Payload.Posix;
using SharpProspero.Payload.Process;

namespace SampleApp;

internal static unsafe class Program
{
    // kinfo_proc field offsets:
    private const int KiStructsize = 0;    // int at offset 0
    private const int KiPid = 72;          // pid_t at offset 72
    private const int KiPpid = 76;         // pid_t at offset 76
    private const int KiUid = 88;          // uid_t at offset 88
    private const int KiComm = 447;        // char[MAXCOMLEN+1] at offset 447 (19 bytes)

    [UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
    public static int Main(void* args)
    {
        PayloadArgs* pargs = PayloadEntryPoint.Args;
        PayloadKernelIo io = pargs != null ? new PayloadKernelIo(pargs) : default;

        int* mib = stackalloc int[4];
        mib[0] = PayloadSysctl.CtlKern;
        mib[1] = PayloadSysctl.KernProc;
        mib[2] = PayloadSysctl.KernProcProc;
        mib[3] = 0;

        // First call: determine buffer size.
        nuint bufSize = 0;
        if (PayloadSysctl.sysctl(mib, 4, null, &bufSize, null, 0) != 0)
            return -1;

        // Allocate on stack if small enough, otherwise bail (payloads have limited heap).
        if (bufSize > 512 * 1024 || bufSize == 0)
            return -2;

        byte* buf = stackalloc byte[(int)bufSize];
        if (PayloadSysctl.sysctl(mib, 4, buf, &bufSize, null, 0) != 0)
            return -3;

        // Header
        fixed (byte* hdr = "  PID   PPID    UID  Command\n\0"u8)
            PayloadCrt.Klog(hdr);

        // Walk each kinfo_proc entry.
        byte* ptr = buf;
        byte* end = buf + bufSize;
        byte* line = stackalloc byte[256];

        while (ptr < end)
        {
            int structsize = *(int*)(ptr + KiStructsize);
            if (structsize <= 0) break;

            int pid = *(int*)(ptr + KiPid);
            int ppid = *(int*)(ptr + KiPpid);
            int uid = *(int*)(ptr + KiUid);
            byte* comm = ptr + KiComm;

            // Format: "PID  PPID  UID  Command\n"
            int pos = 0;
            pos += FormatInt(line + pos, pid, 5);
            line[pos++] = (byte)' ';
            pos += FormatInt(line + pos, ppid, 5);
            line[pos++] = (byte)' ';
            pos += FormatInt(line + pos, uid, 6);
            line[pos++] = (byte)' ';
            line[pos++] = (byte)' ';
            for (int i = 0; comm[i] != 0 && i < 19 && pos < 240; i++)
                line[pos++] = comm[i];
            line[pos++] = (byte)'\n';
            line[pos] = 0;
            PayloadCrt.Klog(line);

            ptr += structsize;
        }

        return 0;
    }

    private static int FormatInt(byte* dst, int value, int width)
    {
        byte* digits = stackalloc byte[12];
        int d = 0;
        bool neg = value < 0;
        if (neg) value = -value;
        if (value == 0) { digits[d++] = (byte)'0'; }
        else { while (value > 0) { digits[d++] = (byte)('0' + value % 10); value /= 10; } }
        int totalLen = neg ? d + 1 : d;
        int pad = width > totalLen ? width - totalLen : 0;
        int pos = 0;
        for (int i = 0; i < pad; i++) dst[pos++] = (byte)' ';
        if (neg) dst[pos++] = (byte)'-';
        for (int i = d - 1; i >= 0; i--) dst[pos++] = digits[i];
        return pos;
    }
}
