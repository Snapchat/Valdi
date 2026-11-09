#!/usr/bin/env python3
"""Match changed files against Valdi's hot systems (.github/hot-systems.yml).

Shared by the public sensitive-file notice workflow, the hot-systems glob check,
and any downstream bot that overlays its own globs on the public list.

    hot_systems.py match --files changed.txt [--format comment|json|labels]
    hot_systems.py check [--root DIR]

`match` reads repo-relative paths (one per line, `-` for stdin). `check` fails
when a glob matches no file under the root, so a moved directory can't silently
turn a warning off.

Glob syntax: `**` spans zero or more directories, `*` and `?` stay inside one
path segment, `{a,b}` is alternation (may nest and may contain `/`).
"""

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field, replace
from typing import Iterable, Optional

HERE = os.path.dirname(os.path.abspath(__file__))
OPEN_SOURCE_ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DEFAULT_CONFIG = os.path.join(OPEN_SOURCE_ROOT, ".github", "hot-systems.yml")

GATES = ("flag", "guard")
COMMENT_MARKER = "<!-- valdi-hot-systems -->"
PUBLIC_BLOB_URL = "https://github.com/Snapchat/Valdi/blob/main"
LABEL_PREFIX = "hot/"
MAX_FILES_PER_SYSTEM = 5
_ID_PATTERN = re.compile(r"^[a-z][a-z0-9]*$")


def _scalar(text: str, where: str):
    text = text.strip()
    if len(text) >= 2 and text[0] == text[-1] and text[0] in "\"'":
        return text[1:-1].replace("''", "'") if text[0] == "'" else json.loads(text)
    if text and text[0] in "[{&*!|>%@`":
        raise ValueError(f"{where}: unsupported YAML syntax `{text}` (quote it)")
    if re.fullmatch(r"-?\d+", text):
        return int(text)
    if text in ("true", "false"):
        return text == "true"
    return text


def _strip_comment(line: str) -> str:
    quote = None
    for i, ch in enumerate(line):
        if quote:
            if ch == quote:
                quote = None
        elif ch in "\"'":
            quote = ch
        elif ch == "#" and (i == 0 or line[i - 1] in " \t"):
            return line[:i].rstrip()
    return line.rstrip()


_MAPPING_ITEM = re.compile(r"""^(?:[^"'\s][^:]*|"[^"]*"|'[^']*'):(?:\s|$)""")


def parse_yaml_subset(text: str, where: str = "<yaml>"):
    """Parse the block-style YAML subset hot-systems files use.

    Nested block mappings and sequences, plain and quoted single-line scalars,
    and comments. Flow style, anchors, tags and multi-line scalars are rejected,
    so every consumer reads the file the same way without a YAML dependency.
    """
    rows = []
    for number, raw in enumerate(text.splitlines(), 1):
        if "\t" in raw[: len(raw) - len(raw.lstrip())]:
            raise ValueError(f"{where}:{number}: tabs are not allowed for indentation")
        line = _strip_comment(raw)
        if line.strip():
            rows.append((number, len(line) - len(line.lstrip(" ")), line.strip()))

    def is_item(content: str) -> bool:
        return content == "-" or content.startswith("- ")

    def parse_block(i: int):
        indent = rows[i][1]
        return parse_sequence(i, indent) if is_item(rows[i][2]) else parse_mapping(i, indent)

    def parse_value(i: int, rest: str, indent: int, number: int):
        if rest:
            return _scalar(rest, f"{where}:{number}"), i + 1
        if i + 1 < len(rows) and rows[i + 1][1] > indent:
            return parse_block(i + 1)
        if i + 1 < len(rows) and rows[i + 1][1] == indent and is_item(rows[i + 1][2]):
            return parse_sequence(i + 1, indent)
        return None, i + 1

    def parse_mapping(i: int, indent: int, first: Optional[str] = None):
        result = {}
        while i < len(rows):
            number, col, content = rows[i]
            if first is not None:
                content, first = first, None
            elif col != indent:
                if col > indent:
                    raise ValueError(f"{where}:{number}: unexpected indentation")
                break
            elif is_item(content):
                break
            key, sep, rest = content.partition(":")
            if not sep or (rest and not rest[0].isspace()):
                raise ValueError(f"{where}:{number}: expected `key: value`")
            key = _scalar(key, f"{where}:{number}")
            if key in result:
                raise ValueError(f"{where}:{number}: duplicate key `{key}`")
            result[key], i = parse_value(i, rest.strip(), indent, number)
        return result, i

    def parse_sequence(i: int, indent: int):
        result = []
        while i < len(rows):
            number, col, content = rows[i]
            if col != indent or not is_item(content):
                if col > indent:
                    raise ValueError(f"{where}:{number}: unexpected indentation")
                break
            item = content[1:].lstrip()
            if not item:
                value, i = parse_value(i, "", indent, number)
            elif _MAPPING_ITEM.match(item):
                value, i = parse_mapping(i, indent + len(content) - len(item), first=item)
            else:
                value, i = _scalar(item, f"{where}:{number}"), i + 1
            result.append(value)
        return result, i

    if not rows:
        return None
    value, end = parse_block(0)
    if end != len(rows):
        raise ValueError(f"{where}:{rows[end][0]}: unexpected content")
    return value


