#!/usr/bin/env python3
"""Fail when a native element attribute declared in TypeScript has no iOS or Android implementation.

Every attribute that module authors can set on a native element (`<view maskImage=...>`,
`<label font=...>`, ...) is declared in valdi_tsx's NativeTemplateElements.d.ts. Each one must be
registered with the attributes binder on BOTH platforms, or by the shared C++ runtime. If a
platform registration is deleted, the attribute silently becomes a no-op on that platform: the
TypeScript still type-checks, the runtime just drops the value. This check catches that statically.

Declared set:
  Properties of every interface annotated `@NativeTemplateElement(...)` in
  NativeTemplateElements.d.ts, resolved through `extends` chains to interfaces in the same file.
  Elements annotated `slot: true` are skipped (they have no native backing).

Implemented sets (attribute names only, no per-view-class scoping):
  iOS      `bind*Attribute*:@"name"` and composite parts `initWithAttribute:@"name"` in
           Objective-C / Swift under the iOS source roots.
  Android  `bind*Attribute*("name"` and `CompositeAttributePart("name"` in Kotlin / Java under the
           Android source roots.
  Shared   Names bound by the C++ runtime's attribute binders (Yoga layout, accessibility, scroll,
           asset, transform, sticky, view-node callbacks). These count for both platforms.

Registration arguments may be string literals or string constants defined anywhere under the same
platform's source roots (`NSString *const kX = @"x"`, `const val X = "x"`). Edge-expanding binders
(`[@"touchAreaExtension" stringByAppendingString:direction]`, `bindPaddingOrMarginAttributes("margin"`)
register the prefix plus its Top/Right/Bottom/Left variants. Gesture attributes registered via a
function+predicate binder derive `<name>Predicate` and `<name>Disabled` siblings; those count as
implemented when `<name>` is.

Known blind spots: matching is by attribute name across all view classes (a name registered for one
view satisfies every element that declares it); a registration whose apply block is a no-op still
counts; names built dynamically in other ways are not seen; macOS and the web renderer are not
checked.

Intentionally single-platform or runtime-handled attributes go in allowlist.txt. Stale allowlist
entries (attribute no longer declared, or now implemented on the listed platform) are errors too,
so the list can't quietly rot.

Exit codes:
  0  every declared attribute is implemented on iOS and Android (or allowlisted)
  1  missing implementations or stale allowlist entries

Usage:
  check_native_impl.py <open_source_root> [--allowlist PATH] [--verbose]
"""

import argparse
import re
import sys
from pathlib import Path

PLATFORMS = ("ios", "android")
RUNTIME = "runtime"

DECLARATIONS_FILE = "src/valdi_modules/src/valdi/valdi_tsx/src/NativeTemplateElements.d.ts"

# Source roots, relative to the open_source root. Globs are matched with Path.glob.
IOS_SOURCES = [
    "valdi/src/valdi/ios/**/*.m",
    "valdi/src/valdi/ios/**/*.mm",
    "valdi/src/valdi/ios/**/*.swift",
    "valdi_core/src/valdi_core/ios/**/*.m",
    "valdi_core/src/valdi_core/ios/**/*.mm",
    "valdi_core/src/valdi_core/ios/**/*.swift",
    "src/valdi_modules/src/valdi/*/ios/**/*.m",
    "src/valdi_modules/src/valdi/*/ios/**/*.mm",
    "src/valdi_modules/src/valdi/*/ios/**/*.swift",
]
ANDROID_SOURCES = [
    "valdi/src/java/**/*.kt",
    "valdi/src/java/**/*.java",
    "valdi_core/src/valdi_core/android/**/*.kt",
    "valdi_core/src/valdi_core/android/**/*.java",
    "src/valdi_modules/src/valdi/*/android/**/*.kt",
    "src/valdi_modules/src/valdi/*/android/**/*.java",
]
# Only the runtime's binding code. Deliberately excludes DefaultAttributeProcessors.cpp: a
# preprocessor (e.g. maskImage's gradient parser) transforms a value but applies nothing, so it
# must not count as an implementation.
SHARED_CPP_SOURCES = [
    "valdi/src/valdi/runtime/Attributes/AttributesBindingContextImpl.cpp",
    "valdi/src/valdi/runtime/Attributes/AccessibilityAttributes.cpp",
    "valdi/src/valdi/runtime/Attributes/AssetAttributes.cpp",
    "valdi/src/valdi/runtime/Attributes/DefaultAttributes.cpp",
    "valdi/src/valdi/runtime/Attributes/ScrollAttributes.cpp",
    "valdi/src/valdi/runtime/Attributes/ScrollAnchorAttributes.cpp",
    "valdi/src/valdi/runtime/Attributes/StickyAttributes.cpp",
    "valdi/src/valdi/runtime/Attributes/Yoga/YogaAttributes.cpp",
]

