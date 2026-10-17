// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Xml;

/// <summary>The kind of node a <see cref="XmlReader"/> is positioned on.</summary>
public enum XmlNodeType
{
    /// <summary>No node; before the first read or after the end.</summary>
    None,
    /// <summary>The <c>&lt;?xml ... ?&gt;</c> declaration.</summary>
    XmlDeclaration,
    /// <summary>An element start tag. A self-closing tag also reports <see cref="XmlReader.IsEmptyElement"/>.</summary>
    Element,
    /// <summary>An element end tag.</summary>
    EndElement,
    /// <summary>Character data between tags.</summary>
    Text,
    /// <summary>A <c>&lt;![CDATA[ ... ]]&gt;</c> section.</summary>
    CData,
    /// <summary>A <c>&lt;!-- ... --&gt;</c> comment.</summary>
    Comment,
    /// <summary>A <c>&lt;? ... ?&gt;</c> processing instruction.</summary>
    ProcessingInstruction,
    /// <summary>Character data that is entirely whitespace.</summary>
    Whitespace,
}

/// <summary>An attribute on an element: its name and unescaped value.</summary>
/// <remarks>Creates an attribute.</remarks>
public readonly struct XmlAttribute(string name, string value)
{
    /// <summary>The attribute name, including any namespace prefix.</summary>
    public string Name { get; } = name;

    /// <summary>The attribute value, with entity references resolved.</summary>
    public string Value { get; } = value;
}

/// <summary>Thrown when XML is malformed, with the line and column where the problem was found.</summary>
/// <remarks>Creates the exception at a position.</remarks>
public sealed class XmlException(string message, int line, int column) : Exception($"{message} (line {line}, column {column})")
{

    /// <summary>The 1-based line where parsing failed.</summary>
    public int Line { get; } = line;
    /// <summary>The 1-based column where parsing failed.</summary>
    public int Column { get; } = column;
}

/// <summary>
/// A forward-only pull reader over XML text. Call <see cref="Read"/> to advance to each node and inspect
/// <see cref="NodeType"/>, <see cref="Name"/>, <see cref="Value"/>, and <see cref="Attributes"/>. It has no
/// dependencies and does not fetch external entities or DTDs, so it is safe on a constrained target.
/// </summary>
public sealed class XmlReader
{
    private readonly string _s;
    private int _pos;
    private int _line = 1;
    private int _col = 1;
    private readonly List<XmlAttribute> _attributes = [];

    /// <summary>Creates a reader over <paramref name="xml"/>.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="xml"/> is null.</exception>
    public XmlReader(string xml)
    {
        ArgumentNullException.ThrowIfNull(xml);
        _s = xml;
    }

    /// <summary>The kind of the current node.</summary>
    public XmlNodeType NodeType { get; private set; } = XmlNodeType.None;
    /// <summary>The element or processing-instruction name of the current node; empty otherwise.</summary>
    public string Name { get; private set; } = string.Empty;
    /// <summary>The text, comment, CDATA, or processing-instruction body of the current node; empty otherwise.</summary>
    public string Value { get; private set; } = string.Empty;
    /// <summary>True when the current element is self-closing (<c>&lt;a/&gt;</c>), which emits no end tag.</summary>
    public bool IsEmptyElement { get; private set; }
    /// <summary>The attributes of the current element; empty for other node types.</summary>
    public IReadOnlyList<XmlAttribute> Attributes => _attributes;

    /// <summary>The 1-based line of the current position.</summary>
    public int Line => _line;
    /// <summary>The 1-based column of the current position.</summary>
    public int Column => _col;

    /// <summary>Advances to the next node. Returns false at the end of the document.</summary>
    /// <exception cref="XmlException">The XML is malformed.</exception>
    public bool Read()
    {
        _attributes.Clear();
        Name = string.Empty;
        Value = string.Empty;
        IsEmptyElement = false;

        while (true)
        {
            if (_pos >= _s.Length)
            {
                NodeType = XmlNodeType.None;
                return false;
            }

            if (_s[_pos] != '<')
                return ReadText();

            // A markup construct.
            if (StartsWith("<!--")) return ReadComment();
            if (StartsWith("<![CDATA[")) return ReadCData();
            if (StartsWith("<!DOCTYPE") || StartsWith("<!")) { SkipDoctypeOrDecl(); continue; }
            if (StartsWith("<?")) return ReadProcessingInstruction();
            if (StartsWith("</")) return ReadEndElement();
            return ReadElement();
        }
    }

