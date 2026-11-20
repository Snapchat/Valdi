#!/usr/bin/env python3
"""Extract the public native API surface of the Valdi runtime.

The TypeScript extractor (extract_api.py) only sees what module authors write
in TSX. Native consumers also subclass Valdi views, write attribute binders
against the binder APIs, and start the runtime from Kotlin / Objective-C /
Swift. This script lists those classes and members so that removing or
re-signing one shows up in the API surface check.

Static source scan, stdlib only: regex + brace matching, no compiler. It is
deliberately conservative about what it records:

  Kotlin    public and protected classes / interfaces / objects and their
            public or protected funs, vals, vars and constructors. `override`
            members are skipped (removing one leaves the inherited member).
  Java      public classes and their public or protected members.
  ObjC      every @interface / @protocol / category in a public header, with
            its methods and properties, plus NS_ENUM / NS_OPTIONS values and
            extern functions / constants.
  Swift     public and open types, extensions and their public members, and
            every requirement of a public protocol.

For every recorded class it also lists the attribute names the class binds
(`bind*Attribute("name"` in Kotlin, `bind*Attribute:@"name"` in Objective-C),
so a view that silently loses a binding is caught too.

Output shape:

  {
    "android": {"<fq class name>": {"declaration": str,
                                    "members": {name: [signature, ...]},
                                    "bindings": [attribute, ...]}},
    "ios":     {"<class / @protocol name>": {...}}
  }
"""

import json
import re
import sys
from pathlib import Path

# Source roots, relative to open_source/. A trailing "/*" means "this directory
# only"; otherwise the directory is scanned recursively.
ANDROID_ROOTS = [
    # Runtime entry points (ValdiRuntime, IValdiRuntime, ViewFactory, ...).
    "valdi/src/java/com/snap/valdi/*",
    # View classes consumers subclass or reference.
    "valdi/src/java/com/snap/valdi/views",
    # Attribute binder API and the built-in binders.
    "valdi/src/java/com/snap/valdi/attributes",
]
ANDROID_MODULE_GLOB = "src/valdi_modules/src/valdi/*/android"

IOS_ROOTS = [
    # Runtime entry points (SCValdiRuntime, SCValdiRuntimeManager, SCValdiContext, ...).
    "valdi/src/valdi/ios/*",
    # View classes and the UIView/UIControl categories that bind their attributes.
    "valdi/src/valdi/ios/Views",
    "valdi/src/valdi/ios/Categories",
    # Swift-facing runtime manager.
    "valdi/src/valdi/swift",
    # Swift runtime entry points (AsyncValdiRuntime, marshalling).
    "valdi_core/src/valdi_core/ios/swift/*",
]
# Individual files outside the roots above.
IOS_FILES = [
    # The binder protocol every iOS attribute binder is written against.
    "valdi_core/src/valdi_core/ios/valdi_core/SCValdiAttributesBinderBase.h",
]
IOS_MODULE_GLOB = "src/valdi_modules/src/valdi/*/ios"

# Headers that are not part of the consumer-facing surface even though they
# live next to public ones.
_IOS_PRIVATE_HEADER = re.compile(r"\+(?:Private|CPP)\.h$")


def scanned_roots() -> list:
    """Every path (relative to open_source/) the native extractor reads."""
    roots = [r.rstrip("/*") for r in ANDROID_ROOTS + IOS_ROOTS] + IOS_FILES
    return sorted(set(roots + [ANDROID_MODULE_GLOB, IOS_MODULE_GLOB]))


# ---------------------------------------------------------------------------
# Lexical helpers
# ---------------------------------------------------------------------------


def _mask(text: str, keep_strings: bool = False, objc: bool = False) -> str:
    """Blank out comments (and string contents unless keep_strings).

    The result has the same length and line structure as the input, so
    offsets and brace depths computed on it apply to the original text.
    """
    out = list(text)
    n = len(text)
    i = 0

    def blank(a, b):
        for k in range(a, b):
            if out[k] != "\n":
                out[k] = " "

    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j == -1 else j
            blank(i, j)
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j == -1 else j + 2
            blank(i, j)
            i = j
        elif text.startswith('"""', i):
            j = text.find('"""', i + 3)
            j = n if j == -1 else j + 3
            if not keep_strings:
                blank(i + 3, j - 3)
            i = j
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"' and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            if not keep_strings:
                blank(i + 1, j - 1)
            i = j
        elif c == "'" and objc:
            j = i + 1
            while j < n and text[j] != "'" and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            blank(i + 1, j - 1)
            i = j
        elif c == "'" and re.match(r"'(?:\\.|[^'\\\n])'", text[i : i + 4] + "    "):
            # Kotlin / Java char literal such as '{' or '\n'.
            m = re.match(r"'(?:\\.|[^'\\\n])'", text[i:])
            blank(i + 1, i + m.end() - 1)
            i += m.end()
        else:
            i += 1
    return "".join(out)


