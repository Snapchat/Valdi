import 'jasmine';
import { spawnCliCommand } from './cliUtils';

describe('cliUtils', () => {
  describe('spawnCliCommand', () => {
    it('forwards each output chunk only once when not quiet', async () => {
      const writeSpy = spyOn(process.stdout, 'write').and.returnValue(true);

      await spawnCliCommand('echo first; sleep 0.5; echo second', undefined, 'pipe', false, false);

      const written = writeSpy.calls.allArgs().map(args => String(args[0]));
      expect(written.join('')).toBe('first\nsecond\n');
    });
  });
});
