#!/usr/bin/env python3
"""Extract the public API surface from Valdi TypeScript declaration files.

Parses NativeTemplateElements.d.ts and Component.ts to produce a canonical
JSON representation of every interface, type alias, enum, and class that
module authors can depend on.
"""

import json
import re
import sys
from pathlib import Path


def _strip_comments(text: str) -> str:
    """Remove block comments and single-line comments."""
    text = re.sub(r"/\*[\s\S]*?\*/", "", text)
    text = re.sub(r"//.*$", "", text, flags=re.MULTILINE)
    return text


def _normalize_type(t: str) -> str:
    """Collapse whitespace in a type signature to a single space."""
    return re.sub(r"\s+", " ", t).strip()


def _parse_interfaces(text: str) -> dict:
    """Extract interfaces with extends chains and property declarations."""
    interfaces = {}
    pattern = re.compile(
        r"(?:export\s+)?interface\s+(\w+)"
        r"(?:\s+extends\s+([\w\s,<>]+?))?"
        r"\s*\{"
    )
    pos = 0
    while pos < len(text):
        m = pattern.search(text, pos)
        if not m:
            break
        name = m.group(1)
        extends_raw = m.group(2) or ""
        extends = [e.strip() for e in extends_raw.split(",") if e.strip()]

        brace_start = m.end() - 1
        depth = 1
        i = brace_start + 1
        while i < len(text) and depth > 0:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
            i += 1
        body = text[brace_start + 1 : i - 1]

        properties = _parse_properties(body)
        interfaces[name] = {"extends": sorted(extends), "properties": properties}
        pos = i
    return interfaces


def _parse_properties(body: str) -> dict:
    """Extract property name -> type from an interface body."""
    props = {}
    body = _strip_comments(body)

    tokens = body.strip()
    if not tokens:
        return props

    i = 0
    while i < len(tokens):
        # Skip whitespace
        while i < len(tokens) and tokens[i] in " \t\n\r":
            i += 1
        if i >= len(tokens):
            break

        # Try to match a property name (with optional ?)
        prop_match = re.match(r"(readonly\s+)?(\w+)(\??)\s*:\s*", tokens[i:])
        if not prop_match:
            # Skip to next semicolon or line
            next_semi = tokens.find(";", i)
            if next_semi == -1:
                break
            i = next_semi + 1
            continue

        prop_name = prop_match.group(2)
        optional = prop_match.group(3) == "?"
        i += prop_match.end()

        # Now extract the type until the matching semicolon,
        # respecting nested braces, parens, angle brackets, and arrow functions
        type_str, end = _extract_type(tokens, i)
        if type_str is not None:
            normalized = _normalize_type(type_str)
            if optional:
                props[prop_name] = f"{normalized} (optional)"
            else:
                props[prop_name] = normalized
            i = end + 1  # skip the semicolon
        else:
            break

    return dict(sorted(props.items()))


def _extract_type(text: str, start: int):
    """Extract a type expression starting at `start`, ending at ';'.

    Returns (type_string, semicolon_index) or (None, -1).
    """
    depth_brace = 0
    depth_paren = 0
    depth_angle = 0
    i = start
    while i < len(text):
        ch = text[i]
        if ch == "{":
            depth_brace += 1
        elif ch == "}":
            if depth_brace > 0:
                depth_brace -= 1
            else:
                # End of enclosing interface
                return None, -1
        elif ch == "(":
            depth_paren += 1
        elif ch == ")":
            depth_paren -= 1
        elif ch == "<":
            depth_angle += 1
        elif ch == ">":
            if depth_angle > 0:
                depth_angle -= 1
        elif ch == ";" and depth_brace == 0 and depth_paren == 0 and depth_angle == 0:
            return text[start:i], i
        i += 1
    return None, -1


def _parse_type_aliases(text: str) -> dict:
    """Extract type alias definitions."""
    aliases = {}
    # Match single-line and multi-line type aliases
    pattern = re.compile(r"(?:export\s+)?type\s+(\w+)(?:<[^>]*>)?\s*=\s*")
    pos = 0
    while pos < len(text):
        m = pattern.search(text, pos)
        if not m:
            break
        name = m.group(1)
        # Skip internal contravariance types
        if name.startswith("_"):
            pos = m.end()
            continue
        # Find the semicolon that ends this type
        i = m.end()
        depth_brace = depth_paren = depth_angle = 0
        while i < len(text):
            ch = text[i]
            if ch == "{": depth_brace += 1
            elif ch == "}": depth_brace -= 1
            elif ch == "(": depth_paren += 1
            elif ch == ")": depth_paren -= 1
            elif ch == "<": depth_angle += 1
            elif ch == ">" and depth_angle > 0: depth_angle -= 1
            elif ch == ";" and depth_brace <= 0 and depth_paren <= 0 and depth_angle <= 0:
                break
            i += 1
        if i < len(text):
            definition = _normalize_type(text[m.end() : i])
            aliases[name] = definition
        pos = i + 1
    return dict(sorted(aliases.items()))


