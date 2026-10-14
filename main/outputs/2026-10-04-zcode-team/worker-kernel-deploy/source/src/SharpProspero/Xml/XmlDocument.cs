// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Xml;

/// <summary>
/// An element in an XML tree: a name, attributes, direct text content, and child elements. Build a tree
/// with <see cref="AddElement"/> and <see cref="SetAttribute"/>, or read one from <see cref="XmlDocument.Parse"/>,
/// and query it with <see cref="Attribute"/>, <see cref="Element"/>, <see cref="Elements"/>, and
/// <see cref="Descendants"/>.
/// </summary>
public sealed class XmlElement
{
    private readonly List<XmlAttribute> _attributes = [];
    private readonly List<XmlElement> _children = [];

    /// <summary>Creates an element with a name.</summary>
    /// <exception cref="ArgumentException">The name is null or empty.</exception>
    public XmlElement(string name)
    {
        if (string.IsNullOrEmpty(name))
            throw new ArgumentException("Element name must not be empty.", nameof(name));
        Name = name;
    }

    /// <summary>The element name, including any namespace prefix.</summary>
    public string Name { get; set; }
    /// <summary>The element's direct text content, with entity references resolved.</summary>
    public string Text { get; set; } = string.Empty;
    /// <summary>The attributes on this element.</summary>
    public IReadOnlyList<XmlAttribute> Attributes => _attributes;
    /// <summary>The child elements of this element, in document order.</summary>
    public IReadOnlyList<XmlElement> Children => _children;

    /// <summary>Returns the value of the named attribute, or null if there is none.</summary>
    public string? Attribute(string name)
    {
        foreach (XmlAttribute a in _attributes)
            if (a.Name == name)
                return a.Value;
        return null;
    }

    /// <summary>Returns the value of the named attribute, or <paramref name="fallback"/> if there is none.</summary>
    public string AttributeOrDefault(string name, string fallback) => Attribute(name) ?? fallback;

    /// <summary>Sets (or replaces) an attribute, and returns this element for chaining.</summary>
    public XmlElement SetAttribute(string name, string value)
    {
        for (int i = 0; i < _attributes.Count; i++)
        {
            if (_attributes[i].Name == name)
            {
                _attributes[i] = new XmlAttribute(name, value);
                return this;
            }
        }
        _attributes.Add(new XmlAttribute(name, value));
        return this;
    }

    /// <summary>Adds an existing element as a child, and returns this element for chaining.</summary>
    public XmlElement Add(XmlElement child)
    {
        ArgumentNullException.ThrowIfNull(child);
        _children.Add(child);
        return this;
    }

    /// <summary>Creates a named child element, adds it, and returns the new child.</summary>
    public XmlElement AddElement(string name)
    {
        var child = new XmlElement(name);
        _children.Add(child);
        return child;
    }

    /// <summary>The first child element with the given name, or null.</summary>
    public XmlElement? Element(string name)
    {
        foreach (XmlElement e in _children)
            if (e.Name == name)
                return e;
        return null;
    }

    /// <summary>The direct child elements with the given name.</summary>
    public IEnumerable<XmlElement> Elements(string name)
    {
        foreach (XmlElement e in _children)
            if (e.Name == name)
                yield return e;
    }

    /// <summary>Every descendant element with the given name, at any depth, in document order.</summary>
    public IEnumerable<XmlElement> Descendants(string name)
    {
        foreach (XmlElement e in _children)
        {
            if (e.Name == name)
                yield return e;
            foreach (XmlElement d in e.Descendants(name))
                yield return d;
        }
    }

    /// <summary>Writes this element and its subtree to <paramref name="writer"/>.</summary>
    public void WriteTo(XmlWriter writer)
    {
        ArgumentNullException.ThrowIfNull(writer);
        writer.WriteStartElement(Name);
        foreach (XmlAttribute a in _attributes)
            writer.WriteAttribute(a.Name, a.Value);
        if (Text.Length > 0)
            writer.WriteString(Text);
        foreach (XmlElement child in _children)
            child.WriteTo(writer);
        writer.WriteEndElement();
    }
}

