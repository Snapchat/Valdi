import Separator from "inquirer/lib/objects/separator";
import { ALL_ARCHITECTURES, ANSI_COLORS, Architecture, PLATFORM } from "../core/constants";
import { CliError } from "../core/errors";
import { getApplicationTargetTagForPlatform, resolveBazelBuildArgs, selectBazelTarget, SharedCommandParameters } from "./applicationUtils";
import { ArgumentsResolver } from "./ArgumentsResolver";
import { CliChoice, getUserChoice } from "./cliUtils";
import { getAllArchitecturesForAndroid, getArchitectureForAndroidDevice, getConnectedDevices } from "./deviceUtils";
import { wrapInColor } from "./logUtils";
import { BazelClient } from "./BazelClient";

export interface BuildInfo {
    platform: PLATFORM;
    application: string;
    bazelArgs: string; 
    selectedDevice: string | undefined
}

export interface CommandParameters extends SharedCommandParameters {
  application: string | undefined;
  device_id: string | undefined;
  target_output_path: string | undefined;
  simulator: boolean | undefined;
}

// Prompts User to choose from the list of available devices
// No prompt if only one device is available
async function getDeviceChoice(platform: string): Promise<string> {
  const availableDevices = await getConnectedDevices(platform);

  if (availableDevices.length === 0) {
    throw new CliError('No devices connected...');
  } else if (availableDevices.length === 1) {
    // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
    const selectedDevice: string = availableDevices[0]!;
    console.log('Only one device connected, using it:', wrapInColor(selectedDevice, ANSI_COLORS.GREEN_COLOR));
    return selectedDevice;
  }

  // Prompt user to select a device
  const choices: Array<CliChoice<string> | Separator> = [];

  if (platform === PLATFORM.ANDROID) {
    choices.push(new Separator('-----Android Devices-----'));
  } else if (platform === PLATFORM.IOS) {
    choices.push(new Separator('-----iOS Devices-----'));
  }
  choices.push(...availableDevices.map(device => ({ name: device, value: device })));

  return await getUserChoice(choices, 'Please select a device to perform the install:');
}

/**
 * Whether to build for a physical device. Defaults to the simulator; only an explicit
 * `--simulator=false` (or `--no-simulator`) selects a device build.
 */
export function isDeviceBuild(simulator: boolean | undefined): boolean {
  return simulator === false;
}

export async function getBuildInfo(argv: ArgumentsResolver<CommandParameters>, bazel: BazelClient, checkDevice: boolean): Promise<BuildInfo> {
  const platform = argv.getArgument('platform') as PLATFORM;
  const forDevice = isDeviceBuild(argv.getArgument('simulator'));

  let selectedDevice: string | undefined;
  let architectures: Architecture[] = ALL_ARCHITECTURES;

  if (platform === PLATFORM.ANDROID) {
    if (checkDevice) {
        selectedDevice = await argv.getArgumentOrResolve('device_id', () => {
        console.log('Detecting Connected Devices...');
        return getDeviceChoice(platform);
        });
        architectures = [await getArchitectureForAndroidDevice(selectedDevice)];
    } else {
        architectures = getAllArchitecturesForAndroid();
    }
  }

  const bazelArgs = resolveBazelBuildArgs(
    platform,
    argv.getArgument('build_config'),
    architectures,
    forDevice,
    argv.getArgument('enable_runtime_logs'),
    argv.getArgument('enable_runtime_traces'),
    argv.getArgument('bazel_args'),
  );

  // Application Selection
  // ---------------------
  const application = await argv.getArgumentOrResolve('application', () => {
    console.log('No application specified querying available targets...');
    return selectBazelTarget(
      bazel,
      getApplicationTargetTagForPlatform(platform),
      'valdi_application',
      'Please select an application target to install:',
    );
  });

  return {
    platform: platform,
    application: application,
    bazelArgs: bazelArgs,
    selectedDevice: selectedDevice,
  };
}