def _parse_enums(text: str) -> dict:
    """Extract const enum definitions with their members."""
    enums = {}
    pattern = re.compile(r"(?:export\s+)?(?:const\s+)?enum\s+(\w+)\s*\{")
    pos = 0
    while pos < len(text):
        m = pattern.search(text, pos)
        if not m:
            break
        name = m.group(1)
        brace_start = m.end() - 1
        depth = 1
        i = brace_start + 1
        while i < len(text) and depth > 0:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
            i += 1
        body = text[brace_start + 1 : i - 1]
        members = {}
        for mm in re.finditer(r"(\w+)\s*=\s*([^,}]+)", body):
            members[mm.group(1)] = mm.group(2).strip()
        enums[name] = dict(sorted(members.items()))
        pos = i
    return dict(sorted(enums.items()))


def _parse_class_methods(text: str) -> dict:
    """Extract public method signatures from class declarations."""
    classes = {}
    class_pattern = re.compile(
        r"export\s+class\s+(\w+)"
        r"(?:<[^{]*?>)?"
        r"(?:\s+extends\s+([\w<>,\s]+?))?"
        r"(?:\s+implements\s+([\w<>,\s]+?))?"
        r"\s*\{"
    )
    pos = 0
    while pos < len(text):
        m = class_pattern.search(text, pos)
        if not m:
            break
        name = m.group(1)
        extends = m.group(2).strip() if m.group(2) else None

        brace_start = m.end() - 1
        depth = 1
        i = brace_start + 1
        while i < len(text) and depth > 0:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
            i += 1
        body = text[brace_start + 1 : i - 1]
        body_clean = _strip_comments(body)

        methods = {}
        # Extract methods by finding name( and then balancing parens
        method_start_re = re.compile(r"(?:readonly\s+)?(\w+)\s*\(")
        pos_m = 0
        while pos_m < len(body_clean):
            mm = method_start_re.search(body_clean, pos_m)
            if not mm:
                break
            mname = mm.group(1)
            if mname in ("constructor", "if", "return", "new", "throw", "console"):
                pos_m = mm.end()
                continue
            # Balance parens to find end of params
            paren_start = mm.end() - 1
            depth_p = 1
            j = paren_start + 1
            while j < len(body_clean) and depth_p > 0:
                if body_clean[j] == "(":
                    depth_p += 1
                elif body_clean[j] == ")":
                    depth_p -= 1
                j += 1
            if depth_p != 0:
                pos_m = mm.end()
                continue
            params_str = body_clean[paren_start + 1 : j - 1]
            # Look for : return_type after the closing paren
            rest = body_clean[j:].lstrip()
            ret_match = re.match(r":\s*([^;\n{]+)", rest)
            if not ret_match:
                pos_m = j
                continue
            if mname.startswith("_"):
                pos_m = j
                continue
            params = _normalize_type(params_str)
            ret = _normalize_type(ret_match.group(1))
            sig = f"({params}): {ret}"
            methods[mname] = sig
            pos_m = j

        # Also extract readonly properties
        prop_re = re.compile(r"readonly\s+(\w+)\s*:\s*([^;]+);")
        for pm in prop_re.finditer(body_clean):
            pname = pm.group(1)
            ptype = _normalize_type(pm.group(2))
            methods[pname] = f"(readonly) {ptype}"

        # Extract static properties
        static_re = re.compile(r"static\s+(\w+)\s*=\s*([^;]+);")
        for sm in static_re.finditer(body_clean):
            sname = sm.group(1)
            sval = _normalize_type(sm.group(2))
            methods[sname] = f"(static) {sval}"

        info = {"methods": dict(sorted(methods.items()))}
        if extends:
            info["extends"] = _normalize_type(extends)
        classes[name] = info
        pos = i
    return dict(sorted(classes.items()))


def extract_api(open_source_root: Path) -> dict:
    """Extract the full API surface from Valdi source files."""
    valdi_tsx = (
        open_source_root
        / "src"
        / "valdi_modules"
        / "src"
        / "valdi"
        / "valdi_tsx"
        / "src"
    )
    valdi_core = (
        open_source_root
        / "src"
        / "valdi_modules"
        / "src"
        / "valdi"
        / "valdi_core"
        / "src"
    )

    nte_path = valdi_tsx / "NativeTemplateElements.d.ts"
    component_path = valdi_core / "Component.ts"

    if not nte_path.exists():
        print(f"ERROR: {nte_path} not found", file=sys.stderr)
        sys.exit(1)
    if not component_path.exists():
        print(f"ERROR: {component_path} not found", file=sys.stderr)
        sys.exit(1)

    nte_text = _strip_comments(nte_path.read_text())
    component_text = component_path.read_text()

    return {
        "interfaces": _parse_interfaces(nte_text),
        "types": _parse_type_aliases(nte_text),
        "enums": _parse_enums(nte_text),
        "classes": _parse_class_methods(component_text),
    }


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <open_source_root>", file=sys.stderr)
        sys.exit(1)

    root = Path(sys.argv[1])
    api = extract_api(root)
    print(json.dumps(api, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
