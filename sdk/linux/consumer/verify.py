#!/usr/bin/env python3
"""Compile the consumer using only the staged SDK and an explicit LLVM toolchain."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def run(args, **kwargs):
    result = subprocess.run(args, text=True, capture_output=True, timeout=60, **kwargs)
    if result.returncode:
        raise RuntimeError(f'{args[0]} exited {result.returncode}\n{result.stdout}{result.stderr}')
    return result


def load_sdk(sdk):
    # Loading the verifier must not add bytecode files to the sealed SDK itself.
    previous = sys.dont_write_bytecode
    try:
        sys.dont_write_bytecode = True
        spec = importlib.util.spec_from_file_location('sdk', sdk / 'tools/sdk.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        sys.dont_write_bytecode = previous


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--cxx', type=Path, required=True)
    parser.add_argument('--toolchain-lib', type=Path, required=True)
    parser.add_argument('--source-revision', required=True)
    parser.add_argument('--unavailable-source-path', type=Path, action='append', required=True,
                        help='Producer checkout/output path that must be absent or inaccessible')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    module = load_sdk(sdk)
    manifest = module.verify(sdk, args.source_revision, 'quickjs',
                             ('compiled-module', 'generated-native-function', 'native-typescript', 'host-main-queue'))
    if Path('/dev/dri').exists() or list(Path('/tmp').glob('.X11-unix/X*')):
        raise ValueError('Run this qualification without display sockets or GPU devices')
    environment = {k: v for k, v in os.environ.items() if k not in
                   ('DISPLAY', 'WAYLAND_DISPLAY', 'XDG_RUNTIME_DIR', 'DBUS_SESSION_BUS_ADDRESS')}
    environment['LD_LIBRARY_PATH'] = str(args.toolchain_lib.resolve())
    for path in args.unavailable_source_path:
        if path.exists() and os.access(path, os.R_OK | os.X_OK):
            raise ValueError('Producer source/output remains accessible: ' + str(path))
    version = run([str(args.cxx), '--version'], env=environment).stdout
    if not re.search(r'clang version 19\.1\.7\b', version):
        raise ValueError('Consumer requires the matched LLVM 19.1.7 toolchain')
    with tempfile.TemporaryDirectory(prefix='valdi-external-consumer-') as temporary:
        work = Path(temporary)
        source = work / 'main.cpp'
        shutil.copyfile(Path(__file__).with_name('main.cpp'), source)
        binary = work / 'service'
        # Preserve clang++'s argv[0]: resolving its symlink to clang changes the
        # driver's C++ link mode even though the executable bytes are identical.
        command = [str(args.cxx.absolute()), '-std=c++20', '-stdlib=libc++', '-fno-exceptions', str(source)]
        seen = set()
        closure = manifest['closures']['fixture_module']
        for kind, flag in [('includes', '-I'), ('quote_includes', '-iquote'), ('system_includes', '-isystem')]:
            for include in closure[kind]:
                item = (flag, include)
                if item not in seen:
                    command += [flag, str(sdk / include)]
                    seen.add(item)
        for define in closure['defines']:
            command.append('-D' + define)
        command.append('-Wl,--start-group')
        for entry in closure['link']:
            if 'flag' in entry:
                command.append(module.relocate_flag(entry['flag']))
                continue
            if entry['alwayslink']:
                command.append('-Wl,--whole-archive')
            command.append(str(sdk / entry['archive']))
            if entry['alwayslink']:
                command.append('-Wl,--no-whole-archive')
        command += ['-Wl,--end-group', '-L' + str(args.toolchain_lib.resolve()),
                    *manifest['profile']['consumer_link_flags'], '-o', str(binary)]
        build = run(command, cwd=work, env=environment)
        dynamic = run(['readelf', '-d', str(binary)]).stdout
        if re.search(r'NEEDED.*lib(?:SDL|glfw|X11|wayland|EGL|GLX|vulkan)', dynamic, re.I):
            raise ValueError('Headless consumer depends on a window/display library')
        versions = run(['readelf', '--version-info', str(binary)]).stdout
        (work / 'corrupt.valdimodule').write_bytes(b'not a Valdi module')
        scenarios = {}
        for scenario in ('success', 'missing', 'corrupt', 'entry-error'):
            result = run([str(binary), scenario, str(work)], cwd=work, env=environment)
            if f'PASS {scenario} 3 runtime cycles' not in result.stdout:
                raise ValueError('Consumer did not complete scenario: ' + scenario)
            scenarios[scenario] = {'stdout': result.stdout, 'stderr': result.stderr}
            print(f'PASS {scenario}: 3 runtime cycles', flush=True)
        report = {'source_revision': args.source_revision, 'manifest_sha256': module.digest(sdk / 'manifest.json'),
                  'compiler_version': version, 'compile_source_count': 1,
                  'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                  'elf_dynamic': dynamic, 'glibc_versions': sorted(set(re.findall(r'GLIBC_[0-9.]+', versions))),
                  'display_environment_removed': True, 'gpu_devices_absent': True,
                  'producer_paths_inaccessible': [str(path) for path in args.unavailable_source_path],
                  'compile_stderr': build.stderr, 'scenarios': scenarios}
        args.report.write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({'scenarios': list(scenarios), 'runtime_cycles': 12, 'engine_sources_compiled': 0}))


if __name__ == '__main__':
    main()
