/* eslint-disable @typescript-eslint/no-unsafe-call */
/* eslint-disable @typescript-eslint/no-unsafe-member-access */
/* eslint-disable @typescript-eslint/no-unsafe-assignment */
/* eslint-disable @typescript-eslint/no-unsafe-argument */
import { ANSI_COLORS } from '../core/constants';
import { CliError } from '../core/errors';
import { ArgumentsResolver } from './ArgumentsResolver';
import { wrapInColor } from './logUtils';

function logReproduceWith(keys: string[], additionalResolvedArguments: any) {
  const allArguments = ['valdi', ...process.argv.slice(2)];
  keys.sort();

  for (const key of keys.sort()) {
    const value = additionalResolvedArguments[key];
    if (Array.isArray(value)) {
      for (const item of value) {
        allArguments.push(`--${key}`, item.toString());
      }
    } else {
      allArguments.push(`--${key}`, value.toString());
    }
  }

  console.log(
    `\n${wrapInColor('Note: Use the following command next time to reproduce without prompts', ANSI_COLORS.GREEN_COLOR)}\n${allArguments.join(' ')}\n`,
  );
}

export function logReproduceThisCommandIfNeeded<T>(argumentsResolver: ArgumentsResolver<T>) {
  const keys = Object.keys(argumentsResolver.additionalResolvedArguments);
  if (keys.length > 0) {
    logReproduceWith(keys, argumentsResolver.additionalResolvedArguments);
  }
}

export function makeCommandHandler<T>(cb: (argv: ArgumentsResolver<T>) => Promise<void>): (argv: T) => void {
  const command = async (argv: T) => {
    const argumentsResolver = new ArgumentsResolver(argv);
    try {
      await cb(argumentsResolver);
      // A command may set a nonzero exit code itself (e.g. agent-check) without throwing.
      process.exitCode ??= 0;
    } catch (error) {
      process.exitCode = 1;

      if (error instanceof CliError) {
        // Managed Error
        console.log(wrapInColor(error.message, error.textColor));
      } else if (error instanceof Error) {
        // Unmanaged Error
        console.log(error.stack);
        console.log(wrapInColor(error.message, ANSI_COLORS.RED_COLOR));
      } else {
        // Something went really wrong
        throw error;
      }
    }
  };

  return (argv: T) => {
    void command(argv);
  };
}
