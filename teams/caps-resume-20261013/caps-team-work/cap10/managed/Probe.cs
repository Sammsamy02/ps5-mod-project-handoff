// Source-only diagnostic assembly. No managed runtime host currently exists.
// Staged source bridge only; no implemented managed host or resolved native imports.
// Future host invokes synchronously within the genuine native ready callback.
using System;
using System.Runtime.InteropServices;

namespace ModBridge.Cap10
{
    public static class Probe
    {
        [DllImport("__Internal", EntryPoint = "modbridge_cap10_abi_version", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern int AbiVersion();
        [DllImport("__Internal", EntryPoint = "modbridge_cap10_runtime_probe", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern int RuntimeProbe(int cookie, out int reply);

        // Host invokes this static managed method after loading Probe.dll IN Townfall.
        // No Unity/BepInEx references; this proves managed execution + runtime bridge only.
        public static int Run(int cookie)
        {
            if (AbiVersion() != 1) return -1;
            int reply;
            int status = RuntimeProbe(cookie, out reply);
            if (status != 0) return status;
            return reply == cookie ? 0 : -2;
        }

        // Host must catch this exception at the managed invocation boundary.
        public static void ThrowForHostBoundaryTest()
        {
            throw new InvalidOperationException("CAP10 expected managed exception");
        }
    }
}