def _load_yaml(path: str):
    with open(path, encoding="utf-8") as f:
        return parse_yaml_subset(f.read(), path)


def _expand_braces(pattern: str) -> list[str]:
    depth = 0
    start = -1
    for i, ch in enumerate(pattern):
        if ch == "{":
            if depth == 0:
                start = i
            depth += 1
        elif ch == "}" and depth > 0:
            depth -= 1
            if depth == 0:
                options, part_depth, last = [], 0, start + 1
                for j in range(start + 1, i):
                    c = pattern[j]
                    if c == "{":
                        part_depth += 1
                    elif c == "}":
                        part_depth -= 1
                    elif c == "," and part_depth == 0:
                        options.append(pattern[last:j])
                        last = j + 1
                options.append(pattern[last:i])
                head, tail = pattern[:start], pattern[i + 1 :]
                return [expanded for option in options for expanded in _expand_braces(head + option + tail)]
    if depth != 0:
        raise ValueError(f"unbalanced braces in glob: {pattern}")
    return [pattern]


def _translate(pattern: str) -> str:
    out = []
    i = 0
    n = len(pattern)
    while i < n:
        ch = pattern[i]
        if ch == "*":
            if i + 1 < n and pattern[i + 1] == "*":
                at_segment_start = i == 0 or pattern[i - 1] == "/"
                i += 2
                if at_segment_start and i < n and pattern[i] == "/":
                    out.append("(?:[^/]*/)*")
                    i += 1
                else:
                    out.append(".*")
                continue
            out.append("[^/]*")
        elif ch == "?":
            out.append("[^/]")
        else:
            out.append(re.escape(ch))
        i += 1
    return "".join(out)


def compile_glob(pattern: str) -> re.Pattern:
    alternatives = [_translate(p) for p in _expand_braces(pattern)]
    return re.compile("^(?:" + "|".join(alternatives) + ")$")


@dataclass
class HotSystem:
    id: str
    label: str
    gate: str
    note: str
    globs: list[str]
    catch_all: bool = False
    exclude: list[str] = field(default_factory=list)
    _compiled: list[tuple[str, re.Pattern]] = field(default_factory=list, init=False, repr=False)
    _excluded: list[re.Pattern] = field(default_factory=list, init=False, repr=False)

    def __post_init__(self) -> None:
        self._compiled = [(g, compile_glob(g)) for g in self.globs]
        self._excluded = [compile_glob(g) for g in self.exclude]

    def matching_glob(self, path: str) -> Optional[str]:
        if any(regex.match(path) for regex in self._excluded):
            return None
        for glob, regex in self._compiled:
            if regex.match(path):
                return glob
        return None

    def with_globs(self, extra: list[str]) -> "HotSystem":
        return replace(self, globs=self.globs + extra)

    def with_exclude(self, extra: list[str]) -> "HotSystem":
        return replace(self, exclude=self.exclude + extra)


