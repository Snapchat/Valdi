import { execSync } from 'child_process';
import type { Argv } from 'yargs';
import { ANSI_COLORS } from '../core/constants';
import { CliError } from '../core/errors';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { BazelClient } from '../utils/BazelClient';
import type { CliChoice } from '../utils/cliUtils';
import { getUserChoice, spawnCliCommand } from '../utils/cliUtils';
import { logReproduceThisCommandIfNeeded, makeCommandHandler } from '../utils/errorUtils';
import { wrapInColor } from '../utils/logUtils';

interface CommandParameters {
  module: string | undefined;
  target: string | undefined;
}

function killExistingHotreloaders(): void {
  const commands = [
    'pkill -f valdi_companion',
    'pkill -f run_hotreloader',
  ];
  for (const cmd of commands) {
    try {
      execSync(cmd, { stdio: 'ignore' });
    } catch {
      // Process not found — nothing to kill
    }
  }
}

async function hotreloadResolvedTarget(client: BazelClient, resolvedTarget: string) {
  killExistingHotreloaders();
  await client.buildTarget(resolvedTarget);
  const workspaceRoot = await client.getWorkspaceRoot();
  const buildOutputs = await client.queryBuildOutputs([resolvedTarget]);

  if (buildOutputs.length === 0) {
    throw new CliError(`No build outputs found for target: ${resolvedTarget}`);
  }

  const hotreloadCommand = buildOutputs[0] ?? '';

  await spawnCliCommand(hotreloadCommand, workspaceRoot, 'inherit', true, false);
}

async function hotreloadTarget(client: BazelClient, target: string) {
  const resolvedLabel = await client.resolveLabel(target);
  let resolvedTarget: string;
  if (resolvedLabel.name) {
    resolvedTarget = resolvedLabel.toString();
  } else {
    const targets = await client.query(`kind('valdi_hotreload', ${resolvedLabel.toString()}/...)`);
    if (targets.length === 0) {
      throw new Error(`Could not resolve hot reload target for label ${resolvedLabel.toString()}`);
    }
    if (targets.length !== 1) {
      throw new Error(`Resolved more than 1 hot reload target for ${resolvedLabel.toString()}: ${targets.join(',')}`);
    }
    resolvedTarget = targets[0] as string;
  }

  await hotreloadResolvedTarget(client, resolvedTarget);
}

async function getHotreloadTargetByModuleName(client: BazelClient, moduleName: string): Promise<string> {
  const targets = await client.query(`filter(${moduleName}_hotreload, kind('valdi_hotreload rule', //...))`);
  if (targets.length === 0) {
    throw new CliError(`No hotreload target found for module: ${moduleName}`);
  }

  return targets[0]?.trim() ?? '';
}

async function valdiHotreload(argv: ArgumentsResolver<CommandParameters>) {
  const client = new BazelClient();

  // TODO(3136): Try discovering and building with default path to modules first
  const target = await argv.getArgumentOrResolve('target', async () => {
    const module = argv.getArgument('module');
    if (module) {
      console.log('Finding target for module:', wrapInColor(module, ANSI_COLORS.GREEN_COLOR));
      const target = await getHotreloadTargetByModuleName(client, module);
      console.log('Found target:', wrapInColor(target, ANSI_COLORS.GREEN_COLOR));
      return target;
    }
    const targets = await client.query('attr("tags", "valdi_application", //...)');
    if (targets.length === 0) {
      throw new CliError(`Could not resolve Valdi application Bazel target`);
    }

    if (targets.length === 1) {
      return targets[0] as string;
    } else {
      const choices: Array<CliChoice<string>> = targets.map((target, index) => ({
        name: `${index + 1}. ${target}`,
        value: target,
      }));

      return await getUserChoice(choices, 'Please choose a target to hot reload:');
    }
  });

  logReproduceThisCommandIfNeeded(argv);

  await hotreloadResolvedTarget(client, target);
}

export const command = 'hotreload [--module module_name] [--target target_name]';
export const describe = 'Starts the hotreloader for the application';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .option('module', {
      describe: 'Name of the module to hotreload',
      type: 'string',
      requiresArg: true,
    })
    .option('target', {
      describe: 'Bazel target path to hotreload',
      type: 'string',
      requiresArg: true,
    });
};

export const handler = makeCommandHandler(valdiHotreload);
