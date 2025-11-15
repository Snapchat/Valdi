import fs from 'fs';
import path from 'path';
import { ANSI_COLORS } from '../core/constants';
import { CliError } from '../core/errors';
import { LoadingIndicator } from '../utils/LoadingIndicator';
import { spawnCliCommand } from '../utils/cliUtils';
import { wrapInColor } from '../utils/logUtils';
import { ANDROID_BUILD_TOOLS_VERSION, ANDROID_NDK_VERSION, ANDROID_PLATFORM_VERSION } from './versions';

export const HOME_DIR = process.env['HOME'] ?? '';
const ANDROID_HOME_DIR_SUFFIX = '.valdi/android_home';

export interface EnvVariable {
  name: string;
  value: string;
}

export class DevSetupHelper {
  async download(url: string): Promise<ArrayBuffer> {
    return new LoadingIndicator(async () => {
      const response = await fetch(url);

      return response.arrayBuffer();
    })
      .setText(`${wrapInColor('Downloading', ANSI_COLORS.YELLOW_COLOR)} ${url}...`)
      .show();
  }

  async downloadToPath(url: string, dest: string): Promise<void> {
    const body = await this.download(url);

    const dir = path.dirname(dest);
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }

    await fs.promises.writeFile(dest, new Uint8Array(body));
  }

  async runShell(
    guide: string,
    commands: string[],
    additionalEnvVariables: { [key: string]: string } = {},
  ): Promise<string> {
    let command = '';
    for (const key in additionalEnvVariables) {
      command += `export ${key}="${additionalEnvVariables[key] as string}"\n`;
    }
    command += commands.join(' && ');
    console.log(`${wrapInColor(guide, ANSI_COLORS.YELLOW_COLOR)}...`);
    for (const command of commands) {
      console.log(`- ${command}`);
    }

    const result = await spawnCliCommand(command, undefined, 'inherit', false, true);
    return result.stdout;
  }

  async writeEnvVariablesToRcFile(envVariables: readonly EnvVariable[]): Promise<void> {
    const expectedEnvVariable = envVariables.map(e => `export ${e.name}=${e.value}`);
    const rcFile = this.getRcFile();
    if (!rcFile) {
      console.log();
      console.log(
        `${wrapInColor(`Unrecognized shell ${process.env['SHELL'] ?? ''}. Please add the following env variables to your shell configuration file manually:`, ANSI_COLORS.RED_COLOR)}`,
      );
      for (const l of expectedEnvVariable) {
        console.log(l);
      }
      console.log();
      return;
    }

    return new LoadingIndicator(async () => {
      let originalRcLines: readonly string[] = [];
      let rcLines: string[] = [];
      if (fs.existsSync(rcFile)) {
        const fileBody = await fs.promises.readFile(rcFile, 'utf8');
        originalRcLines = fileBody.split('\n');
        rcLines = [...originalRcLines];
      }

      const valdiConfHeader = `# Valdi configuration end`;
      let insertionIndex = rcLines.indexOf(valdiConfHeader);

      if (insertionIndex < 0) {
        rcLines.push(`# Valdi configuration begin`, valdiConfHeader);
        insertionIndex = rcLines.length - 1;
      }

      for (const envVariable of expectedEnvVariable) {
        if (!rcLines.includes(envVariable)) {
          rcLines.splice(insertionIndex++, 0, envVariable);
        }
      }

      if (rcLines.length !== originalRcLines.length) {
        const content = rcLines.join('\n');
        await fs.promises.writeFile(rcFile, content, 'utf8');
      }
    })
      .setText(
        wrapInColor(
          `Updating ${rcFile} to include ${envVariables.map(e => e.name).join(', ')}...`,
          ANSI_COLORS.YELLOW_COLOR,
        ),
      )
      .show();
  }

  onComplete(): void {
    let suffix = '';
    const rcFile = this.getRcFile();

    if (rcFile) {
      suffix += ` Please run ${wrapInColor(`source ${rcFile}`, ANSI_COLORS.YELLOW_COLOR)} or restart your terminal.`;
    }
    console.log(`${wrapInColor('Dev setup completed!', ANSI_COLORS.GREEN_COLOR)}.${suffix}`);
  }

  async setupAndroidSDK(javaHomeOverride?: string | undefined): Promise<void> {
    console.log(wrapInColor('Setting up Android SDK...', ANSI_COLORS.YELLOW_COLOR));
    if (!process.env['ANDROID_HOME']) {
      process.env['ANDROID_HOME'] = path.join(HOME_DIR, ANDROID_HOME_DIR_SUFFIX);
      await this.writeEnvVariablesToRcFile([{ name: 'ANDROID_HOME', value: `"$HOME/${ANDROID_HOME_DIR_SUFFIX}"` }]);
    }

    const androidHome = process.env['ANDROID_HOME'] ?? '';
    const sdkManagerBin = path.join(androidHome, 'cmdline-tools/latest/bin/sdkmanager');

    // Verify Android SDK Command-line Tools is installed
    if (!fs.existsSync(sdkManagerBin)) {
      throw new CliError(
        'Android SDK Command-line Tools is not installed. Follow the developer setup guide to install it.',
        ANSI_COLORS.RED_COLOR,
      );
    }

    const sdkManagerEnvVariables: { [key: string]: string } = {};
    if (javaHomeOverride) {
      sdkManagerEnvVariables['JAVA_HOME'] = javaHomeOverride;
    }

    await this.runShell(
      'Installing Android platform',
      [`${sdkManagerBin} --install 'platforms;${ANDROID_PLATFORM_VERSION}'`],
      sdkManagerEnvVariables,
    );
    await this.runShell(
      'Installing Android NDK',
      [`${sdkManagerBin} --install 'ndk;${ANDROID_NDK_VERSION}'`],
      sdkManagerEnvVariables,
    );
    await this.runShell(
      'Installing Android build-tools',
      [`${sdkManagerBin} --install 'build-tools;${ANDROID_BUILD_TOOLS_VERSION}'`],
      sdkManagerEnvVariables,
    );

    const ndkBundle = path.join(androidHome, 'ndk-bundle');
    if (!fs.existsSync(ndkBundle)) {
      fs.symlinkSync(`ndk/${ANDROID_NDK_VERSION}`, ndkBundle);
    }
    await this.writeEnvVariablesToRcFile([{ name: 'ANDROID_NDK_HOME', value: `"$ANDROID_HOME/ndk-bundle"` }]);
  }

  private getRcFile(): string | undefined {
    const homeDir = process.env['HOME'] ?? '';
    const shell = process.env['SHELL'] ?? '';
    if (shell.endsWith('/zsh')) {
      return path.join(homeDir, '.zshrc');
    } else if (shell.endsWith('/bash')) {
      return path.join(homeDir, '.bashrc');
    } else if (shell.endsWith('/ksh')) {
      return path.join(homeDir, '.kshrc');
    } else if (shell.endsWith('/tcsh')) {
      return path.join(homeDir, '.tcshrc');
    } else {
      return undefined;
    }
  }
}
