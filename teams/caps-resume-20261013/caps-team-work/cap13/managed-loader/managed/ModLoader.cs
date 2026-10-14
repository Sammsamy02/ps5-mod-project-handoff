// CAP13 managed route — managed-side loader. SOURCE STAGE ONLY.
// Compiled with the cap10 recipe into Mbm.ModLoader.dll; the integrator
// embeds those exact bytes (hash-pinned) the way cap10 embeds ProbeImage
// (a placeholder MbmLoaderImage.hpp stages that slot — fail-closed until
// regenerated from a real compile).
//
// HARD CONSTRAINT: mscorlib-only (see DESIGN/managed-dll-route.md section
// 3a). Reflection, AppDomain, StringBuilder, List<T> are corlib in .NET 4.x;
// no System.Core / no LINQ / no System.dll anywhere in this file.
//
// Role split (by design): the NATIVE bridge owns discovery, hashing,
// admission closure, image loading and the P/Invoke provider. This managed
// loader owns what is genuinely a type-system job: binding the admitted
// assemblies, reflection discovery of MbmMod subclasses, lifecycle
// dispatch, and per-mod exception isolation. It performs NO file I/O
// (System.Native is unproven on target) and NO paths (native owns the
// mod-root overlay); it sees only admitted assembly short names.
using System;
using System.Collections.Generic;
using System.Reflection;
using System.Text;
using Mbm.ModSdk;

namespace Mbm.ModLoader
{
    public static class Loader
    {
        // ---- loader-level status codes (returned by LoaderMain) ----
        // 0                      : all admitted mods loaded (or none admitted)
        // -1 .. -99              : negated count of mods that failed to load
        // -100                   : loader-level failure (ABI/environment)
        private const int LoaderFailure = -100;

        // Consecutive OnTick failures before a mod is disabled (design 3d).
        private const int TickFailureLimit = 3;

        private sealed class ModState
        {
            internal MbmMod Instance;
            internal ModHost Host;
            internal string DisplayName;
            internal int ConsecutiveTickFailures;
            internal bool TickEnabled;
            internal bool Loaded;
        }

        private static readonly List<ModState> Mods = new List<ModState>();
        private static bool _booted;

        /// <summary>Entry invoked by the native bridge exactly once, after
        /// ModSdk and all admitted mod assemblies are loaded and their
        /// __Internal dllmaps inserted. Signature (static int(int)) matches
        /// the cap10 InvocationBoundary admission shape on purpose.</summary>
        public static int LoaderMain(int cookie)
        {
            int failed = 0;
            try
            {
                if (Native.AbiVersion() != 1) return LoaderFailure;
                if (_booted) return LoaderFailure; // one boot per runtime
                _booted = true;

                int count = 0;
                if (Native.HostQuery(Native.QueryModCount, out long modCount) == 0)
                {
                    if (modCount > 0 && modCount <= 256) count = (int)modCount;
                }

                for (int i = 0; i < count; i++)
                {
                    if (!LoadOne(i)) failed++;
                }
                Log(Mbm.LogInfo, "loader boot cookie=" + cookie +
                    " admitted=" + count + " failed=" + failed);
            }
            catch (Exception e)
            {
                Log(Mbm.LogError, "loader main exception: " + e.GetType().FullName);
                return LoaderFailure;
            }
            return failed == 0 ? 0 : -failed;
        }

        /// <summary>Bind admitted assembly i, find its MbmMod subclass,
        /// construct it and run OnLoad. One bad mod never stops the loop.</summary>
        private static bool LoadOne(int index)
        {
            string assemblyName;
            int status = Native.ModAssemblyName(index, out assemblyName);
            if (status != 0 || string.IsNullOrEmpty(assemblyName))
            {
                Log(Mbm.LogError, "mod[" + index + "] name unavailable status=" + status);
                return false;
            }
            try
            {
                Assembly assembly = FindAdmittedAssembly(assemblyName);
                if (assembly == null)
                {
                    Log(Mbm.LogError, "mod '" + assemblyName + "' not bound in domain");
                    return false;
                }

                Type[] types;
                try
                {
                    types = assembly.GetTypes();
                }
                catch (ReflectionTypeLoadException rtl)
                {
                    // Surface per-type loader exceptions precisely, never swallow.
                    Exception[] loaderExceptions = rtl.LoaderExceptions;
                    int shown = loaderExceptions == null ? 0 : loaderExceptions.Length;
                    for (int k = 0; k < shown && k < 4; k++)
                    {
                        Exception le = loaderExceptions[k];
                        Log(Mbm.LogError, "mod '" + assemblyName + "' type loader exception[" +
                            k + "]: " + (le == null ? "null" : le.GetType().FullName));
                    }
                    types = rtl.Types; // admit whatever actually loaded
                }
                if (types == null)
                {
                    Log(Mbm.LogError, "mod '" + assemblyName + "' exposed no types");
                    return false;
                }

                Type modType = FindModType(assemblyName, types);
                if (modType == null) return false; // precise log inside

                ModState state = new ModState();
                state.DisplayName = assemblyName + ":" + modType.Name;

                object instance = Activator.CreateInstance(modType);
                MbmMod mod = instance as MbmMod;
                if (mod == null)
                {
                    Log(Mbm.LogError, "mod '" + state.DisplayName +
                        "' constructed a non-MbmMod instance");
                    return false;
                }

                state.Host = new ModHost(assemblyName);
                state.Instance = mod;
                mod.Host = state.Host;

                // OnLoad under per-mod exception isolation.
                try
                {
                    mod.OnLoad(state.Host);
                    state.Loaded = true;
                    state.TickEnabled = true;
                    Mods.Add(state);
                    Log(Mbm.LogInfo, "mod '" + state.DisplayName + "' loaded");
                    return true;
                }
                catch (Exception e)
                {
                    mod.Host = null;
                    Log(Mbm.LogError, "mod '" + state.DisplayName +
                        "' OnLoad exception: " + e.GetType().FullName);
                    return false;
                }
            }
            catch (Exception e)
            {
                Log(Mbm.LogError, "mod '" + assemblyName +
                    "' load exception: " + e.GetType().FullName);
                return false;
            }
        }

