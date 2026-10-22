import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('sdk_package', Path(__file__).with_name('package.py'))
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'source'
        self.execution = self.root / 'execution'
        self.source.mkdir()
        self.execution.mkdir()
        (self.source / 'LICENSE').write_text('test source license')
        for name, content in [('src/public.hpp', b'header'), ('lib/runtime.a', b'archive'), ('compiler', b'tool')]:
            p = self.execution / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(content)
        self.closure = {'headers': ['src/public.hpp'], 'defines': ['FEATURE=1'], 'includes': ['.'],
                        'quote_includes': ['src'], 'system_includes': [], 'additional_inputs': [],
                        'link': [{'archive': 'lib/runtime.a', 'alwayslink': True, 'pic': True, 'owner': '//runtime'},
                                 {'flag': '-lpthread'}, {'flag': '-lpthread'}]}
        self.metadata = {'schema': 'valdi.linux-sdk-export/1', 'runtime': self.closure,
                         'fixture': copy.deepcopy(self.closure),
                         'tools': {'compiler': {'label': '//compiler', 'files': ['compiler']}}}
        self.metadata_path = self.root / 'export.json'

    def stage(self):
        self.metadata_path.write_text(json.dumps(self.metadata))
        return sdk.stage(self.source, self.execution, self.metadata_path, self.root / 'sdk', 'a' * 40,
                         {'engine': 'quickjs'}, {})

    def test_stage_materializes_closure_and_preserves_link_semantics(self):
        manifest = self.stage()
        root = self.root / 'sdk'
        self.assertEqual(len(list((root / 'lib').iterdir())), 1)
        self.assertTrue(manifest['closures']['runtime']['link'][0]['alwayslink'])
        self.assertEqual([e['flag'] for e in manifest['closures']['runtime']['link'] if 'flag' in e],
                         ['-lpthread', '-lpthread'])
        self.assertNotIn(str(self.execution), json.dumps(manifest))
        self.assertEqual(sdk.verify(root, 'a' * 40, 'quickjs')['files'], manifest['files'])

    def test_missing_header_or_archive_fails_without_partial_sdk(self):
        for path in ('src/public.hpp', 'lib/runtime.a'):
            with self.subTest(path=path):
                original = (self.execution / path).read_bytes()
                (self.execution / path).unlink()
                with self.assertRaises(FileNotFoundError):
                    self.stage()
                self.assertFalse((self.root / 'sdk').exists())
                (self.execution / path).write_bytes(original)

    def test_rejects_paths_and_unrelocatable_flags(self):
        for value in ('../escape', '/absolute', 'a/../b', 'a//b', ''):
            with self.subTest(value=value), self.assertRaises(ValueError):
                sdk.member(value)
        for value in ('-Wl,-rpath,/checkout/lib', '@params', '-Wl,--start-group', '-Wl,--as-needed', '-Wl,-Bstatic', '-static'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                sdk.relocate_flag(value)

    def test_repeated_archive_is_emitted_once_with_combined_alwayslink(self):
        entry = {**self.metadata['runtime']['link'][0], 'alwayslink': False}
        self.metadata['runtime']['link'].insert(0, entry)
        manifest = self.stage()
        archives = [e for e in manifest['closures']['runtime']['link'] if 'archive' in e]
        self.assertEqual(len(archives), 1)
        self.assertTrue(archives[0]['alwayslink'])

    def test_shared_readonly_bazel_headers_are_materialized_once(self):
        (self.execution / 'src/public.hpp').chmod(0o444)
        self.stage()
        sdk.verify(self.root / 'sdk')

    def test_alwayslink_lo_archive_gets_a_cc_import_compatible_name(self):
        (self.execution / 'lib/native.pic.lo').write_bytes(b'archive')
        self.metadata['runtime']['link'][0]['archive'] = 'lib/native.pic.lo'
        manifest = self.stage()
        self.assertTrue(manifest['closures']['runtime']['link'][0]['archive'].endswith('.a'))
        self.assertTrue(manifest['closures']['runtime']['link'][0]['alwayslink'])

    def test_rejects_payload_tampering_and_tool_module_mismatch(self):
        manifest = self.stage()
        root = self.root / 'sdk'
        with self.assertRaisesRegex(ValueError, 'revision mismatch'):
            sdk.verify(root, 'b' * 40)
        with self.assertRaisesRegex(ValueError, 'engine mismatch'):
            sdk.verify(root, expected_engine='hermes')
        with self.assertRaisesRegex(ValueError, 'Unqualified SDK capabilities'):
            sdk.verify(root, required_capabilities=['window'])
        manifest['tools']['compiler']['source_revision'] = 'b' * 40
        (root / 'manifest.json').write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, 'source identity mismatch'):
            sdk.verify(root)
        manifest['tools']['compiler']['source_revision'] = 'a' * 40
        (root / 'manifest.json').write_text(json.dumps(manifest))
        (root / manifest['closures']['runtime']['headers'][0]).write_text('tampered')
        with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
            sdk.verify(root)

    def test_rejects_symlink_and_output_overwrite(self):
        self.stage()
        with self.assertRaisesRegex(ValueError, 'overwrite'):
            self.stage()
        (self.root / 'sdk' / 'escape').symlink_to(self.source / 'LICENSE')
        with self.assertRaisesRegex(ValueError, 'not links'):
            sdk.verify(self.root / 'sdk')

    def test_packaging_fixed_inputs_is_deterministic(self):
        self.stage()
        one, two = self.root / 'one.tgz', self.root / 'two.tgz'
        sdk.archive(self.root / 'sdk', one)
        sdk.archive(self.root / 'sdk', two)
        self.assertEqual(one.read_bytes(), two.read_bytes())

    def test_rejects_extra_link_inputs(self):
        self.metadata['runtime']['additional_inputs'] = ['script.ld']
        with self.assertRaisesRegex(ValueError, 'additional_inputs'):
            self.stage()

    def test_accepts_only_the_generated_untracked_dependency_lock(self):
        sdk.check_source_state('?? MODULE.bazel.lock\n')
        for status in (' M MODULE.bazel.lock\n', '?? sdk/linux/unreviewed.ts\n', ' M valdi/BUILD.bazel\n'):
            with self.subTest(status=status), self.assertRaisesRegex(ValueError, 'clean committed checkout'):
                sdk.check_source_state(status)

    def test_loading_consumer_verifier_does_not_modify_the_sealed_sdk(self):
        self.stage()
        root = self.root / 'sdk'
        before = sdk.inventory(root)
        spec = importlib.util.spec_from_file_location('consumer_verifier', Path(__file__).with_name('consumer') / 'verify.py')
        consumer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(consumer)
        loaded = consumer.load_sdk(root)
        loaded.verify(root, 'a' * 40, 'quickjs')
        self.assertEqual(before, sdk.inventory(root))


if __name__ == '__main__':
    unittest.main()