DERIVED_SUFFIXES = ("Predicate", "Disabled")

DEFAULT_ALLOWLIST = Path(__file__).parent / "allowlist.txt"

RED = "\033[31m"
GREEN = "\033[32m"
BOLD = "\033[1m"
RESET = "\033[0m"

# ---------------------------------------------------------------------------
# Declarations (TypeScript)
# ---------------------------------------------------------------------------

_ELEMENT_ANNOTATION = re.compile(r"@NativeTemplateElement\(\s*\{([^}]*)\}\s*\)")
_INTERFACE_HEADER = re.compile(r"(?:export\s+)?interface\s+(\w+)(?:\s+extends\s+([^{]+?))?\s*\{")
_PROPERTY = re.compile(r"(?:readonly\s+)?(\w+)\??\s*[:(]")


def _strip_comments(text: str) -> str:
    text = re.sub(r"/\*[\s\S]*?\*/", "", text)
    return re.sub(r"//[^\n]*", "", text)


def _matching_brace(text: str, open_index: int) -> int:
    """Return the index of the `}` matching the `{` at open_index."""
    depth = 0
    for i in range(open_index, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    raise ValueError("unbalanced braces in declaration file")


def _split_extends(raw: str) -> list:
    """Split an extends clause on top-level commas, dropping generic arguments."""
    names, depth, current = [], 0, ""
    for ch in raw:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        elif ch == "," and depth == 0:
            names.append(current)
            current = ""
            continue
        if depth == 0 and ch not in "<>":
            current += ch
    names.append(current)
    return [n.strip() for n in names if n.strip()]


def _top_level_members(body: str) -> list:
    """Member names declared directly in an interface body (not inside nested object types)."""
    members, depth, statement_start = [], 0, True
    i = 0
    while i < len(body):
        ch = body[i]
        if ch in "{([":
            depth += 1
        elif ch in "})]":
            depth -= 1
        elif ch in ";," and depth == 0:
            statement_start = True
        elif depth == 0 and statement_start and not ch.isspace():
            m = _PROPERTY.match(body, i)
            if m:
                members.append(m.group(1))
            statement_start = False
        i += 1
    return members


def parse_interfaces(text: str) -> dict:
    """Map interface name -> {"extends": [...], "members": [...]} (comments stripped first)."""
    text = _strip_comments(text)
    interfaces = {}
    for m in _INTERFACE_HEADER.finditer(text):
        open_index = m.end() - 1
        body = text[open_index + 1 : _matching_brace(text, open_index)]
        interfaces[m.group(1)] = {
            "extends": _split_extends(m.group(2) or ""),
            "members": _top_level_members(body),
        }
    return interfaces


def _parse_annotation_args(raw: str) -> dict:
    """Parse `{ios: 'X', layout: true, jsx: 'view'}` into a dict of strings."""
    args = {}
    for key, value in re.findall(r"(\w+)\s*:\s*('[^']*'|\"[^\"]*\"|\w+)", raw):
        args[key] = value.strip("'\"")
    return args


def parse_elements(text: str) -> dict:
    """Map element interface name -> annotation args, for every @NativeTemplateElement."""
    elements = {}
    for m in _ELEMENT_ANNOTATION.finditer(text):
        header = _INTERFACE_HEADER.search(text, m.end())
        if header is None:
            continue
        elements[header.group(1)] = _parse_annotation_args(m.group(1))
    return elements


def _resolve_members(name: str, interfaces: dict, seen=None) -> set:
    seen = seen if seen is not None else set()
    if name in seen or name not in interfaces:
        return set()
    seen.add(name)
    members = set(interfaces[name]["members"])
    for parent in interfaces[name]["extends"]:
        members |= _resolve_members(parent, interfaces, seen)
    return members


def parse_declarations(text: str) -> dict:
    """Map declared attribute name -> sorted list of element tags (jsx names) declaring it."""
    interfaces = parse_interfaces(text)
    declared = {}
    for element, args in parse_elements(text).items():
        if args.get("slot") == "true":
            continue
        tag = args.get("jsx", element)
        for attribute in _resolve_members(element, interfaces):
            declared.setdefault(attribute, set()).add(tag)
    return {k: sorted(v) for k, v in declared.items()}


# ---------------------------------------------------------------------------
# Implementations (native sources)
# ---------------------------------------------------------------------------

# Each pattern captures either a string literal (`"name"`) or an identifier naming a string
# constant (`kSCValdiFontAttribute`), which is resolved through the platform's constant table.
_NAME = r"(\"\w+\"|[A-Za-z_]\w*)"
_IOS_PATTERNS = [
    re.compile(r"\bbind\w*Attribute\w*:\s*@?" + _NAME),
    re.compile(r"\binitWithAttribute:\s*@?" + _NAME),
    # Swift: binder.bindStringAttribute("name", ...)
    re.compile(r"\bbind\w*Attribute\w*\(\s*" + _NAME),
]
# [@"touchAreaExtension" stringByAppendingString:direction] inside an edge loop.
_IOS_EDGE_PREFIX = re.compile(r"\binitWithAttribute:\s*\[@\"(\w+)\"\s+stringByAppendingString:")
_IOS_CONSTANT = re.compile(r"NSString\s*\*\s*(?:const\s+)?(\w+)\s*=\s*@\"(\w+)\"")

_ANDROID_PATTERNS = [
    re.compile(r"\bbind\w*Attribute\w*\(\s*" + _NAME),
    re.compile(r"\bCompositeAttributePart\(\s*" + _NAME),
]
_ANDROID_EDGE_PREFIX = re.compile(r"(?!x)x")  # none today
_ANDROID_CONSTANT = re.compile(r"(?:const\s+val|static\s+final\s+String)\s+(\w+)\s*(?::\s*String\s*)?=\s*\"(\w+)\"")

_CPP_PATTERNS = [
    # binder.bindViewNodeCallback("onViewCreate", ...), bindYGStyleLengthAttribute("top", ...)
    re.compile(r"\bbind\w*\(\s*(?:STRING_LITERAL\()?(\"\w+\")"),
    # composite parts: parts.emplace_back(STRING_LITERAL("transform"), ...)
    re.compile(r"\bemplace_back\(\s*(?:STRING_LITERAL\(|_attributeIds\.getIdForName\()(\"\w+\")"),
    # auto srcAttributeId = _attributeIds.getIdForName("src");
    re.compile(r"\bauto\s+\w*AttributeId\s*=\s*_?\w*\.getIdForName\(\s*(\"\w+\")"),
]
# bindPaddingOrMarginAttributes("margin", ...) binds margin + marginTop/Right/Bottom/Left.
_CPP_EDGE_PREFIX = re.compile(r"\bbind(?:PaddingOrMargin|Edge)Attributes\(\s*\"(\w+)\"")
_CPP_CONSTANT = re.compile(r"(?!x)x")  # literals only

EDGE_SUFFIXES = ("Top", "Right", "Bottom", "Left")


def _strip_code_comments(text: str) -> str:
    """Drop // and /* */ comments so commented-out registrations don't count."""
    text = re.sub(r"/\*[\s\S]*?\*/", "", text)
    return re.sub(r"(?<![:\"])//[^\n]*", "", text)


class _Language:
    def __init__(self, patterns, edge_prefix, constant):
        self.patterns = patterns
        self.edge_prefix = edge_prefix
        self.constant = constant

    def constants(self, text: str) -> dict:
        return dict(self.constant.findall(_strip_code_comments(text)))

    def extract(self, text: str, constants=None) -> set:
        """Attribute names registered in `text`. `constants` defaults to the file's own constants."""
        text = _strip_code_comments(text)
        constants = self.constants(text) if constants is None else constants
        names = set()
        for pattern in self.patterns:
            for token in pattern.findall(text):
                if token.startswith('"'):
                    names.add(token.strip('"'))
                elif token in constants:
                    names.add(constants[token])
        for prefix in self.edge_prefix.findall(text):
            names.add(prefix)
            names.update(prefix + edge for edge in EDGE_SUFFIXES)
        return names


IOS = _Language(_IOS_PATTERNS, _IOS_EDGE_PREFIX, _IOS_CONSTANT)
ANDROID = _Language(_ANDROID_PATTERNS, _ANDROID_EDGE_PREFIX, _ANDROID_CONSTANT)
SHARED_CPP = _Language(_CPP_PATTERNS, _CPP_EDGE_PREFIX, _CPP_CONSTANT)


def extract_ios(text: str, constants=None) -> set:
    return IOS.extract(text, constants)


def extract_android(text: str, constants=None) -> set:
    return ANDROID.extract(text, constants)


def extract_shared_cpp(text: str) -> set:
    return SHARED_CPP.extract(text)


def _collect(root: Path, globs: list, language: _Language) -> set:
    """Two passes: build a source-root-wide constant table, then extract registrations."""
    texts = []
    for pattern in globs:
        for path in sorted(root.glob(pattern)):
            texts.append(path.read_text(encoding="utf-8", errors="replace"))
    constants = {}
    for text in texts:
        constants.update(language.constants(text))
    names = set()
    for text in texts:
        names |= language.extract(text, constants)
    return names


def _with_derived(names: set) -> set:
    return names | {n + suffix for n in names for suffix in DERIVED_SUFFIXES}


# ---------------------------------------------------------------------------
# Allowlist
# ---------------------------------------------------------------------------


def parse_allowlist(text: str) -> dict:
    """Parse `attribute  platforms  reason` lines into attribute -> (set(platforms), reason).

    platforms is `ios`, `android`, `ios,android`, or `runtime`. `runtime` marks an attribute
    consumed by the TypeScript renderer or the C++ runtime itself (key, ref, style, ...), never by a
    platform binder; it is allowed to be missing on both platforms. A reason is required.
    """
    entries = {}
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 2)
        if len(parts) < 3:
            raise ValueError(f"allowlist line {lineno}: expected '<attribute> <platforms> <reason>': {raw!r}")
        attribute, platforms_raw, reason = parts
        platforms = set(platforms_raw.split(","))
        unknown = platforms - set(PLATFORMS) - {RUNTIME}
        if unknown:
            raise ValueError(f"allowlist line {lineno}: unknown platform(s) {sorted(unknown)}")
        if RUNTIME in platforms and len(platforms) > 1:
            raise ValueError(f"allowlist line {lineno}: '{RUNTIME}' cannot be combined with platforms")
        if attribute in entries:
            raise ValueError(f"allowlist line {lineno}: duplicate entry for {attribute!r}")
        entries[attribute] = (platforms, reason)
    return entries


