#!/usr/bin/env python3
"""Build and stage the experimental Linux SDK from one clean source checkout."""

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile

HERE = Path(__file__).resolve().parent
SCHEMA = 'valdi.linux-sdk/1'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def member(value, allow_root=False):
    if value == '.' and allow_root:
        return PurePosixPath('.')
    if not value or value.startswith('/') or any(p in ('', '.', '..') for p in value.split('/')):
        raise ValueError('Expected a contained relative SDK path: ' + value)
    return PurePosixPath(value)


def inventory(root):
    files = {}
    for path in sorted(root.rglob('*')):
        name = path.relative_to(root).as_posix()
        if name == 'manifest.json':
            continue
        if path.is_symlink():
            raise ValueError('SDK payload must contain regular files, not links: ' + name)
        if path.is_file():
            files[name] = {'sha256': digest(path), 'executable': bool(path.stat().st_mode & 0o111)}
        elif not path.is_dir():
            raise ValueError('SDK contains a nonregular member: ' + name)
    return files


def verify(root, expected_revision=None, expected_engine=None, required_capabilities=()):
    root = root.resolve()
    manifest = json.loads((root / 'manifest.json').read_text())
    if manifest.get('schema') != SCHEMA:
        raise ValueError('Unsupported SDK manifest schema')
    if expected_revision and manifest['source_revision'] != expected_revision:
        raise ValueError('SDK source/compiler/module revision mismatch')
    if expected_engine and manifest['profile']['engine'] != expected_engine:
        raise ValueError('SDK JavaScript engine mismatch')
    if any(tool['source_revision'] != manifest['source_revision'] for tool in manifest['tools'].values()):
        raise ValueError('SDK compiler/runtime source identity mismatch')
    absent = set(required_capabilities) - set(manifest['profile'].get('capabilities', []))
    if absent:
        raise ValueError('Unqualified SDK capabilities: ' + ', '.join(sorted(absent)))
    if manifest.get('files') != inventory(root):
        raise ValueError('SDK file inventory/checksum mismatch')
    for closure in manifest['closures'].values():
        for name in closure['headers']:
            member(name)
            if name not in manifest['files']:
                raise ValueError('Missing declared SDK header: ' + name)
        for entry in closure['link']:
            if 'archive' in entry:
                member(entry['archive'])
                if entry['archive'] not in manifest['files']:
                    raise ValueError('Missing declared SDK archive: ' + entry['archive'])
    return manifest


def copy_input(execution, value, destination):
    source = execution / member(value)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.is_dir():
        destination.mkdir(exist_ok=True)
        for child in sorted(source.iterdir()):
            copy_input(execution, value + '/' + child.name, destination / child.name)
    else:
        if destination.exists():
            if not destination.is_file() or digest(source) != digest(destination):
                raise ValueError('Conflicting SDK input: ' + value)
            return
        shutil.copyfile(source, destination, follow_symlinks=True)
        destination.chmod(0o755 if source.stat().st_mode & 0o111 else 0o644)


def relocate_flag(flag):
    # Linker scripts, response files and absolute rpaths need an explicit export
    # contract, not a best-effort text substitution.
    if '/' in flag or '\n' in flag or '\r' in flag or '@' in flag:
        raise ValueError('Nonrelocatable linker option: ' + flag)
    if any(token in flag for token in ('--start-group', '--end-group', '--whole-archive', '--no-whole-archive')):
        raise ValueError('Bazel import cannot preserve a positional group option: ' + flag)
    if flag != '-pthread' and not re.fullmatch(r'-l[A-Za-z0-9_+.-]+', flag):
        raise ValueError('Unsupported stateful/non-system linker option: ' + flag)
    return flag