def _depths(code: str) -> list:
    """Brace depth *before* each character, or -1 inside parentheses.

    Declarations are only looked for where the depth matches the enclosing
    block, so -1 keeps constructor parameters (`val x: Int` inside a
    multi-line primary constructor) from being read as members.
    """
    depths = [0] * (len(code) + 1)
    d = 0
    paren = 0
    for i, c in enumerate(code):
        depths[i] = d if paren == 0 else -1
        if c == "{":
            d += 1
        elif c == "}":
            d = max(d - 1, 0)
        elif c == "(":
            paren += 1
        elif c == ")":
            paren = max(paren - 1, 0)
    depths[len(code)] = d
    return depths


def _match_brace(code: str, open_idx: int) -> int:
    """Index of the brace closing the one at open_idx (or len(code))."""
    d = 0
    for i in range(open_idx, len(code)):
        if code[i] == "{":
            d += 1
        elif code[i] == "}":
            d -= 1
            if d == 0:
                return i
    return len(code)


def _norm(sig: str) -> str:
    sig = re.sub(r"\s+", " ", sig).strip()
    sig = re.sub(r"([(<\[])\s+", r"\1", sig)
    sig = re.sub(r"\s+([)>\],])", r"\1", sig)
    sig = re.sub(r",\s*([)>])", r"\1", sig)
    sig = re.sub(r"\s*:\s*", ": ", sig)
    return sig


def _header_end(code: str, start: int, stop_at_equals: bool):
    """Scan a Kotlin / Swift declaration header.

    Returns (end_index, has_body). The header ends at the body brace, at '='
    (property initialiser / expression body) when stop_at_equals, at ';', at
    the enclosing block's closing brace, or at a newline that does not
    continue the declaration.
    """
    paren = angle = 0
    i = start
    n = len(code)
    while i < n:
        c = code[i]
        if c in "([":
            paren += 1
        elif c in ")]":
            paren -= 1
        elif c == "<":
            angle += 1
        elif c == ">" and angle > 0 and code[i - 1] != "-":
            angle -= 1
        elif paren <= 0:
            if c == "{":
                return i, True
            if c == "}" or c == ";":
                return i, False
            if (
                c == "="
                and stop_at_equals
                and angle == 0
                and code[i + 1 : i + 2] != "="
                and code[i - 1] not in "!<>="
            ):
                return i, False
            if stop_at_equals and code.startswith(" by ", i):
                return i, False
            if c == "\n" and angle == 0:
                before = code[start:i].rstrip()
                after = code[i + 1 :].lstrip(" \t\r\n")
                continued = before.endswith((",", ":", "->", "(")) or re.match(
                    r"(?::|,|->|where\b|\.)", after
                )
                if not continued:
                    return i, False
        i += 1
    return n, False


# ---------------------------------------------------------------------------
# Kotlin
# ---------------------------------------------------------------------------

_KT_MODIFIERS = (
    "public|private|protected|internal|open|abstract|final|override|data|sealed|enum|"
    "annotation|inner|inline|value|companion|lateinit|const|operator|infix|suspend|"
    "external|tailrec|actual|expect"
)
_KT_DECL = re.compile(
    r"^[ \t]*(?P<ann>(?:@[\w.:]+(?:\([^)\n]*\))?[ \t]+)*)"
    r"(?P<mods>(?:(?:" + _KT_MODIFIERS + r")[ \t]+)*)"
    r"(?P<kw>fun[ \t]+interface|class|interface|object|fun|val|var|constructor|typealias)\b",
    re.M,
)
_KT_BINDING = re.compile(r"\bbind\w*Attribute\w*\(\s*\"(\w+)\"")


