#!/usr/bin/env bash
# Integration test: validates the Valdi custom Bazel registry.
#
# Phase 1 — Structural validation (JSON, patch integrity)
# Phase 2 — Consumer simulation: creates a throwaway bzlmod project that
#           imports Valdi via local_path_override, pins patched modules
#           with single_version_override, and points --registry at the
#           local registry so patches from source.json are applied.
#           Runs `bazel mod graph` to verify pinning, then builds
#           @valdi//libs/dummy:dummy to prove patches applied correctly.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OPEN_SOURCE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
REGISTRY_DIR="$OPEN_SOURCE_DIR/registry"

ERRORS=0
fail() { echo "FAIL: $1"; ERRORS=$((ERRORS + 1)); }
pass() { echo "  ok: $1"; }

# ─── Phase 1: Structural validation ─────────────────────────────────

echo "=== Phase 1: Registry structural validation ==="
echo

# --- bazel_registry.json ---
if [ ! -f "$REGISTRY_DIR/bazel_registry.json" ]; then
    fail "bazel_registry.json missing"
else
    if python3 -c "import json,sys; json.load(open(sys.argv[1]))" "$REGISTRY_DIR/bazel_registry.json" 2>/dev/null; then
        pass "bazel_registry.json valid"
    else
        fail "bazel_registry.json invalid JSON"
    fi
fi

