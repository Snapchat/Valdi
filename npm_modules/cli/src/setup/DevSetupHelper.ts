import fs from 'fs';
import path from 'path';
import { ANSI_COLORS } from '../core/constants';
import { LoadingIndicator } from '../utils/LoadingIndicator';
import { spawnCliCommand } from '../utils/cliUtils';
import { wrapInColor } from '../utils/logUtils';

export const HOME_DIR = process.env['HOME'] ?? '';

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
    console.log();
    console.log(wrapInColor('📝 Next steps:', ANSI_COLORS.BLUE_COLOR));
    console.log('  1. Restart your terminal or source your shell configuration');
    console.log('  2. Install VSCode/Cursor extensions for the best development experience:');
    console.log(`     ${wrapInColor('https://github.com/Snapchat/Valdi/blob/main/docs/INSTALL.md#vscodecursor-setup-optional-but-recommended', ANSI_COLORS.BLUE_COLOR)}`);
    console.log('  3. Run `valdi doctor` to verify your setup');
    console.log('  4. Create your first project with `valdi bootstrap`');
  }

  async setupShellAutoComplete(): Promise<void> {
    const rcFile = this.getRcFile();
    if (!rcFile) {
      console.log(
        wrapInColor(
          'Could not determine shell configuration file, skipping autocomplete setup...',
          ANSI_COLORS.YELLOW_COLOR,
        ),
      );
      return;
    }

    const shell = process.env['SHELL'] ?? '';
    let autoCompleteLines: string[] = [];

    if (shell.endsWith('/zsh')) {
      autoCompleteLines = ['autoload -U compinit && compinit', 'autoload -U bashcompinit && bashcompinit'];
    } else if (shell.endsWith('/bash')) {
      // Bash completion is typically handled by bash-completion package
      // We'll check if it's already enabled
      autoCompleteLines = ['# Bash completion is enabled'];
    }

    if (autoCompleteLines.length === 0 || autoCompleteLines[0] === '# Bash completion is enabled') {
      // For bash, we don't need to add anything as it's usually auto-loaded
      if (shell.endsWith('/bash')) {
        console.log(wrapInColor('Shell autocomplete setup complete (bash)', ANSI_COLORS.GREEN_COLOR));
      }
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

      // Check if autocomplete is already configured
      let needsUpdate = false;
      for (const line of autoCompleteLines) {
        if (!rcLines.includes(line)) {
          needsUpdate = true;
          break;
        }
      }

      if (needsUpdate) {
        // Find the Valdi configuration section or add to the top
        const valdiConfBegin = rcLines.indexOf('# Valdi configuration begin');
        const insertIndex = valdiConfBegin >= 0 ? valdiConfBegin : 0;

        // Insert autocomplete lines at the beginning (before Valdi config or at top)
        for (let i = autoCompleteLines.length - 1; i >= 0; i--) {
          const line = autoCompleteLines[i];
          if (line && !rcLines.includes(line)) {
            rcLines.splice(insertIndex, 0, line);
          }
        }

        const content = rcLines.join('\n');
        await fs.promises.writeFile(rcFile, content, 'utf8');
      }
    })
      .setText(wrapInColor('Setting up shell autocomplete...', ANSI_COLORS.YELLOW_COLOR))
      .show();
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
