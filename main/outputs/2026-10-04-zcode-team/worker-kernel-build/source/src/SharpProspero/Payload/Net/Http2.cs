// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Net;

/// <summary>
/// HTTP/2 client for a payload context. Wraps the full HTTP/2 lifecycle: network
/// initialisation, TLS context, HTTP/2 init, template creation, request creation, send,
/// status retrieval, and response reading.
/// </summary>
/// <remarks>
/// <para>The HTTP/2 lifecycle uses three SPRX modules:</para>
/// <list type="bullet">
/// <item><description><c>libSceNet</c> for <c>sceNetInit</c> and <c>sceNetPoolCreate/Destroy</c></description></item>
/// <item><description><c>libSceSsl</c> for <c>sceSslInit/Term</c></description></item>
/// <item><description><c>libSceHttp2</c> for all HTTP/2 operations</description></item>
/// </list>
/// <para>A payload template using this class must include all three in its DT_NEEDED list and
/// add <c>&lt;DirectPInvoke Include="libSceNet" /&gt;</c>,
/// <c>&lt;DirectPInvoke Include="libSceSsl" /&gt;</c>, and
/// <c>&lt;DirectPInvoke Include="libSceHttp2" /&gt;</c> in its csproj.</para>
/// </remarks>
public static unsafe partial class PayloadHttp2
{
    // ---- libSceNet ----

    private const string LibNet = "libSceNet";

    /// <summary>Initialises the network library.</summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(LibNet)]
    public static partial int sceNetInit();

    /// <summary>Creates a network memory pool.</summary>
    /// <param name="name">A NUL-terminated UTF-8 pool name.</param>
    /// <param name="size">Pool size in bytes.</param>
    /// <param name="flags">Pool flags (typically 0).</param>
    /// <returns>A pool id on success, or a negative error code.</returns>
    [LibraryImport(LibNet)]
    public static partial int sceNetPoolCreate(byte* name, int size, int flags);

    /// <summary>Destroys a network memory pool.</summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(LibNet)]
    public static partial int sceNetPoolDestroy(int memId);

    // ---- libSceSsl ----

    private const string LibSsl = "libSceSsl";

    /// <summary>Starts the TLS service with a pool of <paramref name="poolSize"/> bytes.</summary>
    /// <returns>A TLS context id on success, or a negative error code.</returns>
    [LibraryImport(LibSsl)]
    public static partial int sceSslInit(nuint poolSize);

    /// <summary>Stops the TLS service.</summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(LibSsl)]
    public static partial int sceSslTerm(int sslCtxId);

    // ---- libSceHttp2 ----

    private const string LibHttp2 = "libSceHttp2";

    /// <summary>Initialises the HTTP/2 service.</summary>
    /// <param name="netMemId">A network pool id from <see cref="sceNetPoolCreate"/>.</param>
    /// <param name="sslCtxId">A TLS context id from <see cref="sceSslInit"/>.</param>
    /// <param name="poolSize">HTTP/2 pool size in bytes.</param>
    /// <param name="flags">Flags (typical value is 1).</param>
    /// <returns>An HTTP/2 context id on success, or a negative error code.</returns>
    [LibraryImport(LibHttp2)]
    public static partial int sceHttp2Init(int netMemId, int sslCtxId, nuint poolSize, int flags);

    /// <summary>Terminates the HTTP/2 service.</summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(LibHttp2)]
    public static partial int sceHttp2Term(int httpCtxId);

    /// <summary>Creates an HTTP/2 request template.</summary>
    /// <param name="httpCtxId">An HTTP/2 context id from <see cref="sceHttp2Init"/>.</param>
    /// <param name="userAgent">A NUL-terminated UTF-8 user-agent string.</param>
    /// <param name="httpVer">HTTP version (typical value is 3 for HTTP/2).</param>
    /// <param name="isAutoProxyConf">Whether to automatically configure proxy (typical value is
    /// 1).</param>
    /// <returns>A template id on success, or a negative error code.</returns>
    [LibraryImport(LibHttp2)]
    public static partial int sceHttp2CreateTemplate(int httpCtxId, byte* userAgent, int httpVer,
        int isAutoProxyConf);

    /// <summary>Deletes an HTTP/2 template.</summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(LibHttp2)]
    public static partial int sceHttp2DeleteTemplate(int tmplId);