# --- Per-module validation ---
for module_dir in "$REGISTRY_DIR"/modules/*/; do
    module_name=$(basename "$module_dir")

    metadata="$module_dir/metadata.json"
    if [ ! -f "$metadata" ]; then
        fail "$module_name: metadata.json missing"
        continue
    fi

    versions=$(python3 -c "
import json, sys
for v in json.load(open(sys.argv[1])).get('versions', []):
    print(v)
" "$metadata")

    for version in $versions; do
        version_dir="$module_dir/$version"

        if [ ! -f "$version_dir/MODULE.bazel" ]; then
            fail "$module_name@$version: MODULE.bazel missing"
            continue
        fi

        if [ ! -f "$version_dir/source.json" ]; then
            fail "$module_name@$version: source.json missing"
            continue
        fi

        # Validate source.json has required fields
        if ! python3 -c "
import json, sys
s = json.load(open(sys.argv[1]))
assert 'url' in s, 'missing url'
assert 'integrity' in s, 'missing integrity'
" "$version_dir/source.json" 2>/dev/null; then
            fail "$module_name@$version: source.json missing url or integrity"
            continue
        fi

        # Validate module name matches directory
        declared_name=$(python3 -c "
import re, sys
text = open(sys.argv[1]).read()
m = re.search(r'module\s*\(([^)]+)\)', text, re.DOTALL)
if m:
    n = re.search(r'(?<!\w)name\s*=\s*\"([^\"]+)\"', m.group(1))
    print(n.group(1) if n else '')
" "$version_dir/MODULE.bazel")
        if [ "$declared_name" != "$module_name" ]; then
            fail "$module_name@$version: MODULE.bazel name=\"$declared_name\" != directory \"$module_name\""
            continue
        fi

        # Verify patch integrity
        patches=$(python3 -c "
import json, sys
for name, integrity in json.load(open(sys.argv[1])).get('patches', {}).items():
    print(f'{name}|{integrity}')
" "$version_dir/source.json")

        if [ -n "$patches" ]; then
            while IFS='|' read -r patch_name expected_integrity; do
                patch_file="$version_dir/patches/$patch_name"
                if [ ! -f "$patch_file" ]; then
                    fail "$module_name@$version: patch '$patch_name' missing"
                    continue
                fi
                actual_hash=$(openssl dgst -sha256 -binary "$patch_file" | openssl base64)
                expected_hash=${expected_integrity#sha256-}
                if [ "$actual_hash" != "$expected_hash" ]; then
                    fail "$module_name@$version: patch '$patch_name' integrity mismatch"
                fi
            done <<< "$patches"
        fi

        pass "$module_name@$version"
    done
done

# ─── Phase 2: Consumer simulation ───────────────────────────────────

echo
echo "=== Phase 2: Consumer project simulation ==="
echo

CONSUMER_DIR=$(mktemp -d)
trap 'rm -rf "$CONSUMER_DIR"' EXIT

FILE_REGISTRY="file://$REGISTRY_DIR/"

# Extract local_path_override entries from Valdi's MODULE.bazel so the
# consumer can override every Valdi sub-module with absolute paths.
SUB_MODULE_OVERRIDES=$(python3 -c "
import re, sys, os

valdi_dir = sys.argv[1]
text = open(os.path.join(valdi_dir, 'MODULE.bazel')).read()

# Find all local_path_override blocks
for m in re.finditer(
    r'local_path_override\s*\(\s*'
    r'module_name\s*=\s*\"([^\"]+)\"\s*,\s*'
    r'path\s*=\s*\"([^\"]+)\"\s*'
    r',?\s*\)',
    text,
):
    name, rel_path = m.group(1), m.group(2)
    abs_path = os.path.normpath(os.path.join(valdi_dir, rel_path))
    print(f'local_path_override(module_name = \"{name}\", path = \"{abs_path}\")')
" "$OPEN_SOURCE_DIR")

# Generate single_version_override lines from registry metadata.
# Pins each module's version so MVS can't escalate via transitive deps.
# No registry= parameter needed — --registry flag order in .bazelrc ensures
# our registry is checked first, so patches from source.json are applied.
REGISTRY_OVERRIDES=$(python3 -c "
import json, os, sys

registry_dir = sys.argv[1]
modules_dir = os.path.join(registry_dir, 'modules')

for module_name in sorted(os.listdir(modules_dir)):
    metadata_path = os.path.join(modules_dir, module_name, 'metadata.json')
    if not os.path.isfile(metadata_path):
        continue
    versions = json.load(open(metadata_path)).get('versions', [])
    if not versions:
        continue
    version = versions[0]
    print(f'single_version_override(module_name = \"{module_name}\", version = \"{version}\")')
" "$REGISTRY_DIR")

# Write .bazelversion
cp "$OPEN_SOURCE_DIR/npm_modules/cli/.metadata/.bazelversion.template" "$CONSUMER_DIR/.bazelversion" 2>/dev/null \
    || echo "7.2.1" > "$CONSUMER_DIR/.bazelversion"

# Write MODULE.bazel — a minimal consumer that depends on Valdi.
# Toolchains are inherited from Valdi via the valdi_toolchains extension.
# Consumer only needs: bazel_dep, sub-module overrides, and version pins.
cat > "$CONSUMER_DIR/MODULE.bazel" <<MODEOF
module(name = "registry_test", version = "0.0.0")

bazel_dep(name = "valdi", version = "0.1")
local_path_override(module_name = "valdi", path = "$OPEN_SOURCE_DIR")

# Valdi sub-modules need absolute path overrides (local_path_override is root-module-only)
$SUB_MODULE_OVERRIDES

# Pin registry modules: version only (--registry order delivers patches from source.json)
$REGISTRY_OVERRIDES

# websocketpp and zlib are not in our registry — pin version only
bazel_dep(name = "websocketpp", version = "0.8.2.bcr.3")
single_version_override(module_name = "zlib", version = "1.3.2")
MODEOF

# Write .bazelrc — custom registry first (for patches in source.json), BCR as fallback
cat > "$CONSUMER_DIR/.bazelrc" <<RCEOF
common --enable_bzlmod
common --noenable_workspace
common --registry=$FILE_REGISTRY
common --registry=https://bcr.bazel.build
RCEOF

# Write empty BUILD so Bazel recognizes the workspace
touch "$CONSUMER_DIR/BUILD.bazel"

echo "Consumer project created at: $CONSUMER_DIR"
echo "Registry URL: $FILE_REGISTRY"
echo

# --- 2a: Module resolution check ---
echo "--- Module resolution (bazel mod graph) ---"
echo

cd "$CONSUMER_DIR"

BAZEL_CMD="bazelisk"
if ! command -v bazelisk &>/dev/null; then
    if command -v bazel &>/dev/null; then
        BAZEL_CMD="bazel"
    elif command -v bzl &>/dev/null; then
        BAZEL_CMD="bzl"
    else
        fail "no bazel, bazelisk, or bzl found on PATH"
    fi
fi

MOD_GRAPH_STDERR=$(mktemp)
MOD_GRAPH=$(USE_BAZEL_VERSION=7.2.1 $BAZEL_CMD mod graph --output json --depth 99 2>"$MOD_GRAPH_STDERR") || true

if [ -z "$MOD_GRAPH" ]; then
    echo "stderr from bazel mod graph:"
    grep -E "ERROR|WARNING|not found" "$MOD_GRAPH_STDERR" | head -10
    fail "bazel mod graph failed in consumer project"
    rm -f "$MOD_GRAPH_STDERR"
else
    RESOLUTION_ERRORS=$(python3 -c "
import json, sys, os

def find_modules_tree(node, found):
    key = node.get('key', '')
    if '@' in key:
        name, version = key.split('@', 1)
        if name not in found:
            found[name] = version
    for dep in node.get('dependencies', []):
        find_modules_tree(dep, found)
    for dep in node.get('indirectDependencies', []):
        find_modules_tree(dep, found)

registry_dir = sys.argv[1]
graph = json.loads(sys.argv[2])
errors = 0

# Walk the dependency tree to find all resolved module versions
# Bazel 7: nested tree with 'key'/'dependencies' fields
# Bazel 8: flat dict where keys are 'module@version' strings
resolved_modules = {}
if isinstance(graph, dict) and 'key' in graph:
    find_modules_tree(graph, resolved_modules)
elif isinstance(graph, dict):
    for key in graph:
        if '@' in key:
            name, version = key.split('@', 1)
            if name not in resolved_modules:
                resolved_modules[name] = version

modules_dir = os.path.join(registry_dir, 'modules')
for module_name in sorted(os.listdir(modules_dir)):
    metadata_path = os.path.join(modules_dir, module_name, 'metadata.json')
    if not os.path.isfile(metadata_path):
        continue
    versions = json.load(open(metadata_path)).get('versions', [])
    if not versions:
        continue
    expected = versions[0]

    resolved = resolved_modules.get(module_name)
    if resolved is None:
        print(f'  skip: {module_name} not in dep graph')
    elif resolved == expected:
        print(f'  ok: {module_name} resolved to {resolved}')
    else:
        print(f'FAIL: {module_name} expected {expected}, got {resolved}')
        errors += 1

sys.exit(errors)
" "$REGISTRY_DIR" "$MOD_GRAPH") && RESOLUTION_EXIT=0 || RESOLUTION_EXIT=$?
    echo "$RESOLUTION_ERRORS"
    ERRORS=$((ERRORS + RESOLUTION_EXIT))
    rm -f "$MOD_GRAPH_STDERR"
fi

# --- 2b: Build check ---
echo
echo "--- Build check (@valdi//libs/dummy:dummy) ---"
echo

if USE_BAZEL_VERSION=7.2.1 $BAZEL_CMD build @valdi//libs/dummy:dummy 2>&1; then
    pass "consumer build of @valdi//libs/dummy:dummy succeeded"
else
    fail "consumer build of @valdi//libs/dummy:dummy failed"
fi

# ─── Summary ─────────────────────────────────────────────────────────

echo
echo "================================"
if [ "$ERRORS" -eq 0 ]; then
    echo "ALL REGISTRY CHECKS PASSED"
    exit 0
else
    echo "FAILED: $ERRORS error(s)"
    exit 1
fi
