#!/usr/bin/env python3
"""Host-side checks on HelloWorld's native modules and its exported npm package.

NativeModuleRegistrationTest asserts every native module factory is registered and exported
on each platform. WebPackageLayoutTest asserts the file layout of the npm package that
collapse_web_paths builds.

HelloWorld declares its native modules in TypeScript (src/valdi/hello_world/src/*.d.ts) and
implements them per platform: a Kotlin factory, an Objective-C factory, C++ factories, and
web stubs. Each platform registers its factories differently, and a factory that is not
registered, or registered under the wrong module path, only fails at runtime on a device
when TypeScript imports it. The androidTest needs an emulator, so nothing on a host caught
that. This test checks the built outputs instead:

- Android: the registration manifest the annotation processor writes into the compiled
  library, and that each registered class extends the compiler-generated factory.
- iOS: each factory subclasses the generated factory and calls VALDI_REGISTER_MODULE().
  Apple targets don't build on Linux, so this side reads the factory sources and the
  generated headers and sources, which the compiler emits on any host.
- C++: a host binary linked with the factories lists what ModuleFactoryRegistry holds,
  and what each factory's loadModule() exports.
- Web: the RegisterNativeModules.js shipped in the exported npm package.

Each platform's module paths and export names are checked against the .d.ts declarations,
so removing or renaming a factory, its module path, or an export fails here.

The package layout test spells out the expected native files rather than deriving them from
the _web_native tree: that tree is laid out by the same _dest_native() the package uses, so
a regression in the path logic would move both and pass unnoticed.
"""

import os
import pathlib
import re
import subprocess
import unittest
import zipfile

from python.runfiles import runfiles

DECLARATIONS_DIR = "apps/helloworld/src/valdi/hello_world/src"
MODULE_PATH_PREFIX = "hello_world/src/"

ANDROID_LIBRARY_JAR = "apps/helloworld/src/android/libnative_module_android.jar"
# Compiler outputs: Kotlin from android.release.srcjar, Objective-C from ios.release.api.{hdrs,srcs}.
ANDROID_GENERATED_DIR = "apps/helloworld/src/valdi/hello_world/android"
IOS_GENERATED_DIR = "apps/helloworld/src/valdi/hello_world/ios"
IOS_FACTORY_SOURCES_DIR = "apps/helloworld/src/ios"
CPP_DUMP_BINARY = "apps/helloworld/dump_cpp_module_factories"
NPM_PACKAGE_DIR = "apps/helloworld/hello_world_export_npm"
REGISTER_NATIVE_MODULES = "src/RegisterNativeModules.js"

# Where the runtime looks up @RegisterValdiModule classes (see ValdiAnnotationProcessor).
ANDROID_REGISTRATION_PREFIX = "assets/valdi_native_modules/"

# Module path -> the factory each platform registers for it. Renaming or removing a factory
# without updating this table fails the test, the way the declarations pin the module paths.
EXPECTED_ANDROID_FACTORIES = {
    "hello_world/src/NativeModule": "com.snap.valdi.helloworld.MyNativeModuleFactory",
}
EXPECTED_IOS_FACTORIES = {
    "hello_world/src/NativeModule": "SCMyNativeModuleFactory",
}
# CppModule ships on every platform; DesktopMyNativeModule only on macOS and Linux, which
# is where this test runs.
EXPECTED_CPP_MODULES = {
    "hello_world/src/CppModule",
    "hello_world/src/NativeModule",
}
EXPECTED_WEB_FACTORIES = {
    "hello_world/src/CppModule": "hello_world/web/CppModule",
    "hello_world/src/NativeModule": "hello_world/web/NativeModule",
}

# hello_world's web/ native modules (web_register_native_module_id_overrides in its BUILD).
# The package exports map `./*` to `./*.d.ts`, so each .d.ts must sit beside its .js.
EXPECTED_HELLO_WORLD_NATIVE_FILES = {
    "native/hello_world/web/CppModule.d.ts",
    "native/hello_world/web/CppModule.js",
    "native/hello_world/web/NativeModule.d.ts",
    "native/hello_world/web/NativeModule.js",
}

