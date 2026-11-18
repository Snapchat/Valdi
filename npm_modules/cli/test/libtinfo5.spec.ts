import 'jasmine';
import { execSync } from 'child_process';
import { LIBTINFO5_INSTALL_COMMAND, installLibtinfo5 } from '../src/setup/libtinfo5';

describe('installLibtinfo5', () => {
  beforeEach(() => {
    spyOn(console, 'log');
  });

  it('installs from the distro repositories', async () => {
    const commands: string[][] = [];
    const installed = await installLibtinfo5({
      runShell: async (_guide, cmds) => {
        commands.push(cmds);
        return '';
      },
    });
    expect(installed).toBe(true);
    expect(commands).toEqual([[LIBTINFO5_INSTALL_COMMAND]]);
  });

  it('does not download or pin a package', () => {
    expect(LIBTINFO5_INSTALL_COMMAND).toBe('sudo apt-get install -y libtinfo5');
  });

  it('warns instead of throwing when the package is unavailable', async () => {
    const installed = await installLibtinfo5({
      runShell: async () => {
        throw new Error('E: Unable to locate package libtinfo5');
      },
    });
    expect(installed).toBe(false);
  });

  describe('on Linux', () => {
    beforeEach(() => {
      if (process.platform !== 'linux' || !commandExists('apt-get')) {
        pending('needs apt-get on Linux');
      }
    });

    it('runs an apt-get command that the installed apt accepts', () => {
      // The package is gone on Ubuntu 24.04+, so "unable to locate" is as good as success here:
      // either way apt parsed the command and looked the package up.
      let output: string;
      try {
        output = execSync(`${LIBTINFO5_INSTALL_COMMAND.replace(/^sudo /, '')} --simulate 2>&1`, {
          encoding: 'utf8',
        });
      } catch (error) {
        output = String((error as { stdout?: string }).stdout ?? error);
      }
      expect(output).toMatch(/libtinfo5/);
    });
  });
});

function commandExists(name: string): boolean {
  try {
    execSync(`command -v ${name}`, { stdio: 'ignore' });
    return true;
  } catch {
    return false;
  }
}