    /// <summary>Creates an HTTP/2 request from a template with the given URL.</summary>
    /// <param name="tmplId">A template id from <see cref="sceHttp2CreateTemplate"/>.</param>
    /// <param name="method">A NUL-terminated UTF-8 method string (e.g. "GET\0").</param>
    /// <param name="url">A NUL-terminated UTF-8 URL string.</param>
    /// <param name="contentLength">Content length for request body (0 for GET).</param>
    /// <returns>A request id on success, or a negative error code.</returns>
    [LibraryImport(LibHttp2)]
    public static partial int sceHttp2CreateRequestWithURL(int tmplId, byte* method, byte* url,
        ulong contentLength);

    /// <summary>Deletes an HTTP/2 request.</summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(LibHttp2)]
    public static partial int sceHttp2DeleteRequest(int reqId);

    /// <summary>Sends an HTTP/2 request, optionally with body data.</summary>
    /// <param name="reqId">A request id from <see cref="sceHttp2CreateRequestWithURL"/>.</param>
    /// <param name="postData">Body data, or null for a bodyless request (e.g. GET).</param>
    /// <param name="size">Size of <paramref name="postData"/> in bytes.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(LibHttp2)]
    public static partial int sceHttp2SendRequest(int reqId, void* postData, nuint size);

    /// <summary>Reads the HTTP response status code.</summary>
    /// <param name="reqId">A request id that has been sent.</param>
    /// <param name="statusCode">On success, the HTTP status code (e.g. 200).</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(LibHttp2)]
    public static partial int sceHttp2GetStatusCode(int reqId, int* statusCode);

    /// <summary>Reads response body data.</summary>
    /// <param name="reqId">A request id that has been sent.</param>
    /// <param name="data">A buffer to read into.</param>
    /// <param name="size">The buffer size in bytes.</param>
    /// <returns>The number of bytes read, zero at the end of the response, or a negative error
    /// code.</returns>
    [LibraryImport(LibHttp2)]
    public static partial int sceHttp2ReadData(int reqId, void* data, nuint size);

    // ---- Managed convenience methods ----

    /// <summary>
    /// Initialises the full HTTP/2 stack (network, TLS, HTTP/2) in the required order.
    /// Returns all three resource ids; the caller tears down in reverse order with
    /// <see cref="Cleanup"/>.
    /// </summary>
    /// <param name="poolName">A NUL-terminated UTF-8 pool name.</param>
    /// <param name="netPoolSize">Network pool size (typical value is 32*1024).</param>
    /// <param name="sslPoolSize">TLS pool size (typical value is 256*1024).</param>
    /// <param name="httpPoolSize">HTTP/2 pool size (typical value is 256*1024).</param>
    /// <param name="netMemId">On success, the network pool id.</param>
    /// <param name="sslCtxId">On success, the TLS context id.</param>
    /// <param name="httpCtxId">On success, the HTTP/2 context id.</param>
    /// <returns>Zero on success, or the first non-zero error code.</returns>
    public static int InitStack(ReadOnlySpan<byte> poolName, int netPoolSize, nuint sslPoolSize,
        nuint httpPoolSize, out int netMemId, out int sslCtxId, out int httpCtxId)
    {
        netMemId = -1;
        sslCtxId = -1;
        httpCtxId = -1;

        int result = sceNetInit();
        if (result != 0) return result;

        fixed (byte* p = poolName)
        {
            netMemId = sceNetPoolCreate(p, netPoolSize, 0);
            if (netMemId < 0) return netMemId;
        }

        sslCtxId = sceSslInit(sslPoolSize);
        if (sslCtxId < 0) return sslCtxId;

        httpCtxId = sceHttp2Init(netMemId, sslCtxId, httpPoolSize, 1);
        if (httpCtxId < 0) return httpCtxId;

        return 0;
    }

    /// <summary>
    /// Tears down the HTTP/2 stack in reverse order. Silently ignores ids that are -1
    /// (not yet initialised).
    /// </summary>
    public static void Cleanup(int reqId, int tmplId, int httpCtxId, int sslCtxId, int netMemId)
    {
        if (reqId != -1) sceHttp2DeleteRequest(reqId);
        if (tmplId != -1) sceHttp2DeleteTemplate(tmplId);
        if (httpCtxId != -1) sceHttp2Term(httpCtxId);
        if (sslCtxId != -1) sceSslTerm(sslCtxId);
        if (netMemId != -1) sceNetPoolDestroy(netMemId);
    }
}
