#!/usr/bin/env python3
"""Checks the Valdi docs for problems that break or degrade the developers.snap.com build.

The site pulls docs/ into a strict Docusaurus build, so a broken link or a stray `<T>`
in one page blocks the deploy for the whole site. This catches those in the PR that
adds them.

Checks:
  links       relative links inside docs/ point at a page that exists and is published
  anchors     `#fragment` links match a heading id, slugged the way Docusaurus does it
  mdx         prose has no bare `<` or `{` that the MDX parser rejects
  repo-links  links that leave docs/ (published as github.com/Snapchat/Valdi links) exist

Usage:
  python3 tools/docs_check/check_docs.py [--docs-root DIR] [--warn anchors]

Exits non-zero when any error-level check finds a problem.
"""

import argparse
import fnmatch
import json
import os
import re
import sys
from dataclasses import dataclass
from typing import Dict, Iterable, List, Optional, Set, Tuple
from urllib.parse import unquote

DEFAULT_DOCS_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs"))
OPTIONS_FILE = os.path.join("website", "docs-options.json")
CHECKS = ("links", "anchors", "mdx", "repo-links", "outside-repo")

# Raw HTML that MDX accepts in prose. Anything else after `<` is parsed as JSX and
# either fails to parse or renders an unknown element.
HTML_TAGS = {
    "a", "abbr", "address", "article", "aside", "audio", "b", "bdi", "bdo", "blockquote", "br",
    "caption", "center", "cite", "code", "col", "colgroup", "dd", "del", "details", "dfn", "div",
    "dl", "dt", "em", "figcaption", "figure", "font", "footer", "h1", "h2", "h3", "h4", "h5", "h6",
    "header", "hr", "i", "iframe", "img", "ins", "kbd", "li", "main", "mark", "nav", "ol", "p",
    "picture", "pre", "q", "s", "samp", "section", "small", "source", "span", "strike", "strong",
    "sub", "summary", "sup", "svg", "table", "tbody", "td", "tfoot", "th", "thead", "time", "tr",
    "track", "tt", "u", "ul", "var", "video", "wbr",
}
# MDX is JSX, so these need `/>`: a bare `<br>` waits for a `</br>` that never comes.
VOID_TAGS = {"br", "col", "hr", "img", "source", "track", "wbr"}

FENCE_RE = re.compile(r"^\s{0,3}(`{3,}|~{3,})")
BLOCKQUOTE_RE = re.compile(r"^(\s{0,3}>\s?)+")
HEADING_RE = re.compile(r"^\s{0,3}(#{1,6})\s+(.*?)\s*#*\s*$")
# An allowed HTML tag, including `{...}` attribute values up to two braces deep.
HTML_TAG_RE = re.compile(r"(?<!\\)</?([a-zA-Z][a-zA-Z0-9-]*)(?:[^<>{}]|\{(?:[^{}]|\{[^{}]*\})*\})*>")
EXPLICIT_ID_RE = re.compile(r"\s*\{#([^}]+)\}\s*$")
INLINE_CODE_RE = re.compile(r"(`+)(.+?)\1")
LINK_RE = re.compile(r"(!?)\[((?:[^\[\]]|\[[^\]]*\])*)\]\(\s*<?([^)\s>]*)>?(?:\s+\"[^\"]*\")?\s*\)")
REF_DEF_RE = re.compile(r"^\s{0,3}\[[^\]]+\]:\s*<?(\S+?)>?(?:\s+.*)?$")
HTML_ATTR_RE = re.compile(r"<(?:a|img|source|video)\b[^>]*?\b(?:href|src)\s*=\s*[\"']([^\"']+)[\"']", re.I)
FRONT_MATTER_RE = re.compile(r"\A---\n.*?\n---\n", re.S)
# github-slugger keeps letters, numbers, marks, `_`, `-` and spaces; everything else
# (punctuation, symbols, emoji) is dropped. Python's \w covers letters, digits and `_`.
SLUG_STRIP_RE = re.compile(r"[^\w\- ]", re.UNICODE)


@dataclass(frozen=True)
class Problem:
    check: str
    path: str
    line: int
    message: str

    def format(self) -> str:
        return f"{self.path}:{self.line}: [{self.check}] {self.message}"


class Slugger:
    """Port of github-slugger, which Docusaurus uses for heading ids."""

    def __init__(self) -> None:
        self.occurrences: Dict[str, int] = {}

    def slug(self, value: str, maintain_case: bool = False) -> str:
        base = value if maintain_case else value.lower()
        base = SLUG_STRIP_RE.sub("", base).replace(" ", "-")
        slug = base
        while slug in self.occurrences:
            self.occurrences[base] += 1
            slug = f"{base}-{self.occurrences[base]}"
        self.occurrences[slug] = 0
        return slug