ALLOWED_TOP_LEVEL = {"package.json", "src", "native"}

# Build-tree path segments that mean a file was copied with its source path instead of a package path.
FORBIDDEN_SEGMENTS = {"..", ".valdi_build", "bazel-out", "external", "_main"}

NATIVE_PATH = re.compile(r"^native/[^/]+/web/.+")

# Files collapse_web_paths copies at their build path rather than a package path. Tolerated by
# the stray-file check instead of fixed in the rule, because fixing them changes the shipped
# package layout. None are under native/, so the .d.ts-next-to-.js checks still apply in full.
KNOWN_PASSTHROUGH_PATHS = [
    # Source declarations of modules outside src/valdi_modules/src/valdi/ (hello_world's own
    # CppModule.d.ts and NativeModule.d.ts): _dest only rewrites that prefix.
    re.compile(r"^(?!src/|native/).+/src/[^/]+\.d\.ts$"),
    # Generated declarations, e.g. valdi_test's Strings.d.ts via drawing's test_deps: _dest strips
    # .valdi_build/compile/typescript/output/ but not .valdi_build/compile/generated_ts/.
    re.compile(r"^(?!native/)(.+/)?\.valdi_build/compile/generated_ts/.+\.d\.ts$"),
]


def _is_known_passthrough(path: str) -> bool:
    return any(p.match(path) for p in KNOWN_PASSTHROUGH_PATHS)


def _rlocation(path: str) -> pathlib.Path:
    resolved = runfiles.Create().Rlocation("{}/{}".format(os.environ.get("TEST_WORKSPACE", "_main"), path))
    if not resolved or not os.path.exists(resolved):
        raise FileNotFoundError(path)
    return pathlib.Path(resolved)


def _read_declarations() -> dict:
    """Module path -> export names, parsed from the module's .d.ts files."""
    declarations = {}
    for dts in sorted(_rlocation(DECLARATIONS_DIR).glob("*.d.ts")):
        source = dts.read_text(encoding="utf-8")
        exports = set(re.findall(r"^export\s+(?:declare\s+)?(?:const|let|function|class)\s+(\w+)", source, re.M))
        declarations[MODULE_PATH_PREFIX + dts.name[: -len(".d.ts")]] = exports
    return declarations


def _exported_modules(declarations: dict) -> set:
    # @ExportModule makes the compiler generate a typed Kotlin and Objective-C factory; only
    # those modules have Android and iOS implementations.
    exported = set()
    for module_path in declarations:
        dts = _rlocation(DECLARATIONS_DIR) / (module_path[len(MODULE_PATH_PREFIX):] + ".d.ts")
        if "@ExportModule" in dts.read_text(encoding="utf-8"):
            exported.add(module_path)
    return exported


class NativeModuleRegistrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.declarations = _read_declarations()
        cls.exported_modules = _exported_modules(cls.declarations)

    def test_declarations_match_expected_modules(self) -> None:
        # Guards the premise: an empty oracle would make every check below pass vacuously,
        # and a new .d.ts must come with a factory on every platform that implements it.
        self.assertEqual(set(self.declarations), set(EXPECTED_WEB_FACTORIES))
        self.assertEqual(self.exported_modules, set(EXPECTED_ANDROID_FACTORIES))
        self.assertEqual(self.exported_modules, set(EXPECTED_IOS_FACTORIES))
        for module_path, exports in self.declarations.items():
            self.assertTrue(exports, "{} declares no exports".format(module_path))

    def test_android_factories_are_registered(self) -> None:
        with zipfile.ZipFile(_rlocation(ANDROID_LIBRARY_JAR)) as jar:
            names = jar.namelist()
            registered = {
                n[len(ANDROID_REGISTRATION_PREFIX):]
                for n in names
                if n.startswith(ANDROID_REGISTRATION_PREFIX) and not n.endswith("/")
            }
            self.assertEqual(registered, set(EXPECTED_ANDROID_FACTORIES.values()))
            for module_path, factory in EXPECTED_ANDROID_FACTORIES.items():
                class_file = factory.replace(".", "/") + ".class"
                self.assertIn(class_file, names, "{} is registered but not compiled".format(factory))
                # The constant pool names the superclass; it must be the generated factory,
                # whose getModulePath() the next test pins.
                base = self._android_generated_factory(module_path)
                self.assertIn(base.replace(".", "/").encode(), jar.read(class_file), "{} does not extend {}".format(factory, base))

    def test_android_generated_factories_export_declared_names(self) -> None:
        generated = self._generated_sources(ANDROID_GENERATED_DIR, (".kt",))
        for module_path in EXPECTED_ANDROID_FACTORIES:
            name = self._module_name(module_path)
            factory = re.search(r"abstract class {}ModuleFactory\b[^{{]*\{{(.*?)\n\}}".format(name), generated, re.S)
            self.assertIsNotNone(factory, "no generated {}ModuleFactory".format(name))
            self.assertRegex(factory.group(1), r'getModulePath\(\)[^{]*\{\s*return\s+"' + re.escape(module_path) + '"')
            module = re.search(r"interface {}Module\b[^{{]*\{{(.*?)\n\}}".format(name), generated, re.S)
            self.assertIsNotNone(module, "no generated {}Module".format(name))
            for export in self.declarations[module_path]:
                self.assertRegex(module.group(1), r"\b{}\b".format(re.escape(export)), "{} not in {}Module".format(export, name))

    def test_ios_factories_are_registered(self) -> None:
        sources = {m.name: m.read_text(encoding="utf-8") for m in _rlocation(IOS_FACTORY_SOURCES_DIR).glob("*.m")}
        registered = {}
        for source in sources.values():
            for impl, body in re.findall(r"@implementation\s+(\w+)(.*?)@end", source, re.S):
                if "VALDI_REGISTER_MODULE()" in body:
                    base = re.search(r"@interface\s+{}\s*:\s*(\w+)".format(impl), source)
                    self.assertIsNotNone(base, "{} registers but declares no superclass".format(impl))
                    registered[impl] = base.group(1)
        self.assertEqual(set(registered), set(EXPECTED_IOS_FACTORIES.values()))

        generated = self._generated_sources(IOS_GENERATED_DIR, (".h", ".m"))
        for module_path, factory in EXPECTED_IOS_FACTORIES.items():
            base = registered[factory]
            self.assertRegex(generated, r"@interface\s+{}\s*:\s*SCValdiGeneratedModuleFactory".format(base))
            impl = re.search(r"@implementation\s+{}\b(.*?)@end".format(base), generated, re.S)
            self.assertIsNotNone(impl, "no generated implementation of {}".format(base))
            self.assertIn('return @"{}";'.format(module_path), impl.group(1), "{} is not {}".format(base, module_path))
            for export in self.declarations[module_path]:
                self.assertRegex(generated, r"\b{}\b".format(re.escape(export)))

    def test_cpp_factories_are_registered(self) -> None:
        output = subprocess.run([str(_rlocation(CPP_DUMP_BINARY))], check=True, capture_output=True, text=True).stdout
        registered = {}
        for line in output.splitlines():
            module_path, _, exports = line.partition("\t")
            self.assertNotIn(module_path, registered, "{} registered twice".format(module_path))
            registered[module_path] = set(filter(None, exports.split(",")))
        hello_world = {p: e for p, e in registered.items() if p.startswith(MODULE_PATH_PREFIX)}
        self.assertEqual(set(hello_world), EXPECTED_CPP_MODULES)
        for module_path, exports in hello_world.items():
            self.assertEqual(exports, self.declarations[module_path], module_path)

    def test_web_factories_are_registered(self) -> None:
        package_dir = _rlocation(NPM_PACKAGE_DIR)
        source = (package_dir / REGISTER_NATIVE_MODULES).read_text(encoding="utf-8")
        required = dict(re.findall(r"var (\w+) = require\('\.\./native/([^']+)'\);", source))
        registered = {}
        for module_id, var in re.findall(r"registerModule\('([^']+)', \(\) => (\w+)\)", source):
            self.assertIn(var, required, module_id)
            registered[module_id] = required[var]
        hello_world = {p: f for p, f in registered.items() if p.startswith(MODULE_PATH_PREFIX)}
        self.assertEqual(hello_world, EXPECTED_WEB_FACTORIES)
        for module_path, native_file in hello_world.items():
            shipped = package_dir / "native" / (native_file + ".js")
            self.assertTrue(shipped.exists(), "{} is registered but not shipped".format(shipped))
            text = shipped.read_text(encoding="utf-8")
            for export in self.declarations[module_path]:
                self.assertRegex(text, r"\bexports\.{0}\b|\bexport\s+(?:const|function)\s+{0}\b".format(re.escape(export)), "{} does not export {}".format(native_file, export))

    def _module_name(self, module_path: str) -> str:
        return module_path[len(MODULE_PATH_PREFIX):]

    def _android_generated_factory(self, module_path: str) -> str:
        generated = self._generated_sources(ANDROID_GENERATED_DIR, (".kt",))
        package = re.search(r"^package\s+([\w.]+)", generated, re.M).group(1)
        return "{}.{}ModuleFactory".format(package, self._module_name(module_path))

    def _generated_sources(self, directory: str, suffixes: tuple) -> str:
        files = sorted(p for p in _rlocation(directory).rglob("*") if p.is_file() and p.suffix in suffixes)
        self.assertTrue(files, "no {} files under {}".format(suffixes, directory))
        return "\n".join(p.read_text(encoding="utf-8") for p in files)

class WebPackageLayoutTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        package_dir = _rlocation(NPM_PACKAGE_DIR)
        cls.files = sorted(p.relative_to(package_dir).as_posix() for p in package_dir.rglob("*") if p.is_file())

    def test_hello_world_native_modules(self) -> None:
        actual = {f for f in self.files if f.startswith("native/hello_world/") and f.endswith((".js", ".d.ts"))}
        self.assertEqual(
            actual,
            EXPECTED_HELLO_WORLD_NATIVE_FILES,
            "\nmissing: {}\nunexpected: {}".format(
                sorted(EXPECTED_HELLO_WORLD_NATIVE_FILES - actual), sorted(actual - EXPECTED_HELLO_WORLD_NATIVE_FILES)
            ),
        )

    def test_hello_world_web_files_only_under_native(self) -> None:
        stray = [
            f
            for f in self.files
            if "hello_world/web/" in f and not f.startswith("native/hello_world/web/")
        ]
        self.assertEqual(stray, [])

    def test_every_native_js_has_declaration_beside_it(self) -> None:
        files = set(self.files)
        missing = [
            f[: -len(".js")] + ".d.ts"
            for f in self.files
            if f.startswith("native/") and f.endswith(".js") and f[: -len(".js")] + ".d.ts" not in files
        ]
        self.assertEqual(missing, [])

    def test_no_file_outside_package_layout(self) -> None:
        unexpected = []
        for f in self.files:
            if _is_known_passthrough(f):
                continue
            parts = f.split("/")
            if parts[0] not in ALLOWED_TOP_LEVEL or FORBIDDEN_SEGMENTS.intersection(parts):
                unexpected.append(f)
            elif parts[0] == "native" and not NATIVE_PATH.match(f):
                unexpected.append(f)
        self.assertEqual(unexpected, [])


if __name__ == "__main__":
    unittest.main()