@dataclass
class Match:
    system: HotSystem
    files: list[str]


def _require_str(entry: dict, key: str, where: str) -> str:
    value = entry.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{where}: `{key}` must be a non-empty string")
    return value.strip()


def _globs_list(value, loc: str, key: str, required: bool) -> list[str]:
    if value is None and not required:
        return []
    if not isinstance(value, list) or (required and not value) or not all(isinstance(g, str) and g for g in value):
        raise ValueError(f"{loc}: `{key}` must be a {'non-empty ' if required else ''}list of strings")
    for glob in value:
        if glob.startswith("/") or glob.startswith("./"):
            raise ValueError(f"{loc}: glob `{glob}` must be repo-relative")
        compile_glob(glob)
    return value


def parse_systems(data, where: str, prefix: str = "", shared_exclude: Optional[list[str]] = None) -> list[HotSystem]:
    if not isinstance(data, dict) or data.get("version") != 1:
        raise ValueError(f"{where}: expected a mapping with `version: 1`")
    entries = data.get("systems")
    if not isinstance(entries, list) or not entries:
        raise ValueError(f"{where}: `systems` must be a non-empty list")
    if shared_exclude is None:
        shared_exclude = _globs_list(data.get("exclude"), where, "exclude", required=False)
    systems = []
    seen = set()
    for index, entry in enumerate(entries):
        loc = f"{where}: systems[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{loc}: expected a mapping")
        system_id = _require_str(entry, "id", loc)
        if not _ID_PATTERN.match(system_id):
            raise ValueError(f"{loc}: id `{system_id}` must be lowercase letters and digits")
        if system_id in seen:
            raise ValueError(f"{loc}: duplicate id `{system_id}`")
        seen.add(system_id)
        gate = _require_str(entry, "gate", loc)
        if gate not in GATES:
            raise ValueError(f"{loc}: gate must be one of {GATES}, got `{gate}`")
        globs = _globs_list(entry.get("globs"), loc, "globs", required=True)
        catch_all = entry.get("catch_all", False)
        if not isinstance(catch_all, bool):
            raise ValueError(f"{loc}: `catch_all` must be true or false")
        exclude = _globs_list(entry.get("exclude"), loc, "exclude", required=False)
        systems.append(
            HotSystem(
                id=system_id,
                label=_require_str(entry, "label", loc),
                gate=gate,
                note=_require_str(entry, "note", loc),
                globs=[prefix + g for g in globs],
                catch_all=catch_all,
                exclude=[prefix + g for g in shared_exclude + exclude],
            )
        )
    if sum(s.catch_all for s in systems) > 1:
        raise ValueError(f"{where}: at most one `catch_all` system")
    return systems


def load_systems(path: str = DEFAULT_CONFIG, prefix: str = "") -> list[HotSystem]:
    return parse_systems(_load_yaml(path), path, prefix)


