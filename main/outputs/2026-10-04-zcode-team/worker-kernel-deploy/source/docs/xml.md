---
title: XML
parent: Data and utilities
grand_parent: Application Modules
nav_order: 5
---

# XML

`SharpProspero.Xml` reads and writes XML for a configuration or data file without pulling in a system module. It gives you both a small tree model for whole documents and a forward-only reader and writer for streaming, and it never loads an external DTD or entity, so it stays safe on a constrained target.

{: .note }
> Parsing is checked for well-formedness — matching tags, one root element — and resolves the standard entity references (`&lt;`, `&gt;`, `&amp;`, `&quot;`, `&apos;`, and numeric `&#nn;` or `&#xNN;`; a numeric reference outside Unicode or in the surrogate range is rejected). It refuses documents nested deeper than 256 levels so an adversarial file cannot overflow the stack. A `<!DOCTYPE>` is skipped rather than fetched, and an entity the parser does not recognize is reported as an error instead of looked up.

## The document model

For most configuration and level files you want the whole document in memory as a tree. Three types make that up: `XmlDocument` holds the single root; `XmlElement` is a node with a name, attributes, direct text, and child elements; and `XmlAttribute` is a name/value pair.

`XmlDocument.Parse` turns text into a tree and throws `XmlException` if the markup is malformed. Query the tree with `Attribute`, `Element`, `Elements`, and `Descendants`:

```csharp
using SharpProspero.Xml;
using SharpProspero.Storage;

XmlDocument doc = XmlDocument.Parse(PackageFile.ReadAllText("/app0/level.xml"));

string levelName = doc.Root.AttributeOrDefault("name", "untitled");

foreach (XmlElement enemy in doc.Root.Element("enemies")!.Elements("enemy"))
{
    string type = enemy.AttributeOrDefault("type", "grunt");
    int x = int.Parse(enemy.AttributeOrDefault("x", "0"));
    // place the enemy at x...
}
```

`Attribute` returns the value or `null`; `AttributeOrDefault` returns a fallback instead. `Element` returns the first matching child (or `null`), `Elements` enumerates the direct children with a given name, and `Descendants` walks the whole subtree for a name at any depth. `Children` lists the direct child elements in document order and `Attributes` lists every attribute, for walking a tree whose names you do not know in advance. `Text` is the element's own text with entities resolved, and it is settable, so the same property reads a document and builds one.

### Building a tree

You build a document the same way you read one. `SetAttribute` and `Add` return the element they were called on, so calls chain; `AddElement` creates a child and returns the new child so you can fill it in:

```csharp
using SharpProspero.Xml;

var save = new XmlElement("save").SetAttribute("slot", "1");
save.AddElement("score").Text = "1200";

string xml = new XmlDocument(save).ToXml(indent: true);
```

`ToXml` writes the tree back to text, indented when you ask, with an XML declaration by default (`ToXml(indent: true, declaration: false)` drops the declaration). `XmlDocument.ToString` is the indented form.

## Streaming with XmlReader

When a document is large, or you only need a few values from it, skip the tree and pull nodes one at a time. `XmlReader` is a forward-only pull parser: call `Read` to advance and inspect `NodeType`, `Name`, `Value`, `Attributes`, and `IsEmptyElement`.

```csharp
using SharpProspero.Xml;

var reader = new XmlReader(text);
while (reader.Read())
{
    switch (reader.NodeType)
    {
        case XmlNodeType.Element:
            // reader.Name is the tag; reader.Attributes holds its attributes.
            // A self-closing <tag/> also sets reader.IsEmptyElement and emits no end tag.
            break;
        case XmlNodeType.Text:
            // reader.Value is the character data, with entities resolved.
            break;
        case XmlNodeType.EndElement:
            break;
    }
}
```

`Read` returns `false` at the end of the document. Each `XmlAttribute` in `Attributes` exposes a `Name` and an unescaped `Value`. `NodeType` is one of:

| `XmlNodeType` | Node |
|---------------|------|
| `None` | Before the first read, or after the end |
| `XmlDeclaration` | The `<?xml ... ?>` declaration |
| `Element` | A start tag (self-closing tags also set `IsEmptyElement`) |
| `EndElement` | An end tag |
| `Text` | Character data between tags |
| `CData` | A `<![CDATA[ ... ]]>` section |
| `Comment` | A `<!-- ... -->` comment |
| `ProcessingInstruction` | A `<? ... ?>` instruction |
| `Whitespace` | Character data that is entirely whitespace |

`XmlDocument.Parse` is itself built on this reader, so both paths agree on what counts as well-formed.

## Writing with XmlWriter

`XmlWriter` builds output directly, without a tree. It escapes text and attribute values, checks that names are legal, and self-closes an element that gets no content. Elements open and close in pairs, and attributes must be written right after `WriteStartElement`, before any content or child.

```csharp
using SharpProspero.Xml;

var writer = new XmlWriter(indent: true);
writer.WriteDeclaration();
writer.WriteStartElement("save");
writer.WriteAttribute("slot", "1");
writer.WriteElementString("score", "1200");
writer.WriteEndElement();

string xml = writer.ToString();
```

`WriteElementString` is the shorthand for an open/content/close triple. `WriteString` writes escaped text into the current element, `WriteCData` writes a verbatim CDATA section, and `WriteComment` writes a comment. `ToString` closes any elements still open and returns the text. Every write method returns the writer, so a whole document can be produced as one chained expression.

{: .warning }
> `WriteAttribute` only works while a start tag is still open. Once you write content or a child element the start tag closes, and a later `WriteAttribute` throws `InvalidOperationException`. A name that contains whitespace or markup characters is rejected up front rather than written into output that cannot be read back. A control character other than tab, carriage return or line feed is refused in text and in an attribute value — a document cannot carry one at all, escaped or not — so `WriteString`, `WriteAttribute` and `WriteElementString` raise `ArgumentException` naming the character rather than produce output no reader loads.

## Handling malformed input

Any parse failure — a mismatched end tag, an unterminated attribute, an unknown entity, a second root — surfaces as an `XmlException` that carries the exact position. Catch it and read `Line` and `Column`:

```csharp
using SharpProspero.Xml;
using SharpProspero.Diagnostics;

try
{
    XmlDocument doc = XmlDocument.Parse(text);
    // use doc...
}
catch (XmlException ex)
{
    Log.Warning($"Bad XML at line {ex.Line}, column {ex.Column}: {ex.Message}");
}
```

Both `Line` and `Column` are 1-based, and the message already includes the position, so it reads well in a log on its own.

For a lighter-weight configuration format, see the JSON, INI, and CSV readers on [Files and storage](storage.md).