    private bool ReadText()
    {
        int start = _pos;
        while (_pos < _s.Length && _s[_pos] != '<')
            Advance();
        string raw = _s[start.._pos];
        Value = Unescape(raw, start);
        NodeType = IsAllWhitespace(raw) ? XmlNodeType.Whitespace : XmlNodeType.Text;
        return true;
    }

    private bool ReadComment()
    {
        AdvanceBy(4); // <!--
        int start = _pos;
        int end = _s.IndexOf("-->", _pos, StringComparison.Ordinal);
        if (end < 0)
            throw Error("Unterminated comment.");
        Value = _s[start..end];
        AdvanceTo(end + 3);
        NodeType = XmlNodeType.Comment;
        return true;
    }

    private bool ReadCData()
    {
        AdvanceBy(9); // <![CDATA[
        int start = _pos;
        int end = _s.IndexOf("]]>", _pos, StringComparison.Ordinal);
        if (end < 0)
            throw Error("Unterminated CDATA section.");
        Value = _s[start..end];
        AdvanceTo(end + 3);
        NodeType = XmlNodeType.CData;
        return true;
    }

    private bool ReadProcessingInstruction()
    {
        AdvanceBy(2); // <?
        int nameStart = _pos;
        while (_pos < _s.Length && !IsWhitespace(_s[_pos]) && !StartsWith("?>"))
            Advance();
        Name = _s[nameStart.._pos];
        SkipWhitespace();
        int start = _pos;
        int end = _s.IndexOf("?>", _pos, StringComparison.Ordinal);
        if (end < 0)
            throw Error("Unterminated processing instruction.");
        Value = _s[start..end];
        AdvanceTo(end + 2);
        NodeType = Name.Equals("xml", StringComparison.Ordinal) ? XmlNodeType.XmlDeclaration : XmlNodeType.ProcessingInstruction;
        if (NodeType == XmlNodeType.XmlDeclaration)
            ParseDeclarationAttributes(Value);
        return true;
    }

    private bool ReadEndElement()
    {
        AdvanceBy(2); // </
        Name = ReadName();
        SkipWhitespace();
        Expect('>');
        NodeType = XmlNodeType.EndElement;
        return true;
    }

    private bool ReadElement()
    {
        Advance(); // <
        Name = ReadName();
        if (Name.Length == 0)
            throw Error("Element name expected.");

        while (true)
        {
            SkipWhitespace();
            if (_pos >= _s.Length)
                throw Error("Unterminated start tag.");
            char c = _s[_pos];
            if (c == '>')
            {
                Advance();
                break;
            }
            if (c == '/')
            {
                Advance();
                Expect('>');
                IsEmptyElement = true;
                break;
            }
            ReadAttribute();
        }

        NodeType = XmlNodeType.Element;
        return true;
    }

    private void ReadAttribute()
    {
        string name = ReadName();
        if (name.Length == 0)
            throw Error("Attribute name expected.");
        SkipWhitespace();
        Expect('=');
        SkipWhitespace();
        if (_pos >= _s.Length || (_s[_pos] != '"' && _s[_pos] != '\''))
            throw Error("Quoted attribute value expected.");
        char quote = _s[_pos];
        Advance();
        int start = _pos;
        while (_pos < _s.Length && _s[_pos] != quote)
            Advance();
        if (_pos >= _s.Length)
            throw Error("Unterminated attribute value.");
        string raw = _s[start.._pos];
        Advance(); // closing quote
        _attributes.Add(new XmlAttribute(name, Unescape(raw, start)));
    }

