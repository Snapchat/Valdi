import fs from 'fs';
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
import { makeTempDir } from '../utils/tempDir';
import { wrapInColor } from '../utils/logUtils';
import { waitForShutdownSignal } from '../utils/processUtils';
import { startWebApplicationSession } from '../utils/webApplicationSession';
import { inferWebHotReloadTarget, registerWebHotReloadSession } from '../utils/webHotReloadSession';
import { openUrlInDefaultBrowser } from '../utils/webServerUtils';

interface InstallCommandParameters extends CommandParameters {
  host: string | undefined;
  open: boolean | undefined;
  port: number | undefined;
}

async function valdiInstall(argv: ArgumentsResolver<InstallCommandParameters>) {
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
    case PLATFORM.WEB: {
      const outputFilePath = await argv.getArgumentOrResolve('target_output_path', () => {
        console.log('Resolving output paths...');
        return getOutputFilePath(
          bazel,
          applicationExtensionForPlatform(buildInfo.platform),
          buildInfo.application,
          buildInfo.bazelArgs,
        );
      });
      await bazel.buildTarget(buildInfo.application, buildInfo.bazelArgs);

      const tempDir = makeTempDir();
      fs.mkdirSync(tempDir);
      try {
        const workspaceRoot = await bazel.getWorkspaceRoot();
        const applicationTarget = (await bazel.resolveLabel(buildInfo.application)).toString();
        const hotreloadTarget = inferWebHotReloadTarget(applicationTarget);
        const session = await startWebApplicationSession({
          archivePath: outputFilePath,
          host: argv.getArgument('host') ?? '127.0.0.1',
          liveReload: hotreloadTarget !== undefined,
          port: argv.getArgument('port') ?? 0,
          workingDir: tempDir,
        });
        const hotReloadRegistration =
          hotreloadTarget === undefined
            ? undefined
            : await registerWebHotReloadSession({
                applicationTarget,
                bazelArgs: buildInfo.bazelArgs,
                hotreloadTarget,
                onPublish: async () => {
                  await session.publish();
                  console.log(`Reloaded web application at ${wrapInColor(session.url, ANSI_COLORS.GREEN_COLOR)}`);
                },
                workspaceRoot,
              });
        try {
          console.log(`Serving web application at ${wrapInColor(session.url, ANSI_COLORS.GREEN_COLOR)}`);
          if (hotreloadTarget) {
            console.log(
              `For hot reload, run ${wrapInColor(`valdi hotreload --target ${hotreloadTarget}`, ANSI_COLORS.GREEN_COLOR)} in another terminal.`,
            );
          }
          if (argv.getArgument('open') ?? true) {
            openUrlInDefaultBrowser(session.url);
          }
          logReproduceThisCommandIfNeeded(argv);
          console.log('Press Ctrl+C to stop the web server.');
          await waitForShutdownSignal();
        } finally {
          await hotReloadRegistration?.close();
          await session.close();
        }
      } finally {
        fs.rmSync(tempDir, { force: true, recursive: true });
      }
      break;
    }
  }
}

export const command = 'install <platform>';
export const describe = 'Build and install the application to the connected device or local web server';
export const builder = makeArgsBuilder((yargs: Argv<InstallCommandParameters>) => {
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
    })
    .option('open', {
      describe: 'Open the web application in the default browser after starting the server',
      type: 'boolean',
      default: true,
    })
    .option('host', {
      describe: 'Host to bind when serving a web application',
      type: 'string',
      default: '127.0.0.1',
      requiresArg: true,
    })
    .option('port', {
      describe: 'Port to bind when serving a web application. Use 0 to select an available port automatically',
      type: 'number',
      default: 0,
      requiresArg: true,
    });
});
export const handler = makeCommandHandler(valdiInstall);

// TODOs:
// - Cached bazel query results

// Extract 'custom_package' from android_binary
// Extract 'bundle_id' from ios_application target