class _Collector:
    """Accumulates containers and the source spans they cover."""

    def __init__(self):
        self.containers = {}
        self.spans = []  # (start, end, container_name)

    def container(self, name: str, declaration: str):
        entry = self.containers.setdefault(
            name, {"declaration": declaration, "members": {}, "bindings": []}
        )
        if declaration and not entry["declaration"]:
            entry["declaration"] = declaration
        return entry

    def add_member(self, container: str, name: str, signature: str):
        members = self.containers[container]["members"]
        sigs = members.setdefault(name, [])
        if signature not in sigs:
            sigs.append(signature)

    def owner_at(self, pos: int):
        best = None
        for start, end, name in self.spans:
            if start <= pos <= end and (best is None or start >= best[0]):
                best = (start, end, name)
        return best[2] if best else None


def _kt_name(kw: str, rest: str) -> str:
    rest = re.sub(r"^\s*<[^>]*>\s*", "", rest)
    if kw == "fun":
        head = rest.split("(", 1)[0]
    elif kw in ("val", "var"):
        head = re.split(r"[:=]", rest, 1)[0]
    else:
        head = rest
    words = re.findall(r"[\w`]+", head)
    return words[-1].strip("`") if words else ""


def _kt_enum_entries(code: str, body_start: int, body_end: int) -> list:
    entries = []
    depth = 0
    seg_start = body_start + 1
    segments = []
    i = body_start + 1
    while i < body_end:
        c = code[i]
        if c in "({[":
            depth += 1
        elif c in ")}]":
            depth -= 1
        elif depth == 0 and c in ",;":
            segments.append(code[seg_start:i])
            seg_start = i + 1
            if c == ";":
                break
        i += 1
    else:
        segments.append(code[seg_start:body_end])
    for seg in segments:
        m = re.match(r"\s*(?:@[\w.]+(?:\([^)]*\))?\s+)*([A-Za-z_]\w*)\s*(?:\(|\{|$)", seg)
        if m and not _KT_DECL.match(seg.lstrip("\n")):
            entries.append(m.group(1))
    return entries


def _parse_kotlin(text: str, rel_path: str, col: _Collector):
    code = _mask(text)
    depths = _depths(code)
    pkg_m = re.search(r"^\s*package\s+([\w.]+)", code, re.M)
    pkg = pkg_m.group(1) if pkg_m else ""
    file_container = (pkg + "." if pkg else "") + Path(rel_path).stem + "Kt"

    def walk(block_start, block_end, depth, container, container_kind):
        for m in _KT_DECL.finditer(code, block_start, block_end):
            if depths[m.start("kw")] != depth:
                continue
            mods = m.group("mods").split()
            kw = re.sub(r"\s+", " ", m.group("kw"))
            is_type = kw in ("class", "interface", "object", "fun interface")
            if "private" in mods or "internal" in mods:
                continue
            if kw == "constructor" and container is None:
                continue
            hdr_end, has_body = _header_end(code, m.end("kw"), stop_at_equals=not is_type)
            rest = code[m.end("kw") : hdr_end]
            visible_mods = [x for x in mods if x != "public"]

            if is_type:
                name_m = re.match(r"\s*([\w`]+)", rest)
                if name_m:
                    simple = name_m.group(1).strip("`")
                elif kw == "object" and "companion" in mods:
                    simple = "Companion"
                else:
                    continue
                if container is None:
                    fq = (pkg + "." if pkg else "") + simple
                else:
                    fq = container + "." + simple
                decl = _norm(" ".join(visible_mods + [kw]) + " " + rest)
                col.container(fq, decl)
                if has_body:
                    close = _match_brace(code, hdr_end)
                    col.spans.append((hdr_end, close, fq))
                    if "enum" in mods:
                        for entry in _kt_enum_entries(code, hdr_end, close):
                            col.add_member(fq, entry, "enum entry " + entry)
                    walk(hdr_end + 1, close, depth + 1, fq, kw)
                continue

            # Removing an override leaves the inherited member callable.
            if "override" in mods:
                continue
            target = container
            if target is None:
                target = file_container
                col.container(target, "file " + Path(rel_path).name)
            name = "constructor" if kw == "constructor" else _kt_name(kw, rest)
            if not name:
                continue
            sig = _norm(" ".join(visible_mods + [kw]) + " " + rest)
            col.add_member(target, name, sig)

    walk(0, len(code), 0, None, None)

    # Attribute bindings, attributed to the innermost recorded class.
    bind_code = _mask(text, keep_strings=True)
    for b in _KT_BINDING.finditer(bind_code):
        owner = col.owner_at(b.start())
        if owner and b.group(1) not in col.containers[owner]["bindings"]:
            col.containers[owner]["bindings"].append(b.group(1))