def heading_text(raw: str) -> str:
    """Approximates mdast-util-to-string for a heading's inline markdown."""
    text = INLINE_CODE_RE.sub(lambda m: m.group(2), raw)
    text = re.sub(r"!\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"(\*\*|__|\*|_|~~)(?=\S)(.+?)(?<=\S)\1", r"\2", text)
    text = re.sub(r"\\([\\`*_{}\[\]()#+\-.!<>])", r"\1", text)
    return text


def prose_lines(content: str) -> Iterable[Tuple[int, str]]:
    """Yields (line_number, text) for lines outside front matter and fenced code."""
    front = FRONT_MATTER_RE.match(content)
    skip_until = content[: front.end()].count("\n") if front else 0
    fence: Optional[str] = None
    in_comment = False
    for number, line in enumerate(content.split("\n"), start=1):
        if number <= skip_until:
            continue
        if in_comment:
            if "-->" not in line:
                continue
            in_comment = False
            line = line.split("-->", 1)[1]
        if line.rfind("<!--") > line.rfind("-->"):
            in_comment = True
            line = line[: line.rfind("<!--")]
        match = FENCE_RE.match(BLOCKQUOTE_RE.sub("", line))
        if fence:
            if match and match.group(1)[0] == fence[0] and len(match.group(1)) >= len(fence):
                fence = None
            continue
        if match:
            fence = match.group(1)
            continue
        # MDX has no indented code blocks, so indented lines are prose (often list continuations).
        yield number, line


def heading_ids(content: str) -> Set[str]:
    slugger = Slugger()
    ids: Set[str] = set()
    first = True
    for _, line in prose_lines(content):
        for html_id in re.findall(r"<[a-zA-Z][^>]*\b(?:id|name)\s*=\s*[\"']([^\"']+)[\"']", strip_inline_code(line)):
            ids.add(html_id)
        match = HEADING_RE.match(BLOCKQUOTE_RE.sub("", line))
        if not match:
            continue
        raw = match.group(2)
        explicit = EXPLICIT_ID_RE.search(raw)
        heading_id = explicit.group(1) if explicit else slugger.slug(heading_text(raw))
        # Docusaurus renders a leading H1 as the page title, without an id, but it still
        # takes its slug: a later "## Same Text" gets "same-text-1".
        if not (first and len(match.group(1)) == 1):
            ids.add(heading_id)
        first = False
    return ids


def strip_inline_code(line: str) -> str:
    return INLINE_CODE_RE.sub(lambda m: " " * len(m.group(0)), line)


class DocsChecker:
    def __init__(self, docs_root: str) -> None:
        self.docs_root = os.path.abspath(docs_root)
        options_path = os.path.join(self.docs_root, OPTIONS_FILE)
        with open(options_path, encoding="utf-8") as f:
            self.options = json.load(f)
        repo_links = self.options.get("repoLinks", {})
        self.repo_root = os.path.normpath(os.path.join(self.docs_root, repo_links.get("rootPath", "..")))
        self.docs = sorted(self._collect_docs())
        self._ids_cache: Dict[str, Set[str]] = {}

    def _collect_docs(self) -> Iterable[str]:
        include = self.options.get("include", ["**/*.{md,mdx}"])
        exclude = self.options.get("exclude", [])
        for dirpath, dirnames, filenames in os.walk(self.docs_root):
            dirnames[:] = [d for d in dirnames if d != "node_modules"]
            for name in filenames:
                rel = os.path.relpath(os.path.join(dirpath, name), self.docs_root).replace(os.sep, "/")
                if _glob_any(rel, include) and not _glob_any(rel, exclude):
                    yield rel

    def ids_for(self, rel: str) -> Set[str]:
        if rel not in self._ids_cache:
            with open(os.path.join(self.docs_root, rel), encoding="utf-8") as f:
                self._ids_cache[rel] = heading_ids(f.read())
        return self._ids_cache[rel]

    def run(self, checks: Iterable[str]) -> List[Problem]:
        problems: List[Problem] = []
        for rel in self.docs:
            with open(os.path.join(self.docs_root, rel), encoding="utf-8") as f:
                content = f.read()
            if {"links", "anchors", "repo-links", "outside-repo"} & set(checks):
                problems.extend(p for p in self._check_links(rel, content) if p.check in checks)
            if "mdx" in checks:
                problems.extend(self._check_mdx(rel, content))
        return problems

    def _check_links(self, rel: str, content: str) -> Iterable[Problem]:
        for number, line in prose_lines(content):
            scan = strip_inline_code(line)
            targets = [m.group(3) for m in LINK_RE.finditer(scan)]
            targets += [m.group(1) for m in HTML_ATTR_RE.finditer(scan)]
            ref = REF_DEF_RE.match(scan)
            if ref:
                targets.append(ref.group(1))
            for target in targets:
                problem = self._check_target(rel, number, target)
                if problem:
                    yield problem

    def _check_target(self, rel: str, number: int, target: str) -> Optional[Problem]:
        if not target or re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target) or target.startswith("//"):
            return None
        path_part, _, fragment = target.partition("#")
        path_part = unquote(path_part.split("?", 1)[0])
        if path_part.startswith("/"):
            return Problem("links", rel, number, f"site-absolute link {target!r}; use a relative path")

        if not path_part:
            target_rel = rel
        else:
            resolved = os.path.normpath(os.path.join(self.docs_root, os.path.dirname(rel), path_part))
            inside = resolved == self.docs_root or resolved.startswith(self.docs_root + os.sep)
            if not (resolved == self.repo_root or resolved.startswith(self.repo_root + os.sep)):
                return Problem("outside-repo", rel, number, f"{target!r} leaves the public repo, so it 404s on GitHub")
            if not os.path.exists(resolved):
                if inside:
                    return Problem("links", rel, number, f"{target!r} does not exist")
                return Problem("repo-links", rel, number, f"{target!r} does not exist in the repo")
            if not inside:
                # The site rewrites these to github.com/Snapchat/Valdi; existence is all we can check.
                return None
            if os.path.isdir(resolved):
                return Problem("links", rel, number, f"{target!r} is a directory; link to a page in it")
            target_rel = os.path.relpath(resolved, self.docs_root).replace(os.sep, "/")
            if not target_rel.endswith((".md", ".mdx")):
                return None
            if target_rel not in self.docs:
                return Problem("links", rel, number, f"{target!r} is excluded from the docs site")

        if fragment and target_rel.endswith((".md", ".mdx")):
            if unquote(fragment) not in self.ids_for(target_rel):
                return Problem("anchors", rel, number, f"{target!r}: no heading with id #{fragment} in {target_rel}")
        return None

    def _check_mdx(self, rel: str, content: str) -> Iterable[Problem]:
        for number, line in prose_lines(content):
            scan = strip_inline_code(line)
            scan = re.sub(r"<!--.*?-->", "", scan)
            scan = EXPLICIT_ID_RE.sub("", scan) if HEADING_RE.match(BLOCKQUOTE_RE.sub("", scan)) else scan
            scan = re.sub(r"\{/\*.*?\*/\}", "", scan)
            scan = re.sub(r"\]\(<[^>]*>", "](", scan)
            scan = re.sub(r"\]:\s*<[^>]*>", "]: ", scan)
            # MDX turns off autolinks, so `<https://...>` is a parse error.
            for match in re.finditer(r"(?<!\\)<((?:https?|mailto):[^>\s]+)>", scan):
                yield Problem("mdx", rel, number, f"autolink {match.group(0)!r} fails to parse in MDX; use [text]({match.group(1)})")
            scan = re.sub(r"(?<!\\)<(?:https?|mailto):[^>\s]+>", "", scan)
            for match in HTML_TAG_RE.finditer(scan):
                closed = match.group(0).endswith("/>") or re.match(rf"\s*</{match.group(1)}\s*>", scan[match.end():], re.I)
                if match.group(1).lower() in VOID_TAGS and not match.group(0).startswith("</") and not closed:
                    yield Problem("mdx", rel, number, f"{match.group(0)[:20]!r} needs to self-close in MDX, like <{match.group(1)} />")
            scan = HTML_TAG_RE.sub(lambda m: "" if m.group(1).lower() in HTML_TAGS else m.group(0), scan)
            for match in re.finditer(r"(?<!\\)<(/?)([^\s>]*)", scan):
                name = re.match(r"[a-zA-Z][a-zA-Z0-9-]*", match.group(2))
                tag = name.group(0).lower() if name else ""
                if match.group(2) == "" and not match.group(1):
                    nxt = scan[match.end():match.end() + 1]
                    if nxt in ("", " ", "\t"):
                        continue
                if tag in HTML_TAGS:
                    continue
                snippet = scan[match.start():match.start() + 20].strip()
                yield Problem("mdx", rel, number, f"bare {snippet!r} is parsed as JSX; wrap it in backticks or escape the <")
            for match in re.finditer(r"(?<!\\)[{}]", scan):
                yield Problem("mdx", rel, number, f"bare {match.group(0)!r} is parsed as an MDX expression; wrap it in backticks or escape it")