/// <summary>
/// A parsed XML document: its single root element, and the tools to read one from text or write one back.
/// Parsing is well-formedness-checked (matching tags, one root) and resolves entity references; it never
/// loads external DTDs or entities.
/// </summary>
public sealed class XmlDocument
{
    /// <summary>Creates a document with the given root element.</summary>
    public XmlDocument(XmlElement root)
    {
        ArgumentNullException.ThrowIfNull(root);
        Root = root;
    }

    /// <summary>The document's root element.</summary>
    public XmlElement Root { get; set; }

    // The tree is walked recursively when serialized and queried, so parsing bounds the nesting depth to
    // keep an adversarially deep document from overflowing the stack later.
    private const int MaxDepth = 256;

    private static bool IsAllWhitespace(string s)
    {
        foreach (char c in s)
            if (c is not (' ' or '\t' or '\r' or '\n'))
                return false;
        return true;
    }

    /// <summary>Parses XML text into a document.</summary>
    /// <exception cref="XmlException">The XML is malformed or has no single root element.</exception>
    public static XmlDocument Parse(string xml)
    {
        var reader = new XmlReader(xml);
        XmlElement? root = null;
        var stack = new Stack<XmlElement>();

        while (reader.Read())
        {
            switch (reader.NodeType)
            {
                case XmlNodeType.Element:
                    {
                        var element = new XmlElement(reader.Name);
                        foreach (XmlAttribute a in reader.Attributes)
                            element.SetAttribute(a.Name, a.Value);

                        if (stack.Count > 0)
                            stack.Peek().Add(element);
                        else if (root is null)
                            root = element;
                        else
                            throw new XmlException("An XML document must have exactly one root element.", reader.Line, reader.Column);

                        if (!reader.IsEmptyElement)
                        {
                            stack.Push(element);
                            if (stack.Count > MaxDepth)
                                throw new XmlException($"XML is nested deeper than {MaxDepth} levels.", reader.Line, reader.Column);
                        }
                        break;
                    }
                case XmlNodeType.EndElement:
                    {
                        if (stack.Count == 0)
                            throw new XmlException($"Unexpected end tag '</{reader.Name}>'.", reader.Line, reader.Column);
                        XmlElement open = stack.Pop();
                        if (open.Name != reader.Name)
                            throw new XmlException($"End tag '</{reader.Name}>' does not match start tag '<{open.Name}>'.", reader.Line, reader.Column);
                        // Whitespace held as tentative leaf text is only kept for a genuine leaf; drop it once the
                        // element turns out to be a container, so element indentation does not leak into the tree.
                        if (open.Children.Count > 0 && IsAllWhitespace(open.Text))
                            open.Text = string.Empty;
                        break;
                    }
                case XmlNodeType.Text:
                case XmlNodeType.CData:
                    if (stack.Count > 0)
                    {
                        XmlElement current = stack.Peek();
                        current.Text = current.Text.Length == 0 ? reader.Value : current.Text + reader.Value;
                    }
                    break;
                case XmlNodeType.Whitespace:
                    // Tentatively keep whitespace only while the element has no children, so a leaf whose sole
                    // content is whitespace (<a> </a>) preserves it; the EndElement case discards it otherwise.
                    if (stack.Count > 0 && stack.Peek().Children.Count == 0)
                    {
                        XmlElement current = stack.Peek();
                        current.Text += reader.Value;
                    }
                    break;
                    // Comments, processing instructions, and the declaration are not part of the tree.
            }
        }

        if (stack.Count > 0)
            throw new XmlException($"Unclosed element '<{stack.Peek().Name}>'.", reader.Line, reader.Column);
        if (root is null)
            throw new XmlException("The document has no root element.", 1, 1);
        return new XmlDocument(root);
    }

    /// <summary>Serializes the document to XML text, optionally indented, with an XML declaration.</summary>
    public string ToXml(bool indent = false, bool declaration = true)
    {
        var writer = new XmlWriter(indent);
        if (declaration)
            writer.WriteDeclaration();
        Root.WriteTo(writer);
        return writer.ToString();
    }

    /// <summary>Returns the indented XML text of the document.</summary>
    public override string ToString() => ToXml(indent: true);
}