def parse_overlay(data, where: str, systems: list[HotSystem]) -> list[HotSystem]:
    """Merge an overlay into `systems`.

    An overlay entry with an existing id only adds globs and excludes (label,
    gate and note stay public). An entry with a new id must be complete and is
    appended. A top-level `exclude:` applies to every system.
    """
    if not isinstance(data, dict) or data.get("version") != 1:
        raise ValueError(f"{where}: expected a mapping with `version: 1`")
    entries = data.get("systems")
    if not isinstance(entries, list) or not entries:
        raise ValueError(f"{where}: `systems` must be a non-empty list")
    shared_exclude = _globs_list(data.get("exclude"), where, "exclude", required=False)
    by_id = {s.id: s for s in systems}
    merged = [replace(s) for s in systems]
    merged_by_id = {s.id: s for s in merged}
    new_entries = []
    for index, entry in enumerate(entries):
        loc = f"{where}: systems[{index}]"
        if not isinstance(entry, dict):
            raise ValueError(f"{loc}: expected a mapping")
        system_id = _require_str(entry, "id", loc)
        if system_id in by_id:
            extra = set(entry) - {"id", "globs", "exclude"}
            if extra:
                raise ValueError(
                    f"{loc}: `{system_id}` exists in the public file; only `globs` and `exclude` can be added"
                )
            globs = _globs_list(entry.get("globs"), loc, "globs", required=False)
            exclude = _globs_list(entry.get("exclude"), loc, "exclude", required=False)
            if not globs and not exclude:
                raise ValueError(f"{loc}: add `globs` or `exclude`")
            merged_by_id[system_id] = merged_by_id[system_id].with_globs(globs).with_exclude(exclude)
        else:
            new_entries.append(entry)
    result = [merged_by_id[s.id].with_exclude(shared_exclude) for s in merged]
    if new_entries:
        result.extend(parse_systems({"version": 1, "systems": new_entries}, where, shared_exclude=shared_exclude))
    return result


def load_with_overlay(public_path: str, overlay_path: str, public_prefix: str) -> list[HotSystem]:
    systems = load_systems(public_path, prefix=public_prefix)
    return parse_overlay(_load_yaml(overlay_path), overlay_path, systems)


def match_files(systems: list[HotSystem], files: Iterable[str]) -> list[Match]:
    hits: dict[str, list[str]] = {}
    for path in files:
        path = path.strip()
        if not path:
            continue
        named = False
        for system in systems:
            if not system.catch_all and system.matching_glob(path):
                hits.setdefault(system.id, []).append(path)
                named = True
        if not named:
            for system in systems:
                if system.catch_all and system.matching_glob(path):
                    hits.setdefault(system.id, []).append(path)
    return [Match(s, sorted(set(hits[s.id]))) for s in systems if s.id in hits]


def labels(matches: list[Match]) -> list[str]:
    return [LABEL_PREFIX + m.system.id for m in matches]


def _files_line(files: list[str], strip_prefix: str) -> str:
    shown = [f"`{f[len(strip_prefix):] if strip_prefix and f.startswith(strip_prefix) else f}`" for f in files]
    if len(shown) > MAX_FILES_PER_SYSTEM:
        rest = len(shown) - MAX_FILES_PER_SYSTEM
        shown = shown[:MAX_FILES_PER_SYSTEM] + [f"and {rest} more"]
    return ", ".join(shown)


def render_comment(
    matches: list[Match],
    policy_link: str,
    flag_doc_link: str,
    checklist: bool = True,
    strip_prefix: str = "",
    footer: str = "",
    guide_link: str = "",
) -> str:
    """Render the hot-systems PR comment. Empty string when nothing matched."""
    if not matches:
        return ""
    lines = [COMMENT_MARKER, "## Valdi hot systems touched", ""]
    if guide_link:
        lines += [f"**What to do:** [your PR got this comment, now what?]({guide_link})", ""]
    for m in matches:
        lines.append(f"- **{m.system.label}** (`{LABEL_PREFIX}{m.system.id}`, gate: {m.system.gate}): {m.system.note}")
        lines.append(f"  - {_files_line(m.files, strip_prefix)}")
    lines.append("")
    flagged = [m for m in matches if m.system.gate == "flag"]
    guarded = [m for m in matches if m.system.gate == "guard"]
    if flagged:
        lines.append(
            "Significant changes to the **flag** systems above ship behind a runtime tweak that defaults to the "
            f"old behavior ([how to flag a change]({flag_doc_link}))."
        )
        lines.append("")
    if guarded:
        lines.append(
            "Significant changes to the **guard** systems above can't be covered by a runtime flag; they need the "
            "system's tests to pass and a release bake instead."
        )
        lines.append("")
    lines.append(
        "A change is significant if it alters lifetime or teardown order, threading or locks, a bridge contract, "
        "error behavior, a default, a base-class contract, the startup path or a build-host format, or if it removes "
        f"something that looks dead, adds hot-path logging, or refactors hot code. See [the policy]({policy_link})."
    )
    if checklist:
        lines += [
            "",
            "- [ ] Not significant (say why in the PR description), or",
            "- [ ] Behind a flag: `VALDI_ENABLE_<AREA>_<CHANGE>`, default off, or",
            "- [ ] Guard system: no runtime flag can cover it, and the system's tests ran",
            "- [ ] Lifetime / raising-callsite audit or thread and lock map in the PR description "
            "(teardown, threading, marshalling)",
        ]
    lines += ["", "<sub>Advisory only. This comment never blocks a merge.</sub>"]
    if footer:
        lines += [footer]
    return "\n".join(lines)


