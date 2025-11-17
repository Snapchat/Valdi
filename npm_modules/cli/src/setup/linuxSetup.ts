import fs from 'fs';
import path from 'path';
import { checkCommandExists } from '../utils/cliUtils';
import { DevSetupHelper, HOME_DIR } from './DevSetupHelper';
import { ANDROID_LINUX_COMMANDLINE_TOOLS } from './versions';
import { wrapInColor } from '../utils/logUtils';
import { ANSI_COLORS } from '../core/constants';

const BAZELISK_URL = 'https://github.com/bazelbuild/bazelisk/releases/download/v1.26.0/bazelisk-linux-amd64';

export async function getLinuxPackageManager(): Promise<string | null> {
  let pm: string | null = null;
  const distro = await fs.promises.readFile('/etc/os-release', 'utf8');
  const match = distro.match(/^ID="?([^"\n]*)"?$/m);

  const distroToPackageManager: { [key: string]: string } = {
    ubuntu: "apt",
    fedora: "dnf",
    debian: "apt",
    arch: "pacman",
    linuxmint: "apt",
    opensuse: "zypper",
    rhel: "dnf",
    centos: "dnf",
    manjaro: "pacman",
  };

  if (match && match[1]) {
    return distroToPackageManager[match[1]] || null;
  } else {
    if (checkCommandExists('pacman')) {
      pm = 'pacman';
    } else if (checkCommandExists('dnf')) {
      pm = 'dnf';
    } else if (checkCommandExists('yum')) {
      pm = 'yum';
    } else if (checkCommandExists('apt')) {
      pm = 'apt';
    }
  }

  return pm;
}

export async function linuxSetup(): Promise<void> {
  const devSetup = new DevSetupHelper();
  const pm = await getLinuxPackageManager();

  if (pm === "apt") {
    await devSetup.runShell('Installing dependencies from apt', [
      `sudo apt-get install zlib1g-dev git-lfs watchman libfontconfig-dev adb`,
    ]);

    await devSetup.runShell('Installing libtinfo5', [
      `wget https://security.ubuntu.com/ubuntu/pool/universe/n/ncurses/libtinfo5_6.3-2ubuntu0.1_amd64.deb`,
      `sudo apt install ./libtinfo5_6.3-2ubuntu0.1_amd64.deb`,
    ]);

    if (!checkCommandExists('java')) {
      await devSetup.runShell('Installing Java Runtime Environment', ['sudo apt install default-jre']);
    }
  } else {
    console.log(wrapInColor('Unsupported package manager, please install the required dependencies manually:', ANSI_COLORS.RED_COLOR));
    console.log(wrapInColor('zlib git-lfs watchman libfontconfig-dev adb libtinfo5 openjdk-17', ANSI_COLORS.RED_COLOR));
    console.log()
  }

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
