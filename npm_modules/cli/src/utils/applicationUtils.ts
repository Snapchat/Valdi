import path from 'path';
import type { Argv } from 'yargs';
import {
  ANDROID_ARM64_BUILD_FLAGS,
  ANDROID_ARMV7_BUILD_FLAGS,
  ANDROID_BAZEL_APPLICATION_TAG,
  ANDROID_BUILD_FLAGS,
  ANDROID_EXPORTED_LIBRARY_TAG,
  ANDROID_X86_64_BUILD_FLAGS,
  ANSI_COLORS,
  Architecture,
  CLI_BAZEL_APPLICATION_TAG,
  CLI_BUILD_FLAGS,
  DEBUG_BUILD_FLAGS,
  ENABLE_RUNTIME_LOGS_BUILD_FLAGS,
  ENABLE_RUNTIME_TRACES_BUILD_FLAGS,
  IOS_BAZEL_APPLICATION_TAG,
  IOS_BUILD_FLAGS,
  IOS_DEVICE_BUILD_FLAGS,
  IOS_EXPORTED_LIBRARY_TAG,
  LINUX_BAZEL_APPLICATION_TAG,
  LINUX_BUILD_FLAGS,
  MACOS_BAZEL_APPLICATION_TAG,
  MACOS_BUILD_FLAGS,
  PLATFORM,
  RELEASE_BUILD_FLAGS,
  VALID_PLATFORMS,
} from '../core/constants';
import { CliError } from '../core/errors';
import type { BazelClient } from './BazelClient';
import type { CliChoice } from './cliUtils';
import { getUserChoice } from './cliUtils';
import { wrapInColor } from './logUtils';

export interface SharedCommandParameters {
  platform: string | undefined;
  bazel_args: string | undefined;
  build_config: 'debug' | 'release';
  enable_runtime_logs: boolean | undefined;
  enable_runtime_traces: boolean | undefined;
}

export function makeArgsBuilder<T extends SharedCommandParameters>(
  nestedBuilder: (yargs: Argv<T>) => void,
): (yargs: Argv<T>) => void {
  return yargs => {
    yargs
      .positional('platform', {
        describe: 'Platform to build the library or application for: <ios|android>',
        type: 'string',
        choices: VALID_PLATFORMS,
        coerce: arg => {
          return arg === undefined ? undefined : String(arg).toLowerCase();
        },
      })
      .option('build_config', {
        describe: 'Specifiy whether the application or library should be built in release or debug mode',
        type: 'string',
        choices: ['debug', 'release'],
        default: 'debug',
        requiresArg: true,
      })
      .option('enable_runtime_logs', {
        describe: 'Specifiy Valdi runtime logs should be enabled',
        type: 'boolean',
        default: false,
      })
      .option('enable_runtime_traces', {
        describe: 'Specifiy Valdi runtime traces should be enabled',
        type: 'boolean',
        default: false,
      })
      .option('bazel_args', {
        describe: `Additional arguments to pass to bazel build. Use format ${wrapInColor('--bazel_args="<args>"', ANSI_COLORS.YELLOW_COLOR)}.`,
        type: 'string',
        requiresArg: true,
      });

    nestedBuilder(yargs);
  };
}

export async function selectBazelTarget(
  bazel: BazelClient,
  tag: string,
  targetTypeName: string,
  promptText: string,
): Promise<string> {
  const unfilteredAvailableTargets = await bazel.queryTargetsForTag(tag);
  const availableTargets = unfilteredAvailableTargets.filter(c => !c.endsWith('_DO_NOT_USE'));

  if (availableTargets.length === 0) {
    throw new CliError(`No ${targetTypeName} targets found...`);
  } else if (availableTargets.length === 1) {
    console.log(
      `Only one ${targetTypeName} target found, using it:`,
      wrapInColor(availableTargets[0], ANSI_COLORS.GREEN_COLOR),
    );

    return availableTargets[0] ?? '';
  }

  // Prompt user to select an application target
  const choices: Array<CliChoice<string>> = availableTargets.map((target, index) => ({
    name: `${index + 1}. ${target}`,
    value: target,
  }));

  return await getUserChoice(choices, promptText);
}

export function getApplicationTargetTagForPlatform(platform: PLATFORM): string {
  switch (platform) {
    case PLATFORM.IOS: {
      return IOS_BAZEL_APPLICATION_TAG;
    }
    case PLATFORM.ANDROID: {
      return ANDROID_BAZEL_APPLICATION_TAG;
    }
    case PLATFORM.MACOS: {
      return MACOS_BAZEL_APPLICATION_TAG;
    }
    case PLATFORM.LINUX: {
      return LINUX_BAZEL_APPLICATION_TAG;
    }
    case PLATFORM.CLI: {
      return CLI_BAZEL_APPLICATION_TAG;
    }
  }
}