def stage_closure(execution, staged, closure, archives):
    result = {key: closure[key] for key in ('defines',)}
    if closure['additional_inputs']:
        raise ValueError('Linker additional_inputs need an explicit relocation contract')
    result['headers'] = []
    for value in closure['headers']:
        name = 'include/' + str(member(value))
        copy_input(execution, value, staged / name)
        destination = staged / name
        if destination.is_dir():
            result['headers'].extend(p.relative_to(staged).as_posix() for p in sorted(destination.rglob('*')) if p.is_file())
        else:
            result['headers'].append(name)
    for key in ('includes', 'quote_includes', 'system_includes'):
        result[key] = ['include' if value == '.' else 'include/' + str(member(value)) for value in closure[key]]
    result['link'] = []
    emitted = {}
    for entry in closure['link']:
        if 'flag' in entry:
            result['link'].append({'flag': relocate_flag(entry['flag'])})
            continue
        value = entry['archive']
        member(value)
        if value not in archives:
            basename = Path(value).name
            if not basename.endswith('.a'):
                basename += '.a'
            name = f'lib/{len(archives):04d}-{basename}'
            copy_input(execution, value, staged / name)
            archives[value] = name
        if value in emitted:
            emitted[value]['alwayslink'] = emitted[value]['alwayslink'] or entry['alwayslink']
        else:
            emitted[value] = {**entry, 'archive': archives[value]}
            result['link'].append(emitted[value])
    if not any('archive' in item for item in result['link']):
        raise ValueError('SDK static-library closure is empty')
    return result


def write_bazel(staged, closures, profile):
    imports = {}
    for closure in closures.values():
        for entry in closure['link']:
            if 'archive' in entry:
                key = (entry['archive'], entry['alwayslink'])
                imports.setdefault(key, f'archive_{len(imports)}')
    lines = ['load("@rules_cc//cc:defs.bzl", "cc_import", "cc_library")',
             'package(default_visibility = ["//visibility:public"])']
    for (path, always), label in imports.items():
        lines.append(f'cc_import(name = {label!r}, static_library = {path!r}, alwayslink = {always!r})')
    for name, closure in closures.items():
        deps = [':' + imports[(e['archive'], e['alwayslink'])] for e in closure['link'] if 'archive' in e]
        flags = [e['flag'] for e in closure['link'] if 'flag' in e] + profile.get('consumer_link_flags', [])
        includes = list(dict.fromkeys(closure['includes'] + closure['quote_includes'] + closure['system_includes']))
        lines.append(f'cc_library(name = {name!r}, hdrs = {closure["headers"]!r}, '
                     f'includes = {includes!r}, defines = {closure["defines"]!r}, deps = {deps!r}, linkopts = {flags!r})')
    (staged / 'BUILD.bazel').write_text('\n'.join(lines) + '\n')
    (staged / 'MODULE.bazel').write_text('module(name = "valdi_linux_sdk", version = "0.0.0")\n'
                                      'bazel_dep(name = "rules_cc", version = "0.2.20")\n')


def notices(source, execution, staged, metadata):
    repositories = set()
    for closure in (metadata['runtime'], metadata['fixture']):
        paths = closure['headers'] + [e['archive'] for e in closure['link'] if 'archive' in e]
        for value in paths:
            if value.startswith('external/'):
                repositories.add(value.split('/')[1])
    # Swift statically links its compiler runtime; include the source toolchain's
    # notices even though it is not in the embedding CcInfo closure.
    repositories.update(p.name for p in (execution / 'external').glob('*swift*') if p.is_dir())
    roots = [('valdi', source)] + [(name, execution / 'external' / name) for name in sorted(repositories)]
    missing = []
    for name, base in roots:
        found = False
        for directory, dirs, files in os.walk(base):
            dirs[:] = [d for d in dirs if d not in ('.git', 'node_modules', '.linux-tools') and not d.startswith('bazel-')]
            for filename in files:
                if not re.match(r'^(licen[cs]e|copying|notice|copyright)([._-].*)?$', filename, re.I):
                    continue
                path = Path(directory) / filename
                target = staged / 'notices' / name / path.relative_to(base)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, target)
                found = True
        if not found:
            missing.append(name)
    return {'repositories': sorted(repositories), 'without_discovered_notice': missing, 'human_audit': 'pending'}