# ---------------------------------------------------------------------------
# Check
# ---------------------------------------------------------------------------


def find_missing(declared: dict, implemented: dict, shared: set) -> dict:
    """Map attribute -> sorted list of platforms where it has no implementation."""
    missing = {}
    for attribute in sorted(declared):
        absent = [
            p for p in PLATFORMS if attribute not in _with_derived(implemented[p] | shared)
        ]
        if absent:
            missing[attribute] = absent
    return missing


def evaluate(declared: dict, implemented: dict, shared: set, allowlist: dict):
    """Return (errors, stale) lists of human-readable strings."""
    missing = find_missing(declared, implemented, shared)
    errors = []
    for attribute, platforms in missing.items():
        allowed = allowlist.get(attribute, (set(), ""))[0]
        if RUNTIME in allowed:
            continue
        unallowed = [p for p in platforms if p not in allowed]
        if unallowed:
            elements = ", ".join(f"<{t}>" for t in declared[attribute])
            errors.append(f"{attribute} (on {elements}): no {' or '.join(unallowed)} implementation")

    stale = []
    for attribute, (platforms, _reason) in sorted(allowlist.items()):
        if attribute not in declared:
            stale.append(f"{attribute}: allowlisted but no longer declared in TypeScript")
            continue
        if RUNTIME in platforms:
            continue
        implemented_on = sorted(p for p in platforms if p not in missing.get(attribute, []))
        if implemented_on:
            stale.append(f"{attribute}: allowlisted for {','.join(implemented_on)} but is now implemented there")
    return errors, stale