# ---------------------------------------------------------------------------
# Java
# ---------------------------------------------------------------------------

_JAVA_TYPE = re.compile(r"\b(class|interface|enum|@interface|record)\s+(\w+)")


def _parse_java(text: str, rel_path: str, col: _Collector):
    code = _mask(text)
    pkg_m = re.search(r"^\s*package\s+([\w.]+)\s*;", code, re.M)
    pkg = pkg_m.group(1) if pkg_m else ""

    def statements(start, end):
        """Yield (header, body_open_index_or_None) for each member statement."""
        i = start
        while i < end:
            while i < end and code[i] in " \t\r\n;":
                i += 1
            if i >= end:
                return
            paren = 0
            j = i
            while j < end:
                c = code[j]
                if c == "(":
                    paren += 1
                elif c == ")":
                    paren -= 1
                elif paren == 0 and c in ";{":
                    break
                j += 1
            header = code[i:j]
            if j < end and code[j] == "{":
                close = _match_brace(code, j)
                if "=" in header.split("(")[0]:
                    # Field initialised with an array / anonymous class: skip to ';'.
                    k = code.find(";", close)
                    yield header, None
                    i = (k if k != -1 else close) + 1
                else:
                    yield header, j
                    i = close + 1
            else:
                yield header, None
                i = j + 1

    def walk(start, end, outer, outer_kind):
        first = True
        for header, body in statements(start, end):
            header_clean = re.sub(r"@\w+(?:\.\w+)*(?:\([^)]*\))?", " ", header)
            words = header_clean.split()
            tm = _JAVA_TYPE.search(header_clean)
            if outer_kind == "enum" and first and not tm:
                first = False
                # Enum constants precede the first ';'.
                for entry in re.findall(r"(?:^|,)\s*([A-Za-z_]\w*)", re.sub(r"\([^)]*\)", "", header_clean)):
                    col.add_member(outer, entry, "enum entry " + entry)
                continue
            first = False
            public = "public" in words or "protected" in words or outer_kind in ("interface", "@interface")
            if "private" in words or not public:
                continue
            if tm and tm.start() < (header_clean.find("(") if "(" in header_clean else len(header_clean)):
                name = tm.group(2)
                fq = (outer + "." if outer else (pkg + "." if pkg else "")) + name
                decl = _norm(" ".join(w for w in words if w != "public"))
                col.container(fq, decl)
                if body is not None:
                    close = _match_brace(code, body)
                    col.spans.append((body, close, fq))
                    walk(body + 1, close, fq, tm.group(1))
                continue
            if outer is None:
                continue
            sig_src = header_clean.split("=", 1)[0] if "(" not in header_clean.split("=", 1)[0] else header_clean
            sig = _norm(" ".join(w for w in sig_src.split() if w != "public"))
            if "(" in sig:
                name = re.findall(r"(\w+)\s*\(", sig)[0]
            else:
                ids = re.findall(r"\w+", sig)
                if not ids:
                    continue
                name = ids[-1]
            col.add_member(outer, name, sig)

    walk(0, len(code), None, None)

    bind_code = _mask(text, keep_strings=True)
    for b in _KT_BINDING.finditer(bind_code):
        owner = col.owner_at(b.start())
        if owner and b.group(1) not in col.containers[owner]["bindings"]:
            col.containers[owner]["bindings"].append(b.group(1))


# ---------------------------------------------------------------------------
# Objective-C
# ---------------------------------------------------------------------------

_OBJC_BLOCK = re.compile(r"@(interface|protocol)\b")
_OBJC_ENUM = re.compile(
    r"typedef\s+NS_(?:ENUM|OPTIONS|CLOSED_ENUM)\s*\(\s*\w+\s*,\s*(\w+)\s*\)\s*\{([^}]*)\}"
)
_OBJC_EXTERN = re.compile(
    r"^[ \t]*(?:extern|FOUNDATION_EXPORT|FOUNDATION_EXTERN|UIKIT_EXTERN)\s+(?!\"C\")([^;{]+);",
    re.M,
)
_OBJC_BINDING = re.compile(r"\bbind\w*Attribute\w*:\s*@\"(\w+)\"")
_OBJC_IMPL = re.compile(r"@implementation\s+(\w+)(?:\s*\(\s*(\w*)\s*\))?")


