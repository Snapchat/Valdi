#!/usr/bin/env python3
"""Asserts every native web module ships its .d.ts next to its .js in the npm package.

The package exports map `./*` to `./*.d.ts`, so a host importing
`<pkg>/native/<module>/web/<File>` only gets types when `native/<module>/web/<File>.d.ts`
exists. The collapsed native tree is the oracle: it holds each module's web/ outputs
in the same layout the package uses under native/.
"""

import os
import pathlib
import unittest

from python.runfiles import runfiles

PACKAGE_DIR = "experiments/helloworld_playground/hello_experiment_npm"
NATIVE_TREE_DIR = "experiments/helloworld_playground/hello_experiment_npm_web_native"

# Same exclusions collapse_web_paths applies: test files never ship in the package.
TEST_PATH_SUBSTRINGS = ("/test/", ".test.", ".spec.")


def _rlocation(path: str) -> pathlib.Path:
    resolved = runfiles.Create().Rlocation("{}/{}".format(os.environ.get("TEST_WORKSPACE", "_main"), path))
    if not resolved or not os.path.exists(resolved):
        raise FileNotFoundError(path)
    return pathlib.Path(resolved)


def _is_test_path(path: pathlib.Path) -> bool:
    # Leading slash so a module named `test` matches "/test/" like the full repo path does.
    posix_path = "/" + path.as_posix()
    return any(sub in posix_path for sub in TEST_PATH_SUBSTRINGS)


class NativeDeclarationsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.package_dir = _rlocation(PACKAGE_DIR)
        native_tree = _rlocation(NATIVE_TREE_DIR)
        self.declarations = []
        for dts in native_tree.rglob("*.d.ts"):
            rel = dts.relative_to(native_tree)
            # Only declarations that pair with a native .js; stubs like JSX.stub.d.ts are not modules.
            if _is_test_path(rel) or not dts.with_name(dts.name[: -len(".d.ts")] + ".js").exists():
                continue
            self.declarations.append(rel.as_posix())
        self.declarations.sort()

    def test_native_tree_has_declarations(self) -> None:
        # Guards the premise: an empty oracle would make the checks below pass vacuously.
        self.assertTrue(self.declarations)

    def test_declarations_ship_next_to_native_js(self) -> None:
        for rel in self.declarations:
            native_js = self.package_dir / "native" / (rel[: -len(".d.ts")] + ".js")
            self.assertTrue(native_js.exists(), "{} is not shipped".format(native_js))
            self.assertTrue((self.package_dir / "native" / rel).exists(), "native/{} is missing".format(rel))

    def test_declaration_imports_are_repaired(self) -> None:
        # CollapseWebPaths rewrites `../src/X` to `<pkg>/src/../src/X`, then repairs it to
        # `<pkg>/src/<module>/src/X` only for files under native/.
        for rel in self.declarations:
            dts = self.package_dir / "native" / rel
            if dts.exists():
                self.assertNotIn("/src/../", dts.read_text(encoding="utf-8"), "native/{}".format(rel))


if __name__ == "__main__":
    unittest.main()
