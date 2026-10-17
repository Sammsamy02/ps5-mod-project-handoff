using System;
using System.Reflection;
static class Refs
{
    static int Main(string[] args)
    {
        var probe = Assembly.LoadFrom(args[0]);
        foreach (var r in probe.GetReferencedAssemblies())
            Console.WriteLine("Probe references: " + r.FullName);
        var corlib = typeof(object).Assembly;
        Console.WriteLine("Runtime corlib: " + corlib.GetName().FullName);
        Console.WriteLine("Runtime corlib path: " + corlib.Location);
        Console.WriteLine("ImageRuntimeVersion: " + corlib.ImageRuntimeVersion);
        return 0;
    }
}
