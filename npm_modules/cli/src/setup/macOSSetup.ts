import fs from 'fs';
import { CliError } from '../core/errors';
import { checkCommandExists, runCliCommand } from '../utils/cliUtils';
import { DevSetupHelper } from './DevSetupHelper';
import { ANDROID_MACOS_COMMANDLINE_TOOLS } from './versions';

export async function macOSSetup(): Promise<void> {
  const devSetup = new DevSetupHelper();

  if (!checkCommandExists('xcode-select')) {
    throw new CliError(
      'Xcode is not installed. Please install Xcode from the App Store as setup requires Xcode command line tools',
    );
  }

  if (!checkCommandExists('brew')) {
    await devSetup.runShell('Homebrew not found. Installing', [
      '/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"',
    ]);
  }

  await devSetup.runShell('Installing dependencies from Homebrew', [
    'brew install npm bazelisk openjdk@17 temurin git-lfs watchman ios-webkit-debug-proxy',
  ]);

  if (!fs.existsSync('/Library/Java/JavaVirtualMachines/openjdk-17.jdk')) {
    await devSetup.runShell('Setting up JDK', [
      'sudo ln -sfn /opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk /Library/Java/JavaVirtualMachines/openjdk-17.jdk',
    ]);
  }

  await devSetup.writeEnvVariablesToRcFile([
    { name: 'PATH', value: '"/opt/homebrew/opt/openjdk@17/bin:$PATH"' },
    { name: 'JAVA_HOME', value: '`/usr/libexec/java_home -v 17`' },
  ]);

  const javaHome = await runCliCommand('/usr/libexec/java_home');

  await devSetup.setupAndroidSDK(ANDROID_MACOS_COMMANDLINE_TOOLS, javaHome.stdout.trim());

  devSetup.onComplete();
}
