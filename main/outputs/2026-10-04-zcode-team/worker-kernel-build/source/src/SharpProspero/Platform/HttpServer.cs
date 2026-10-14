// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using System;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Platform;

/// <summary>A parsed HTTP request the server hands to a handler.</summary>
public sealed class HttpServerRequest
{
    internal HttpServerRequest(string method, string target, string path, string query,
        Dictionary<string, string> headers, byte[] body)
    {
        Method = method;
        Target = target;
        Path = path;
        Query = query;
        Headers = headers;
        Body = body;
    }

    /// <summary>The request method, upper-case (<c>GET</c>, <c>POST</c>, ...).</summary>
    public string Method { get; }

    /// <summary>The raw request target, path and query together.</summary>
    public string Target { get; }

    /// <summary>The path part of the target, percent-decoded.</summary>
    public string Path { get; }

    /// <summary>The query string without its leading <c>?</c>, or empty.</summary>
    public string Query { get; }

    /// <summary>The request headers, keyed without case sensitivity.</summary>
    public IReadOnlyDictionary<string, string> Headers { get; }

    /// <summary>The request body, empty when there is none.</summary>
    public byte[] Body { get; }

    /// <summary>The value of <paramref name="name"/>, or null when it is not present.</summary>
    public string? Header(string name) =>
        Headers.TryGetValue(name, out string? value) ? value : null;

    /// <summary>The body decoded as UTF-8 text.</summary>
    public string BodyText() => Body.Length == 0 ? string.Empty : Encoding.UTF8.GetString(Body);
}

/// <summary>A response a handler returns for the server to send.</summary>
public sealed class HttpServerResponse
{
    /// <summary>The status code (200, 404, ...).</summary>
    public int StatusCode { get; set; } = 200;

    /// <summary>The reason phrase shown after the code.</summary>
    public string ReasonPhrase { get; set; } = "OK";

    /// <summary>The value for the <c>Content-Type</c> header.</summary>
    public string ContentType { get; set; } = "text/plain; charset=utf-8";

    /// <summary>Extra response headers.</summary>
    public Dictionary<string, string> Headers { get; } = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>The response body.</summary>
    public byte[] Body { get; set; } = [];

    /// <summary>A text response with the given status.</summary>
    public static HttpServerResponse Text(string text, int status = 200, string reason = "OK") => new()
    {
        StatusCode = status,
        ReasonPhrase = reason,
        ContentType = "text/plain; charset=utf-8",
        Body = Encoding.UTF8.GetBytes(text ?? string.Empty),
    };

    /// <summary>An HTML response.</summary>
    public static HttpServerResponse Html(string html, int status = 200) => new()
    {
        StatusCode = status,
        ReasonPhrase = status == 200 ? "OK" : "",
        ContentType = "text/html; charset=utf-8",
        Body = Encoding.UTF8.GetBytes(html ?? string.Empty),
    };

    /// <summary>A JSON response.</summary>
    public static HttpServerResponse Json(string json, int status = 200) => new()
    {
        StatusCode = status,
        ReasonPhrase = status == 200 ? "OK" : "",
        ContentType = "application/json; charset=utf-8",
        Body = Encoding.UTF8.GetBytes(json ?? string.Empty),
    };

    /// <summary>A binary response with the given content type.</summary>
    public static HttpServerResponse Bytes(byte[] body, string contentType) => new()
    {
        ContentType = contentType,
        Body = body ?? [],
    };

    /// <summary>A 404 Not Found response.</summary>
    public static HttpServerResponse NotFound(string message = "Not Found") =>
        Text(message, 404, "Not Found");

    /// <summary>A redirect to <paramref name="location"/>.</summary>
    public static HttpServerResponse Redirect(string location, int status = 302)
    {
        var response = Text(string.Empty, status, "Found");
        response.Headers["Location"] = location;
        return response;
    }
}

/// <summary>
/// A small HTTP/1.1 server built on the SDK's own sockets, so a module can offer a page or an API over
/// the network — a remote control panel or a file browser a phone or a computer opens in a browser.
/// Start it on a port, then call <see cref="PollOnce"/> each frame so it never blocks the loop, or
/// <see cref="Run"/> to serve until told to stop. Each request is answered and its connection closed.
/// </summary>
/// <remarks>
/// It handles one request per connection (<c>Connection: close</c>), which keeps it simple and robust
/// for a control panel or a browser. It reads the whole request into memory, so it caps the header and
/// body sizes.
/// </remarks>
/// <example>
/// <code>
/// using var server = HttpServer.Start(8080);
/// // In the frame loop:
/// server.PollOnce(request => request.Path == "/"
///     ? HttpServerResponse.Html("&lt;h1&gt;Hello from C#&lt;/h1&gt;")
///     : HttpServerResponse.NotFound());
/// </code>
/// </example>
public sealed class HttpServer : IDisposable
{
    /// <summary>The largest request header block accepted, in bytes.</summary>
    public const int MaxHeaderBytes = 64 * 1024;

