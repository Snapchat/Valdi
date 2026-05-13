import type { StdioOptions } from 'child_process';
import path from 'path';
import { ANSI_COLORS, BAZEL_BIN_ENV, BAZEL_EXECUTABLES } from '../core/constants';
import { CliError } from '../core/errors';
import { Digraph } from './Digraph';
import { LoadingIndicator } from './LoadingIndicator';
import type { CommandResult } from './cliUtils';
import { checkCommandExists, runCliCommand, spawnCliCommand } from './cliUtils';
import { wrapInColor } from './logUtils';

const BAZEL_QUIET_OPTIONS = '--noshow_progress --noshow_loading_progress --ui_event_filters=-info,-progress';
const BAZEL_PREFIXED_OPTIONS = '--max_idle_secs=5';
const BAZEL_POSTFIXED_OPTIONS = '--color=yes';
const DEFAULT_STDIO_MODE: StdioOptions = ['pipe', 'pipe', 'inherit'];

let cached_bazel_executable: string | undefined;

function getBazelCli(): string {
  if (!cached_bazel_executable) {
    const bazelBinEnv = process.env[BAZEL_BIN_ENV];
    if (bazelBinEnv) {
      cached_bazel_executable = bazelBinEnv;
      return bazelBinEnv;
    }

    const found = BAZEL_EXECUTABLES.find(element => checkCommandExists(element));
    if (!found) {
      throw new Error('Could not find bazel executable (bzl or bazel)');
    }
    cached_bazel_executable = found;
  }

  return cached_bazel_executable;
}

function getBazelCommandString(baseCommand: string, extraArgs: string, quiet: boolean): string {
  return `${getBazelCli()} ${BAZEL_PREFIXED_OPTIONS} ${baseCommand} ${extraArgs} ${quiet ? BAZEL_QUIET_OPTIONS : ''} ${BAZEL_POSTFIXED_OPTIONS}`.trim();
}

function escapeQuery(str: string): string {
  return str.replaceAll('"', '\\"');
}

export class BazelLabel {
  constructor(
    readonly repo: string | undefined,
    readonly target: string,
    readonly name: string | undefined,
  ) {}

  static fromString(str: string): BazelLabel {
    let repo: string | undefined;
    if (str.startsWith('@')) {
      const slashSlashIndex = str.indexOf('//');
      if (slashSlashIndex <= 0) {
        throw new Error(`Expected // after repo name (got ${str})`);
      }

      repo = str.slice(1, slashSlashIndex);
      str = str.slice(slashSlashIndex);
    }
    if (str.endsWith('/')) {
      str = str.slice(0, -1);
    }

    const separatorIndex = str.indexOf(':');
    if (separatorIndex >= 0) {
      const target = str.slice(0, separatorIndex);
      const name = str.slice(separatorIndex + 1);
      return new BazelLabel(repo, target, name);
    } else {
      return new BazelLabel(repo, str, undefined);
    }
  }

  get apparentRepo(): string | undefined {
    if (!this.repo) return undefined;
    const tildeIndex = this.repo.indexOf('~');
    return tildeIndex >= 0 ? this.repo.slice(0, tildeIndex) : this.repo;
  }

  toString(): string {
    const repoPrefix = this.repo ? `@${this.repo}` : '';
    if (this.name) {
      return `${repoPrefix}${this.target}:${this.name}`;
    } else {
      return repoPrefix + this.target;
    }
  }

  toBuildableString(): string {
    const repoPrefix = this.apparentRepo ? `@${this.apparentRepo}` : '';
    if (this.name) {
      return `${repoPrefix}${this.target}:${this.name}`;
    } else {
      return repoPrefix + this.target;
    }
  }
}

class BazelCommandCache {
  private cache = new Map<string, Promise<string>>();

  constructor(readonly execute: (arg: string) => Promise<string>) {}

  get(args: string): Promise<string> {
    const existingPromise = this.cache.get(args);
    if (existingPromise) {
      return existingPromise;
    }

    const ret = this.execute(args);
    this.cache.set(args, ret);

    return ret;
  }
}

export class BazelClient {
  private workspaceRootCache: BazelCommandCache;
  private executionRootCache: BazelCommandCache;

  constructor() {
    this.workspaceRootCache = new BazelCommandCache(async () => {
      return this.runCommandAndGetTrimmedStdOut('info workspace');
    });

    this.executionRootCache = new BazelCommandCache(async () => {
      return this.runCommandAndGetTrimmedStdOut('info execution_root');
    });
  }

  async resolveLabel(str: string): Promise<BazelLabel> {
    const unresolvedLabel = BazelLabel.fromString(str);

    const requiresResolve = unresolvedLabel.target.split('/').includes('..');
    if (!requiresResolve) {
      return unresolvedLabel;
    }

    const resolvedTarget = await this.resolveTarget(unresolvedLabel.target);
    return new BazelLabel(unresolvedLabel.repo, resolvedTarget, unresolvedLabel.name);
  }

  async getWorkspaceRoot(): Promise<string> {
    return this.workspaceRootCache.get(process.cwd());
  }

  async getExecutionRoot(): Promise<string> {
    return this.executionRootCache.get(process.cwd());
  }

  async queryTargetsForKinds(kinds: string[]): Promise<string[]> {
    const command = `query "kind(${kinds.join('|')}, //...)"`;

    const { stdout: targetStrings } = await this.spawnCommand(command);
    const targets = targetStrings.split('\n').filter(target => target.length > 0);

    return targets;
  }

