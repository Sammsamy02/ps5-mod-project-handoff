// The smallest payload the toolchain builds: one call to the kernel log and a zero return.
// Useful as a bring-up probe when a larger payload is failing before its own code runs.

using System.Runtime.InteropServices;
using SharpProspero.Payload;

namespace SampleApp;

internal static unsafe class Program
{
    [UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
    public static int Main(void* args)
    {
        PayloadCrt.Klog("sp:echo:hello\n\0"u8);
        return 0;
    }
}