def stage(source, execution, metadata_path, output, revision, profile, build_environment,
          source_repository='https://github.com/Snapchat/Valdi'):
    if not re.fullmatch(r'[0-9a-f]{40}', revision):
        raise ValueError('Source revision must be a full Git commit ID')
    if output.exists():
        raise ValueError('Refusing to overwrite an existing SDK')
    metadata = json.loads(metadata_path.read_text())
    if metadata.get('schema') != 'valdi.linux-sdk-export/1':
        raise ValueError('Unsupported CcInfo export schema')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.valdi-sdk-', dir=output.parent) as temporary:
        staged = Path(temporary) / 'sdk'
        staged.mkdir()
        archives = {}
        closures = {name: stage_closure(execution, staged, metadata[key], archives)
                    for name, key in [('runtime', 'runtime'), ('fixture_module', 'fixture')]}
        for closure in closures.values():
            closure['defines'] = list(dict.fromkeys(closure['defines'] + profile.get('consumer_defines', [])))
        write_bazel(staged, closures, profile)
        tool_records = {}
        for name, record in metadata['tools'].items():
            files = []
            for value in record['files']:
                path = 'build-tools/' + str(member(value))
                copy_input(execution, value, staged / path)
                files.append(path)
            tool_records[name] = {**record, 'files': files, 'source_revision': revision}
        # Compiler companion launchers still need their Bazel runfiles. Keep the
        # complete matched source recipe as the supported rebuild reference;
        # tool payload execution is not yet a qualified standalone toolchain.
        provenance = staged / 'provenance'
        provenance.mkdir()
        shutil.copyfile(metadata_path, provenance / 'native-export.json')
        for name in ('MODULE.bazel', 'MODULE.bazel.lock', '.bazelversion'):
            if (source / name).exists():
                shutil.copyfile(source / name, provenance / name)
        shutil.copytree(HERE / 'consumer', staged / 'consumer', ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
        shutil.copyfile(HERE / 'README.md', staged / 'README.md')
        (staged / 'tools').mkdir()
        shutil.copyfile(HERE / 'package.py', staged / 'tools/sdk.py')
        notice_record = notices(source, execution, staged, metadata)
        manifest = {
            'schema': SCHEMA, 'source_revision': revision,
            'profile': profile, 'build_environment': build_environment,
            'compatibility': {
                'stable_cxx_abi': False, 'compiler_module_match': 'exact-source-and-profile',
                'pic': 'per-archive', 'minimum_glibc_qualified': None,
                'tool_execution': 'source-build-only; standalone build-tools payload is unqualified',
                'tool_source': {'repository': source_repository, 'revision': revision,
                                'target': '//sdk/linux:source_toolchain'},
            },
            'tools': tool_records, 'closures': closures, 'notices': notice_record,
            'files': inventory(staged),
        }
        (staged / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
        verify(staged, revision, profile['engine'])
        staged.rename(output)
    return manifest


def archive(root, output):
    verify(root)
    if output.exists():
        raise ValueError('Refusing to overwrite an existing archive')
    # This makes packaging deterministic for fixed input bytes. Native compiler
    # output reproducibility still needs independent-build qualification.
    with output.open('xb') as file, gzip.GzipFile(filename='', mode='wb', fileobj=file, mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w') as tar:
            for path in sorted(root.rglob('*')):
                if not path.is_file():
                    continue
                info = tarfile.TarInfo('sdk/' + path.relative_to(root).as_posix())
                info.size = path.stat().st_size
                info.mode = 0o755 if path.stat().st_mode & 0o111 else 0o644
                with path.open('rb') as stream:
                    tar.addfile(info, stream)


def check_source_state(status):
    # Public Valdi does not check in MODULE.bazel.lock. Bazel creates it even
    # when all tracked sources are unchanged; stage that exact resolution file.
    dirty = [line for line in status.splitlines() if line != '?? MODULE.bazel.lock']
    if dirty:
        raise ValueError('SDK source must be a clean committed checkout')


def build(args):
    source = Path(os.environ.get('BUILD_WORKSPACE_DIRECTORY', HERE.parents[1]))
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        raise ValueError('This SDK profile requires a native Linux x86_64 builder')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip()
    dirty = subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=all'], cwd=source, text=True)
    check_source_state(dirty)
    profile = json.loads((HERE / 'profile.json').read_text())
    # Never let a resource tuning option silently replace the sealed SDK profile.
    allowed = ('--jobs=', '--local_resources=', '--action_env=LD_LIBRARY_PATH=',
               '--host_action_env=LD_LIBRARY_PATH=',
               '--@rules_rust//rust/settings:extra_exec_rustc_flag=-Lnative=')
    for option in args.bazel_option:
        if not option.startswith(allowed):
            raise ValueError('SDK profile override is not allowed: ' + option)
    bazel = [args.bazel, '--nosystem_rc', '--nohome_rc']
    if args.output_base:
        bazel += ['--output_base=' + str(args.output_base)]
    options = profile['flags'] + args.bazel_option
    subprocess.run([*bazel, 'build', '//sdk/linux:export', *options], cwd=source, check=True)
    check_source_state(subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=all'],
                                               cwd=source, text=True))
    if subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip() != revision:
        raise ValueError('SDK source revision changed during the build')
    execution = Path(subprocess.check_output([*bazel, 'info', 'execution_root'], cwd=source, text=True).strip())
    bin_dir = Path(subprocess.check_output([*bazel, 'info', 'bazel-bin', *options], cwd=source, text=True).strip())
    return stage(source, execution, bin_dir / 'sdk/linux/export.json', args.output.resolve(), revision, profile,
                 {'system': platform.platform(), 'libc': list(platform.libc_ver()),
                  'bazel': subprocess.check_output([*bazel, 'version', '--gnu_format'], cwd=source, text=True).strip(),
                  'extra_bazel_options': [option.split('=LD_LIBRARY_PATH=')[0] + '=LD_LIBRARY_PATH=<builder-libraries>'
                                          if '=LD_LIBRARY_PATH=' in option else
                                          option.split('=-Lnative=')[0] + '=-Lnative=<builder-gcc-runtime>'
                                          if '=-Lnative=' in option else option for option in args.bazel_option]},
                 args.source_repository)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    command = commands.add_parser('build')
    command.add_argument('--output', type=Path, required=True)
    command.add_argument('--bazel', default='bazel')
    command.add_argument('--output-base', type=Path)
    command.add_argument('--bazel-option', action='append', default=[])
    command.add_argument('--source-repository', default='https://github.com/Snapchat/Valdi')
    command = commands.add_parser('archive')
    command.add_argument('sdk', type=Path)
    command.add_argument('--output', type=Path, required=True)
    command = commands.add_parser('verify')
    command.add_argument('sdk', type=Path)
    command.add_argument('--source-revision')
    command.add_argument('--engine')
    command.add_argument('--require-capability', action='append', default=[])
    args = parser.parse_args()
    if args.command == 'build':
        manifest = build(args)
    elif args.command == 'verify':
        manifest = verify(args.sdk, args.source_revision, args.engine, args.require_capability)
    else:
        archive(args.sdk, args.output)
        print(json.dumps({'archive': str(args.output), 'sha256': digest(args.output)}))
        return
    print(json.dumps({'source_revision': manifest['source_revision'], 'engine': manifest['profile']['engine'],
                      'files': len(manifest['files'])}))


if __name__ == '__main__':
    main()
