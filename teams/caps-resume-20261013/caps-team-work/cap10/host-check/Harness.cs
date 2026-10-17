using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Security.Cryptography;

internal static class Harness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void Begin(int kind, int cookie);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int Read(int index);
    static void Check(bool ok, string message) { if (!ok) throw new Exception(message); }
    public static int Main(string[] args)
    {
        try { return Run(args); }
        catch (Exception error) { Console.Error.WriteLine(error); return 1; }
    }
    private static int Run(string[] args)
    {
        byte[] bytes = File.ReadAllBytes(args[0]);
        string digest = Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
        Check(digest == "c328dca11447b2c41ab1580582b9576911b0291a4d14d89a09a708e69e1c108d", "compiled artifact identity");
        Assembly assembly = AssemblyLoadContext.Default.LoadFromStream(new MemoryStream(bytes, false));
        Type probe = assembly.GetType("ModBridge.Cap10.Probe", true);
        MethodInfo run = probe.GetMethod("Run", BindingFlags.Public | BindingFlags.Static);
        MethodInfo deliberate = probe.GetMethod("ThrowForHostBoundaryTest", BindingFlags.Public | BindingFlags.Static);
        Check(run != null && run.ReturnType == typeof(int) && run.GetParameters().Length == 1 && run.GetParameters()[0].ParameterType == typeof(int), "actual static Run signature");
        Check(deliberate != null && deliberate.ReturnType == typeof(void) && deliberate.GetParameters().Length == 0, "actual static Throw signature");
        Check(assembly.GetReferencedAssemblies().Select(x => x.Name).SequenceEqual(new[] { "mscorlib" }), "framework reference binding");
        nint native = NativeLibrary.Load(Path.GetFullPath(args[1]));
        // CoreCLR caches resolved addresses. Retain test provider until this process exits;
        // deliberately do not NativeLibrary.Free while import stubs still refer to it.
        NativeLibrary.SetDllImportResolver(assembly, (name, _, __) => {
            Check(name == "__Internal", "exact library policy"); return native;
        });
        Begin begin = Marshal.GetDelegateForFunctionPointer<Begin>(NativeLibrary.GetExport(native, "cap10_host_begin"));
        Read read = Marshal.GetDelegateForFunctionPointer<Read>(NativeLibrary.GetExport(native, "cap10_host_read"));
        Action<int> checkRun = cookie => {
            begin(1, cookie);
            object result = run.Invoke(null, new object[] { cookie });
            Check(result != null && result.GetType() == typeof(int) && (int)result == 0, "actual boxed managed result");
            Check(read(0) == 1 && read(1) == 1 && read(2) == 0 && read(3) == cookie && read(4) == cookie, "actual native call order/count/cookie/reply");
        };
        checkRun(0x1234567);
        begin(0, 0);
        bool expectedException = false;
        try { deliberate.Invoke(null, null); }
        catch (TargetInvocationException exc) { expectedException = exc.InnerException?.GetType() == typeof(InvalidOperationException); }
        Check(expectedException, "actual deliberate managed exception class");
        Check(read(0) == 0 && read(1) == 0 && read(2) == 0, "zero native calls during deliberate throw");
        checkRun(0x2345678);
        Console.WriteLine("{\"verdict\":\"REAL_PROBE_HOST_PASS\",\"runtime\":\"CoreCLR .NET10 Mac only\",\"assembly_sha256\":\"" + digest + "\",\"real_managed_runs\":2,\"real_managed_exceptions\":1,\"native_abi_calls\":2,\"native_probe_calls\":2,\"mono_proof\":false,\"townfall_proof\":false,\"engine_readiness_proof\":false}");
        return 0;
    }
}
