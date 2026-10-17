// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace SharpProspero.Storage;

/// <summary>The kind of value a <see cref="JsonValue"/> holds.</summary>
public enum JsonType
{
    /// <summary>The literal <c>null</c>.</summary>
    Null,

    /// <summary>A <c>true</c> or <c>false</c>.</summary>
    Boolean,

    /// <summary>A number, held as a double.</summary>
    Number,

    /// <summary>A string.</summary>
    String,

    /// <summary>An ordered list of values.</summary>
    Array,

    /// <summary>A set of named values, kept in the order they were added.</summary>
    Object,
}

/// <summary>Thrown when text handed to <see cref="JsonValue.Parse"/> is not valid JSON.</summary>
/// <remarks>Creates the error with <paramref name="message"/>.</remarks>
public sealed class JsonException(string message) : Exception(message);

/// <summary>
/// A JSON value: a null, a boolean, a number, a string, an array or an object, read from and written to
/// text with no system module. Use it to read a configuration file, a manifest or a reply from a
/// service, and to build one to send or save. Reading a value that is not there, or reading it as the
/// wrong kind, returns the fallback you give rather than throwing, so a missing field is easy to handle.
/// </summary>
/// <remarks>
/// <see cref="Parse"/> reads text into a value; <see cref="Write"/> turns a value back into text, either
/// compact or indented. Objects keep their keys in the order they were added, so a file read and written
/// back keeps its shape. This is a self-contained calculation, so it works the same on the device and in
/// tests.
/// </remarks>
/// <example>
/// <code>
/// JsonValue config = JsonValue.Load("/data/config.json");
/// int volume = config.GetInt("volume", 80);
/// bool music = config["audio"].GetBool("music", true);
///
/// var reply = JsonValue.NewObject();
/// reply["ok"] = true;
/// reply["items"] = JsonValue.NewArray().Add("a").Add("b");
/// string text = reply.Write(indented: true);
/// </code>
/// </example>
public sealed class JsonValue
{
    private const int MaxDepth = 256;

    private readonly bool _bool;
    private readonly double _number;
    private readonly string? _string;
    private readonly List<JsonValue>? _array;
    private readonly List<string>? _keys;
    private readonly Dictionary<string, JsonValue>? _members;

    private JsonValue(JsonType type)
    {
        Type = type;
        if (type == JsonType.Object)
        {
            _keys = [];
            _members = [];
        }
        else if (type == JsonType.Array)
        {
            _array = [];
        }
    }

    private JsonValue(bool value) { Type = JsonType.Boolean; _bool = value; }

    private JsonValue(double value) { Type = JsonType.Number; _number = value; }

    private JsonValue(string value) { Type = JsonType.String; _string = value; }

    /// <summary>The kind of value this holds.</summary>
    public JsonType Type { get; }

    /// <summary>The shared literal <c>null</c>.</summary>
    public static JsonValue Null { get; } = new(JsonType.Null);

    /// <summary>Creates an empty object, to which named values are added.</summary>
    public static JsonValue NewObject() => new(JsonType.Object)
    {
    };

    /// <summary>Creates an empty array, to which values are added.</summary>
    public static JsonValue NewArray() => new(JsonType.Array);

    /// <summary>A boolean value.</summary>
    public static JsonValue Of(bool value) => new(value);

    /// <summary>A number value.</summary>
    public static JsonValue Of(double value) => new(value);

    /// <summary>A string value, or <see cref="Null"/> when <paramref name="value"/> is null.</summary>
    public static JsonValue Of(string? value) => value is null ? Null : new(value);

    /// <summary>Wraps a boolean.</summary>
    public static implicit operator JsonValue(bool value) => Of(value);

    /// <summary>Wraps a number.</summary>
    public static implicit operator JsonValue(double value) => Of(value);

    /// <summary>Wraps a whole number.</summary>
    public static implicit operator JsonValue(int value) => Of(value);

    /// <summary>Wraps a string, or <see cref="Null"/> when it is null.</summary>
    public static implicit operator JsonValue(string? value) => Of(value);

    /// <summary>Whether this is the literal <c>null</c>.</summary>
    public bool IsNull => Type == JsonType.Null;

    /// <summary>The boolean value, or <paramref name="fallback"/> when this is not a boolean.</summary>
    public bool AsBool(bool fallback = false) => Type == JsonType.Boolean ? _bool : fallback;

    /// <summary>The number, or <paramref name="fallback"/> when this is not a number.</summary>
    public double AsNumber(double fallback = 0d) => Type == JsonType.Number ? _number : fallback;

    /// <summary>The number truncated to an integer, or <paramref name="fallback"/> when this is not a number.</summary>
    public int AsInt(int fallback = 0) => Type == JsonType.Number ? (int)_number : fallback;

