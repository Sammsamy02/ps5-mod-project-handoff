// CAP13 managed route — mod-facing SDK contract. SOURCE STAGE ONLY.
// Compiled by the integrator with the cap10 recipe (pinned Roslyn csc,
// /nostdlib+ /noconfig, Mono-selected v4.7.2 reference mscorlib) into
// Mbm.ModSdk.dll. See DESIGN/managed-dll-route.md section 6b.
//
// HARD CONSTRAINT: mscorlib-only. The target runtime deploys ONLY the
// statically registered corlib today (cap10 packaging). No System.Core,
// no System.dll, no LINQ, no System.IO in this assembly. Any BCL reference
// beyond mscorlib here would fail the admission closure at discovery.
using System;
using System.Runtime.InteropServices;

namespace Mbm.ModSdk
{
    /// <summary>Declaration attribute every admitted mod carries on its
    /// MbmMod subclass. Discovery double-checks it managed-side; the native
    /// facts record is authoritative for admission.</summary>
    [AttributeUsage(AttributeTargets.Class, AllowMultiple = false, Inherited = false)]
    public sealed class MbmModAttribute : Attribute
    {
        public string Id { get; }
        public string Name { get; }
        public string Version { get; }
        public MbmModAttribute(string id, string name, string version)
        {
            Id = id; Name = name; Version = version;
        }
    }

    /// <summary>Status codes shared with abi/MbmManagedAbi.h. Kept literal on
    /// purpose: this SDK must not reference the native header.</summary>
    internal static class Cs
    {
        internal const int Ok = 0;
        internal const int BadArgument = -1;
        internal const int Unsupported = -2;
        internal const int NotFound = -3;
        internal const int Marshal = -4;
        internal const int EngineUnbound = -5;
        internal const int BufferTooSmall = -6;
        internal const int HostNotReady = -7;
    }

    /// <summary>Host services handed to a mod at OnLoad. Wraps the frozen v1
    /// __Internal ABI. All methods are failure-tolerant: they return false /
    /// -1 style results and never throw for host-side failures, so a mod's
    /// error handling stays in its own hands. Mod code is allowed to throw;
    /// the loader isolates that per-mod.</summary>
    public sealed class ModHost
    {
        internal ModHost(string assemblyName) { _assemblyName = assemblyName; }

        private readonly string _assemblyName;
        /// <summary>Short name of the mod assembly this host instance serves.</summary>
        public string AssemblyName { get { return _assemblyName; } }

        private bool _stopped;
        /// <summary>Mod requests quiesce: its OnTick stops firing. Idempotent.</summary>
        public void Stop() { _stopped = true; }
        internal bool StopRequested { get { return _stopped; } }

        /// <summary>Log through the host's diagnostic channel. Never throws.</summary>
        public bool Log(int level, string message)
        {
            if (message == null || level < 0 || level > 3) return false;
            return Native.Log(level, message) == Cs.Ok;
        }

        /// <summary>Engine ticks since resident-host start. -1 if unavailable.</summary>
        public long TickCount
        {
            get
            {
                long value;
                return Native.HostQuery(Native.QueryTickCount, out value) == Cs.Ok ? value : -1;
            }
        }

        /// <summary>Find an engine object by its path. Returns null when the
        /// surface is unbound, the path is missing, or the host is not in
        /// steady state. Caller MUST Release every non-null result.</summary>
        public EngineObject FindObject(string objectPath)
        {
            if (objectPath == null || objectPath.Length == 0) return null;
            IntPtr handle;
            int status = Native.FindObject(objectPath, out handle);
            if (status != Cs.Ok || handle == IntPtr.Zero) return null;
            return new EngineObject(this, handle);
        }

        internal void Release(IntPtr handle)
        {
            if (handle == IntPtr.Zero) return;
            IntPtr doomed = handle;
            Native.Release(ref doomed);
        }

        /// <summary>An engine object handle. v1 is strictly read-only and
        /// numeric-property-only by design (see the support matrix). Methods
        /// return false instead of throwing for engine-side failures.</summary>
        public sealed class EngineObject
        {
            internal EngineObject(ModHost owner, IntPtr handle)
            {
                _owner = owner; _handle = handle; _released = false;
            }

