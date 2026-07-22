import type { Argv } from 'yargs';
import { ANSI_COLORS } from '../core/constants';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { BazelClient } from '../utils/BazelClient';
import { logReproduceThisCommandIfNeeded, makeCommandHandler } from '../utils/errorUtils';
import { wrapInColor } from '../utils/logUtils';

interface CommandParameters {
  target: string;
}

async function valdiWebHotreload(argv: ArgumentsResolver<CommandParameters>) {
  const client = new BazelClient();
  const target = argv.getArgument('target');

  if (!target) {
    throw new Error('--target is required (e.g. //apps/helloworld:dev)');
  }

  logReproduceThisCommandIfNeeded(argv);

  console.log(
    wrapInColor(
      `Starting webpack-dev-server via ${target} ...`,
      ANSI_COLORS.GREEN_COLOR,
    ),
  );

  // `--define enable_web=true` is required for any playground target that
  // depends on the webpack toolchain via the composer_example_npm workspace;
  // forward it unconditionally so users don't have to remember.
  await client.runTarget(target, '--define enable_web=true');
}

export const command = 'web-hotreload --target <bazel-target>';
export const describe =
  'Start a Valdi web playground with hot module reload (webpack-dev-server). '
  + 'Pass the Bazel label of the playground :dev target created by '
  + 'valdi_web_playground(hmr_config = ...).';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs.option('target', {
    describe: 'Bazel label of the playground :dev js_binary (e.g. //apps/helloworld:dev)',
    type: 'string',
    requiresArg: true,
    demandOption: true,
  });
};

export const handler = makeCommandHandler(valdiWebHotreload);
