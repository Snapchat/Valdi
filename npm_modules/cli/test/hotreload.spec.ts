import 'jasmine';
import * as fs from 'fs';
import path from 'path';
import { buildHotreloadCommand, isCompilerRecompilationSucceededEvent } from '../src/commands/hotreload';

describe('hotreload command helpers', () => {
  describe('buildHotreloadCommand', () => {
    it('leaves the generated script command unchanged without forwarded options', () => {
      expect(buildHotreloadCommand('bazel-bin/app/run_hotreloader.sh', false)).toBe('bazel-bin/app/run_hotreloader.sh');
    });

    it('quotes generated script paths safely', () => {
      expect(buildHotreloadCommand("bazel-bin/app's/run_hotreloader.sh", true)).toBe(
        `'bazel-bin/app'\\''s/run_hotreloader.sh' --hotreload-json-events`,
      );
    });

    it('enables recompilation events for a running web host', () => {
      expect(buildHotreloadCommand('bazel-bin/app/run_hotreloader.sh', true)).toBe(
        `'bazel-bin/app/run_hotreloader.sh' --hotreload-json-events`,
      );
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
