// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Services;

/// <summary>
/// Reads hardware information from the kernel in a payload context: the console model name,
/// its serial number, CPU frequency, CPU temperature, SoC sensor temperatures, and fan duty.
/// Hardware-name functions (<c>sceKernelGetHwModelName</c>, <c>sceKernelGetHwSerialNumber</c>)
/// are exported by <c>libkernel_sys</c>; the remaining functions are exported by
/// <c>libkernel</c>.
/// </summary>
/// <remarks>
/// A payload template that uses this class must include both
/// <c>&lt;DirectPInvoke Include="libkernel_sys" /&gt;</c> and
/// <c>&lt;DirectPInvoke Include="libkernel" /&gt;</c> in its csproj.
/// </remarks>
public static unsafe partial class PayloadHardwareInfo
{
    private const string Lib = "libkernel_sys";
    private const string LibKernel = "libkernel";

    /// <summary>
    /// Writes the console's model name into <paramref name="buffer"/>. The buffer should be at
    /// least 1000 bytes.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceKernelGetHwModelName(byte* buffer);

    /// <summary>
    /// Writes the console's serial number into <paramref name="buffer"/>. The buffer should be
    /// at least 1000 bytes.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceKernelGetHwSerialNumber(byte* buffer);

    /// <summary>
    /// Returns the CPU frequency in Hz. Divide by <c>1_000_000</c> for MHz.
    /// </summary>
    [LibraryImport(LibKernel)]
    public static partial long sceKernelGetCpuFrequency();

    /// <summary>
    /// Writes the CPU temperature in degrees Celsius into <paramref name="temperature"/>.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(LibKernel)]
    public static partial int sceKernelGetCpuTemperature(int* temperature);

    /// <summary>
    /// Writes the SoC sensor temperature for <paramref name="sensor"/> into
    /// <paramref name="temperature"/> in degrees Celsius. Sensor 0 is the primary SoC sensor.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(LibKernel)]
    public static partial int sceKernelGetSocSensorTemperature(int sensor, int* temperature);

    /// <summary>Reads the console model name as a managed string.</summary>
    /// <param name="modelName">When the method returns zero, the model name string; otherwise
    /// an empty string.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int GetModelName(out string modelName)
    {
        byte* buf = stackalloc byte[1000];
        buf[0] = 0;
        int result = sceKernelGetHwModelName(buf);
        modelName = result == 0 ? Marshal.PtrToStringUTF8((nint)buf) ?? string.Empty : string.Empty;
        return result;
    }

    /// <summary>Reads the console serial number as a managed string.</summary>
    /// <param name="serialNumber">When the method returns zero, the serial number string;
    /// otherwise an empty string.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int GetSerialNumber(out string serialNumber)
    {
        byte* buf = stackalloc byte[1000];
        buf[0] = 0;
        int result = sceKernelGetHwSerialNumber(buf);
        serialNumber = result == 0 ? Marshal.PtrToStringUTF8((nint)buf) ?? string.Empty : string.Empty;
        return result;
    }

    /// <summary>Reads the CPU temperature in degrees Celsius.</summary>
    /// <param name="temperature">When the method returns zero, the temperature; otherwise
    /// zero.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int GetCpuTemperature(out int temperature)
    {
        int temp;
        int result = sceKernelGetCpuTemperature(&temp);
        temperature = result == 0 ? temp : 0;
        return result;
    }

    /// <summary>Reads a SoC sensor temperature in degrees Celsius.</summary>
    /// <param name="sensor">The sensor index (0 is the primary SoC sensor).</param>
    /// <param name="temperature">When the method returns zero, the temperature; otherwise
    /// zero.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int GetSocSensorTemperature(int sensor, out int temperature)
    {
        int temp;
        int result = sceKernelGetSocSensorTemperature(sensor, &temp);
        temperature = result == 0 ? temp : 0;
        return result;
    }

    /// <summary>
    /// Reads the current fan duty cycle. The duty value ranges from 0 (off) to 255 (full speed).
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(LibKernel)]
    public static partial int sceKernelGetCurrentFanDuty(int* unk, int* duty);

    /// <summary>Reads the current fan duty cycle as a percentage (0–100).</summary>
    /// <param name="duty">When the method returns zero, the raw duty value (0–255); otherwise
    /// zero.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int GetCurrentFanDuty(out int duty)
    {
        int unk;
        int d;
        int result = sceKernelGetCurrentFanDuty(&unk, &d);
        duty = result == 0 ? d : 0;
        return result;
    }
}
