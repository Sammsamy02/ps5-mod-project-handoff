// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Numerics;

/// <summary>
/// A spatial index over a 2D region that answers "what is in this area" without testing every item. It
/// subdivides space into quadrants as it fills, so a range query — collision broad-phase, off-screen
/// culling, picking under the cursor — visits only the parts of the world near the query instead of the
/// whole scene. Rebuild or clear it each frame for moving items, or keep it for a static world.
/// </summary>
/// <typeparam name="T">The item stored at each rectangle, such as an entity or an id.</typeparam>
public sealed class Quadtree<T>
{
    private readonly RectF _bounds;
    private readonly int _maxItemsPerNode;
    private readonly int _maxDepth;
    private Node _root;

    /// <summary>Creates a quadtree covering <paramref name="bounds"/>.</summary>
    /// <param name="bounds">The world region the tree indexes.</param>
    /// <param name="maxItemsPerNode">How many items a node holds before it subdivides.</param>
    /// <param name="maxDepth">The deepest the tree subdivides, bounding memory and recursion.</param>
    /// <exception cref="ArgumentOutOfRangeException">A limit is not positive.</exception>
    public Quadtree(RectF bounds, int maxItemsPerNode = 8, int maxDepth = 8)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(maxItemsPerNode);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(maxDepth);
        if (bounds.IsEmpty)
            throw new ArgumentException("The bounds must have a positive width and height.", nameof(bounds));
        _bounds = bounds;
        _maxItemsPerNode = maxItemsPerNode;
        _maxDepth = maxDepth;
        _root = new Node(bounds, 0);
    }

    /// <summary>How many items are in the tree.</summary>
    public int Count { get; private set; }

    /// <summary>Adds <paramref name="item"/> occupying <paramref name="bounds"/>.</summary>
    public void Insert(T item, RectF bounds)
    {
        InsertInto(_root, new Entry(item, bounds));
        Count++;
    }

    /// <summary>Returns the items whose rectangles overlap <paramref name="area"/>.</summary>
    public List<T> Query(RectF area)
    {
        var results = new List<T>();
        Query(area, results);
        return results;
    }

    /// <summary>Adds the items whose rectangles overlap <paramref name="area"/> to <paramref name="results"/>, for reuse across frames.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="results"/> is null.</exception>
    public void Query(RectF area, List<T> results)
    {
        ArgumentNullException.ThrowIfNull(results);
        QueryNode(_root, area, results, isRoot: true);
    }

    /// <summary>Removes every item, keeping the same bounds and limits.</summary>
    public void Clear()
    {
        _root = new Node(_bounds, 0);
        Count = 0;
    }

    private void InsertInto(Node node, in Entry entry)
    {
        if (node.Children is not null)
        {
            int child = ChildContaining(node, entry.Bounds);
            if (child >= 0)
            {
                InsertInto(node.Children[child], entry);
                return;
            }

            node.Items.Add(entry); // straddles the split lines, so it lives at this level
            return;
        }

        node.Items.Add(entry);
        if (node.Items.Count > _maxItemsPerNode && node.Depth < _maxDepth)
            Quadtree<T>.Subdivide(node);
    }

    private static void Subdivide(Node node)
    {
        float halfWidth = node.Bounds.Width / 2f;
        float halfHeight = node.Bounds.Height / 2f;
        float x = node.Bounds.X;
        float y = node.Bounds.Y;
        int depth = node.Depth + 1;

        node.Children =
        [
            new Node(new RectF(x, y, halfWidth, halfHeight), depth),
            new Node(new RectF(x + halfWidth, y, halfWidth, halfHeight), depth),
            new Node(new RectF(x, y + halfHeight, halfWidth, halfHeight), depth),
            new Node(new RectF(x + halfWidth, y + halfHeight, halfWidth, halfHeight), depth),
        ];

        // Push each item down to the child that fully contains it; items on a split line stay here.
        var retained = new List<Entry>();
        foreach (Entry entry in node.Items)
        {
            int child = ChildContaining(node, entry.Bounds);
            if (child >= 0)
                node.Children[child].Items.Add(entry);
            else
                retained.Add(entry);
        }

        node.Items = retained;
    }

    private static int ChildContaining(Node node, RectF bounds)
    {
        Node[] children = node.Children!;
        for (int i = 0; i < children.Length; i++)
        {
            if (children[i].Bounds.Contains(bounds))
                return i;
        }

        return -1;
    }

    private static void QueryNode(Node node, RectF area, List<T> results, bool isRoot)
    {
        // A child's items are guaranteed within its bounds, so a bounds miss can prune it. The root can
        // also hold items that extend beyond its bounds (an item inserted outside the tree lands there),
        // so the root is never pruned by its own bounds — its items are always scanned.
        if (!isRoot && !node.Bounds.Intersects(area))
            return;

        foreach (Entry entry in node.Items)
        {
            if (entry.Bounds.Intersects(area))
                results.Add(entry.Item);
        }

        if (node.Children is not null)
        {
            foreach (Node child in node.Children)
                QueryNode(child, area, results, isRoot: false);
        }
    }

    private readonly record struct Entry(T Item, RectF Bounds);

    private sealed class Node(RectF bounds, int depth)
    {
        public RectF Bounds { get; } = bounds;

        public int Depth { get; } = depth;

        public List<Entry> Items { get; set; } = [];

        public Node[]? Children { get; set; }
    }
}
