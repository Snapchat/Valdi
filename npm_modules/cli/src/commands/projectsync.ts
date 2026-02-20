/* eslint-disable @typescript-eslint/no-non-null-assertion */
/* eslint-disable @typescript-eslint/no-explicit-any */
/* eslint-disable @typescript-eslint/no-unsafe-assignment */
/* eslint-disable @typescript-eslint/no-unsafe-member-access */
import fsSync from 'fs';
import fs from 'fs/promises';
import type { JSONPath } from 'jsonc-parser';
import path from 'path';
import type { Argv } from 'yargs';
import { CliError } from '../core/errors';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { BazelClient, BazelLabel } from '../utils/BazelClient';
import { byString } from '../utils/byString';
import { logReproduceThisCommandIfNeeded, makeCommandHandler } from '../utils/errorUtils';
import { JSONCFile } from '../utils/jsonUtils';
import { relativePathTo } from '../utils/pathUtils';

const VALDI_BASE_TARGET = BazelLabel.fromString('@valdi//modules:valdi_base');
const VALDI_CORE_TARGET = BazelLabel.fromString('@valdi//src/valdi_modules/valdi_core:valdi_core');

interface VSCodeLaunchConfiguration {
  name: string;
  type: string;
  request: string;
  websocketAddress: string;
  sourceMapPathOverrides: { [key: string]: string };
  sourceMaps: boolean;
  restart: boolean;
  resolveSourceMapLocations: unknown[];
}

interface VSCodeLaunchInfo {
  version?: string;
  configurations?: VSCodeLaunchConfiguration[];
}

const VSCODE_VALDI_LAUNCH_CONFIGURATION: VSCodeLaunchConfiguration = {
  name: 'Valdi attach',
  type: 'node',
  request: 'attach',
  websocketAddress: 'ws://localhost:13595',
  sourceMapPathOverrides: {},
  sourceMaps: true,
  restart: false,
  //This empty value is important so that vscode-js-debug just uses the inline source maps
  resolveSourceMapLocations: [],
};

const DEFAULT_VSCODE_LAUNCH_JSON: VSCodeLaunchInfo = {
  version: '0.2.0',
  configurations: [VSCODE_VALDI_LAUNCH_CONFIGURATION],
};

interface CommandParameters {
  target: string[] | undefined;
  vscode_launch_path: string | undefined;
}

interface TargetDescription {
  label: BazelLabel;
  paths: readonly string[];
}

interface TsConfigMatchedTarget {
  target: TargetDescription;
  dependencies: TargetDescription[];
}

interface TsConfigDir {
  dir: string;
  matchedTargets: TsConfigMatchedTarget[];
}

interface BazelWorkspaceInfo {
  workspaceName: string;
  workspaceRoot: string;
  executionRoot: string;
}

interface ProjectSyncOutput {
  target: BazelLabel;
  dependencies: BazelLabel[];
  tsGeneratedDir: string | undefined;
}

interface ProjectSyncOutputJSON {
  target: string;
  dependencies: string[];
  ts_generated_dir: string | undefined;
}

async function parseProjectSyncJSON(jsonFilePath: string): Promise<ProjectSyncOutput> {
  const jsonFilePathContent = await fs.readFile(jsonFilePath);
  const json = JSON.parse(jsonFilePathContent.toString('utf8')) as ProjectSyncOutputJSON;
  let tsGeneratedDir: string | undefined;

  if (json.ts_generated_dir) {
    const dir = path.dirname(jsonFilePath);

    tsGeneratedDir = path.resolve(dir, json.ts_generated_dir);
  }

  return {
    target: BazelLabel.fromString(json.target),
    dependencies: json.dependencies.map(d => BazelLabel.fromString(d)),
    tsGeneratedDir,
  };
}

function isExternalLabel(workspaceInfo: BazelWorkspaceInfo, label: BazelLabel): boolean {
  return !!label.repo && label.repo !== workspaceInfo.workspaceName;
}

function getSymlinkedBazelExecutionRoot(workspaceInfo: BazelWorkspaceInfo): string {
  return path.join(workspaceInfo.workspaceRoot, `bazel-${workspaceInfo.workspaceName}`);
}

function bazelLabelToAbsolutePath(workspaceInfo: BazelWorkspaceInfo, label: BazelLabel): string {
  const targetPath = label.target.slice(2);
  if (isExternalLabel(workspaceInfo, label)) {
    return path.join(getSymlinkedBazelExecutionRoot(workspaceInfo), 'external', label.repo!, targetPath);
  } else {
    return path.join(workspaceInfo.workspaceRoot, targetPath);
  }
}

