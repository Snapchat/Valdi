import { ANSI_COLORS } from '../core/constants';
import { wrapInColor } from '../utils/logUtils';

export interface ShellRunner {
  runShell(guide: string, commands: string[]): Promise<string>;
}

export const LIBTINFO5_INSTALL_COMMAND = 'sudo apt-get install -y libtinfo5';

/**
 * Installs libtinfo5 from the distro's own repositories, so apt verifies it against the signed
 * repository indexes. Releases that dropped the package (Ubuntu 24.04+) get a warning, not a failure.
 * @returns whether the package was installed
 */
export async function installLibtinfo5(shell: ShellRunner): Promise<boolean> {
  try {
    await shell.runShell('Installing libtinfo5 (required for Android tools)', [LIBTINFO5_INSTALL_COMMAND]);
    return true;
  } catch (error) {
    console.log(
      wrapInColor(
        'Warning: libtinfo5 is not available from your package repositories. Some Android command-line tools may not work.',
        ANSI_COLORS.YELLOW_COLOR,
      ),
    );
    console.log(
      wrapInColor(`Details: ${error instanceof Error ? error.message : 'Unknown error'}`, ANSI_COLORS.YELLOW_COLOR),
    );
    console.log(
      wrapInColor(
        'On releases that dropped it, enable a repository that still ships libtinfo5 and rerun setup.',
        ANSI_COLORS.YELLOW_COLOR,
      ),
    );
    return false;
  }
}