def tracked_files(root: str) -> list[str]:
    try:
        out = subprocess.run(
            ["git", "-C", root, "ls-files", "-z", "--", "."],
            check=True,
            capture_output=True,
        ).stdout.decode("utf-8")
        return [p for p in out.split("\0") if p]
    except (OSError, subprocess.CalledProcessError):
        found = []
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [d for d in dirnames if d != ".git"]
            for name in filenames:
                found.append(os.path.relpath(os.path.join(dirpath, name), root).replace(os.sep, "/"))
        return found


def dead_globs(systems: list[HotSystem], files: list[str]) -> list[tuple[str, str]]:
    """(system id, glob) pairs where the glob, or one of its `{a,b}` branches, matches no file."""
    dead = []
    for system in systems:
        for glob in system.globs + system.exclude:
            for branch in _expand_braces(glob):
                regex = compile_glob(branch)
                if not any(regex.match(f) for f in files):
                    dead.append((system.id, glob if branch == glob else f"{glob} (branch {branch})"))
    return dead


def _read_files(path: str) -> list[str]:
    if path == "-":
        return sys.stdin.read().splitlines()
    with open(path, encoding="utf-8") as f:
        return f.read().splitlines()


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", default=DEFAULT_CONFIG)
    sub = parser.add_subparsers(dest="command", required=True)

    match_cmd = sub.add_parser("match")
    match_cmd.add_argument("--files", required=True, help="file with one repo-relative path per line, or -")
    match_cmd.add_argument("--format", choices=("comment", "json", "labels"), default="comment")
    match_cmd.add_argument("--policy-link", default=f"{PUBLIC_BLOB_URL}/CONTRIBUTING.md#safe-changes")
    match_cmd.add_argument(
        "--flag-doc-link", default=f"{PUBLIC_BLOB_URL}/docs/docs/workflow-flagging-changes.md"
    )
    match_cmd.add_argument(
        "--guide-link", default=f"{PUBLIC_BLOB_URL}/docs/docs/workflow-hot-systems-comment.md"
    )

    check_cmd = sub.add_parser("check")
    check_cmd.add_argument("--root", default=OPEN_SOURCE_ROOT)

    args = parser.parse_args(argv)
    systems = load_systems(args.config)

    if args.command == "check":
        dead = dead_globs(systems, tracked_files(args.root))
        for system_id, glob in dead:
            print(f"{args.config}: `{system_id}` glob matches no file: {glob}", file=sys.stderr)
        if dead:
            print(
                "Fix or remove the globs above so the hot-systems notice keeps covering these areas.", file=sys.stderr
            )
            return 1
        print(f"OK: {sum(len(s.globs) for s in systems)} globs in {len(systems)} systems all match a file.")
        return 0

    matches = match_files(systems, _read_files(args.files))
    if args.format == "json":
        print(
            json.dumps(
                [{"id": m.system.id, "label": m.system.label, "gate": m.system.gate, "files": m.files} for m in matches]
            )
        )
    elif args.format == "labels":
        print("\n".join(labels(matches)))
    else:
        print(render_comment(matches, args.policy_link, args.flag_doc_link, guide_link=args.guide_link))
    return 0


if __name__ == "__main__":
    sys.exit(main())
