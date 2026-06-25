import { type Argv } from 'yargs';
import { ANSI_COLORS, PLATFORM } from '../core/constants';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { BazelClient } from '../utils/BazelClient';
import {
  applicationExtensionForPlatform,
  getOutputFilePath,
  makeArgsBuilder,
} from '../utils/applicationUtils';
import type { CommandParameters} from '../utils/buildInfo';
import { getBuildInfo } from '../utils/buildInfo';
import { installAndroidApk } from '../utils/deviceUtils';
import { logReproduceThisCommandIfNeeded, makeCommandHandler } from '../utils/errorUtils';
import { wrapInColor } from '../utils/logUtils';


async function valdiInstall(argv: ArgumentsResolver<CommandParameters>) {
  const bazel = new BazelClient();

  const buildInfo = await getBuildInfo(argv, bazel, true);  

  // Output Selection
  // ----------------

  // Perform build and install
  // -------------------------
  console.log(`Building: ${wrapInColor(buildInfo.application, ANSI_COLORS.GREEN_COLOR)}`);

  switch (buildInfo.platform) {
    case PLATFORM.ANDROID: {
      const outputFilePath = await argv.getArgumentOrResolve('target_output_path', () => {
        console.log('Resolving output paths...');
        return getOutputFilePath(bazel, applicationExtensionForPlatform(buildInfo.platform), buildInfo.application, buildInfo.bazelArgs);
      });
      await bazel.buildTarget(buildInfo.application, buildInfo.bazelArgs);

      console.log('Installing Android application...');
      await installAndroidApk(outputFilePath, buildInfo.selectedDevice);
      logReproduceThisCommandIfNeeded(argv);
      break;
    }
    case PLATFORM.IOS: {
      console.log('Installing iOS application...');
      await bazel.runTarget(buildInfo.application, buildInfo.bazelArgs);
      logReproduceThisCommandIfNeeded(argv);
      break;
    }
    case PLATFORM.MACOS: {
      logReproduceThisCommandIfNeeded(argv);
      console.log('Installing MacOS application...');
      await bazel.runTarget(buildInfo.application, buildInfo.bazelArgs);
      break;
    }
    case PLATFORM.LINUX: {
      logReproduceThisCommandIfNeeded(argv);
      console.log('Running Linux application...');
      await bazel.runTarget(buildInfo.application, buildInfo.bazelArgs);
      break;
    }
    case PLATFORM.CLI: {
      logReproduceThisCommandIfNeeded(argv);
      console.log('Running CLI application...');
      await bazel.runTarget(buildInfo.application, buildInfo.bazelArgs);
      break;
    }
  }
}

export const command = 'install <platform>';
export const describe = 'Build and install the application to the connected device';
export const builder = makeArgsBuilder((yargs: Argv<CommandParameters>) => {
  yargs
    .option('application', {
      describe: 'Name of the application to install',
      type: 'string',
      requiresArg: true,
    })
    .option('device_id', {
      describe: 'Device ID that will receive the application',
      type: 'string',
      requiresArg: true,
    })
    .option('target_output_path', {
      describe: 'The output path for the Bazel target',
      type: 'string',
      requiresArg: true,
    })
    .option('simulator', {
      describe: 'Whether to build for simulator or for device',
      type: 'boolean',
    });
});
export const handler = makeCommandHandler(valdiInstall);

// TODOs:
// - Cached bazel query results

// Extract 'custom_package' from android_binary
// Extract 'bundle_id' from ios_application target