            private readonly ModHost _owner;
            private IntPtr _handle;
            private bool _released;

            public bool ReadNumber(string propertyName, out double value)
            {
                value = 0.0;
                if (_released || propertyName == null) return false;
                double read;
                int status = Native.ReadNumber(_handle, propertyName, out read);
                if (status != Cs.Ok) return false;
                value = read;
                return true;
            }

            /// <summary>Release the native handle. Idempotent; the object is
            /// unusable afterwards.</summary>
            public void Release()
            {
                if (_released) return;
                _released = true;
                IntPtr doomed = _handle;
                _owner.Release(doomed);
                _handle = IntPtr.Zero;
            }
        }
    }

    /// <summary>The mod base class. Subclass it, mark it with
    /// [MbmMod(...)], give it a public parameterless constructor. Lifecycle:
    /// OnLoad once after load; OnTick once per admitted engine tick;
    /// OnUnload once at shutdown. Every callback is exception-isolated per
    /// mod: a throwing callback is counted and the mod is disabled after 3
    /// consecutive failures (never crashes the tick loop or sibling mods).</summary>
    public abstract class MbmMod
    {
        /// <summary>Valid until OnUnload returns; null only if the loader
        /// could not construct a host (then OnLoad is not called either).</summary>
        public ModHost Host { get; internal set; }

        /// <summary>Called once, after this mod's assembly passed admission
        /// and its dependencies loaded. Exception here = mod disabled.</summary>
        public virtual void OnLoad(ModHost host) { }

        /// <summary>Called once per engine tick while enabled. Exception
        /// here = counted; 3 consecutive = mod disabled.</summary>
        public virtual void OnTick() { }

        /// <summary>Best-effort shutdown call. "Unload" means quiesce: the
        /// assembly stays resident for the process lifetime.</summary>
        public virtual void OnUnload() { }
    }

    /// <summary>__Internal P/Invokes onto the frozen v1 ABI
    /// (abi/MbmManagedAbi.h). Exact spelling, Cdecl, no exceptions across the
    /// boundary. On target these resolve through the cap13 owned-name
    /// dl-fallback provider — the only sanctioned interop channel.</summary>
    internal static class Native
    {
        private const string Bridge = "__Internal";

        // mbm_cs_host_query keys (abi/MbmManagedAbi.h).
        internal const int QueryTickCount = 1;
        internal const int QueryModCount = 2;
        internal const int QueryBootId = 3;

        [DllImport(Bridge, EntryPoint = "mbm_cs_abi_version",
            CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int AbiVersion();

        [DllImport(Bridge, EntryPoint = "mbm_cs_log",
            CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern int LogRaw(int level, string message);
        internal static int Log(int level, string message)
        {
            if (message == null) return Cs.BadArgument;
            return LogRaw(level, message);
        }

        [DllImport(Bridge, EntryPoint = "mbm_cs_host_query",
            CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern int HostQueryRaw(int key, out long value);
        internal static int HostQuery(int key, out long value)
        {
            value = 0;
            return HostQueryRaw(key, out value);
        }

        [DllImport(Bridge, EntryPoint = "mbm_cs_engine_find_object",
            CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern int FindObjectRaw(string objectPath, out IntPtr handle);
        internal static int FindObject(string objectPath, out IntPtr handle)
        {
            handle = IntPtr.Zero;
            if (objectPath == null) return Cs.BadArgument;
            return FindObjectRaw(objectPath, out handle);
        }

        [DllImport(Bridge, EntryPoint = "mbm_cs_engine_read_number",
            CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern int ReadNumberRaw(IntPtr handle, string propertyName,
            out double value);
        internal static int ReadNumber(IntPtr handle, string propertyName, out double value)
        {
            value = 0.0;
            if (handle == IntPtr.Zero || propertyName == null) return Cs.BadArgument;
            return ReadNumberRaw(handle, propertyName, out value);
        }

        [DllImport(Bridge, EntryPoint = "mbm_cs_engine_release",
            CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        private static extern int ReleaseRaw(ref IntPtr handle);
        internal static int Release(ref IntPtr handle) { return ReleaseRaw(ref handle); }
    }
}
