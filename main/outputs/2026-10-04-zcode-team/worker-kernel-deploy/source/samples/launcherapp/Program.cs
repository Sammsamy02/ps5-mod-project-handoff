// Minimal LncUtil launcher payload: calls sceLncUtilLaunchApp("PPSA33286") so the
// eboot auth path can be exercised remotely while klog captures the result.

using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

using SharpProspero.Payload;
using SharpProspero.Interop.SystemService;
using Native = SharpProspero.Interop.SystemService.SystemService;

namespace LauncherApp;

internal static unsafe class Program
{
    [UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
    public static int Main(void* args)
    {
        PayloadCrt.Klog("launcher: start\n\0"u8);

        byte* title = stackalloc byte[16];
        byte[] id = Encoding.ASCII.GetBytes("PPSA33286\0");
        for (int i = 0; i < id.Length; i++) title[i] = id[i];

        int rc = Native.sceLncUtilLaunchApp(title, null, null);

        PayloadCrt.Klog(Encoding.ASCII.GetBytes($"launcher: sceLncUtilLaunchApp rc=0x{rc:x8}\n\0"));
        Thread.Sleep(4000);
        PayloadCrt.Klog("launcher: done\n\0"u8);
        return 0;
    }
}
