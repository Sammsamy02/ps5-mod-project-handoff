// Actual Mono --interpreter HOST execution. No target/strict resolver/AOT-only claim.
using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
internal static class MonoInterpreterHarness
{
    [DllImport("cap10-host-test", EntryPoint="cap10_host_begin", CallingConvention=CallingConvention.Cdecl, ExactSpelling=true)]
    static extern void Begin(int kind, int cookie);
    [DllImport("cap10-host-test", EntryPoint="cap10_host_read", CallingConvention=CallingConvention.Cdecl, ExactSpelling=true)]
    static extern int Read(int index);
    static void Check(bool value, string why) { if (!value) throw new Exception(why); }
    static string Digest(byte[] bytes) { using (var sha=SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant(); }
    static void Run(MethodInfo method, int cookie)
    {
        Begin(1,cookie);
        object result=method.Invoke(null,new object[]{cookie});
        Check(result!=null && result.GetType()==typeof(int) && (int)result==0,"actual boxed result");
        Check(Read(0)==1 && Read(5)==1 && Read(1)==1 && Read(6)==1 && Read(2)==0 && Read(3)==cookie && Read(4)==cookie && Read(7)==1 && Read(8)==0 && Read(9)==1,"actual native entry/exit/order/cookie/reply");
    }
    static int Main(string[] args)
    {
        try
        {
            const string expected="c328dca11447b2c41ab1580582b9576911b0291a4d14d89a09a708e69e1c108d";
            Check(Digest(File.ReadAllBytes(args[0]))==expected,"actual input identity");
            var assembly=Assembly.LoadFrom(args[0]);var probe=assembly.GetType("ModBridge.Cap10.Probe",true);
            var run=probe.GetMethod("Run",BindingFlags.Public|BindingFlags.Static);
            var deliberate=probe.GetMethod("ThrowForHostBoundaryTest",BindingFlags.Public|BindingFlags.Static);
            Check(run!=null && run.ReturnType==typeof(int) && run.GetParameters().Length==1 && run.GetParameters()[0].ParameterType==typeof(int),"actual static signature");
            Check(deliberate!=null && deliberate.ReturnType==typeof(void) && deliberate.GetParameters().Length==0,"actual throw signature");
            Run(run,0x56789ab);Begin(0,0);bool threw=false;
            try { deliberate.Invoke(null,null); }
            catch(TargetInvocationException ex) { threw=ex.InnerException!=null && ex.InnerException.GetType()==typeof(InvalidOperationException); }
            Check(threw && Read(0)==0 && Read(5)==0 && Read(1)==0 && Read(6)==0 && Read(2)==0,"actual deliberate exception/zero native calls");
            Run(run,0x6789abc);Check(Digest(File.ReadAllBytes(args[0]))==expected,"input unchanged");
            Console.WriteLine("{\"verdict\":\"REAL_MONO_INTERPRETER_HOST_PASS\",\"real_managed_runs\":2,\"real_managed_exceptions\":1,\"interpreter_cli_requested\":true,\"native_jit_disabled_proof\":false,\"aot_only_mode_proof\":false,\"strict_resolver_proof\":false,\"townfall_proof\":false}");return 0;
        }
        catch(Exception ex) { Console.Error.WriteLine(ex);return 1; }
    }
}
