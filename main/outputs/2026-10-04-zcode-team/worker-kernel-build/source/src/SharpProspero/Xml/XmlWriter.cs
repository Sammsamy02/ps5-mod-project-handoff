// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Xml;

/// <summary>
/// Builds XML text with correct escaping and optional indentation. Elements are opened and closed in
/// pairs; attributes and content go to the element most recently opened. It writes into a
/// <see cref="System.Text.StringBuilder"/>, so it has no stream or platform dependency.
/// </summary>
/// <remarks>Creates a writer. Set <paramref name="indent"/> for human-readable, indented output.</remarks>
public sealed class XmlWriter(bool indent = false, string indentUnit = "  ")
{
    private readonly StringBuilder _sb = new();
    private readonly bool _indent = indent;
    private readonly string _indentUnit = indentUnit ?? "  ";
    private readonly List<Frame> _open = [];
    private bool _startTagOpen;

    private struct Frame
    {
        public string Name;
        public bool HadChildElement;
    }

    /// <summary>Writes the <c>&lt;?xml ... ?&gt;</c> declaration. Call before the root element.</summary>
    public XmlWriter WriteDeclaration(string version = "1.0", string encoding = "utf-8", string? standalone = null)
    {
        _sb.Append("<?xml version=\"").Append(version).Append("\" encoding=\"").Append(encoding).Append('"');
        if (standalone is not null)
            _sb.Append(" standalone=\"").Append(standalone).Append('"');
        _sb.Append("?>");
        return this;
    }

    /// <summary>Opens an element start tag. Attributes and content that follow belong to this element.</summary>
    /// <exception cref="ArgumentException">The name is null or empty.</exception>
    public XmlWriter WriteStartElement(string name)
    {
        if (string.IsNullOrEmpty(name))
            throw new ArgumentException("Element name must not be empty.", nameof(name));
        CloseStartTag();
        if (_open.Count > 0)
        {
            Frame parent = _open[^1];
            parent.HadChildElement = true;
            _open[^1] = parent;
        }
        NewLineAndIndent(_open.Count);
        _sb.Append('<').Append(CheckName(name));
        _open.Add(new Frame { Name = name });
        _startTagOpen = true;
        return this;
    }

    /// <summary>Writes an attribute on the currently open start tag.</summary>
    /// <exception cref="InvalidOperationException">No element start tag is open for attributes.</exception>
    public XmlWriter WriteAttribute(string name, string value)
    {
        if (!_startTagOpen)
            throw new InvalidOperationException("Attributes can only be written right after WriteStartElement.");
        _sb.Append(' ').Append(CheckName(name)).Append("=\"");
        AppendEscaped(value, attribute: true);
        _sb.Append('"');
        return this;
    }

    /// <summary>Writes escaped text content into the current element.</summary>
    public XmlWriter WriteString(string text)
    {
        CloseStartTag();
        AppendEscaped(text, attribute: false);
        return this;
    }

    /// <summary>Writes a CDATA section (unescaped, verbatim character data).</summary>
    /// <exception cref="ArgumentException">The text contains the CDATA terminator "]]&gt;".</exception>
    public XmlWriter WriteCData(string text)
    {
        if (text.Contains("]]>", StringComparison.Ordinal))
            throw new ArgumentException("CDATA content must not contain ']]>'.", nameof(text));
        CloseStartTag();
        _sb.Append("<![CDATA[").Append(text).Append("]]>");
        return this;
    }

    /// <summary>Writes a comment.</summary>
    /// <exception cref="ArgumentException">The text contains "--".</exception>
    public XmlWriter WriteComment(string text)
    {
        if (text.Contains("--", StringComparison.Ordinal))
            throw new ArgumentException("Comment content must not contain '--'.", nameof(text));
        CloseStartTag();
        NewLineAndIndent(_open.Count);
        _sb.Append("<!--").Append(text).Append("-->");
        return this;
    }

    /// <summary>Closes the most recently opened element. Self-closes it if it had no content.</summary>
    /// <exception cref="InvalidOperationException">There is no open element to close.</exception>
    public XmlWriter WriteEndElement()
    {
        if (_open.Count == 0)
            throw new InvalidOperationException("There is no open element to close.");
        Frame frame = _open[^1];
        _open.RemoveAt(_open.Count - 1);
        if (_startTagOpen)
        {
            _sb.Append("/>");
            _startTagOpen = false;
            return this;
        }
        if (frame.HadChildElement)
            NewLineAndIndent(_open.Count);
        _sb.Append("</").Append(frame.Name).Append('>');
        return this;
    }

    /// <summary>Writes a complete <c>&lt;name&gt;value&lt;/name&gt;</c> element.</summary>
    public XmlWriter WriteElementString(string name, string value)
    {
        WriteStartElement(name);
        WriteString(value);
        WriteEndElement();
        return this;
    }

    /// <summary>Closes any still-open elements and returns the XML text.</summary>
    public override string ToString()
    {
        while (_open.Count > 0)
            WriteEndElement();
        return _sb.ToString();
    }

    private void CloseStartTag()
    {
        if (_startTagOpen)
        {
            _sb.Append('>');
            _startTagOpen = false;
        }
    }

    private void NewLineAndIndent(int depth)
    {
        if (!_indent)
            return;
        if (_sb.Length > 0)
            _sb.Append('\n');
        for (int i = 0; i < depth; i++)
            _sb.Append(_indentUnit);
    }

    // An element or attribute name written verbatim into a tag, so it must not contain characters that
    // would break the markup. Reject them up front rather than emit XML that cannot be parsed back.
    private static string CheckName(string name)
    {
        if (string.IsNullOrEmpty(name) || name.AsSpan().IndexOfAny(" \t\r\n<>\"'=&/".AsSpan()) >= 0)
            throw new ArgumentException($"'{name}' is not a valid XML name.", nameof(name));
        return name;
    }

    private void AppendEscaped(string text, bool attribute)
    {
        foreach (char c in text)
        {
            // A document cannot carry these at all, escaped or not: the numeric reference for one is as
            // illegal as the character itself. Writing them produced a document no conforming reader
            // will load, and the reader here accepted them, so it round-tripped within this SDK and
            // only came apart somewhere else. Refuse at the point of writing, naming the character.
            if (char.IsControl(c) && c is not ('\t' or '\n' or '\r'))
                throw new ArgumentException(
                    $"A document cannot carry the character U+{(int)c:X4}.", nameof(text));

            switch (c)
            {
                case '&': _sb.Append("&amp;"); break;
                case '<': _sb.Append("&lt;"); break;
                case '>': _sb.Append("&gt;"); break;
                case '"' when attribute: _sb.Append("&quot;"); break;
                case '\n' when attribute: _sb.Append("&#10;"); break;
                case '\r': _sb.Append("&#13;"); break; // always escaped: a parser normalizes a literal CR to LF
                case '\t' when attribute: _sb.Append("&#9;"); break;
                default: _sb.Append(c); break;
            }
        }
    }
}