async function buildProjectSyncs(bazel: BazelClient, workspaceRoot: string, projectSyncTargets: readonly string[]) {
  await bazel.buildTargets(projectSyncTargets);
  const buildOutputs = await bazel.queryBuildOutputs(projectSyncTargets);
  const filteredBuildOutputs = buildOutputs.filter(c => c.endsWith('/projectsync.json')).sort(byString(v => v));

  return await Promise.all(filteredBuildOutputs.map(b => parseProjectSyncJSON(path.resolve(workspaceRoot, b))));
}

async function syncPathsByLabel(
  client: BazelClient,
  workspaceInfo: BazelWorkspaceInfo,
  pathsByLabel: Map<string, string[]>,
  projectSyncOutputs: readonly ProjectSyncOutput[],
) {
  for (const projectSyncOutput of projectSyncOutputs) {
    const paths: string[] = [];
    paths.push(bazelLabelToAbsolutePath(workspaceInfo, projectSyncOutput.target));

    if (projectSyncOutput.tsGeneratedDir) {
      paths.push(projectSyncOutput.tsGeneratedDir);
    }

    pathsByLabel.set(projectSyncOutput.target.toString(), paths);
  }

  // TODO(simon): Refactor projectsync bzl rule so that it recursively resolves
  // all of these without us having to take care of it.
  const missingTargets = new Set<string>();

  for (const projectSyncOutput of projectSyncOutputs) {
    for (const dependency of projectSyncOutput.dependencies) {
      const target = dependency.toString();
      if (!pathsByLabel.has(target)) {
        missingTargets.add(target);
      }
    }
  }

  if (missingTargets.size > 0) {
    const targets = [...missingTargets.values()].sort().map(f => `${f}_projectsync`);
    const newProjectSyncOutputs = await buildProjectSyncs(client, workspaceInfo.workspaceRoot, targets);
    await syncPathsByLabel(client, workspaceInfo, pathsByLabel, newProjectSyncOutputs);
  }
}

async function collectTsConfigDirs(
  bazel: BazelClient,
  workspaceInfo: BazelWorkspaceInfo,
  projectSyncOutputs: readonly ProjectSyncOutput[],
): Promise<TsConfigDir[]> {
  const pathsByLabel = new Map<string, string[]>();

  await syncPathsByLabel(bazel, workspaceInfo, pathsByLabel, projectSyncOutputs);

  const makeTargetDescription = (label: BazelLabel): TargetDescription => {
    const target = label.toString();
    const paths = pathsByLabel.get(target);

    if (!paths) {
      throw new CliError(`Could not resolve TypeScript import paths for target ${target}`);
    }
    return {
      label,
      paths,
    };
  };

  const tsConfigDirs = new Map<string, TsConfigDir>();

  for (const projectSyncOutput of projectSyncOutputs) {
    if (projectSyncOutput.target.repo) {
      // Ignore external repo deps
      continue;
    }

    const tsConfigDirPath = bazelLabelToAbsolutePath(workspaceInfo, projectSyncOutput.target);
    let tsConfigDir = tsConfigDirs.get(tsConfigDirPath);
    if (!tsConfigDir) {
      tsConfigDir = { dir: tsConfigDirPath, matchedTargets: [] };
      tsConfigDirs.set(tsConfigDirPath, tsConfigDir);
    }

    const dependencies = projectSyncOutput.dependencies.map(d => makeTargetDescription(d));

    tsConfigDir.matchedTargets.push({
      target: makeTargetDescription(projectSyncOutput.target),
      dependencies: dependencies,
    });
  }

  return [...tsConfigDirs.values()].sort(byString(v => v.dir));
}

function removeTsFileExtension(path: string): string {
  if (path.endsWith('.d.ts')) {
    return path.slice(0, -5);
  } else if (path.endsWith('.ts')) {
    return path.slice(0, -3);
  }
  return path;
}

