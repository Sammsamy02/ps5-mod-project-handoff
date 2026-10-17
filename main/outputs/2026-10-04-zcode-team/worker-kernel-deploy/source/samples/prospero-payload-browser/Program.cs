// Initialises UserService, calls
// sceSystemServiceLaunchWebBrowser to open a URL, then tears down UserService.

using System.Runtime.InteropServices;
using SharpProspero.Payload.Services;

namespace SampleApp;

internal static unsafe class Program
{
    [UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
    public static int Main(void* args)
    {
        int ret = PayloadUserService.Initialize();
        if (ret != 0)
            return -1;

        ret = PayloadBrowser.LaunchWebBrowser("http://ps5-host-not-configured.invalid\0"u8);

        PayloadUserService.Terminate();
        return ret != 0 ? -2 : 0;
    }
}
