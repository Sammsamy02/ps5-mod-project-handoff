// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.Kernel;
using System.Text;

namespace SharpProspero.Diagnostics;

/// <summary>
/// A log sink that writes each line to standard output, which appears on the development console when
/// one is attached. It needs no file and no module, so it is a convenient default during development.
/// </summary>
public sealed unsafe class ConsoleLogSink : ILogSink
{
    // Standard output; on a development setup this is the console TTY, on a plain device it goes nowhere.
    private const int StandardOutput = 1;

    /// <inheritdoc/>
    public void Write(LogLevel level, string message)
    {
        string line = LogFormat.Line(level, message);
        byte[] bytes = Encoding.UTF8.GetBytes(line + "\n");
        fixed (byte* p = bytes)
            KernelFile.sceKernelWrite(StandardOutput, p, (nuint)bytes.Length);
    }
}
