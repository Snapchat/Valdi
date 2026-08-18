import { execSync, spawn } from 'child_process';
import type { Argv } from 'yargs';
import { ANSI_COLORS } from '../core/constants';
import { CliError } from '../core/errors';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { BazelClient } from '../utils/BazelClient';
import { type CliChoice, type CommandResult, getUserChoice, spawnCliCommand } from '../utils/cliUtils';
import { logReproduceThisCommandIfNeeded, makeCommandHandler } from '../utils/errorUtils';
import { wrapInColor } from '../utils/logUtils';
import {
  type WebHotReloadSessionDescriptor,
  findWebHotReloadSessions,
  publishWebHotReloadSession,
} from '../utils/webHotReloadSession';

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

function shellQuote(value: string): string {
  return `'${value.replaceAll("'", `'\\''`)}'`;
}

export function buildHotreloadCommand(hotreloadCommand: string, emitRecompilationEvents: boolean): string {
  if (!emitRecompilationEvents) {
    return hotreloadCommand;
  }

  return `${shellQuote(hotreloadCommand)} --hotreload-json-events`;
}

export function isCompilerRecompilationSucceededEvent(line: string): boolean {
  const event = parseCompilerHotreloadEvent(line);
  return event?.event === 'recompilation_succeeded';
}

function parseCompilerHotreloadEvent(line: string): { event?: unknown; source?: unknown } | undefined {
  try {
    const parsed: unknown = JSON.parse(line);
    if (typeof parsed !== 'object' || parsed === null) {
      return undefined;
    }
    const event = parsed as { event?: unknown; source?: unknown };
    return event.source === 'valdi_hotreload' ? event : undefined;
  } catch {
    return undefined;
  }
}

function runHotreloadWithCompilerEvents(
  command: string,
  cwd: string,
  onRecompilationSucceeded: () => void,
): Promise<CommandResult> {
  return new Promise((resolve, reject) => {
    const child = spawn(command, {
      cwd,
      env: { ...process.env, FORCE_COLOR: '1' },
      shell: process.env['SHELL'] ?? '/bin/bash',
      stdio: ['inherit', 'pipe', 'inherit'],
    });
    let pendingLine = '';
    child.stdout?.on('data', (data: Buffer) => {
      pendingLine += data.toString();
      const lines = pendingLine.split('\n');
      pendingLine = lines.pop() ?? '';
      for (const line of lines) {
        const compilerEvent = parseCompilerHotreloadEvent(line.trim());
        if (compilerEvent?.event === 'recompilation_succeeded') {
          onRecompilationSucceeded();
        }
        if (compilerEvent === undefined) {
          process.stdout.write(`${line}\n`);
        }
      }
    });
    child.once('close', code => {
      const compilerEvent = parseCompilerHotreloadEvent(pendingLine.trim());
      if (compilerEvent?.event === 'recompilation_succeeded') {
        onRecompilationSucceeded();
      }
      if (pendingLine !== '' && compilerEvent === undefined) {
        process.stdout.write(pendingLine);
      }
      resolve({ returnCode: code ?? 0, stderr: '', stdout: '' });
    });
    child.once('error', reject);
  });
}

interface WebPublicationScheduler {
  drain(): Promise<void>;
  schedule(): void;
}

function createWebPublicationScheduler(
  client: BazelClient,
  sessions: readonly WebHotReloadSessionDescriptor[],
): WebPublicationScheduler {
  let publicationRequested = false;
  let activePublication: Promise<void> | undefined;

  const publish = async () => {
    const groups = new Map<string, WebHotReloadSessionDescriptor[]>();
    for (const session of sessions) {
      const key = JSON.stringify([session.applicationTarget, session.bazelArgs]);
      const group = groups.get(key) ?? [];
      group.push(session);
      groups.set(key, group);
    }

    for (const group of groups.values()) {
      const representative = group[0];
      if (!representative) {
        continue;
      }
      console.log(
        `Rebuilding web application: ${wrapInColor(representative.applicationTarget, ANSI_COLORS.GREEN_COLOR)}`,
      );
      await client.buildTarget(representative.applicationTarget, representative.bazelArgs);
      await Promise.all(group.map(session => publishWebHotReloadSession(session)));
      console.log(`Published web application to ${group.length} running host${group.length === 1 ? '' : 's'}.`);
    }
  };

  const run = async () => {
    while (publicationRequested) {
      publicationRequested = false;
      try {
        await publish();
      } catch (error) {
        console.error(
          wrapInColor(
            `Web rebuild failed; running hosts kept their last successful build. ${String(error)}`,
            ANSI_COLORS.RED_COLOR,
          ),
        );
      }
    }
  };

  const start = () => {
    if (activePublication !== undefined) {
      return;
    }
    activePublication = run().finally(() => {
      activePublication = undefined;
      if (publicationRequested) {
        start();
      }
    });
  };

  return {
    drain: async () => {
      while (activePublication !== undefined) {
        await activePublication;
      }
    },
    schedule: () => {
      publicationRequested = true;
      start();
    },
  };
}

async function hotreloadResolvedTarget(
  client: BazelClient,
  resolvedTarget: string,
  webSessions: readonly WebHotReloadSessionDescriptor[],
) {
  killExistingHotreloaders();
  await client.buildTarget(resolvedTarget);
  const workspaceRoot = await client.getWorkspaceRoot();
  const buildOutputs = await client.queryBuildOutputs([resolvedTarget]);

  if (buildOutputs.length === 0) {
    throw new CliError(`No build outputs found for target: ${resolvedTarget}`);
  }

  const hotreloadCommand = buildOutputs[0] ?? '';

  if (webSessions.length === 0) {
    await spawnCliCommand(buildHotreloadCommand(hotreloadCommand, false), workspaceRoot, 'inherit', true, false);
  } else {
    console.log(
      `Connected hot reload to ${webSessions.length} running web host${webSessions.length === 1 ? '' : 's'}.`,
    );
    const scheduler = createWebPublicationScheduler(client, webSessions);
    await runHotreloadWithCompilerEvents(buildHotreloadCommand(hotreloadCommand, true), workspaceRoot, () =>
      scheduler.schedule(),
    );
    await scheduler.drain();
  }
}

async function getHotreloadTargetByModuleName(client: BazelClient, moduleName: string): Promise<string> {
  const targets = await client.query(`filter(${moduleName}_hotreload, kind('valdi_hotreload rule', //...))`);
  if (targets.length === 0) {
    throw new CliError(`No hotreload target found for module: ${moduleName}`);
  }

  return targets[0]?.trim() ?? '';
}

async function resolveHotreloadTarget(client: BazelClient, target: string): Promise<string> {
  const resolvedLabel = await client.resolveLabel(target);
  if (resolvedLabel.name) {
    return resolvedLabel.toString();
  }

  const targets = await client.query(`kind('valdi_hotreload', ${resolvedLabel.toString()}/...)`);
  if (targets.length === 0) {
    throw new Error(`Could not resolve hot reload target for label ${resolvedLabel.toString()}`);
  }
  if (targets.length !== 1) {
    throw new Error(`Resolved more than 1 hot reload target for ${resolvedLabel.toString()}: ${targets.join(',')}`);
  }
  return targets[0] as string;
}

async function valdiHotreload(argv: ArgumentsResolver<CommandParameters>) {
  const client = new BazelClient();
  const workspaceRoot = await client.getWorkspaceRoot();
  const activeWebSessions = findWebHotReloadSessions(workspaceRoot);

  // TODO(3136): Try discovering and building with default path to modules first
  const target = await argv.getArgumentOrResolve('target', async () => {
    const module = argv.getArgument('module');
    if (module) {
      console.log('Finding target for module:', wrapInColor(module, ANSI_COLORS.GREEN_COLOR));
      const target = await getHotreloadTargetByModuleName(client, module);
      console.log('Found target:', wrapInColor(target, ANSI_COLORS.GREEN_COLOR));
      return target;
    }
    const activeTargets = [...new Set(activeWebSessions.map(session => session.hotreloadTarget))];
    if (activeTargets.length === 1) {
      const activeTarget = activeTargets[0] ?? '';
      console.log(
        'Using hot reload target from the running web host:',
        wrapInColor(activeTarget, ANSI_COLORS.GREEN_COLOR),
      );
      return activeTarget;
    }
    if (activeTargets.length > 1) {
      return await getUserChoice(
        activeTargets.map((activeTarget, index) => ({
          name: `${index + 1}. ${activeTarget}`,
          value: activeTarget,
        })),
        'Please choose a running web application to hot reload:',
      );
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

  const resolvedTarget = await resolveHotreloadTarget(client, target);
  const webSessions = activeWebSessions.filter(session => session.hotreloadTarget === resolvedTarget);
  await hotreloadResolvedTarget(client, resolvedTarget, webSessions);
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
