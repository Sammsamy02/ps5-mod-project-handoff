// A SharpProspero library built as a relocatable module (.prx). Another module loads it by name and
// calls its exported functions. Each exported function is a static [UnmanagedCallersOnly] method whose
// EntryPoint matches a <ProsperoExportSymbol> in the project file.
//
// A [UnmanagedCallersOnly] method runs across the native boundary, so it must not let a managed
// exception escape and must use only blittable parameter and return types.

using System.Runtime.InteropServices;

namespace SampleApp;

internal static class Library
{
    /// <summary>Adds two integers. Exported as "sampleAdd".</summary>
    [UnmanagedCallersOnly(EntryPoint = "sampleAdd")]
    public static int Add(int a, int b) => a + b;

    /// <summary>Returns the library's version number. Exported as "sampleVersion".</summary>
    [UnmanagedCallersOnly(EntryPoint = "sampleVersion")]
    public static int Version() => 0x01_00;
}