function computeTsCompilerOptions(
  existingTsConfigFile: any,
  tsConfigDir: string,
  matchedTargets: readonly TsConfigMatchedTarget[],
  baseTsFiles: readonly string[],
): unknown {
  let compilerOptions: any;
  if (existingTsConfigFile) {
    compilerOptions = { ...existingTsConfigFile };
  } else {
    compilerOptions = {};
  }

  compilerOptions.paths = {};
  const rootDirs: string[] = [];
  let valdiCoreTarget: TargetDescription | undefined;
  for (const matchedTarget of matchedTargets) {
    const targetRootDirs = matchedTarget.target.paths.map(p => relativePathTo(tsConfigDir, path.dirname(p)));

    for (const targetRootDir of targetRootDirs) {
      if (!rootDirs.includes(targetRootDir)) {
        rootDirs.push(targetRootDir);
      }
    }

    const selfName = matchedTarget.target.label.name ?? '';
    const selfInclude = `${selfName}/*`;

    const selfImportPaths = matchedTarget.target.paths.map(p => `${relativePathTo(tsConfigDir, p)}/*`);

    compilerOptions.paths[selfInclude] = selfImportPaths;

    for (const dependency of matchedTarget.dependencies) {
      if (!dependency.label.name || compilerOptions.paths[dependency.label.name]) {
        // Already present
        continue;
      }

      if (dependency.label.name === 'valdi_core') {
        valdiCoreTarget = dependency;
      }

      const importPaths = dependency.paths.map(p => `${relativePathTo(tsConfigDir, p)}/*`);

      const key = `${dependency.label.name}/*`;
      compilerOptions.paths[key] = importPaths;
    }
  }

  if (valdiCoreTarget) {
    // Add tslib dep
    compilerOptions.paths['tslib'] = valdiCoreTarget.paths.map(p =>
      relativePathTo(tsConfigDir, path.join(p, 'src/tslib')),
    );
  }

  compilerOptions.types = baseTsFiles.map(p => relativePathTo(tsConfigDir, removeTsFileExtension(p)));

  compilerOptions.rootDirs = rootDirs;

  return compilerOptions;
}

function getBaseTsConfigFile(valdiBaseAbsoluteFilePaths: string[]): string | undefined {
  return valdiBaseAbsoluteFilePaths.find(p => path.basename(p) === 'base.tsconfig.json');
}

function updateTsConfigFile(file: JSONCFile<any>, path: JSONPath, value: any): JSONCFile<any> {
  return file.updating(path, value, {
    formattingOptions: {
      insertSpaces: true,
    },
  });
}

async function processTsConfigDir(
  tsConfigDir: TsConfigDir,
  baseTsConfigFilePath: string | undefined,
  baseTsFiles: readonly string[],
): Promise<void> {
  const tsConfigPath = path.join(tsConfigDir.dir, 'tsconfig.json');

  let tsConfigFile: JSONCFile<any>;
  if (fsSync.existsSync(tsConfigPath)) {
    const fileContent = await fs.readFile(tsConfigPath);
    tsConfigFile = JSONCFile.parse(fileContent.toString('utf8'));
  } else {
    tsConfigFile = JSONCFile.fromObject({});
  }

  const tsCompilerOptions = computeTsCompilerOptions(
    tsConfigFile?.value?.compilerOptions,
    tsConfigDir.dir,
    tsConfigDir.matchedTargets,
    baseTsFiles,
  );

  if (baseTsConfigFilePath) {
    const resolvedBaseTsConfigFilePath = relativePathTo(tsConfigDir.dir, baseTsConfigFilePath);
    if (tsConfigFile?.value?.extends !== resolvedBaseTsConfigFilePath) {
      tsConfigFile = updateTsConfigFile(tsConfigFile, ['extends'], resolvedBaseTsConfigFilePath);
    }
  }

  tsConfigFile = updateTsConfigFile(tsConfigFile, ['compilerOptions'], tsCompilerOptions);

  await fs.writeFile(tsConfigPath, tsConfigFile.toJSONString());
}

function computeSourceMapPathOverrides(
  bazelWorkspaceRoot: string,
  tsConfigDirs: readonly TsConfigDir[],
): { [key: string]: string } {
  const output: { [key: string]: string } = {};

  for (const tsConfigDir of tsConfigDirs) {
    for (const matchedTarget of tsConfigDir.matchedTargets) {
      if (matchedTarget.target.label.name) {
        const key = `/${matchedTarget.target.label.name}/*`;
        const relativePath = relativePathTo(bazelWorkspaceRoot, tsConfigDir.dir);
        const expected = `\${workspaceRoot}/${relativePath}/*`;

        const existing = output[key];
        if (existing && existing !== expected) {
          console.error(`Conflicting target name ${matchedTarget.target.label.name}: '${expected}' vs ${existing}`);
        } else {
          output[key] = expected;
        }
      }
    }
  }

  return output;
}