export function getExportedLibraryTargetTagForPlatform(platform: PLATFORM): string {
  switch (platform) {
    case PLATFORM.IOS: {
      return IOS_EXPORTED_LIBRARY_TAG;
    }
    case PLATFORM.ANDROID: {
      return ANDROID_EXPORTED_LIBRARY_TAG;
    }
    case PLATFORM.MACOS: {
      throw new CliError('Exported library is not yet supported for macOS platform');
    }
    case PLATFORM.LINUX: {
      throw new CliError('Exported library is not yet supported for Linux platform');
    }
    case PLATFORM.CLI: {
      throw new CliError('Exported library is not supported for CLI platform');
    }
  }
}

export function applicationExtensionForPlatform(platform: PLATFORM): string {
  switch (platform) {
    case PLATFORM.IOS: {
      return '.ipa';
    }
    case PLATFORM.ANDROID: {
      return '.apk';
    }
    case PLATFORM.MACOS: {
      return '.zip';
    }
    case PLATFORM.LINUX: {
      return '';
    }
    case PLATFORM.CLI: {
      return '';
    }
  }
}

export function exportedLibraryExtensionForPlatform(platform: PLATFORM): string {
  switch (platform) {
    case PLATFORM.IOS: {
      return '.xcframework';
    }
    case PLATFORM.ANDROID: {
      return '.aar';
    }
    case PLATFORM.MACOS: {
      return '.zip';
    }
    case PLATFORM.LINUX: {
      return '.zip';
    }
    case PLATFORM.CLI: {
      return '.zip';
    }
  }
}

// Reads and selects the output path for the build & install
export async function getOutputFilePath(
  bazel: BazelClient,
  expectedExtension: string,
  target: string,
  bazelArgs: string = '',
): Promise<string> {
  const workspaceRootResult = await bazel.getWorkspaceRoot();
  const buildOutputsResult = await bazel.queryBuildOutputs([target], bazelArgs);

  // TODO(yhuang6): Have a way to prompt for selection and memorize it and/or
  //  add an option to specify name of apk.
  let appChoices = buildOutputsResult;
  if (expectedExtension.length > 0) {
    appChoices = buildOutputsResult.filter(
      output => output.endsWith(expectedExtension) && !output.includes('_unsigned'),
    );
  }

  let outputPath;
  if (appChoices.length === 0) {
    throw new CliError('No outputs are found...');
  } else if (appChoices.length === 1) {
    outputPath = appChoices[0];
  } else {
    outputPath = await getUserChoice(
      appChoices.map((output, index) => ({ name: `${index + 1}. ${output}`, value: output })),
      'Please select the app to install:',
    );
  }

  return outputPath === undefined ? '' : path.join(workspaceRootResult, outputPath);
}

function androidBuildFlagsForArchitecture(architecture: Architecture): readonly string[] {
  switch (architecture) {
    case Architecture.ARM64: {
      return ANDROID_ARM64_BUILD_FLAGS;
    }
    case Architecture.ARMV7: {
      return ANDROID_ARMV7_BUILD_FLAGS;
    }
    case Architecture.X86_64: {
      return ANDROID_X86_64_BUILD_FLAGS;
    }
  }
}

export function resolveBazelBuildArgs(
  platform: PLATFORM,
  buildConfig: 'debug' | 'release' | undefined,
  architectures: Architecture[],
  forDevice: boolean,
  enableRuntimeLogs: boolean | undefined,
  enableRuntimeTracing: boolean | undefined,
  additionalBazelArgs: string | undefined,
): string {
  const allArgs: string[] = [];

  if (buildConfig === 'release') {
    allArgs.push(...RELEASE_BUILD_FLAGS);
  } else {
    allArgs.push(...DEBUG_BUILD_FLAGS);
  }

  if (enableRuntimeLogs) {
    allArgs.push(...ENABLE_RUNTIME_LOGS_BUILD_FLAGS);
  }
  if (enableRuntimeTracing) {
    allArgs.push(...ENABLE_RUNTIME_TRACES_BUILD_FLAGS);
  }

  if (platform === PLATFORM.IOS) {
    allArgs.push(IOS_BUILD_FLAGS);

    if (forDevice) {
      allArgs.push(IOS_DEVICE_BUILD_FLAGS);
    }
  }

  if (platform === PLATFORM.CLI) {
    allArgs.push(...CLI_BUILD_FLAGS);
  } else if (platform === PLATFORM.MACOS) {
    allArgs.push(...MACOS_BUILD_FLAGS);
  } else if (platform === PLATFORM.LINUX) {
    allArgs.push(...LINUX_BUILD_FLAGS);
  }

  if (platform === PLATFORM.ANDROID) {
    allArgs.push(...ANDROID_BUILD_FLAGS);

    for (const architecture of architectures) {
      allArgs.push(...androidBuildFlagsForArchitecture(architecture));
    }
  }

  if (additionalBazelArgs) {
    allArgs.push(additionalBazelArgs);
  }
  return allArgs.join(' ');
}
