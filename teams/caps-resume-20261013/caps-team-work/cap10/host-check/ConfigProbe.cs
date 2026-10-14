// Host-only regression for the relocated embedding configuration, not a mod.
using System.IO;
public static class ConfigProbe
{
    public static int ReadByteCount(string path) { return File.ReadAllBytes(path).Length; }
}
