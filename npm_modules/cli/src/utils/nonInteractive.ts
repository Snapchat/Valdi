import { CliError } from '../core/errors';

export interface RequiredFlag {
  /** How the flag is spelled on the command line, e.g. `--template <type>`. */
  readonly flag: string;
  /** Whether the user already supplied a value for it. */
  readonly provided: boolean;
}

/**
 * Commands that fall back to interactive prompts can't be used from scripts, CI or agents: with no TTY the prompts
 * either hang or print garbled output. Fail fast and name the flags that would avoid every prompt instead.
 */
export function requireFlagsWhenNonInteractive(
  command: string,
  flags: readonly RequiredFlag[],
  isInteractive: boolean = Boolean(process.stdin.isTTY),
): void {
  if (isInteractive) {
    return;
  }

  const missing = flags.filter(f => !f.provided).map(f => f.flag);
  if (missing.length > 0) {
    throw new CliError(
      `'${command}' would prompt for input, but stdin is not a terminal. Pass these flags to run it non-interactively: ${missing.join(' ')}`,
    );
  }
}
