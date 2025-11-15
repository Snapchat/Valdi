import fs from 'fs';
import { CliError } from '../core/errors';
import { checkCommandExists, runCliCommand } from '../utils/cliUtils';
import { DevSetupHelper } from './DevSetupHelper';

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
    'brew install npm bazelisk openjdk@11 temurin git-lfs watchman ios-webkit-debug-proxy',
  ]);

  if (!fs.existsSync('/Library/Java/JavaVirtualMachines/openjdk-11.jdk')) {
    await devSetup.runShell('Setting up JDK', [
      'sudo ln -sfn /opt/homebrew/opt/openjdk@11/libexec/openjdk.jdk /Library/Java/JavaVirtualMachines/openjdk-11.jdk',
    ]);
  }

  await devSetup.writeEnvVariablesToRcFile([
    { name: 'PATH', value: '"/opt/homebrew/opt/openjdk@11/bin:$PATH"' },
    { name: 'JAVA_HOME', value: '`/usr/libexec/java_home -v 11`' },
  ]);

  const javaHome = await runCliCommand('/usr/libexec/java_home');

  await devSetup.setupAndroidSDK(javaHome.stdout.trim());

  devSetup.onComplete();
}
