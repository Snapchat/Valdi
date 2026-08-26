import 'jasmine';
import * as fs from 'fs';
import path from 'path';
import {
  type HotreloadLifecycleEvent,
  buildHotreloadCommand,
  formatHotreloadLifecycleEvent,
  hotreloadCleanupCommands,
  isCompilerRecompilationSucceededEvent,
  validateHotreloadPort,
} from '../src/commands/hotreload';
import { CliError } from '../src/core/errors';

describe('hotreload command helpers', () => {
  describe('validateHotreloadPort', () => {
    it('accepts an omitted or valid port', () => {
      expect(validateHotreloadPort(undefined)).toBeUndefined();
      expect(validateHotreloadPort(13702)).toBe(13702);
    });

    it('rejects invalid ports', () => {
      expect(() => validateHotreloadPort(0)).toThrowError(CliError);
      expect(() => validateHotreloadPort(65536)).toThrowError(CliError);
      expect(() => validateHotreloadPort(13702.5)).toThrowError(CliError);
    });
  });

  describe('hotreloadCleanupCommands', () => {
    it('keeps legacy global cleanup when no port is specified', () => {
      expect(hotreloadCleanupCommands(undefined)).toEqual([
        'pkill -f valdi_companion',
        'pkill -f run_hotreloader',
        `pkill -f 'valdi.*--monitor'`,
      ]);
    });

    it('uses same-port scoped cleanup when a port is specified', () => {
      expect(hotreloadCleanupCommands(13702)).toEqual([
        `pkill -f 'run_hotreloader.*--port 13702($| )'`,
        `pkill -f 'valdi.*--monitor.*--port 13702($| )'`,
      ]);
    });
  });

  describe('buildHotreloadCommand', () => {
    it('leaves the generated script command unchanged without forwarded options', () => {
      expect(
        buildHotreloadCommand('bazel-bin/app/run_hotreloader.sh', {
          jsonEvents: false,
          port: undefined,
          target: '//modules/example:example_hotreload',
        }),
      ).toBe('bazel-bin/app/run_hotreloader.sh');
    });

    it('forwards the requested port to the generated script', () => {
      expect(
        buildHotreloadCommand('bazel-bin/app/run_hotreloader.sh', {
          jsonEvents: false,
          port: 13702,
          target: '//modules/example:example_hotreload',
        }),
      ).toBe(`'bazel-bin/app/run_hotreloader.sh' --port 13702`);
    });

    it('quotes generated script paths safely', () => {
      expect(
        buildHotreloadCommand("bazel-bin/app's/run_hotreloader.sh", {
          jsonEvents: false,
          port: 13702,
          target: '//modules/example:example_hotreload',
        }),
      ).toBe(`'bazel-bin/app'\\''s/run_hotreloader.sh' --port 13702`);
    });

    it('forwards JSON event configuration to the compiler', () => {
      expect(
        buildHotreloadCommand('bazel-bin/app/run_hotreloader.sh', {
          jsonEvents: true,
          port: 13702,
          target: "//modules/example's:example_hotreload",
        }),
      ).toBe(
        `'bazel-bin/app/run_hotreloader.sh' --port 13702 --hotreload-json-events ` +
          `--hotreload-target '//modules/example'\\''s:example_hotreload'`,
      );
    });

    it('enables recompilation events for a running web host', () => {
      expect(
        buildHotreloadCommand('bazel-bin/app/run_hotreloader.sh', {
          jsonEvents: true,
          port: undefined,
          target: '//modules/example:example_hotreload',
        }),
      ).toBe(
        `'bazel-bin/app/run_hotreloader.sh' --hotreload-json-events ` +
          `--hotreload-target '//modules/example:example_hotreload'`,
      );
    });
  });

  describe('formatHotreloadLifecycleEvent', () => {
    it('emits a stable machine-readable envelope', () => {
      const line = formatHotreloadLifecycleEvent({
        event: 'hotreload_starting',
        target: '//modules/example:example_hotreload',
        port: 13592,
        time: '2026-07-12T00:00:00.000Z',
      });

      expect(JSON.parse(line)).toEqual({
        source: 'valdi_hotreload',
        event: 'hotreload_starting',
        target: '//modules/example:example_hotreload',
        port: 13592,
        time: '2026-07-12T00:00:00.000Z',
      });
    });

    it('includes the terminal return code when present', () => {
      const line = formatHotreloadLifecycleEvent({
        event: 'hotreload_stopped',
        target: '//modules/example:example_hotreload',
        port: null,
        time: '2026-07-12T00:00:00.000Z',
        returnCode: 2,
      });

      const event = JSON.parse(line) as HotreloadLifecycleEvent;
      expect(event.returnCode).toBe(2);
    });
  });

  describe('compiler lifecycle events', () => {
    it('recognizes only successful compiler recompilation events', () => {
      expect(
        isCompilerRecompilationSucceededEvent(
          '{"source":"valdi_hotreload","event":"recompilation_succeeded","changedFileCount":2}',
        ),
      ).toBeTrue();
      expect(
        isCompilerRecompilationSucceededEvent('{"source":"valdi_compiler","event":"recompilation_failed"}'),
      ).toBeFalse();
      expect(isCompilerRecompilationSucceededEvent('normal compiler output')).toBeFalse();
    });
  });

  describe('generated hotreload script template', () => {
    it('forwards runtime arguments to the compiler invocation', () => {
      const repoRoot = path.resolve(process.cwd(), '../..');
      const valdiCompiledBzl = fs.readFileSync(path.join(repoRoot, 'bzl/valdi/valdi_compiled.bzl'), 'utf8');

      expect(valdiCompiledBzl).toContain(`echo '{executable} {args} "$@"' >> {script_path}`);
    });
  });
});
