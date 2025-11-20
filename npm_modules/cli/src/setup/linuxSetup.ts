import fs from 'fs';
import path from 'path';
import { checkCommandExists } from '../utils/cliUtils';
import { DevSetupHelper, HOME_DIR } from './DevSetupHelper';
import { ANDROID_LINUX_COMMANDLINE_TOOLS } from './versions';

const BAZELISK_URL = 'https://github.com/bazelbuild/bazelisk/releases/download/v1.26.0/bazelisk-linux-amd64';

// Detect which package manager is available
function detectPackageManager(): 'apt' | 'dnf' | 'yum' {
  if (checkCommandExists('apt-get')) {
    return 'apt';
  } else if (checkCommandExists('dnf')) {
    return 'dnf';
  } else if (checkCommandExists('yum')) {
    return 'yum';
  } else {
    throw new Error('No supported package manager found. Please install apt-get, dnf, or yum.');
  }
}

// Get the correct package names for each package manager
function getPackageNames(pm: 'apt' | 'dnf' | 'yum'): string[] {
  const packages = {
    apt: ['zlib1g-dev', 'git-lfs', 'watchman', 'libfontconfig-dev', 'adb'],
    dnf: ['zlib-devel', 'git-lfs', 'watchman', 'fontconfig-devel', 'android-tools'],
    yum: ['zlib-devel', 'git-lfs', 'watchman', 'fontconfig-devel', 'android-tools'],
  };
  return packages[pm];
}

export async function linuxSetup(): Promise<void> {
  const devSetup = new DevSetupHelper();
  const packageManager = detectPackageManager();
  const packages = getPackageNames(packageManager);

  
  // Install dependencies using the detected package manager
  await devSetup.runShell(`Installing dependencies using ${packageManager}`, [
    `sudo ${packageManager} install -y ${packages.join(' ')}`,
  ]);

  // libtinfo5 installation - only for apt-based systems
  if (packageManager === 'apt') {
    await devSetup.runShell('Installing libtinfo5', [
      `wget http://security.ubuntu.com/ubuntu/pool/universe/n/ncurses/libtinfo5_6.3-2ubuntu0.1_amd64.deb`,
      `sudo apt install ./libtinfo5_6.3-2ubuntu0.1_amd64.deb`,
    ]);
  } else {
    // For Fedora/RHEL, ncurses-compat-libs provides libtinfo5
    await devSetup.runShell('Installing ncurses compatibility libraries', [
      `sudo ${packageManager} install -y ncurses-compat-libs`,
    ]);
  }

  // Install Java if not present
  if (!checkCommandExists('java')) {
    const javaPackage = packageManager === 'apt' ? 'default-jre' : 'java-latest-openjdk';
    await devSetup.runShell('Installing Java Runtime Environment', [
      `sudo ${packageManager} install -y ${javaPackage}`,
    ]);
  }

  // Install Bazelisk
  const bazeliskPathSuffix = '.valdi/bin/bazelisk';
  const bazeliskTargetPath = path.join(HOME_DIR, bazeliskPathSuffix);
  await devSetup.downloadToPath(BAZELISK_URL, bazeliskTargetPath);

  // Add executable permission to the downloaded binary
  const stats = fs.statSync(bazeliskTargetPath);
  fs.chmodSync(bazeliskTargetPath, stats.mode | 0o111);

  await devSetup.writeEnvVariablesToRcFile([{ name: 'PATH', value: `"$HOME/.valdi/bin:$PATH"` }]);

  await devSetup.setupAndroidSDK(ANDROID_LINUX_COMMANDLINE_TOOLS);

  devSetup.onComplete();
}