def _glob_any(rel: str, patterns: Iterable[str]) -> bool:
    for pattern in patterns:
        for expanded in _expand_braces(pattern):
            if fnmatch.fnmatchcase(rel, expanded) or (expanded.startswith("**/") and fnmatch.fnmatchcase(rel, expanded[3:])):
                return True
    return False


def _expand_braces(pattern: str) -> List[str]:
    match = re.search(r"\{([^{}]*)\}", pattern)
    if not match:
        return [pattern]
    head, tail = pattern[: match.start()], pattern[match.end():]
    return [p for option in match.group(1).split(",") for p in _expand_braces(head + option + tail)]


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--docs-root", default=DEFAULT_DOCS_ROOT)
    parser.add_argument("--warn", action="append", default=[], choices=CHECKS, help="report this check without failing")
    parser.add_argument("--only", action="append", default=[], choices=CHECKS, help="run only this check")
    args = parser.parse_args(argv)

    checks = args.only or list(CHECKS)
    problems = DocsChecker(args.docs_root).run(checks)
    errors = [p for p in problems if p.check not in args.warn]
    for problem in problems:
        level = "warning" if problem.check in args.warn else "error"
        print(f"{level}: {problem.format()}")
    print(f"\n{len(errors)} error(s), {len(problems) - len(errors)} warning(s)")
    if errors:
        print("See tools/docs_check/README.md for how to fix each kind of problem.")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