        /// <summary>Name-match against assemblies the native bridge already
        /// registered (design section 3b). No Assembly.Load probing: the
        /// ApplicationBase/path probing story does not exist here.</summary>
        private static Assembly FindAdmittedAssembly(string assemblyName)
        {
            Assembly[] loaded = AppDomain.CurrentDomain.GetAssemblies();
            for (int i = 0; i < loaded.Length; i++)
            {
                Assembly candidate = loaded[i];
                if (candidate == null || candidate.IsDynamic) continue;
                AssemblyName name = candidate.GetName();
                if (name != null && name.Name == assemblyName) return candidate;
            }
            return null;
        }

        /// <summary>Discovery rules (design section 2d): non-abstract MbmMod
        /// subclass with [MbmMod] and a public parameterless constructor.
        /// Framework-shape detection is defense in depth: BepInEx/Melon base
        /// classes found in the hierarchy produce a precise refusal even if
        /// native facts missed the shape.</summary>
        private static Type FindModType(string assemblyName, Type[] types)
        {
            Type found = null;
            for (int i = 0; i < types.Length; i++)
            {
                Type t = types[i];
                if (t == null || !t.IsClass || t.IsAbstract) continue;

                string framework = FrameworkShapeOf(t);
                if (framework != null)
                {
                    Log(Mbm.LogError, "mod '" + assemblyName + "' type '" + t.FullName +
                        "' is " + framework + "-shape; framework shim unsupported on " +
                        "this host (Unreal game, no Unity managed surface)");
                    return null;
                }
                if (!IsMbmModSubclass(t)) continue;
                object[] attrs = t.GetCustomAttributes(typeof(MbmModAttribute), false);
                if (attrs == null || attrs.Length == 0)
                {
                    Log(Mbm.LogError, "mod '" + assemblyName + "' type '" + t.FullName +
                        "' subclasses MbmMod without [MbmMod]");
                    return null;
                }
                if (t.GetConstructor(Type.EmptyTypes) == null)
                {
                    Log(Mbm.LogError, "mod '" + assemblyName + "' type '" + t.FullName +
                        "' has no public parameterless constructor");
                    return null;
                }
                if (found != null)
                {
                    Log(Mbm.LogError, "mod '" + assemblyName +
                        "' declares more than one MbmMod type ('" + found.FullName +
                        "', '" + t.FullName + "')");
                    return null;
                }
                found = t;
            }
            if (found == null)
                Log(Mbm.LogError, "mod '" + assemblyName + "' exposes no MbmMod type");
            return found;
        }

        private static bool IsMbmModSubclass(Type t)
        {
            for (Type walk = t.BaseType; walk != null; walk = walk.BaseType)
            {
                if (walk == typeof(MbmMod)) return true;
            }
            return false;
        }

        /// <summary>Walks base-type full names by string because this loader
        /// does NOT reference the framework assemblies (they are refused at
        /// admission; this only recognizes the shape to name it precisely).</summary>
        private static string FrameworkShapeOf(Type t)
        {
            for (Type walk = t.BaseType; walk != null; walk = walk.BaseType)
            {
                string fullName = walk.FullName;
                if (fullName == null) continue;
                if (fullName == "BepInEx.BaseUnityPlugin") return "BepInEx";
                if (fullName == "MelonLoader.MelonMod") return "MelonLoader";
            }
            return null;
        }

