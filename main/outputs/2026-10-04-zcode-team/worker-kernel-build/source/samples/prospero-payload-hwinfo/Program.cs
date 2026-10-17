// Reads the console model name, serial number, SoC temperature, CPU temperature, and CPU
// frequency using the SDK's PayloadHardwareInfo API, then outputs the results via klog.

using System.Runtime.InteropServices;
using SharpProspero.Payload;
using SharpProspero.Payload.Services;

namespace SampleApp;

internal static unsafe class Program
{
    [UnmanagedCallersOnly(EntryPoint = "__managed__Main")]
    public static int Main(void* args)
    {
        if (PayloadHardwareInfo.GetModelName(out string model) == 0)
        {
            byte* s = stackalloc byte[1000];
            int i = 0;
            foreach (char c in model)
            {
                if (i >= 999) break;
                s[i++] = (byte)c;
            }
            s[i] = 0;
            LogField("Model:\t\t "u8, s);
        }

        if (PayloadHardwareInfo.GetSerialNumber(out string serial) == 0)
        {
            byte* s = stackalloc byte[1000];
            int i = 0;
            foreach (char c in serial)
            {
                if (i >= 999) break;
                s[i++] = (byte)c;
            }
            s[i] = 0;
            LogField("S/N:\t\t "u8, s);
        }

        if (PayloadHardwareInfo.GetSocSensorTemperature(0, out int socTemp) == 0)
        {
            LogInt("SoC temp:\t "u8, socTemp, " C\n"u8);
        }

        if (PayloadHardwareInfo.GetCpuTemperature(out int cpuTemp) == 0)
        {
            LogInt("CPU temp:\t "u8, cpuTemp, " C\n"u8);
        }

        long freqHz = PayloadHardwareInfo.sceKernelGetCpuFrequency();
        long freqMhz = freqHz / (1000 * 1000);
        LogInt("CPU freq:\t "u8, (int)freqMhz, " MHz\n"u8);

        return 0;
    }

    private static void LogField(System.ReadOnlySpan<byte> prefix, byte* value)
    {
        byte* line = stackalloc byte[1100];
        int pos = 0;
        for (int i = 0; i < prefix.Length; i++)
            line[pos++] = prefix[i];
        for (int i = 0; value[i] != 0 && pos < 1090; i++)
            line[pos++] = value[i];
        line[pos++] = (byte)'\n';
        line[pos] = 0;
        PayloadCrt.Klog(line);
    }

    private static void LogInt(System.ReadOnlySpan<byte> prefix, int value, System.ReadOnlySpan<byte> suffix)
    {
        byte* line = stackalloc byte[256];
        int pos = 0;
        for (int i = 0; i < prefix.Length; i++)
            line[pos++] = prefix[i];

        // Convert int to decimal string.
        if (value < 0) { line[pos++] = (byte)'-'; value = -value; }
        byte* digits = stackalloc byte[20];
        int d = 0;
        if (value == 0) { digits[d++] = (byte)'0'; }
        else { while (value > 0) { digits[d++] = (byte)('0' + value % 10); value /= 10; } }
        for (int i = d - 1; i >= 0; i--)
            line[pos++] = digits[i];

        for (int i = 0; i < suffix.Length; i++)
            line[pos++] = suffix[i];
        line[pos] = 0;
        PayloadCrt.Klog(line);
    }
}