async function processLaunchJSONFile(
  launchJsonFilePath: string | undefined,
  bazel: BazelClient,
  tsConfigDirs: readonly TsConfigDir[],
): Promise<void> {
  const workspaceRoot = await bazel.getWorkspaceRoot();
  if (!launchJsonFilePath) {
    launchJsonFilePath = path.join(workspaceRoot, '.vscode/launch.json');
  }

  let launchJsonFile: JSONCFile<VSCodeLaunchInfo>;
  if (fsSync.existsSync(launchJsonFilePath)) {
    const launchJsonFileContent = await fs.readFile(launchJsonFilePath, 'utf8');
    launchJsonFile = JSONCFile.parse(launchJsonFileContent);
  } else {
    launchJsonFile = JSONCFile.fromObject(DEFAULT_VSCODE_LAUNCH_JSON);
  }

  const valdiLaunchConfigurationIndex =
    launchJsonFile.value.configurations?.findIndex(c => c.name === VSCODE_VALDI_LAUNCH_CONFIGURATION.name) ?? -1;
  let valdiLaunchConfiguration =
    valdiLaunchConfigurationIndex >= 0
      ? launchJsonFile.value.configurations![valdiLaunchConfigurationIndex]!
      : VSCODE_VALDI_LAUNCH_CONFIGURATION;
  valdiLaunchConfiguration = { ...valdiLaunchConfiguration };
  valdiLaunchConfiguration.sourceMapPathOverrides = computeSourceMapPathOverrides(workspaceRoot, tsConfigDirs);

  if (valdiLaunchConfigurationIndex >= 0) {
    launchJsonFile = launchJsonFile.updating(
      ['configurations', valdiLaunchConfigurationIndex],
      valdiLaunchConfiguration,
      {
        formattingOptions: { insertSpaces: true },
      },
    );
  } else if (launchJsonFile.value.configurations) {
    launchJsonFile = launchJsonFile.updating(
      ['configurations', launchJsonFile.value.configurations.length],
      valdiLaunchConfiguration,
      {
        formattingOptions: { insertSpaces: true },
        isArrayInsertion: true,
      },
    );
  } else {
    launchJsonFile = launchJsonFile.updating(['configurations'], [valdiLaunchConfiguration], {
      formattingOptions: { insertSpaces: true },
    });
  }

  const launchJsonFileDir = path.dirname(launchJsonFilePath);
  if (!fsSync.existsSync(launchJsonFileDir)) {
    await fs.mkdir(launchJsonFileDir, { recursive: true });
  }
  await fs.writeFile(launchJsonFilePath, launchJsonFile.toJSONString());
}

export async function getAllProjectSyncTargets(bazel: BazelClient): Promise<string[]> {
  console.log('Searching for Valdi targets in current workspace...');
  return await bazel.query(`kind('valdi_projectsync', //...)`);
}

export async function runProjectSync(
  bazel: BazelClient,
  inputTargets: string[],
  vscodeLaunchPath: string | undefined,
  generateLaunchJSONFile: boolean,
): Promise<void> {
  console.log('Resolving dependencies of resolved Valdi targets...');
  const workspaceRoot = await bazel.getWorkspaceRoot();
  const executionRoot = await bazel.getExecutionRoot();

  const allProjectSyncOutputs = await buildProjectSyncs(bazel, workspaceRoot, inputTargets);

  console.log('Generating tsconfig.json files...');

  const workspaceName = path.basename(executionRoot);
  const workspaceInfo: BazelWorkspaceInfo = { workspaceName, workspaceRoot, executionRoot };

  const valdiBaseRelativeFilePaths = await bazel.queryBuildOutputs([VALDI_BASE_TARGET.toString()]);
  const valdiBaseAbsoluteFilePaths = valdiBaseRelativeFilePaths.map(v => {
    if (isExternalLabel(workspaceInfo, VALDI_BASE_TARGET)) {
      return path.resolve(getSymlinkedBazelExecutionRoot(workspaceInfo), v);
    } else {
      return path.resolve(workspaceRoot, v);
    }
  });
  const baseTsConfigFilePath = getBaseTsConfigFile(valdiBaseAbsoluteFilePaths);
  const baseTsFiles = valdiBaseAbsoluteFilePaths.filter(v => v.endsWith('.ts'));

  const tsConfigDirs = await collectTsConfigDirs(bazel, workspaceInfo, allProjectSyncOutputs);

  await Promise.all(tsConfigDirs.map(v => processTsConfigDir(v, baseTsConfigFilePath, baseTsFiles)));

  if (generateLaunchJSONFile) {
    console.log('Generating VSCode launch.json file...');
    await processLaunchJSONFile(vscodeLaunchPath, bazel, tsConfigDirs);
  }
}

async function projectSync(args: ArgumentsResolver<CommandParameters>): Promise<void> {
  const bazel = new BazelClient();
  const inputTargets = await args.getArgumentOrResolve('target', () => getAllProjectSyncTargets(bazel));
  await runProjectSync(bazel, inputTargets, args.getArgument('vscode_launch_path'), true);

  logReproduceThisCommandIfNeeded(args);
}

export const command = 'projectsync [--target target]';
export const describe = 'Synchronize VSCode project for the given targets';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs.option('target', {
    describe: 'targets for which to synchronize the VSCode project for',
    type: 'string',
    array: true,
    requiresArg: false,
  });
  yargs.option('vscode_launch_path', {
    describe:
      'A the path to there launch.json file used by VSCode. Will be inferred as .vscode/launch.json from the Bazel workspace root if not specified',
    type: 'string',
  });
};

export const handler = makeCommandHandler(projectSync);