  async queryTargetsForTag(kind: string): Promise<string[]> {
    const command = `query 'attr("tags", "${kind}", //...)'`;

    const { stdout: targetStrings } = await this.spawnCommand(command);
    const targets = targetStrings.split('\n').filter(target => target.length > 0);

    return targets;
  }

  async query(queryStr: string): Promise<string[]> {
    const command = `query "${escapeQuery(queryStr)}"`;
    return await this.runCommandWithLinesOutput(command);
  }

  async graphQuery(queryStr: string): Promise<Digraph> {
    const command = `query "${escapeQuery(queryStr)}"  --output=graph`;
    const { stdout } = await this.runCommand(command);

    return Digraph.parse(stdout.trim());
  }

  async queryBuildOutputs(targets: readonly string[], extraArgs: string = ''): Promise<string[]> {
    const command = `cquery 'set(${targets.join(' ')})' --output files ${extraArgs}`;
    const lines = await this.runCommandWithLinesOutput(command);
    return lines.sort();
  }

  async buildTarget(target: string, extraArgs: string = ''): Promise<CommandResult> {
    // Use 'inherit' as the stdio mode by default as usually the output irrelevant
    // and only the success/failure matters
    const commandBuildTarget = `build ${target} ${extraArgs}`;

    return await this.spawnCommand(commandBuildTarget, 'inherit').catch(() => {
      throw new CliError(`Failed to build target: ${target}`);
    });
  }

  async buildTargets(targets: readonly string[]): Promise<CommandResult> {
    // Use 'inherit' as the stdio mode by default as usually the output irrelevant
    // and only the success/failure matters

    const allTargets = targets.join(' ');
    const commandBuildTarget = `build ${allTargets} --show_result=${targets.length}`;

    return await this.spawnCommand(commandBuildTarget, 'inherit').catch(() => {
      throw new CliError(`Failed to build targets: ${targets.join(' ')}`);
    });
  }

  async runTarget(target: string, extraArgs: string = ''): Promise<CommandResult> {
    const commandBuildTarget = `run ${target} ${extraArgs}`;

    return await this.spawnCommand(commandBuildTarget, 'inherit').catch(() => {
      throw new CliError(`Failed to run target: ${target}`);
    });
  }

  // Run bazel query for targets matching queryKind and filtered to match any of the specified
  // filters.
  async queryTargetsByKindWithFilter(
    queryKind: string,
    filters?: string[],
    stdioMode = DEFAULT_STDIO_MODE,
  ): Promise<string[]> {
    let command = `query "kind(${queryKind}, //...)"`;

    if (filters !== undefined && filters.length > 0) {
      const filtersString = filters.join('|');
      command = `query 'filter("${filtersString}", kind("${queryKind}", //...))'`;
    }

    return await this.spawnCommand(command, stdioMode).then(result => {
      const targets = result.stdout.split('\n').filter(target => target.length > 0);
      return targets;
    });
  }

  async testTargets(targets: string[], extraArgs: string = ''): Promise<CommandResult> {
    const targetsString = targets.join(' ');
    const commandTestTarget = `test ${targetsString} ${extraArgs}`;

    return await this.spawnCommand(commandTestTarget, 'inherit', extraArgs).catch(() => {
      throw new CliError(`Failed to test targets: ${targetsString}`);
    });
  }

  async getVersion(): Promise<[number, string, string]> {
    const commandBazelInfoVersion = `version --gnu_format`;
    const {
      returnCode: returnCode,
      stdout: versionInfo,
      stderr: error,
    } = await this.runCommand(commandBazelInfoVersion);

    return [returnCode, versionInfo.trim(), String(error)];
  }

  private async runCommandWithLinesOutput(command: string): Promise<string[]> {
    const { stdout: outputStrings } = await this.runCommand(command);
    return outputStrings.split('\n').filter(output => output.length > 0);
  }

  private async runCommandAndGetTrimmedStdOut(command: string): Promise<string> {
    const { stdout: root } = await this.runCommand(command);

    return root.trim();
  }

  private runCommand(command: string): Promise<CommandResult> {
    const commandWithOptions = getBazelCommandString(command, '', false);
    return LoadingIndicator.fromTask(runCliCommand(commandWithOptions, undefined, true))
      .setText(`${wrapInColor('Running Bazel command:', ANSI_COLORS.YELLOW_COLOR)} ${getBazelCli()} ${command}`)
      .show();
  }

  private async spawnCommand(
    command: string,
    stdioMode: StdioOptions = DEFAULT_STDIO_MODE,
    extraArgs: string = '',
  ): Promise<CommandResult> {
    const commandWithOptions = getBazelCommandString(command, extraArgs, false);
    console.log(
      `${wrapInColor('Running Bazel command:', ANSI_COLORS.YELLOW_COLOR)} ${getBazelCli()} ${command} ${extraArgs}`,
    );
    return await spawnCliCommand(commandWithOptions, undefined, stdioMode, true, true);
  }

  private async resolveTarget(pathStr: string): Promise<string> {
    const absolutePath = path.resolve(pathStr);
    const root = await this.getWorkspaceRoot();
    if (!absolutePath.startsWith(root)) {
      throw new Error(
        `Could not resolve target at relative path, found Bazel workspace root at '${root}', resolved target path at ${absolutePath}`,
      );
    }

    return '/' + absolutePath.slice(root.length);
  }
}