def run(root: Path, allowlist_path: Path, verbose: bool = False) -> int:
    declarations_path = root / DECLARATIONS_FILE
    declared = parse_declarations(declarations_path.read_text(encoding="utf-8"))
    implemented = {
        "ios": _collect(root, IOS_SOURCES, IOS),
        "android": _collect(root, ANDROID_SOURCES, ANDROID),
    }
    shared = _collect(root, SHARED_CPP_SOURCES, SHARED_CPP)
    allowlist = parse_allowlist(allowlist_path.read_text(encoding="utf-8"))
    try:
        allowlist_display = allowlist_path.resolve().relative_to(root)
    except ValueError:
        allowlist_display = allowlist_path

    # Sanity guard: if the extractors match nothing, a refactor broke them and every attribute
    # would be reported. Fail loudly with a pointer at the extractor instead.
    for name, names in (("iOS", implemented["ios"]), ("Android", implemented["android"]), ("shared C++", shared)):
        if not names:
            print(f"{RED}ERROR{RESET}: found no {name} attribute registrations; the extractor patterns in")
            print(f"       {Path(__file__).name} no longer match the source tree and need updating.")
            return 1

    print(
        f"Declared attributes: {len(declared)} | registered: iOS {len(implemented['ios'])}, "
        f"Android {len(implemented['android'])}, shared C++ {len(shared)} | allowlisted: {len(allowlist)}"
    )
    errors, stale = evaluate(declared, implemented, shared, allowlist)

    if verbose:
        for attribute, platforms in find_missing(declared, implemented, shared).items():
            print(f"  missing {attribute}: {','.join(platforms)}")

    if errors:
        print(f"\n{BOLD}{RED}Declared native attributes with no platform implementation:{RESET}")
        for e in errors:
            print(f"  {RED}ERROR{RESET}: {e}")
        print(
            "\nEach attribute in NativeTemplateElements.d.ts must be registered with the attributes\n"
            "binder on iOS (e.g. `bindAttributes:` in UIView+Valdi.m) and on Android (e.g.\n"
            "ViewAttributesBinder.kt), or by the shared C++ runtime. A missing registration makes\n"
            "the attribute a silent no-op on that platform. Restore the implementation, or, if the\n"
            "attribute is intentionally single-platform, add it to\n"
            f"{allowlist_display} with a reason."
        )
    if stale:
        print(f"\n{BOLD}{RED}Stale allowlist entries (remove them from {allowlist_path.name}):{RESET}")
        for s in stale:
            print(f"  {RED}ERROR{RESET}: {s}")

    if errors or stale:
        return 1
    print(f"{GREEN}OK{RESET}: every declared native attribute is implemented on iOS and Android.")
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("root", type=Path, help="open_source root directory")
    parser.add_argument("--allowlist", type=Path, default=DEFAULT_ALLOWLIST)
    parser.add_argument("--verbose", action="store_true", help="list every missing attribute, allowlisted or not")
    args = parser.parse_args(argv)
    return run(args.root.resolve(), args.allowlist, args.verbose)


if __name__ == "__main__":
    sys.exit(main())
