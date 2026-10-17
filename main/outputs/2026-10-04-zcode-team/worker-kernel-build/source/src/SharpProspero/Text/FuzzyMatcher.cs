// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Text;

/// <summary>The result of a fuzzy match: a score (higher is better) and the indices in the text that matched.</summary>
/// <param name="Score">A relative quality score; compare scores from the same pattern, not across patterns.</param>
/// <param name="MatchedIndices">The positions in the searched text that the pattern's characters landed on.</param>
public readonly record struct FuzzyMatch(int Score, IReadOnlyList<int> MatchedIndices)
{
    /// <summary>Two matches are equal when their score and their matched indices are the same.</summary>
    public bool Equals(FuzzyMatch other)
    {
        if (Score != other.Score || MatchedIndices.Count != other.MatchedIndices.Count)
            return false;
        for (int i = 0; i < MatchedIndices.Count; i++)
        {
            if (MatchedIndices[i] != other.MatchedIndices[i])
                return false;
        }

        return true;
    }

    /// <inheritdoc/>
    public override int GetHashCode()
    {
        var hash = new HashCode();
        hash.Add(Score);
        foreach (int index in MatchedIndices)
            hash.Add(index);
        return hash.ToHashCode();
    }
}

/// <summary>
/// Matches a short pattern against text the way an incremental "type to find" box does: the pattern
/// characters must appear in order but need not be adjacent, matching is case-insensitive, and a match is
/// scored so that adjacent runs and word starts rank higher. Use it to filter and rank a list — files,
/// titles, commands — and to highlight the characters that matched.
/// </summary>
public static class FuzzyMatcher
{
    private const int MatchScore = 16;
    private const int AdjacentBonus = 15;
    private const int WordStartBonus = 30;
    private const int MaxGapPenalty = 10;

    /// <summary>Whether <paramref name="pattern"/> matches <paramref name="text"/> as an ordered subsequence.</summary>
    /// <exception cref="ArgumentNullException">An argument is null.</exception>
    public static bool IsMatch(string pattern, string text) => TryMatch(pattern, text, out _);

    /// <summary>
    /// Matches <paramref name="pattern"/> against <paramref name="text"/>, returning false when the
    /// pattern is not an ordered subsequence. An empty pattern matches anything with a zero score and no
    /// indices.
    /// </summary>
    /// <exception cref="ArgumentNullException">An argument is null.</exception>
    public static bool TryMatch(string pattern, string text, out FuzzyMatch match)
    {
        ArgumentNullException.ThrowIfNull(pattern);
        ArgumentNullException.ThrowIfNull(text);

        if (pattern.Length == 0)
        {
            match = new FuzzyMatch(0, []);
            return true;
        }

        var indices = new List<int>(pattern.Length);
        int score = 0;
        int textPos = 0;
        int previous = -1;

        for (int p = 0; p < pattern.Length; p++)
        {
            char target = char.ToLowerInvariant(pattern[p]);
            int found = -1;
            for (int t = textPos; t < text.Length; t++)
            {
                if (char.ToLowerInvariant(text[t]) == target)
                {
                    found = t;
                    break;
                }
            }

            if (found < 0)
            {
                match = default;
                return false;
            }

            score += MatchScore;
            if (previous < 0)
                score -= Math.Min(found, MaxGapPenalty);       // distance from the start
            else if (found == previous + 1)
                score += AdjacentBonus;                          // consecutive characters read as a word
            else
                score -= Math.Min(found - previous - 1, MaxGapPenalty);

            if (IsWordStart(text, found))
                score += WordStartBonus;

            indices.Add(found);
            previous = found;
            textPos = found + 1;
        }

        match = new FuzzyMatch(score, indices);
        return true;
    }

    /// <summary>
    /// Matches <paramref name="pattern"/> against each item's text and returns the matches, best score
    /// first. Items that do not match are left out. A tie keeps the original order.
    /// </summary>
    /// <exception cref="ArgumentNullException">An argument is null.</exception>
    public static List<(T Item, FuzzyMatch Match)> Rank<T>(string pattern, IEnumerable<T> items, Func<T, string> selector)
    {
        ArgumentNullException.ThrowIfNull(items);
        ArgumentNullException.ThrowIfNull(selector);

        var scored = new List<(T Item, FuzzyMatch Match, int Order)>();
        int order = 0;
        foreach (T item in items)
        {
            if (TryMatch(pattern, selector(item), out FuzzyMatch match))
                scored.Add((item, match, order));
            order++;
        }

        // Sort by descending score; the original-order tiebreak makes equal scores keep their input order,
        // since List.Sort itself is not stable.
        scored.Sort((a, b) =>
        {
            int byScore = b.Match.Score.CompareTo(a.Match.Score);
            return byScore != 0 ? byScore : a.Order.CompareTo(b.Order);
        });

        var result = new List<(T Item, FuzzyMatch Match)>(scored.Count);
        foreach ((T item, FuzzyMatch match, int _) in scored)
            result.Add((item, match));
        return result;
    }

    private static bool IsWordStart(string text, int index)
    {
        if (index == 0)
            return true;
        char previous = text[index - 1];
        char current = text[index];
        if (!char.IsLetterOrDigit(previous))
            return true; // follows a separator
        return char.IsLower(previous) && char.IsUpper(current); // a camelCase boundary
    }
}
