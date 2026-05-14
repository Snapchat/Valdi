#!/usr/bin/env python3
"""Fuzzy icon finder for Valdi's SIGIcon library.

Parses icon names from SIGIcons.tsx and performs fuzzy matching against
a search query. Uses word overlap, substring matching, and a synonym
map to find relevant icons.

Usage:
    python3 find_icon.py "chat bubble" --icons-file /path/to/SIGIcons.tsx
    python3 find_icon.py "settings"
    python3 find_icon.py "arrow right share"
"""

import argparse
import glob
import re
import sys
from typing import List, Tuple

# Synonym map: common search terms -> actual icon name words
# Values are the camelCase-split lowercase words that appear in icon names.
SYNONYMS = {
    "settings": ["gear"],
    "search": ["magnifying", "glass"],
    "home": ["house"],
    "profile": ["person"],
    "audio": ["waveform"],
    "music": ["waveform"],
    "send": ["arrow", "paper", "plane"],
    "delete": ["trash", "can"],
    "edit": ["pencil"],
    "close": ["x", "sign"],
    "dismiss": ["x", "sign"],
    "back": ["arrow", "left"],
    "forward": ["arrow", "right"],
    "menu": ["three", "dot", "horizontal"],
    "more": ["three", "dot", "horizontal"],
    "camera": ["camera"],
    "photo": ["camera"],
    "video": ["video", "camera"],
    "location": ["map", "pin"],
    "pin": ["map", "pin"],
    "notification": ["bell"],
    "alert": ["bell"],
}


def parse_icons(filepath: str) -> List[str]:
    """Parse icon getter names from SIGIcons.tsx."""
    pattern = re.compile(r"static get (\w+)\(\): Asset")
    icons = []
    with open(filepath, "r") as f:
        for line in f:
            m = pattern.search(line)
            if m:
                icons.append(m.group(1))
    return icons


def split_camel_case(name: str) -> List[str]:
    """Split a camelCase name into lowercase words.

    Examples:
        chatBubbleFill -> ['chat', 'bubble', 'fill']
        arrowRightShareFill -> ['arrow', 'right', 'share', 'fill']
        xmarkFill -> ['xmark', 'fill']
        threeDotHorizontalFill -> ['three', 'dot', 'horizontal', 'fill']
    """
    # Insert a split before each uppercase letter that follows a lowercase letter
    # or before an uppercase letter followed by a lowercase letter (for runs of caps)
    parts = re.sub(r"([a-z])([A-Z])", r"\1 \2", name)
    parts = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1 \2", parts)
    return [w.lower() for w in parts.split()]


def expand_query(query_words: List[str]) -> List[str]:
    """Expand query words using the synonym map.

    Returns the original words plus any synonym expansions.
    """
    expanded = list(query_words)
    for word in query_words:
        key = word.lower()
        if key in SYNONYMS:
            for syn in SYNONYMS[key]:
                if syn.lower() not in expanded:
                    expanded.append(syn.lower())
    return expanded


def score_icon(icon_words: List[str], query_words: List[str], expanded_words: List[str]) -> float:
    """Score an icon against the query.

    Scoring factors:
    - Exact word matches (highest weight)
    - Prefix/substring matches on icon words
    - Synonym-expanded matches
    - Fill variant boost
    - Penalty for icons with many extra unmatched words
    """
    # Words without fill/stroke for matching purposes
    icon_content = [w for w in icon_words if w not in ("fill", "stroke")]
    query_content = [w for w in expanded_words if w not in ("fill", "stroke")]

    if not query_content:
        return 0.0

    score = 0.0
    matched_icon_words = set()

    # Exact word matches from original query (highest value)
    for qw in query_words:
        qw_lower = qw.lower()
        if qw_lower in ("fill", "stroke"):
            continue
        for i, iw in enumerate(icon_content):
            if iw == qw_lower:
                score += 10.0
                matched_icon_words.add(i)
                break

    # Exact word matches from expanded synonyms (slightly less value)
    for qw in expanded_words:
        qw_lower = qw.lower()
        if qw_lower in query_words or qw_lower in ("fill", "stroke"):
            continue
        for i, iw in enumerate(icon_content):
            if iw == qw_lower and i not in matched_icon_words:
                score += 8.0
                matched_icon_words.add(i)
                break

    # Prefix matches: query word is a prefix of an icon word
    for qw in expanded_words:
        qw_lower = qw.lower()
        if qw_lower in ("fill", "stroke"):
            continue
        for i, iw in enumerate(icon_content):
            if i not in matched_icon_words and iw.startswith(qw_lower) and len(qw_lower) >= 3:
                score += 5.0
                matched_icon_words.add(i)
                break

    # Substring matches: query word is contained in an icon word
    for qw in expanded_words:
        qw_lower = qw.lower()
        if qw_lower in ("fill", "stroke"):
            continue
        for i, iw in enumerate(icon_content):
            if i not in matched_icon_words and qw_lower in iw and len(qw_lower) >= 3:
                score += 3.0
                matched_icon_words.add(i)
                break

    # Reverse substring: icon word is contained in a query word
    # Require the icon word to be at least 4 chars and at least 40% of the query word
    # to avoid spurious matches like "one" in "nonexistenticon"
    for i, iw in enumerate(icon_content):
        if i not in matched_icon_words:
            for qw in expanded_words:
                qw_lower = qw.lower()
                if qw_lower in ("fill", "stroke"):
                    continue
                if iw in qw_lower and len(iw) >= 4 and len(iw) / len(qw_lower) >= 0.4:
                    score += 2.0
                    matched_icon_words.add(i)
                    break

    if score == 0:
        return 0.0

    # Coverage bonus: reward icons where more of the icon's content words matched
    if icon_content:
        coverage = len(matched_icon_words) / len(icon_content)
        score *= (0.5 + 0.5 * coverage)

    # Query coverage bonus: reward matching more query words
    query_match_count = 0
    for qw in query_content:
        for iw in icon_content:
            if iw == qw or iw.startswith(qw) or qw in iw:
                query_match_count += 1
                break
    if query_content:
        query_coverage = query_match_count / len(query_content)
        score *= (0.5 + 0.5 * query_coverage)

    # Fill variant boost (fill icons are more commonly used)
    is_fill = "fill" in icon_words
    is_stroke = "stroke" in icon_words
    if is_fill:
        score *= 1.05

    # Small penalty for extra unmatched words (prefer shorter, more precise icons)
    unmatched = len(icon_content) - len(matched_icon_words)
    if unmatched > 0:
        score *= (1.0 / (1.0 + 0.1 * unmatched))

    return score