    /// <summary>The number truncated to a long integer, or <paramref name="fallback"/> when this is not a number.</summary>
    public long AsLong(long fallback = 0L) => Type == JsonType.Number ? (long)_number : fallback;

    /// <summary>The string, or <paramref name="fallback"/> when this is not a string.</summary>
    public string AsString(string fallback = "") => Type == JsonType.String ? _string! : fallback;

    /// <summary>The number of items in an array, or named values in an object; zero otherwise.</summary>
    public int Count => Type == JsonType.Array ? _array!.Count : Type == JsonType.Object ? _keys!.Count : 0;

    /// <summary>The object's keys in order, or an empty list when this is not an object.</summary>
    public IReadOnlyList<string> Keys => _keys ?? (IReadOnlyList<string>)[];

    /// <summary>The item at <paramref name="index"/> in an array, or <see cref="Null"/> when out of range or not an array.</summary>
    public JsonValue this[int index]
    {
        get => Type == JsonType.Array && (uint)index < (uint)_array!.Count ? _array[index] : Null;
    }

    /// <summary>
    /// The value named <paramref name="key"/> in an object, or <see cref="Null"/> when absent or not an
    /// object. Setting it adds or replaces the value, and requires an object.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="key"/> is null.</exception>
    /// <exception cref="InvalidOperationException">Setting a value when this is not an object.</exception>
    public JsonValue this[string key]
    {
        get
        {
            ArgumentNullException.ThrowIfNull(key);
            return Type == JsonType.Object && _members!.TryGetValue(key, out JsonValue? value) ? value : Null;
        }
        set
        {
            ArgumentNullException.ThrowIfNull(key);
            if (Type != JsonType.Object)
                throw new InvalidOperationException("Only an object can hold named values.");
            if (_members!.TryAdd(key, value ?? Null))
                _keys!.Add(key);
            else
                _members[key] = value ?? Null;
        }
    }

    /// <summary>Whether an object holds a value named <paramref name="key"/>.</summary>
    public bool ContainsKey(string key) => Type == JsonType.Object && _members!.ContainsKey(key);

    /// <summary>Reads the value named <paramref name="key"/> from an object.</summary>
    /// <returns>True when the object holds the key.</returns>
    public bool TryGet(string key, out JsonValue value)
    {
        if (Type == JsonType.Object && _members!.TryGetValue(key, out JsonValue? found))
        {
            value = found;
            return true;
        }
        value = Null;
        return false;
    }

    /// <summary>Appends <paramref name="item"/> to an array and returns this array, so calls can chain.</summary>
    /// <exception cref="InvalidOperationException">This is not an array.</exception>
    public JsonValue Add(JsonValue item)
    {
        if (Type != JsonType.Array)
            throw new InvalidOperationException("Only an array can take appended values.");
        _array!.Add(item ?? Null);
        return this;
    }

    /// <summary>The named boolean from an object, or <paramref name="fallback"/> when absent or not a boolean.</summary>
    public bool GetBool(string key, bool fallback = false) => this[key].AsBool(fallback);

    /// <summary>The named number from an object, or <paramref name="fallback"/> when absent or not a number.</summary>
    public double GetNumber(string key, double fallback = 0d) => this[key].AsNumber(fallback);

    /// <summary>The named integer from an object, or <paramref name="fallback"/> when absent or not a number.</summary>
    public int GetInt(string key, int fallback = 0) => this[key].AsInt(fallback);

    /// <summary>The named string from an object, or <paramref name="fallback"/> when absent or not a string.</summary>
    public string GetString(string key, string fallback = "") => this[key].AsString(fallback);

    /// <summary>Parses JSON <paramref name="text"/> into a value.</summary>
    /// <exception cref="JsonException"><paramref name="text"/> is not valid JSON.</exception>
    public static JsonValue Parse(string text)
    {
        ArgumentNullException.ThrowIfNull(text);

        // A file saved with a byte-order mark begins with U+FEFF; a leading one is ignored, as the JSON
        // standard allows, so a config file written by an editor still parses.
        if (text.Length > 0 && text[0] == '﻿')
            text = text[1..];

        var parser = new Parser(text);
        JsonValue value = parser.ParseValue(0);
        parser.ExpectEnd();
        return value;
    }

    /// <summary>Parses JSON <paramref name="text"/>, returning false instead of throwing on bad input.</summary>
    public static bool TryParse(string text, out JsonValue value)
    {
        try
        {
            value = Parse(text);
            return true;
        }
        catch (JsonException)
        {
            value = Null;
            return false;
        }
    }