        /// <summary>Native tick entry. Static void — admitted with the same
        /// signature discipline as the cap10 throw-method shape. Iterates a
        /// snapshot of live mods; per-mod exceptions counted, 3 consecutive
        /// failures disable that mod's tick. Never throws to native.</summary>
        public static void Tick()
        {
            try
            {
                for (int i = 0; i < Mods.Count; i++)
                {
                    ModState state = Mods[i];
                    if (state == null || !state.Loaded || !state.TickEnabled) continue;
                    if (state.Host != null && state.Host.StopRequested)
                    {
                        state.TickEnabled = false;
                        Log(Mbm.LogInfo, "mod '" + state.DisplayName + "' requested stop");
                        continue;
                    }
                    try
                    {
                        state.Instance.OnTick();
                        state.ConsecutiveTickFailures = 0;
                    }
                    catch (Exception e)
                    {
                        state.ConsecutiveTickFailures++;
                        Log(Mbm.LogWarning, "mod '" + state.DisplayName +
                            "' tick exception " + state.ConsecutiveTickFailures + "/" +
                            TickFailureLimit + ": " + e.GetType().FullName);
                        if (state.ConsecutiveTickFailures >= TickFailureLimit)
                        {
                            state.TickEnabled = false;
                            Log(Mbm.LogError, "mod '" + state.DisplayName +
                                "' tick disabled after " + TickFailureLimit +
                                " consecutive failures");
                        }
                    }
                }
            }
            catch (Exception e)
            {
                // Loader-loop defect, not a mod exception: report and survive.
                Log(Mbm.LogError, "tick loop exception: " + e.GetType().FullName);
            }
        }

        /// <summary>Native shutdown entry (init thread). Best-effort OnUnload
        /// per mod, then state is cleared. Assemblies stay resident in the
        /// domain — quiesce, not reclaim (design section 3d, honest).</summary>
        public static void Unload()
        {
            try
            {
                for (int i = 0; i < Mods.Count; i++)
                {
                    ModState state = Mods[i];
                    if (state == null || !state.Loaded) continue;
                    try
                    {
                        state.Instance.OnUnload();
                        Log(Mbm.LogInfo, "mod '" + state.DisplayName + "' unloaded (quiesced)");
                    }
                    catch (Exception e)
                    {
                        Log(Mbm.LogError, "mod '" + state.DisplayName +
                            "' OnUnload exception: " + e.GetType().FullName);
                    }
                    state.TickEnabled = false;
                    state.Instance.Host = null;
                }
            }
            catch (Exception e)
            {
                Log(Mbm.LogError, "unload loop exception: " + e.GetType().FullName);
            }
            finally
            {
                Mods.Clear();
            }
        }

        // ---- logging through the host channel (no console, no file I/O) ----

        private const int BridgeBufferStart = 256;

        private static void Log(int level, string message)
        {
            // Defensive truncation: the native bridge rejects oversize anyway;
            // keeping messages bounded here avoids per-call failure noise.
            if (message == null) message = "(null)";
            if (message.Length > BridgeBufferStart) message = message.Substring(0, BridgeBufferStart);
            Native.Log(level, message);
        }
    }

    /// <summary>Log levels mirrored from abi/MbmManagedAbi.h.</summary>
    internal static class Mbm
    {
        internal const int LogTrace = 0;
        internal const int LogInfo = 1;
        internal const int LogWarn = 2;
        internal const int LogError = 3;
        internal const int LogWarning = LogWarn; // readability alias
    }

    /// <summary>__Internal P/Invokes for the loader itself. These resolve on
    /// the ModLoader image's own dllmap inserts (per-symbol, cap10 form).
    /// byte[] (not string) for name retrieval keeps marshaling deterministic;
    /// ASCII decode via Encoding (corlib) — see abi buffer contract.</summary>
    internal static class Native
    {
        private const string Bridge = "__Internal";

        internal const int QueryTickCount = 1;
        internal const int QueryModCount = 2;
        internal const int QueryBootId = 3;

        [DllImport(Bridge, EntryPoint = "mbm_cs_abi_version",
            CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int AbiVersion();

        [DllImport(Bridge, EntryPoint = "mbm_cs_log",
            CallingConvention = CallingConvention.Cdecl, ExactSpelling = true,
            CharSet = CharSet.Ansi)]
        internal static extern int Log(int level, string message);

        [DllImport(Bridge, EntryPoint = "mbm_cs_host_query",
            CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int HostQuery(int key, out long value);

        [DllImport(Bridge, EntryPoint = "mbm_cs_mod_assembly",
            CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern int ModAssemblyRaw(int index, byte[] buffer, ref int len);

        internal static int ModAssemblyName(int index, out string name)
        {
            name = null;
            byte[] buffer = new byte[192];
            int len = buffer.Length;
            int status = ModAssemblyRaw(index, buffer, ref len);
            if (status == -6 && len > 0 && len <= 4096) // BUFFER_TOO_SMALL: retry once
            {
                buffer = new byte[len];
                len = buffer.Length;
                status = ModAssemblyRaw(index, buffer, ref len);
            }
            if (status != 0) return status;
            int end = len;
            if (end > buffer.Length) end = buffer.Length;
            if (end > 0 && buffer[end - 1] == 0) end--; // strip NUL per ABI contract
            name = Encoding.ASCII.GetString(buffer, 0, end);
            return 0;
        }
    }
}
