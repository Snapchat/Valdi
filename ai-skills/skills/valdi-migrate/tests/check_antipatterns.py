#!/usr/bin/env python3
"""
Validates a Valdi TSX file for known anti-patterns.
Usage: python3 check_antipatterns.py <file.tsx>
Returns exit code 0 if clean, 1 if violations found.
"""

import sys
import re

ANTI_PATTERNS = [
    # React hooks
    (r'\buseState\b',           'React hook: useState'),
    (r'\buseEffect\b',          'React hook: useEffect'),
    (r'\buseContext\b',         'React hook: useContext'),
    (r'\buseMemo\b',            'React hook: useMemo'),
    (r'\buseCallback\b',        'React hook: useCallback'),
    (r'\buseRef\b',             'React hook: useRef'),

    # Compose APIs
    (r'@Composable',            'Compose: @Composable annotation'),
    (r'\bremember\s*\{',        'Compose: remember { }'),
    (r'\bmutableStateOf\b',     'Compose: mutableStateOf'),
    (r'\bLaunchedEffect\b',     'Compose: LaunchedEffect'),
    (r'\bDisposableEffect\b',   'Compose: DisposableEffect'),

    # Functional components
    (r'const\s+\w+\s*=\s*\([^)]*\)\s*=>\s*[<(]',  'Functional component arrow fn'),
    (r'function\s+\w+\s*\([^)]*\)\s*\{[^}]*return\s*<',  'Functional component with return JSX'),

    # Wrong naming
    (r'\bthis\.props\b',        'Wrong: this.props (should be this.viewModel)'),
    (r'\bonMount\b',            'Wrong: onMount (should be onCreate)'),
    (r'\bonUnmount\b',          'Wrong: onUnmount (should be onDestroy)'),
    (r'\bmarkNeedsRender\b',    'Wrong: markNeedsRender (use this.setState)'),
    (r'\bscheduleRender\b',     'Wrong: scheduleRender (deprecated)'),

    # return JSX in onRender
    (r'onRender\s*\(\s*\)\s*\{[^}]*return\s*<', 'onRender() should not return JSX'),

    # Inline lambdas in JSX props (e.g. onTap={() => ...})
    (r'on\w+\s*=\s*\{\s*\(\s*\)\s*=>',  'Inline lambda in JSX prop (use class arrow fn)'),

    # map() in JSX context
    (r'\.map\s*\([^)]*=>\s*[^)]*<\w',   'map() returns array (use forEach in onRender)'),

    # new Style inside onRender
    (r'onRender\s*\(\s*\)\s*\{(?:[^}]|\{[^}]*\})*new\s+Style\s*<', 'new Style() inside onRender()'),

    # Wrong import paths (discovered by build failures)
    # Provider is two separate files, not a directory import
    (r"from 'valdi_core/src/provider'",
     "Wrong import: use 'valdi_core/src/provider/createProvider' and/or 'valdi_core/src/provider/withProviders'"),
]

def check_file(path: str) -> list[tuple[int, str, str]]:
    violations = []
    with open(path) as f:
        lines = f.readlines()
    for lineno, line in enumerate(lines, 1):
        stripped = line.lstrip()
        # Skip comment lines
        if stripped.startswith('//') or stripped.startswith('*') or stripped.startswith('#'):
            continue
        for pattern, description in ANTI_PATTERNS:
            if re.search(pattern, line):
                violations.append((lineno, description, line.rstrip()))
    return violations

def main():
    if len(sys.argv) < 2:
        print(f'Usage: {sys.argv[0]} <file.tsx>', file=sys.stderr)
        sys.exit(2)

    path = sys.argv[1]
    violations = check_file(path)

    if not violations:
        print(f'✅  {path}: no anti-patterns found')
        sys.exit(0)
    else:
        print(f'❌  {path}: {len(violations)} violation(s) found')
        for lineno, desc, text in violations:
            print(f'  Line {lineno}: [{desc}]')
            print(f'    {text}')
        sys.exit(1)

if __name__ == '__main__':
    main()
