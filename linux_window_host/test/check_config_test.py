"""Regression cases for root-only settings that Bazel does not inherit."""

from pathlib import Path
import tempfile
import unittest

from check_config import check


class ConfigDriftTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.host = self.root / "linux_window_host"
        self.host.mkdir()
        (self.root / "MODULE.bazel").write_text('''
bazel_dep(name = "rules_swift", version = "3.1.2", repo_name = "swift_alias")
single_version_override(module_name = "rules_swift", patches = ["//:local.patch"])
local_path_override(module_name = "snap_platforms", path = "bzl/platforms")
''')
        (self.host / "MODULE.bazel").write_text('''
single_version_override(module_name = "rules_swift", version = "3.1.2")
single_version_override(module_name = "sdl3", version = "3.4.18")
local_path_override(module_name = "valdi", path = "..")
local_path_override(module_name = "snap_platforms", path = "../bzl/platforms")
''')
        for directory in (self.root, self.host):
            (directory / ".bazelversion").write_text("8.2.1\n")

    def test_matching_settings_with_different_relative_paths(self):
        self.assertEqual([], check(self.root, self.host))

    def test_parent_dependency_version_changes_under_patch_override(self):
        path = self.root / "MODULE.bazel"
        path.write_text(path.read_text().replace('"3.1.2"', '"3.2.0"'))
        self.assertIn("version overrides: rules_swift", check(self.root, self.host)[0])

    def test_new_parent_pin_requires_host_update(self):
        with (self.root / "MODULE.bazel").open("a") as output:
            output.write('single_version_override(module_name = "zlib", version = "1.3.2")\n')
        self.assertIn("version overrides: zlib", check(self.root, self.host)[0])

    def test_shared_direct_dependency_drift(self):
        with (self.host / "MODULE.bazel").open("a") as output:
            output.write('bazel_dep(name = "rules_swift", version = "3.0.0")\n')
        self.assertIn("direct dependency: rules_swift", check(self.root, self.host)[0])

    def test_source_path_drift(self):
        path = self.root / "MODULE.bazel"
        path.write_text(path.read_text().replace('"bzl/platforms"', '"bzl/new_platforms"'))
        self.assertIn("source overrides: snap_platforms", check(self.root, self.host)[0])

    def test_bazel_version_drift(self):
        (self.root / ".bazelversion").write_text("8.3.0\n")
        self.assertIn(".bazelversion differs", check(self.root, self.host)[0])

    def test_unhandled_override_fails_instead_of_silently_skipping(self):
        with (self.root / "MODULE.bazel").open("a") as output:
            output.write('git_override(module_name = "rules_swift", commit = "new")\n')
        with self.assertRaisesRegex(ValueError, "handle git_override"):
            check(self.root, self.host)


if __name__ == "__main__":
    unittest.main()
