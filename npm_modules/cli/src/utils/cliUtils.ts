import type { StdioOptions } from 'child_process';
import { exec, execSync, spawn } from 'child_process';
import inquirer from 'inquirer';
import type Separator from 'inquirer/lib/objects/separator';

export type CommandResult = { returnCode: number; stdout: string; stderr: string };
export type CliChoice<T> = { name: string; value: T };

export function runCliCommand(command: string, cwd?: string, rejectOnFailure?: boolean): Promise<CommandResult> {
  return new Promise((resolve, reject) => {
    exec(command, { cwd: cwd, shell: getShell() }, (error, stdout, stderr) => {
      const returnCode = error ? (error.code ?? 1) : 0;
      if (returnCode !== 0 && rejectOnFailure) {
        reject(new Error(stderr.trim()));
      } else {
        resolve({ returnCode, stdout, stderr });
      }
    });
  });
}

export function spawnCliCommand(
  command: string,
  cwd: string | undefined,
  stdioMode: StdioOptions,
  quiet: boolean,
  rejectOnFailure: boolean,
): Promise<CommandResult> {
  return new Promise((resolve, reject) => {
    const child = spawn(command, {
      stdio: stdioMode,
      shell: getShell(),
      cwd: cwd,
      env: { ...process.env, FORCE_COLOR: '1' },
    });

    // When using mode = 'inherit', the following output and error capture is a no-op

    let stdout = '';
    let stderr = '';

    child.stdout?.on('data', (data: Buffer) => {
      stdout += data.toString();
      if (!quiet) {
        process.stdout.write(data);
      }
    });

    child.stderr?.on('data', (data: Buffer) => {
      stderr += data.toString();
      if (!quiet) {
        process.stderr.write(data);
      }
    });

    child.on('close', code => {
      if (code !== 0 && rejectOnFailure) {
        reject(new Error(stderr.trim()));
      } else {
        resolve({ returnCode: code ?? 0, stdout, stderr });
      }
    });

    child.on('error', err => {
      reject(err);
    });
  });
}

export async function getUserChoice<T>(
  choices: Array<CliChoice<T> | Separator>,
  promptMessage?: string,
  maxChoices?: number,
): Promise<T> {
  const answers = await inquirer.prompt<{ selectedChoice: T }>([
    {
      type: 'list',
      name: 'selectedChoice',
      message: promptMessage ?? 'Please select an option:',
      choices: choices,
      loop: false,
      pageSize: maxChoices ?? 10,
    },
  ]);

  return answers.selectedChoice;
}

export async function getUserConfirmation(promptMessage?: string, defaultConfirm: boolean = false): Promise<boolean> {
  const answer = await inquirer.prompt<{ confirm: boolean }>([
    { type: 'confirm', name: 'confirm', message: promptMessage ?? 'Are you sure?', default: defaultConfirm },
  ]);

  return answer.confirm;
}

export function getScriptDirectory(): string {
  return '';
}

function getShell(): string {
  return process.env['SHELL'] ?? '/bin/bash';
}

export function checkCommandExists(command: string): boolean {
  try {
    execSync(`command -v ${command}`, { stdio: 'ignore' });
    return true;
  } catch {
    return false;
  }
}
