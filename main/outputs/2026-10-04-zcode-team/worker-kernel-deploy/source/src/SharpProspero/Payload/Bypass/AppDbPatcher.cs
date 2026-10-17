// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Posix;
using System;

namespace SharpProspero.Payload.Bypass;

/// <summary>
/// App.db DRM type patcher. Modifies the application database to change the DRM type
/// of installed titles, enabling them to run without license verification.
/// </summary>
/// <remarks>
/// Requires dynamically loading <c>libsqlite3</c> via <see cref="PayloadDlfcn"/> since
/// SQLite is not a standard SPRX module.
/// </remarks>
public static unsafe class PayloadAppDbPatcher
{
    /// <summary>Path to the application database.</summary>
    public static ReadOnlySpan<byte> AppDbPath =>
        "/system_data/priv/mms/app.db\0"u8;

    /// <summary>
    /// SQLite function pointers resolved at runtime through <see cref="PayloadDlfcn"/>.
    /// </summary>
    public struct SqliteFunctions
    {
        /// <summary>sqlite3_open_v2.</summary>
        public nint Open;

        /// <summary>sqlite3_prepare_v2.</summary>
        public nint Prepare;

        /// <summary>sqlite3_step.</summary>
        public nint Step;

        /// <summary>sqlite3_finalize.</summary>
        public nint Finalize;

        /// <summary>sqlite3_close.</summary>
        public nint Close;

        /// <summary>sqlite3_exec.</summary>
        public nint Exec;
    }

    /// <summary>
    /// Resolves the SQLite function pointers by loading <c>libsqlite3</c> at runtime.
    /// </summary>
    /// <returns>The resolved function pointers, or a zeroed struct on failure.</returns>
    public static SqliteFunctions LoadSqlite()
    {
        void* handle = PayloadDlfcn.Open("libsqlite3.sprx\0"u8, PayloadDlfcn.RtldLazy);
        if (handle == null) return default;

        SqliteFunctions fns;
        fns.Open = (nint)PayloadDlfcn.Sym(handle, "sqlite3_open_v2\0"u8);
        fns.Prepare = (nint)PayloadDlfcn.Sym(handle, "sqlite3_prepare_v2\0"u8);
        fns.Step = (nint)PayloadDlfcn.Sym(handle, "sqlite3_step\0"u8);
        fns.Finalize = (nint)PayloadDlfcn.Sym(handle, "sqlite3_finalize\0"u8);
        fns.Close = (nint)PayloadDlfcn.Sym(handle, "sqlite3_close\0"u8);
        fns.Exec = (nint)PayloadDlfcn.Sym(handle, "sqlite3_exec\0"u8);
        return fns;
    }

    /// <summary>
    /// Patches the application database to change the DRM type of installed titles.
    /// Opens <c>app.db</c>, executes an SQL statement to update the DRM type field,
    /// and closes the database.
    /// </summary>
    /// <param name="fns">Resolved SQLite function pointers from <see cref="LoadSqlite"/>.</param>
    /// <param name="sql">A NUL-terminated SQL statement to execute.</param>
    /// <returns>Zero on success, or a non-zero SQLite error code.</returns>
    public static int ExecuteSql(SqliteFunctions fns, byte* sql)
    {
        if (fns.Open == 0 || fns.Exec == 0 || fns.Close == 0) return -1;

        nint db = 0;
        fixed (byte* path = AppDbPath)
        {
            // sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE, null)
            var openFn = (delegate* unmanaged<byte*, nint*, int, nint, int>)fns.Open;
            int rc = openFn(path, &db, 2, 0); // SQLITE_OPEN_READWRITE = 2
            if (rc != 0) return rc;
        }

        // sqlite3_exec(db, sql, null, null, null)
        var execFn = (delegate* unmanaged<nint, byte*, nint, nint, nint*, int>)fns.Exec;
        int result = execFn(db, sql, 0, 0, null);

        // sqlite3_close(db)
        var closeFn = (delegate* unmanaged<nint, int>)fns.Close;
        closeFn(db);

        return result;
    }

    /// <summary>
    /// Patches the DRM type of all disc-type titles in the application database to
    /// enable them to run without a disc. Changes <c>appDrmType</c> from 1 (disc) to
    /// 5 (digital) in the app table.
    /// </summary>
    public static int PatchDrmType(SqliteFunctions fns)
    {
        byte* sql = stackalloc byte[] {
            (byte)'U', (byte)'P', (byte)'D', (byte)'A', (byte)'T', (byte)'E', (byte)' ',
            (byte)'t', (byte)'b', (byte)'l', (byte)'_', (byte)'a', (byte)'p', (byte)'p',
            (byte)'i', (byte)'n', (byte)'f', (byte)'o', (byte)' ',
            (byte)'S', (byte)'E', (byte)'T', (byte)' ',
            (byte)'v', (byte)'a', (byte)'l', (byte)' ', (byte)'=', (byte)' ', (byte)'5',
            (byte)' ', (byte)'W', (byte)'H', (byte)'E', (byte)'R', (byte)'E', (byte)' ',
            (byte)'k', (byte)'e', (byte)'y', (byte)' ', (byte)'=', (byte)' ',
            (byte)'\'', (byte)'a', (byte)'p', (byte)'p', (byte)'D', (byte)'r', (byte)'m',
            (byte)'T', (byte)'y', (byte)'p', (byte)'e', (byte)'\'',
            (byte)' ', (byte)'A', (byte)'N', (byte)'D', (byte)' ',
            (byte)'v', (byte)'a', (byte)'l', (byte)' ', (byte)'=', (byte)' ', (byte)'1',
            0 };
        return ExecuteSql(fns, sql);
    }
}