def _strip_preprocessor(code: str) -> str:
    lines = code.split("\n")
    out = []
    cont = False
    for line in lines:
        if cont or line.lstrip().startswith("#"):
            cont = line.rstrip().endswith("\\")
            out.append(" " * len(line))
        else:
            out.append(line)
    return "\n".join(out)


def _balanced(s: str, i: int, open_c: str = "(", close_c: str = ")") -> int:
    """Index just past the group starting at s[i] == open_c."""
    d = 0
    for k in range(i, len(s)):
        if s[k] == open_c:
            d += 1
        elif s[k] == close_c:
            d -= 1
            if d == 0:
                return k + 1
    return len(s)


def _objc_selector(sig: str) -> str:
    s = sig[1:].lstrip()
    if s.startswith("("):
        s = s[_balanced(s, 0) :]
    parts = []
    i = 0
    while True:
        m = re.compile(r"\s*(\w*)\s*:\s*").match(s, i)
        if not m:
            break
        parts.append(m.group(1) + ":")
        i = m.end()
        if i < len(s) and s[i] == "(":
            i = _balanced(s, i)
        m2 = re.compile(r"\s*\w+").match(s, i)
        if m2:
            i = m2.end()
    if parts:
        return sig[0] + "".join(parts)
    m = re.match(r"\s*(\w+)", s)
    return sig[0] + (m.group(1) if m else "")


def _strip_trailing_macro(s: str) -> str:
    """Remove one trailing UPPER_CASE macro, with its (possibly nested) arguments."""
    if s.endswith(")"):
        depth = 0
        for k in range(len(s) - 1, -1, -1):
            if s[k] == ")":
                depth += 1
            elif s[k] == "(":
                depth -= 1
                if depth == 0:
                    break
        else:
            return s
        m = re.search(r"\s+[A-Z][A-Z0-9_]+\s*$", s[:k])
        return s[: m.start()].rstrip() if m else s
    m = re.search(r"\s+[A-Z][A-Z0-9_]+\s*$", s)
    return s[: m.start()].rstrip() if m else s


def _objc_property_name(sig: str) -> str:
    m = re.search(r"\(\s*\^\s*(\w+)\s*\)", sig)
    if m:
        return m.group(1)
    s = sig.rstrip(";").strip()
    # Drop trailing attribute macros: NS_SWIFT_NAME(x), API_AVAILABLE(ios(13.0)), NS_UNAVAILABLE.
    while True:
        t = _strip_trailing_macro(s)
        if t == s:
            break
        s = t
    words = re.findall(r"\w+", s)
    return words[-1] if words else ""


def _parse_objc_header(text: str, rel_path: str, col: _Collector):
    code = _strip_preprocessor(_mask(text, objc=True))

    pos = 0
    consumed = []
    while True:
        m = _OBJC_BLOCK.search(code, pos)
        if not m:
            break
        kind = m.group(1)
        # Header runs to the end of the line (or the ivar brace).
        line_end = code.find("\n", m.end())
        line_end = len(code) if line_end == -1 else line_end
        header = code[m.end() : line_end]
        stripped = header.strip()
        if kind == "protocol" and re.match(r"^[\w\s,]*;", stripped):
            pos = line_end
            continue  # forward declaration
        end = code.find("@end", line_end)
        end = len(code) if end == -1 else end
        consumed.append((m.start(), end + 4))
        brace = header.find("{")
        if brace != -1:
            header = header[:brace]
        name_m = re.match(r"\s*(\w+)", header)
        if not name_m:
            pos = end
            continue
        name = name_m.group(1)
        cat_m = re.match(r"\s*\w+\s*(?:<[^>]*>)?\s*\(\s*(\w*)\s*\)", header)
        if kind == "protocol":
            key = "@protocol " + name
        elif cat_m:
            if not cat_m.group(1):
                pos = end
                continue  # class extension, not public surface
            key = f"{name} ({cat_m.group(1)})"
        else:
            key = name
        decl = _norm("@" + kind + " " + header)
        col.container(key, decl)

        body_start = line_end
        if brace != -1:
            ivar_open = m.end() + brace
            body_start = _match_brace(code, ivar_open) + 1
        body = code[body_start:end]
        optional = False
        i = 0
        while i < len(body):
            rest = body[i:]
            lm = re.match(r"\s*(@optional|@required)\b", rest)
            if lm:
                optional = lm.group(1) == "@optional"
                i += lm.end()
                continue
            mm = re.match(r"\s*([-+]\s*\(|@property\b)", rest)
            if mm:
                semi = rest.find(";", mm.start(1))
                semi = len(rest) if semi == -1 else semi
                sig = _norm(rest[mm.start(1) : semi])
                if sig.startswith("@property"):
                    member = _objc_property_name(sig)
                else:
                    sig = re.sub(r"^([-+])\s*", r"\1 ", sig)
                    member = _objc_selector(sig.replace(" ", "", 1))
                if optional:
                    sig = "@optional " + sig
                col.add_member(key, member, sig)
                i += semi + 1
                continue
            nl = rest.find("\n", 1)
            if nl == -1:
                break
            i += nl
        pos = end + 4

    outside = list(code)
    for a, b in consumed:
        for k in range(a, min(b, len(outside))):
            if outside[k] != "\n":
                outside[k] = " "
    outside = "".join(outside)

    for em in _OBJC_ENUM.finditer(outside):
        key = "enum " + em.group(1)
        col.container(key, "typedef NS_ENUM " + em.group(1))
        for value in re.findall(r"(?:^|,)\s*(\w+)", em.group(2)):
            col.add_member(key, value, "case " + value)

    for xm in _OBJC_EXTERN.finditer(outside):
        sig = _norm(xm.group(1))
        if "(" in sig:
            name = re.findall(r"(\w+)\s*\(", sig)
        else:
            name = re.findall(r"\w+", sig)[-1:]
        if not name:
            continue
        col.container("<extern>", "C functions and constants")
        col.add_member("<extern>", name[0] if "(" in sig else name[-1], "extern " + sig)