    private void ParseDeclarationAttributes(string decl)
    {
        // The declaration's version / encoding / standalone are exposed as attributes for convenience.
        var sub = new XmlReader("<x " + decl + "/>");
        sub.Read();
        foreach (XmlAttribute a in sub.Attributes)
            _attributes.Add(a);
    }

    private string ReadName()
    {
        int start = _pos;
        while (_pos < _s.Length && IsNameChar(_s[_pos]))
            Advance();
        return _s[start.._pos];
    }

    private void SkipDoctypeOrDecl()
    {
        // Skip <!DOCTYPE ...> including a bracketed internal subset, and any other <! ... > construct.
        Advance(); // <
        int depth = 0;
        while (_pos < _s.Length)
        {
            char c = _s[_pos];
            if (c == '[') depth++;
            else if (c == ']') { if (depth > 0) depth--; }
            else if (c == '>' && depth == 0) { Advance(); return; }
            Advance();
        }
        throw Error("Unterminated declaration.");
    }

    private void SkipWhitespace()
    {
        while (_pos < _s.Length && IsWhitespace(_s[_pos]))
            Advance();
    }

    private void Expect(char c)
    {
        if (_pos >= _s.Length || _s[_pos] != c)
            throw Error($"Expected '{c}'.");
        Advance();
    }

    private bool StartsWith(string token)
        => string.CompareOrdinal(_s, _pos, token, 0, token.Length) == 0 && _pos + token.Length <= _s.Length;

    private string Unescape(string raw, int rawStart)
    {
        if (raw.IndexOf('&') < 0)
            return raw;
        var sb = new StringBuilder(raw.Length);
        for (int i = 0; i < raw.Length; i++)
        {
            char c = raw[i];
            if (c != '&')
            {
                sb.Append(c);
                continue;
            }
            int semi = raw.IndexOf(';', i + 1);
            if (semi < 0)
                throw ErrorAt("Unterminated entity reference.", rawStart + i);
            string entity = raw.Substring(i + 1, semi - i - 1);
            sb.Append(ResolveEntity(entity, rawStart + i));
            i = semi;
        }
        return sb.ToString();
    }

    private string ResolveEntity(string entity, int at)
    {
        switch (entity)
        {
            case "lt": return "<";
            case "gt": return ">";
            case "amp": return "&";
            case "quot": return "\"";
            case "apos": return "'";
        }
        if (entity.Length > 1 && entity[0] == '#')
        {
            bool hex = entity[1] is 'x' or 'X';
            string digits = entity[(hex ? 2 : 1)..];
            if (digits.Length > 0 &&
                int.TryParse(digits, hex ? System.Globalization.NumberStyles.HexNumber : System.Globalization.NumberStyles.Integer,
                    System.Globalization.CultureInfo.InvariantCulture, out int code) &&
                code >= 0 && code <= 0x10FFFF && (code < 0xD800 || code > 0xDFFF))
            {
                return char.ConvertFromUtf32(code);
            }
        }
        throw ErrorAt($"Unknown entity reference '&{entity};'.", at);
    }

    private void Advance()
    {
        char c = _s[_pos++];
        if (c == '\n') { _line++; _col = 1; }
        else _col++;
    }

    private void AdvanceBy(int count)
    {
        for (int i = 0; i < count; i++)
            Advance();
    }

    private void AdvanceTo(int target)
    {
        while (_pos < target)
            Advance();
    }

    private XmlException Error(string message) => new(message, _line, _col);

    private XmlException ErrorAt(string message, int index)
    {
        int line = 1, col = 1;
        for (int i = 0; i < index && i < _s.Length; i++)
        {
            if (_s[i] == '\n') { line++; col = 1; }
            else col++;
        }
        return new XmlException(message, line, col);
    }

    private static bool IsWhitespace(char c) => c is ' ' or '\t' or '\r' or '\n';

    private static bool IsAllWhitespace(string s)
    {
        foreach (char c in s)
            if (!IsWhitespace(c)) return false;
        return true;
    }

    private static bool IsNameChar(char c)
        => char.IsLetterOrDigit(c) || c is '_' or '-' or '.' or ':';
}
