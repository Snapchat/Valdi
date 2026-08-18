import * as fs from 'fs';
import path from 'path';
import type { Argv } from 'yargs';
import { ALL_ARCHITECTURES, ANSI_COLORS, PLATFORM } from '../core/constants';
import { CliError } from '../core/errors';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { BazelClient } from '../utils/BazelClient';
import type { SharedCommandParameters } from '../utils/applicationUtils';
import {
  exportedLibraryExtensionForPlatform,
  getExportedLibraryTargetTagForPlatform,
  getOutputFilePath,
  makeArgsBuilder,
  resolveBazelBuildArgs,
  selectBazelTarget,
} from '../utils/applicationUtils';
import { syncDirectoryContents } from '../utils/directorySync';
import { logReproduceThisCommandIfNeeded, makeCommandHandler } from '../utils/errorUtils';
import { removeFileOrDirAtPath } from '../utils/fileUtils';
import { wrapInColor } from '../utils/logUtils';
import { withTempDir } from '../utils/tempDir';
import { compressDirectoryContentsTo, decompressTo } from '../utils/zipUtils';

const OUTPUT_FORMATS = ['archive', 'directory'] as const;
type OutputFormat = (typeof OUTPUT_FORMATS)[number];

interface CommandParameters extends SharedCommandParameters {
  library: string | undefined;
  target_output_path: string | undefined;
  output_path: string;
  output_format: OutputFormat;
}

async function decompressAndMoveXCFrameworkIntoDestination(
  bazelOutputFilePath: string,
  resolvedOutputPath: string,
): Promise<void> {
  await withTempDir(async tempDir => {
    await decompressTo(bazelOutputFilePath, tempDir);

    const decompressedOutputs = fs.readdirSync(tempDir);
    if (decompressedOutputs.length !== 1) {
      throw new Error(
        `Expected 1 unzipped output, got ${decompressedOutputs.length} (${decompressedOutputs.join(', ')})`,
      );
    }

    // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
    const inputDir = path.join(tempDir, decompressedOutputs[0]!);

    await fs.promises.rename(inputDir, resolvedOutputPath);
  });
}

function getExportOutputExtension(platform: PLATFORM): string {
  return exportedLibraryExtensionForPlatform(platform);
}

async function getTargetToExport(
  argv: ArgumentsResolver<CommandParameters>,
  bazel: BazelClient,
  platform: PLATFORM,
): Promise<string> {
  return await argv.getArgumentOrResolve('library', () => {
    console.log('No library specified querying available targets...');
    return selectBazelTarget(
      bazel,
      getExportedLibraryTargetTagForPlatform(platform),
      'valdi_exported_library',
      'Please select a library to export:',
    );
  });
}

async function valdiExport(argv: ArgumentsResolver<CommandParameters>): Promise<void> {
  const bazel = new BazelClient();
  // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
  const platform = argv.getArgument('platform') as PLATFORM;
  const bazelArgs = resolveBazelBuildArgs(
    platform,
    argv.getArgument('build_config'),
    ALL_ARCHITECTURES,
    /* forDevice */ false,
    argv.getArgument('enable_runtime_logs'),
    argv.getArgument('enable_runtime_traces'),
    argv.getArgument('bazel_args'),
  );

  const resolvedOutputPath = path.resolve(argv.getArgument('output_path'));
  const outputFormat = argv.getArgument('output_format');
  const expectedExtension = getExportOutputExtension(platform);
  if (outputFormat === 'directory' && platform !== PLATFORM.WEB) {
    throw new CliError("The 'directory' output format is only supported for web exports");
  }
  if (outputFormat === 'archive' && !resolvedOutputPath.endsWith(expectedExtension)) {
    throw new CliError(`Output path for platform '${platform}' must have ${expectedExtension} file extension`);
  }

  if (outputFormat === 'archive') {
    removeFileOrDirAtPath(resolvedOutputPath);
  }
  const directoryPath = path.dirname(resolvedOutputPath);
  if (!fs.existsSync(directoryPath)) {
    fs.mkdirSync(directoryPath, { recursive: true });
  }

  const targetToExport = await getTargetToExport(argv, bazel, platform);

  const bazelOutputExtension = platform === PLATFORM.IOS ? '.zip' : platform === PLATFORM.WEB ? '' : expectedExtension;
  const bazelOutputFilePath = await argv.getArgumentOrResolve('target_output_path', () => {
    console.log('Resolving output paths...');
    return getOutputFilePath(bazel, bazelOutputExtension, targetToExport, bazelArgs);
  });

  console.log(`Building: ${wrapInColor(targetToExport, ANSI_COLORS.GREEN_COLOR)}`);

  await bazel.buildTarget(targetToExport, bazelArgs);

  if (platform === PLATFORM.WEB) {
    if (outputFormat === 'directory') {
      console.log(`Synchronizing built target to ${resolvedOutputPath}...`);
      const syncResult = await syncDirectoryContents(bazelOutputFilePath, resolvedOutputPath);
      console.log(
        `Directory export: ${syncResult.added} added, ${syncResult.updated} updated, ` +
          `${syncResult.removed} removed, ${syncResult.unchanged} unchanged`,
      );
    } else {
      console.log(`Copying built target to ${resolvedOutputPath}...`);
      await compressDirectoryContentsTo(bazelOutputFilePath, resolvedOutputPath);
    }
  } else if (platform === PLATFORM.IOS) {
    console.log(`Copying built target to ${resolvedOutputPath}...`);
    await decompressAndMoveXCFrameworkIntoDestination(bazelOutputFilePath, resolvedOutputPath);
  } else {
    console.log(`Copying built target to ${resolvedOutputPath}...`);
    await fs.promises.copyFile(bazelOutputFilePath, resolvedOutputPath);
  }

  logReproduceThisCommandIfNeeded(argv);

  console.log('All done!');
}

export const command = 'export <platform>';
export const describe = 'Build and export a Valdi library';
export const builder = makeArgsBuilder((yargs: Argv<CommandParameters>) => {
  yargs
    .option('library', {
      describe: 'Name of the library to export',
      type: 'string',
      requiresArg: true,
    })
    .option('target_output_path', {
      describe: 'The output path for the Bazel target',
      type: 'string',
      requiresArg: true,
    })
    .option('output_path', {
      describe: 'The path where to store the exported library artifact or directory',
      type: 'string',
      requiresArg: true,
    })
    .option('output_format', {
      choices: OUTPUT_FORMATS,
      default: 'archive' as const,
      describe: 'Export web output as an archive or a content-synchronized directory',
    })
    .demandOption('output_path');
});
export const handler = makeCommandHandler(valdiExport);