    /// <summary>The default cap on a request body, in bytes.</summary>
    public const int DefaultMaxBodyBytes = 8 * 1024 * 1024;

    private readonly TcpListener _listener;
    private readonly SocketPoller _poller;
    private bool _disposed;

    private HttpServer(TcpListener listener, SocketPoller poller)
    {
        _listener = listener;
        _poller = poller;
    }

    /// <summary>The largest request body this server reads; a request over it gets 400 Bad Request.</summary>
    public int MaxBodyBytes { get; set; } = DefaultMaxBodyBytes;

    /// <summary>The port the server listens on.</summary>
    public int Port { get; private set; }

    /// <summary>
    /// Binds and starts a server on <paramref name="port"/>, reachable from the local network. Bind to
    /// the loopback address instead by passing <paramref name="loopbackOnly"/> for a server only this
    /// console reaches.
    /// </summary>
    /// <exception cref="ProsperoException">The port could not be bound.</exception>
    public static HttpServer Start(int port, int backlog = 16, bool loopbackOnly = false)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(port);
        SocketAddress address = loopbackOnly ? SocketAddress.Loopback(port) : SocketAddress.Any(port);
        TcpListener listener = TcpListener.Listen(address, backlog);
        try
        {
            SocketPoller poller = SocketPoller.Create();
            try
            {
                poller.Add(listener.Handle, PollEvents.Read, token: 0);
                return new HttpServer(listener, poller) { Port = port };
            }
            catch
            {
                poller.Dispose();
                throw;
            }
        }
        catch
        {
            listener.Dispose();
            throw;
        }
    }

    /// <summary>
    /// Serves at most one waiting request, then returns. Waits up to
    /// <paramref name="timeoutMicroseconds"/> for a client (zero returns at once when none is waiting),
    /// so a frame loop can call it without stalling. Returns whether a request was served.
    /// </summary>
    /// <exception cref="ProsperoException">The accept or the network read failed.</exception>
    public bool PollOnce(Func<HttpServerRequest, HttpServerResponse> handler, int timeoutMicroseconds = 0)
    {
        ArgumentNullException.ThrowIfNull(handler);
        ObjectDisposedException.ThrowIf(_disposed, this);

        Span<PollReady> ready = stackalloc PollReady[1];
        int count = _poller.Wait(ready, timeoutMicroseconds);
        if (count <= 0 || !ready[0].IsReadable)
            return false;

        using TcpConnection connection = _listener.Accept();
        Handle(connection, handler);
        return true;
    }

    /// <summary>
    /// Serves requests until <paramref name="keepRunning"/> returns false, blocking between them. Use it
    /// when the module can dedicate its loop to the server; otherwise call <see cref="PollOnce"/> each
    /// frame. Checks <paramref name="keepRunning"/> every <paramref name="sliceMicroseconds"/>.
    /// </summary>
    public void Run(Func<HttpServerRequest, HttpServerResponse> handler, Func<bool> keepRunning, int sliceMicroseconds = 250_000)
    {
        ArgumentNullException.ThrowIfNull(handler);
        ArgumentNullException.ThrowIfNull(keepRunning);
        while (keepRunning())
            PollOnce(handler, sliceMicroseconds);
    }

    private void Handle(TcpConnection connection, Func<HttpServerRequest, HttpServerResponse> handler)
    {
        // Cap how long a single slow client can hold the loop.
        connection.SetReceiveTimeout(5_000_000);

        HttpServerResponse response;
        try
        {
            HttpServerRequest? request = ReadRequest(connection);
            response = request is null
                ? HttpServerResponse.Text("Bad Request", 400, "Bad Request")
                : handler(request);
        }
        catch (ProsperoException)
        {
            // The client went away or the read timed out; nothing to send.
            return;
        }
        catch (Exception)
        {
            response = HttpServerResponse.Text("Internal Server Error", 500, "Internal Server Error");
        }

        SendResponse(connection, response);
    }

    private HttpServerRequest? ReadRequest(TcpConnection connection)
    {
        byte[] buffer = new byte[8192];
        var head = new List<byte>(8192);
        int headerEnd = -1;

        // Read until the blank line that ends the header block.
        while (headerEnd < 0)
        {
            int read = connection.Receive(buffer);
            if (read <= 0)
                return null;
            head.AddRange(new ArraySegment<byte>(buffer, 0, read));
            headerEnd = FindHeaderEnd(head);
            if (head.Count > MaxHeaderBytes)
                return null;
        }

        byte[] all = [.. head];
        string headerText = Encoding.ASCII.GetString(all, 0, headerEnd);
        string[] lines = headerText.Split("\r\n");
        if (lines.Length == 0)
            return null;

        string[] requestLine = lines[0].Split(' ');
        if (requestLine.Length < 2)
            return null;
        string method = requestLine[0].ToUpperInvariant();
        string target = requestLine[1];

        var headers = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        for (int i = 1; i < lines.Length; i++)
        {
            int colon = lines[i].IndexOf(':');
            if (colon > 0)
                headers[lines[i][..colon].Trim()] = lines[i][(colon + 1)..].Trim();
        }

        // Split the target into path and query, and percent-decode the path.
        int q = target.IndexOf('?');
        string rawPath = q < 0 ? target : target[..q];
        string query = q < 0 ? string.Empty : target[(q + 1)..];
        string path = PercentDecode(rawPath);

        // Read the body, if the request declares one.
        byte[] body = [];
        int bodyStart = headerEnd + 4; // past the CRLFCRLF
        if (headers.TryGetValue("Content-Length", out string? lengthText)
            && int.TryParse(lengthText, out int contentLength) && contentLength > 0)
        {
            if (contentLength > MaxBodyBytes)
                return null;
            body = new byte[contentLength];
            int have = all.Length - bodyStart;
            if (have > 0)
                Array.Copy(all, bodyStart, body, 0, Math.Min(have, contentLength));
            int received = Math.Max(0, have);
            while (received < contentLength)
            {
                int read = connection.Receive(new Span<byte>(body, received, contentLength - received));
                if (read <= 0)
                    break;
                received += read;
            }
        }

        return new HttpServerRequest(method, target, path, query, headers, body);
    }

    private static void SendResponse(TcpConnection connection, HttpServerResponse response)
    {
        connection.SendAll(Encoding.ASCII.GetBytes(BuildResponseHead(response)));
        if (response.Body.Length > 0)
            connection.SendAll(response.Body);
        connection.Shutdown();
    }

    // The status line and headers of a response, ending with the blank line before the body.
    internal static string BuildResponseHead(HttpServerResponse response)
    {
        var head = new StringBuilder(256);
        head.Append("HTTP/1.1 ").Append(response.StatusCode).Append(' ').Append(response.ReasonPhrase).Append("\r\n");
        head.Append("Content-Type: ").Append(response.ContentType).Append("\r\n");
        head.Append("Content-Length: ").Append(response.Body.Length).Append("\r\n");
        head.Append("Connection: close\r\n");
        foreach (KeyValuePair<string, string> header in response.Headers)
            head.Append(header.Key).Append(": ").Append(header.Value).Append("\r\n");
        head.Append("\r\n");
        return head.ToString();
    }

    private static int FindHeaderEnd(List<byte> data)
    {
        for (int i = 3; i < data.Count; i++)
        {
            if (data[i] == '\n' && data[i - 1] == '\r' && data[i - 2] == '\n' && data[i - 3] == '\r')
                return i - 3;
        }
        return -1;
    }

    internal static string PercentDecode(string value)
    {
        if (value.IndexOf('%') < 0)
            return value;
        var bytes = new List<byte>(value.Length);
        for (int i = 0; i < value.Length; i++)
        {
            if (value[i] == '%' && i + 2 < value.Length
                && Uri.IsHexDigit(value[i + 1]) && Uri.IsHexDigit(value[i + 2]))
            {
                bytes.Add((byte)((Uri.FromHex(value[i + 1]) << 4) | Uri.FromHex(value[i + 2])));
                i += 2;
            }
            else
            {
                bytes.Add((byte)value[i]);
            }
        }
        return Encoding.UTF8.GetString([.. bytes]);
    }

    /// <summary>Stops the server and closes the port.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        _poller.Dispose();
        _listener.Dispose();
    }
}
