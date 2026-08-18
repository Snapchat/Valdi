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
  port: number | undefined;
  jsonEvents: boolean;
}

export interface HotreloadCompilerOptions {
  jsonEvents: boolean;
  port: number | undefined;
  target: string;
}

export type HotreloadLifecycleEventName =
  | 'target_resolved'
  | 'build_started'
  | 'build_failed'
  | 'build_succeeded'
  | 'hotreload_starting'
  | 'target_connected'
  | 'resources_sent'
  | 'recompilation_succeeded'
  | 'hotreload_stopped';

export interface HotreloadLifecycleEvent {
  event: HotreloadLifecycleEventName;
  target: string;
  port: number | null;
  time: string;
  clientId?: number;
  applicationId?: string;
  platform?: string;
  resourceCount?: number;
  changedFileCount?: number;
  returnCode?: number;
}

type HotreloadLifecycleReporter = (event: HotreloadLifecycleEventName, returnCode?: number) => void;

const MIN_PORT = 1;
const MAX_PORT = 65_535;

export function validateHotreloadPort(port: number | undefined): number | undefined {
  if (port === undefined) {
    return undefined;
  }
  if (!Number.isInteger(port) || port < MIN_PORT || port > MAX_PORT) {
    throw new CliError(`Invalid hotreload port: ${port}. Expected an integer between ${MIN_PORT} and ${MAX_PORT}.`);
  }
  return port;
}

export function hotreloadCleanupCommands(port: number | undefined): string[] {
  if (port === undefined) {
    return ['pkill -f valdi_companion', 'pkill -f run_hotreloader', `pkill -f 'valdi.*--monitor'`];
  }

  return [`pkill -f 'run_hotreloader.*--port ${port}($| )'`, `pkill -f 'valdi.*--monitor.*--port ${port}($| )'`];
}

export function killExistingHotreloaders(port: number | undefined): void {
  const commands = hotreloadCleanupCommands(port);
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

export function buildHotreloadCommand(hotreloadCommand: string, options: HotreloadCompilerOptions): string {
  const args: string[] = [];
  if (options.port !== undefined) {
    args.push(`--port ${options.port}`);
  }
  if (options.jsonEvents) {
    args.push('--hotreload-json-events', `--hotreload-target ${shellQuote(options.target)}`);
  }

  if (args.length === 0) {
    return hotreloadCommand;
  }

  return `${shellQuote(hotreloadCommand)} ${args.join(' ')}`;
}

export function formatHotreloadLifecycleEvent(event: HotreloadLifecycleEvent): string {
  return JSON.stringify({ source: 'valdi_hotreload', ...event });
}

function lifecycleReporter(enabled: boolean, target: string, port: number | undefined): HotreloadLifecycleReporter {
  return (event, returnCode) => {
    if (!enabled) {
      return;
    }
    const payload: HotreloadLifecycleEvent = {
      event,
      target,
      port: port ?? null,
      time: new Date().toISOString(),
    };
    if (returnCode !== undefined) {
      payload.returnCode = returnCode;
    }
    console.log(formatHotreloadLifecycleEvent(payload));
  };
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
  relayCompilerEvents: boolean,
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
        if (compilerEvent === undefined || relayCompilerEvents) {
          process.stdout.write(`${line}\n`);
        }
      }
    });
    child.once('close', code => {
      const compilerEvent = parseCompilerHotreloadEvent(pendingLine.trim());
      if (compilerEvent?.event === 'recompilation_succeeded') {
        onRecompilationSucceeded();
      }
      if (pendingLine !== '' && (compilerEvent === undefined || relayCompilerEvents)) {
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
  port: number | undefined,
  jsonEvents: boolean,
  report: HotreloadLifecycleReporter,
  webSessions: readonly WebHotReloadSessionDescriptor[],
) {
  killExistingHotreloaders(port);
  report('build_started');
  try {
    await client.buildTarget(resolvedTarget);
  } catch (error) {
    report('build_failed');
    throw error;
  }
  report('build_succeeded');
  const workspaceRoot = await client.getWorkspaceRoot();
  const buildOutputs = await client.queryBuildOutputs([resolvedTarget]);

  if (buildOutputs.length === 0) {
    throw new CliError(`No build outputs found for target: ${resolvedTarget}`);
  }

  const hotreloadCommand = buildOutputs[0] ?? '';

  report('hotreload_starting');
  let result: CommandResult;
  if (webSessions.length === 0) {
    result = await spawnCliCommand(
      buildHotreloadCommand(hotreloadCommand, {
        jsonEvents,
        port,
        target: resolvedTarget,
      }),
      workspaceRoot,
      'inherit',
      true,
      false,
    );
  } else {
    console.log(
      `Connected hot reload to ${webSessions.length} running web host${webSessions.length === 1 ? '' : 's'}.`,
    );
    const scheduler = createWebPublicationScheduler(client, webSessions);
    result = await runHotreloadWithCompilerEvents(
      buildHotreloadCommand(hotreloadCommand, {
        jsonEvents: true,
        port,
        target: resolvedTarget,
      }),
      workspaceRoot,
      () => scheduler.schedule(),
      jsonEvents,
    );
    await scheduler.drain();
  }
  report('hotreload_stopped', result.returnCode);
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
  const port = validateHotreloadPort(argv.getArgument('port'));
  const jsonEvents = argv.getArgument('jsonEvents');
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
  const report = lifecycleReporter(jsonEvents, resolvedTarget, port);
  report('target_resolved');
  await hotreloadResolvedTarget(client, resolvedTarget, port, jsonEvents, report, webSessions);
}

export const command = 'hotreload [--module module_name] [--target target_name] [--port port] [--json-events]';
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
    })
    .option('port', {
      describe: 'Port of the running app Valdi debugger/hotreload service',
      type: 'number',
      requiresArg: true,
    })
    .option('json-events', {
      describe: 'Emit machine-readable lifecycle events while retaining normal build and runtime output',
      type: 'boolean',
      default: false,
    });
};

export const handler = makeCommandHandler(valdiHotreload);
