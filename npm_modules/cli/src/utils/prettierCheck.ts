import { execFile } from 'child_process';

export type PrettierCheckResult =
  | { status: 'ok' }
  | { status: 'unformatted'; output: string }
  | { status: 'error'; output: string };

// `--list-different` prints the unformatted files on stdout and exits 1. Node also exits 1
// when the prettier script itself is missing, so an empty stdout means "could not run".

// Runs the prettier shipped with the CLI so the check never falls back to `npx`
// fetching it from the registry (and works offline).
export function runPrettierCheck(
  pattern: string,
  cwd: string = process.cwd(),
  // eslint-disable-next-line unicorn/prefer-module
  prettierBin: string = require.resolve('prettier/bin/prettier.cjs'),
): Promise<PrettierCheckResult> {
  return new Promise(resolve => {
    execFile(
      process.execPath,
      [prettierBin, '--list-different', pattern],
      { cwd, maxBuffer: 16 * 1024 * 1024 },
      (error, stdout, stderr) => {
        if (!error) {
          resolve({ status: 'ok' });
        } else if (error.code === 1 && stdout.trim().length > 0) {
          resolve({ status: 'unformatted', output: stdout.trim() });
        } else {
          resolve({ status: 'error', output: `${stdout}${stderr}`.trim() || error.message });
        }
      },
    );
  });
}