    /// <summary>Reads and parses the JSON file at <paramref name="path"/>, or <see cref="Null"/> if it is absent.</summary>
    /// <exception cref="JsonException">The file is not valid JSON.</exception>
    public static JsonValue Load(string path)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        return FileSystem.Exists(path) ? Parse(Encoding.UTF8.GetString(FileSystem.ReadAllBytes(path))) : Null;
    }

    /// <summary>Writes this value to the file at <paramref name="path"/>.</summary>
    public void Save(string path, bool indented = true)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        FileSystem.WriteAllText(path, Write(indented));
    }

    /// <summary>Returns this value as compact JSON text.</summary>
    public override string ToString() => Write(indented: false);

    /// <summary>Returns this value as JSON text, optionally laid out over several lines and indented.</summary>
    public string Write(bool indented = false)
    {
        var builder = new StringBuilder(64);
        WriteTo(builder, indented, 0);
        return builder.ToString();
    }

    private void WriteTo(StringBuilder builder, bool indented, int depth)
    {
        // A value built by hand can nest deeper than a parsed one; the same limit the parser holds keeps
        // a runaway tree from overflowing the stack here.
        if (depth > MaxDepth)
            throw new JsonException("The JSON is nested too deeply to write.");

        switch (Type)
        {
            case JsonType.Null:
                builder.Append("null");
                break;
            case JsonType.Boolean:
                builder.Append(_bool ? "true" : "false");
                break;
            case JsonType.Number:
                WriteNumber(builder, _number);
                break;
            case JsonType.String:
                WriteString(builder, _string!);
                break;
            case JsonType.Array:
                WriteArray(builder, indented, depth);
                break;
            default:
                WriteObject(builder, indented, depth);
                break;
        }
    }

    private void WriteArray(StringBuilder builder, bool indented, int depth)
    {
        if (_array!.Count == 0)
        {
            builder.Append("[]");
            return;
        }
        builder.Append('[');
        for (int i = 0; i < _array.Count; i++)
        {
            if (i > 0)
                builder.Append(',');
            NewLine(builder, indented, depth + 1);
            _array[i].WriteTo(builder, indented, depth + 1);
        }
        NewLine(builder, indented, depth);
        builder.Append(']');
    }

    private void WriteObject(StringBuilder builder, bool indented, int depth)
    {
        if (_keys!.Count == 0)
        {
            builder.Append("{}");
            return;
        }
        builder.Append('{');
        for (int i = 0; i < _keys.Count; i++)
        {
            if (i > 0)
                builder.Append(',');
            NewLine(builder, indented, depth + 1);
            WriteString(builder, _keys[i]);
            builder.Append(indented ? ": " : ":");
            _members![_keys[i]].WriteTo(builder, indented, depth + 1);
        }
        NewLine(builder, indented, depth);
        builder.Append('}');
    }

    private static void NewLine(StringBuilder builder, bool indented, int depth)
    {
        if (!indented)
            return;
        builder.Append('\n');
        builder.Append(' ', depth * 2);
    }

    private static void WriteNumber(StringBuilder builder, double value)
    {
        if (!double.IsFinite(value))
        {
            builder.Append("null"); // JSON has no way to write an infinity or a not-a-number
            return;
        }
        // A whole number within the exactly representable range is written without a decimal point.
        if (value == Math.Floor(value) && Math.Abs(value) < 1e15)
            builder.Append(((long)value).ToString(CultureInfo.InvariantCulture));
        else
            builder.Append(value.ToString("R", CultureInfo.InvariantCulture));
    }

    private static void WriteString(StringBuilder builder, string value)
    {
        builder.Append('"');
        foreach (char c in value)
        {
            switch (c)
            {
                case '"': builder.Append("\\\""); break;
                case '\\': builder.Append("\\\\"); break;
                case '\b': builder.Append("\\b"); break;
                case '\f': builder.Append("\\f"); break;
                case '\n': builder.Append("\\n"); break;
                case '\r': builder.Append("\\r"); break;
                case '\t': builder.Append("\\t"); break;
                default:
                    if (c < 0x20)
                        builder.Append("\\u").Append(((int)c).ToString("x4", CultureInfo.InvariantCulture));
                    else
                        builder.Append(c);
                    break;
            }
        }
        builder.Append('"');
    }

    // A recursive-descent reader over the text. Positions are tracked so an error names where it is.
    private sealed class Parser(string text)
    {
        private int _pos;

        public JsonValue ParseValue(int depth)
        {
            if (depth > MaxDepth)
                throw new JsonException("The JSON is nested too deeply.");
            SkipWhitespace();
            if (_pos >= text.Length)
                throw new JsonException("Unexpected end of JSON.");

            char c = text[_pos];
            return c switch
            {
                '{' => ParseObject(depth),
                '[' => ParseArray(depth),
                '"' => Of(ParseString()),
                't' or 'f' => ParseBool(),
                'n' => ParseNull(),
                _ => ParseNumber(),
            };
        }

        public void ExpectEnd()
        {
            SkipWhitespace();
            if (_pos != text.Length)
                throw new JsonException($"Unexpected character at position {_pos}.");
        }

        private JsonValue ParseObject(int depth)
        {
            _pos++; // {
            JsonValue result = NewObject();
            SkipWhitespace();
            if (Peek() == '}')
            {
                _pos++;
                return result;
            }
            while (true)
            {
                SkipWhitespace();
                if (Peek() != '"')
                    throw new JsonException($"Expected a key string at position {_pos}.");
                string key = ParseString();
                SkipWhitespace();
                if (Peek() != ':')
                    throw new JsonException($"Expected ':' at position {_pos}.");
                _pos++;
                result[key] = ParseValue(depth + 1);
                SkipWhitespace();
                char next = Next();
                if (next == '}')
                    return result;
                if (next != ',')
                    throw new JsonException($"Expected ',' or '}}' at position {_pos - 1}.");
            }
        }

        private JsonValue ParseArray(int depth)
        {
            _pos++; // [
            JsonValue result = NewArray();
            SkipWhitespace();
            if (Peek() == ']')
            {
                _pos++;
                return result;
            }
            while (true)
            {
                result.Add(ParseValue(depth + 1));
                SkipWhitespace();
                char next = Next();
                if (next == ']')
                    return result;
                if (next != ',')
                    throw new JsonException($"Expected ',' or ']' at position {_pos - 1}.");
            }
        }

        private string ParseString()
        {
            _pos++; // opening quote
            var builder = new StringBuilder();
            while (true)
            {
                if (_pos >= text.Length)
                    throw new JsonException("Unterminated string.");
                char c = text[_pos++];
                if (c == '"')
                    return builder.ToString();
                if (c != '\\')
                {
                    builder.Append(c);
                    continue;
                }
                if (_pos >= text.Length)
                    throw new JsonException("Unterminated escape.");
                char e = text[_pos++];
                switch (e)
                {
                    case '"': builder.Append('"'); break;
                    case '\\': builder.Append('\\'); break;
                    case '/': builder.Append('/'); break;
                    case 'b': builder.Append('\b'); break;
                    case 'f': builder.Append('\f'); break;
                    case 'n': builder.Append('\n'); break;
                    case 'r': builder.Append('\r'); break;
                    case 't': builder.Append('\t'); break;
                    case 'u': builder.Append(ParseUnicodeEscape()); break;
                    default: throw new JsonException($"Invalid escape '\\{e}' at position {_pos - 1}.");
                }
            }
        }

        private char ParseUnicodeEscape()
        {
            if (_pos + 4 > text.Length)
                throw new JsonException("Truncated \\u escape.");
            int code = 0;
            for (int i = 0; i < 4; i++)
            {
                code <<= 4;
                char h = text[_pos++];
                code |= h switch
                {
                    >= '0' and <= '9' => h - '0',
                    >= 'a' and <= 'f' => h - 'a' + 10,
                    >= 'A' and <= 'F' => h - 'A' + 10,
                    _ => throw new JsonException($"Invalid \\u escape at position {_pos - 1}."),
                };
            }
            return (char)code;
        }

        private JsonValue ParseNumber()
        {
            int start = _pos;
            if (Peek() == '-')
                _pos++;
            while (_pos < text.Length && IsNumberChar(text[_pos]))
                _pos++;
            ReadOnlySpan<char> span = text.AsSpan(start, _pos - start);
            if (span.IsEmpty || !double.TryParse(span, NumberStyles.Float, CultureInfo.InvariantCulture, out double value))
                throw new JsonException($"Invalid number at position {start}.");
            return Of(value);
        }

        private JsonValue ParseBool()
        {
            if (Matches("true"))
                return Of(true);
            if (Matches("false"))
                return Of(false);
            throw new JsonException($"Invalid literal at position {_pos}.");
        }

        private JsonValue ParseNull()
        {
            if (Matches("null"))
                return Null;
            throw new JsonException($"Invalid literal at position {_pos}.");
        }

        private bool Matches(string literal)
        {
            if (_pos + literal.Length > text.Length || !text.AsSpan(_pos, literal.Length).SequenceEqual(literal))
                return false;
            _pos += literal.Length;
            return true;
        }

        private static bool IsNumberChar(char c)
            => c is (>= '0' and <= '9') or '-' or '+' or '.' or 'e' or 'E';

        private char Peek() => _pos < text.Length ? text[_pos] : '\0';

        private char Next()
        {
            if (_pos >= text.Length)
                throw new JsonException("Unexpected end of JSON.");
            return text[_pos++];
        }

        private void SkipWhitespace()
        {
            while (_pos < text.Length)
            {
                char c = text[_pos];
                if (c is ' ' or '\t' or '\n' or '\r')
                    _pos++;
                else
                    break;
            }
        }
    }
}