def _collect_objc_bindings(text: str, col: _Collector):
    # Boundaries come from the string-blanked copy so "@end" in a literal can't cut a class short;
    # binding names come from the copy that keeps strings.
    structure = _mask(text, objc=True)
    code = _mask(text, keep_strings=True, objc=True)
    for im in _OBJC_IMPL.finditer(structure):
        end = structure.find("@end", im.end())
        end = len(code) if end == -1 else end
        cls, cat = im.group(1), im.group(2)
        key = f"{cls} ({cat})" if cat else cls
        if key not in col.containers:
            key = cls
        if key not in col.containers:
            continue
        bindings = col.containers[key]["bindings"]
        for b in _OBJC_BINDING.finditer(code, im.end(), end):
            if b.group(1) not in bindings:
                bindings.append(b.group(1))


# ---------------------------------------------------------------------------
# Swift
# ---------------------------------------------------------------------------

_SWIFT_MODIFIERS = (
    "public|open|private|fileprivate|internal|package|final|static|class|override|required|"
    "convenience|mutating|nonmutating|dynamic|lazy|weak|unowned|indirect|nonisolated|optional"
)
_SWIFT_DECL = re.compile(
    r"^[ \t]*(?P<ann>(?:@\w+(?:\([^)\n]*\))?[ \t]+)*)"
    r"(?P<mods>(?:(?:" + _SWIFT_MODIFIERS + r")(?:\(\w+\))?[ \t]+)*)"
    r"(?P<kw>class|struct|enum|protocol|extension|actor|func|var|let|init|subscript|case|typealias|associatedtype)\b",
    re.M,
)