def find_icons(query: str, icons_file: str, top_n: int = 5) -> List[Tuple[str, float, List[str]]]:
    """Find the top N matching icons for a query.

    Returns list of (icon_name, score, icon_words) tuples.
    """
    icons = parse_icons(icons_file)
    query_words = query.lower().split()
    expanded_words = expand_query(query_words)

    scored = []
    for icon in icons:
        icon_words = split_camel_case(icon)
        s = score_icon(icon_words, query_words, expanded_words)
        if s > 0:
            scored.append((icon, s, icon_words))

    scored.sort(key=lambda x: -x[1])
    return scored[:top_n]


def find_default_icons_file() -> str:
    """Try to find SIGIcons.tsx in common locations."""
    patterns = [
        # Common icon library locations
        "**/src/components/icon/*Icons.tsx",
        "**/widgets/src/components/icon/*Icons.tsx",
    ]
    for pattern in patterns:
        matches = glob.glob(pattern, recursive=True)
        if matches:
            return matches[0]
    return ""


def main():
    parser = argparse.ArgumentParser(
        description="Fuzzy icon finder for Valdi SIGIcon library"
    )
    parser.add_argument("query", help="Search query (e.g. 'chat bubble', 'settings', 'arrow right')")
    parser.add_argument(
        "--icons-file",
        default="",
        help="Path to SIGIcons.tsx (auto-detected if not provided)",
    )
    parser.add_argument(
        "--top",
        type=int,
        default=5,
        help="Number of results to show (default: 5)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output results as JSON",
    )
    args = parser.parse_args()

    icons_file = args.icons_file
    if not icons_file:
        icons_file = find_default_icons_file()
    if not icons_file:
        print("Error: Could not find SIGIcons.tsx. Please provide --icons-file.", file=sys.stderr)
        sys.exit(1)

    results = find_icons(args.query, icons_file, args.top)

    if args.json:
        import json

        output = []
        for icon_name, score, icon_words in results:
            is_fill = "fill" in icon_words
            is_stroke = "stroke" in icon_words
            variant = "Fill" if is_fill else ("Stroke" if is_stroke else "Other")
            content_words = [w for w in icon_words if w not in ("fill", "stroke")]
            output.append({
                "name": icon_name,
                "variant": variant,
                "words": content_words,
                "score": round(score, 2),
            })
        print(json.dumps(output, indent=2))
    else:
        print(f'Query: "{args.query}"')
        if not results:
            print("  No matching icons found.")
        else:
            # Calculate alignment width from longest icon name
            max_name_len = max(len(r[0]) for r in results)
            for i, (icon_name, score, icon_words) in enumerate(results, 1):
                is_fill = "fill" in icon_words
                is_stroke = "stroke" in icon_words
                variant = "Fill" if is_fill else ("Stroke" if is_stroke else "Other")
                content_words = [w for w in icon_words if w not in ("fill", "stroke")]
                words_str = ", ".join(content_words)
                padding = " " * (max_name_len - len(icon_name))
                variant_padded = f"({variant})" + (" " if variant == "Fill" else "")
                print(f"  {i}. {icon_name}{padding}  {variant_padded} — {words_str}")


if __name__ == "__main__":
    main()
