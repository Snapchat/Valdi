import fs from 'fs';
import { CliError } from '../core/errors';
import { checkCommandExists, runCliCommand } from '../utils/cliUtils';
import { DevSetupHelper } from './DevSetupHelper';
import { ANDROID_MACOS_COMMANDLINE_TOOLS } from './versions';

export async function macOSSetup(): Promise<void> {
  const devSetup = new DevSetupHelper();

  // Check if xcode-select command exists
  if (!checkCommandExists('xcode-select')) {
    throw new CliError(
      'Xcode command line tools are not installed.\n\n' +
        'Please install Xcode from the App Store, then run:\n' +
        '  sudo xcode-select -s /Applications/Xcode.app\n\n' +
        'After installation, run valdi dev_setup again.',
    );
  }

  // Verify Xcode is properly configured
  try {
    const xcodePathResult = await runCliCommand('xcode-select -p');
    const xcodePath = xcodePathResult.stdout.trim();

    if (!xcodePath || !fs.existsSync(xcodePath)) {
      throw new CliError(
        'Xcode command line tools path is not configured correctly.\n\n' +
          'Please run:\n' +
          '  sudo xcode-select -s /Applications/Xcode.app\n\n' +
          'Or if you have Xcode with a different name (e.g., beta versions):\n' +
          '  sudo xcode-select -s /Applications/YourXcode.app\n\n' +
          'After configuration, run valdi dev_setup again.',
      );
    }

    // Extract Xcode.app path from the Developer path
    const xcodeAppPath = xcodePath.replace(/\/Contents\/Developer\/?$/, '');
    
    // Verify the Xcode app exists
    if (!fs.existsSync(xcodeAppPath) || !xcodeAppPath.includes('Xcode')) {
      throw new CliError(
        'Xcode installation not found.\n\n' +
          `xcode-select points to ${xcodePath}\n` +
          `but Xcode app not found at ${xcodeAppPath}\n\n` +
          'Please install Xcode from the App Store:\n' +
          '  https://apps.apple.com/us/app/xcode/id497799835\n\n' +
          'Then configure it with:\n' +
          '  sudo xcode-select -s /Applications/Xcode.app\n\n' +
          'After installation, run valdi dev_setup again.',
      );
    }
  } catch (error) {
    if (error instanceof CliError) {
      throw error;
    }
    throw new CliError(
      'Failed to verify Xcode installation.\n\n' +
        'Please ensure Xcode is installed from the App Store and properly configured:\n' +
        '  sudo xcode-select -s /Applications/Xcode.app\n\n' +
        'After setup, run valdi dev_setup again.',
    );
  }

  if (!checkCommandExists('brew')) {
    await devSetup.runShell('Homebrew not found. Installing', [
      '/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"',
    ]);

    // Configure Homebrew PATH in .zprofile
    const homeDir = process.env['HOME'] ?? '';
    const zprofilePath = `${homeDir}/.zprofile`;
    const brewShellenvLine = 'eval "$(/opt/homebrew/bin/brew shellenv)"';
    
    try {
      let zprofileContent = '';
      if (fs.existsSync(zprofilePath)) {
        zprofileContent = fs.readFileSync(zprofilePath, 'utf8');
      }
      
      if (!zprofileContent.includes(brewShellenvLine)) {
        const newContent = zprofileContent + '\n' + brewShellenvLine + '\n';
        fs.writeFileSync(zprofilePath, newContent, 'utf8');
        console.log('Added Homebrew to PATH in .zprofile');
      }
    } catch {
      console.log('Note: Could not automatically configure Homebrew PATH in .zprofile');
    }
  }

  // Install dependencies, skipping already installed ones
  const packages = ['npm', 'bazelisk', 'openjdk@17', 'temurin', 'git-lfs', 'watchman', 'ios-webkit-debug-proxy'];
  const packagesToInstall: string[] = [];
  
  for (const pkg of packages) {
    try {
      // Check if package is already installed (from any tap)
      const result = await runCliCommand(`brew list ${pkg}`);
      if (result.returnCode !== 0) {
        packagesToInstall.push(pkg);
      }
    } catch {
      // Package not installed, add to list
      packagesToInstall.push(pkg);
    }
  }
  
  if (packagesToInstall.length > 0) {
    await devSetup.runShell(
      `Installing dependencies from Homebrew (${packagesToInstall.length}/${packages.length})`,
      [`brew install ${packagesToInstall.join(' ')}`]
    );
  } else {
    console.log('All Homebrew dependencies are already installed');
  }

  await devSetup.setupGitLfs();

  await devSetup.setupShellAutoComplete();

  // Check if JDK symlink exists and points to the correct location
  const jdkSymlinkPath = '/Library/Java/JavaVirtualMachines/openjdk-17.jdk';
  const jdkSourcePath = '/opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk';
  
  let needsJdkSymlink = false;
  if (fs.existsSync(jdkSymlinkPath)) {
    try {
      const linkTarget = fs.readlinkSync(jdkSymlinkPath);
      if (linkTarget !== jdkSourcePath) {
        needsJdkSymlink = true;
      }
    } catch {
      // Not a symlink or can't read it, needs to be set up
      needsJdkSymlink = true;
    }
  } else {
    needsJdkSymlink = true;
  }
  
  if (needsJdkSymlink) {
    console.log('\nSetting up JDK symlink (requires admin password)...');
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