def _parse_swift(text: str, rel_path: str, col: _Collector):
    code = _mask(text)
    depths = _depths(code)

    def walk(start, end, depth, container, container_kind, default_public):
        for m in _SWIFT_DECL.finditer(code, start, end):
            if depths[m.start("kw")] != depth:
                continue
            mods = m.group("mods").split()
            kw = m.group("kw")
            is_type = kw in ("class", "struct", "enum", "protocol", "extension", "actor")
            explicit_public = "public" in mods or "open" in mods
            hidden = any(x in mods for x in ("private", "fileprivate", "internal", "package"))
            public = explicit_public or (default_public and not hidden)
            hdr_end, has_body = _header_end(code, m.end("kw"), stop_at_equals=not is_type)
            rest = code[m.end("kw") : hdr_end]
            visible_mods = [x for x in mods if x not in ("public",)]

            if is_type:
                name_m = re.match(r"\s*([\w.]+)", rest)
                if not name_m:
                    continue
                type_name = name_m.group(1)
                if kw == "extension":
                    key = "extension " + type_name
                    member_default = explicit_public
                    record = True
                else:
                    key = type_name if container is None else container + "." + type_name
                    member_default = kw == "protocol" or (kw == "enum" and public)
                    record = public
                if not record:
                    continue
                if kw != "extension" or explicit_public:
                    col.container(key, _norm(" ".join(visible_mods + [kw]) + " " + rest))
                if has_body:
                    close = _match_brace(code, hdr_end)
                    walk(hdr_end + 1, close, depth + 1, key, kw, member_default)
                continue

            if container is None or not public or "override" in mods:
                continue
            if container_kind == "extension":
                col.container(container, "extension " + container.split(" ", 1)[1])
            if kw == "case":
                for entry in re.findall(r"(?:^|,)\s*(\w+)", re.sub(r"\([^)]*\)", "", rest)):
                    col.add_member(container, entry, "case " + entry)
                continue
            sig = _norm(" ".join(visible_mods + [kw]) + " " + rest)
            if kw in ("var",) and has_body and container_kind == "protocol":
                close = _match_brace(code, hdr_end)
                sig += " { " + _norm(code[hdr_end + 1 : close]) + " }"
            if kw in ("init", "subscript"):
                name = kw
            else:
                nm = re.match(r"\s*([\w`]+|[^\s(<]+)", rest)
                name = nm.group(1).strip("`") if nm else kw
            col.add_member(container, name, sig)

    walk(0, len(code), 0, None, None, False)


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------


def _iter_files(root: Path, rel: str, suffixes) -> list:
    shallow = rel.endswith("/*")
    base = root / rel.rstrip("/*")
    if base.is_file():
        return [base] if base.suffix in suffixes else []
    if not base.is_dir():
        return []
    it = base.glob("*") if shallow else base.rglob("*")
    return sorted(p for p in it if p.is_file() and p.suffix in suffixes)


def _finish(col: _Collector) -> dict:
    result = {}
    for name in sorted(col.containers):
        entry = col.containers[name]
        result[name] = {
            "declaration": entry["declaration"],
            "members": {k: sorted(v) for k, v in sorted(entry["members"].items())},
            "bindings": sorted(entry["bindings"]),
        }
    return result


def extract_android(root: Path) -> dict:
    col = _Collector()
    rels = list(ANDROID_ROOTS) + sorted(
        str(p.relative_to(root)) for p in root.glob(ANDROID_MODULE_GLOB) if p.is_dir()
    )
    seen = set()
    for rel in rels:
        for path in _iter_files(root, rel, (".kt", ".java")):
            if path in seen:
                continue
            seen.add(path)
            text = path.read_text(encoding="utf-8", errors="replace")
            col.spans = []
            if path.suffix == ".kt":
                _parse_kotlin(text, str(path.relative_to(root)), col)
            else:
                _parse_java(text, str(path.relative_to(root)), col)
    return _finish(col)


def extract_ios(root: Path) -> dict:
    col = _Collector()
    rels = list(IOS_ROOTS) + list(IOS_FILES) + sorted(
        str(p.relative_to(root)) for p in root.glob(IOS_MODULE_GLOB) if p.is_dir()
    )
    headers, impls, swift = [], [], []
    seen = set()
    for rel in rels:
        for path in _iter_files(root, rel, (".h", ".m", ".mm", ".swift")):
            if path in seen:
                continue
            seen.add(path)
            if path.suffix == ".h":
                if not _IOS_PRIVATE_HEADER.search(path.name):
                    headers.append(path)
            elif path.suffix == ".swift":
                swift.append(path)
            else:
                impls.append(path)
    for path in headers:
        _parse_objc_header(path.read_text(encoding="utf-8", errors="replace"), str(path.relative_to(root)), col)
    for path in swift:
        _parse_swift(path.read_text(encoding="utf-8", errors="replace"), str(path.relative_to(root)), col)
    # Bindings last: they attach only to classes declared in a public header.
    for path in impls:
        _collect_objc_bindings(path.read_text(encoding="utf-8", errors="replace"), col)
    return _finish(col)


def extract_native_api(open_source_root: Path) -> dict:
    return {
        "android": extract_android(open_source_root),
        "ios": extract_ios(open_source_root),
    }


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <open_source_root>", file=sys.stderr)
        sys.exit(1)
    api = extract_native_api(Path(sys.argv[1]))
    print(json.dumps(api, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
