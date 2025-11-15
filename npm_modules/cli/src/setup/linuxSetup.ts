import fs from 'fs';
import path from 'path';
import { checkCommandExists, spawnCliCommand } from '../utils/cliUtils';
import { DevSetupHelper, HOME_DIR } from './DevSetupHelper';
import { ANDROID_LINUX_COMMANDLINE_TOOLS } from './versions';

const BAZELISK_URL = 'https://github.com/bazelbuild/bazelisk/releases/download/v1.26.0/bazelisk-linux-amd64';
const WATCHMAN_VERSION = 'v2025.07.21.00';
const WATCHMAN_URL = `https://github.com/facebook/watchman/releases/download/${WATCHMAN_VERSION}/watchman-${WATCHMAN_VERSION}-linux.zip`;

type PackageManager = 'apt' | 'dnf' | 'pacman' | 'zypper' | 'apk' | 'unknown';

export async function linuxSetup(): Promise<void> {
  const devSetup = new DevSetupHelper();
  const packageManager = await getPackageManager();

  switch (packageManager) {
    case 'apt': {
      await aptSetup(devSetup);
      break;
    }
    case 'dnf': {
      await dnfSetup(devSetup);
      break;
    }
    case 'pacman': {
      await pacmanSetup(devSetup);
      break;
    }
    case 'zypper': {
      await zypperSetup(devSetup);
      break;
    }
    case 'apk': {
      await apkSetup(devSetup);
      break;
    }
    case 'unknown': {
      console.log('Unknown package manager');
      break;
    }
  }

  const binDir = path.join(HOME_DIR, '.valdi/bin');
  const libDir = path.join(HOME_DIR, '.valdi/lib');
  const tmpDir = path.join(HOME_DIR, '.valdi/tmp');

  if (!fs.existsSync(binDir)) {
    fs.mkdirSync(binDir, { recursive: true });
  }
  if (!fs.existsSync(libDir)) {
    fs.mkdirSync(libDir, { recursive: true });
  }
  if (!fs.existsSync(tmpDir)) {
    fs.mkdirSync(tmpDir, { recursive: true });
  }

  const bazeliskTargetPath = path.join(binDir, 'bazelisk');
  await devSetup.downloadToPath(BAZELISK_URL, bazeliskTargetPath);

  // Add executable permission to the downloaded binary
  const stats = fs.statSync(bazeliskTargetPath);
  fs.chmodSync(bazeliskTargetPath, stats.mode | 0o111);

  await setupWatchman(devSetup, binDir, libDir, tmpDir);

  await devSetup.writeEnvVariablesToRcFile([
    { name: 'PATH', value: `"$HOME/.valdi/bin:$PATH"` },
    { name: 'LD_LIBRARY_PATH', value: `"$HOME/.valdi/lib:$LD_LIBRARY_PATH"` },
  ]);

  await devSetup.setupAndroidSDK(ANDROID_LINUX_COMMANDLINE_TOOLS);
  devSetup.onComplete();
}

async function setupWatchman(devSetup: DevSetupHelper, binDir: string, libDir: string, tmpDir: string): Promise<void> {
  if (checkCommandExists('watchman')) {
    return;
  }

  const watchmanZip = path.join(tmpDir, `watchman-${WATCHMAN_VERSION}-linux.zip`);
  const watchmanExtractDir = path.join(tmpDir, `watchman-${WATCHMAN_VERSION}-linux`);

  await devSetup.downloadToPath(WATCHMAN_URL, watchmanZip);

  await devSetup.runShell('Installing Watchman', [
    `unzip -q -o "${watchmanZip}" -d "${tmpDir}"`,
    `cp "${watchmanExtractDir}/bin/watchman" "${binDir}/"`,
    `cp "${watchmanExtractDir}/bin/watchmanctl" "${binDir}/"`,
    `cp "${watchmanExtractDir}/lib/"*.so* "${libDir}/"`,
    `chmod +x "${binDir}/watchman"`,
    `chmod +x "${binDir}/watchmanctl"`,
    `rm -f "${watchmanZip}"`,
    `rm -rf "${watchmanExtractDir}"`,
  ]);
}

async function aptSetup(devSetup: DevSetupHelper): Promise<void> {
  await devSetup.runShell('Installing dependencies from apt', [
    `sudo apt-get install -y zlib1g-dev git-lfs libfontconfig-dev adb unzip`,
  ]);
  await devSetup.runShell('Installing libtinfo5', [
    `wget http://security.ubuntu.com/ubuntu/pool/universe/n/ncurses/libtinfo5_6.3-2ubuntu0.1_amd64.deb`,
    `sudo apt install ./libtinfo5_6.3-2ubuntu0.1_amd64.deb`,
    `rm -f libtinfo5_6.3-2ubuntu0.1_amd64.deb`,
  ]);
  if (!checkCommandExists('java')) {
    await devSetup.runShell('Installing Java Runtime Environment', ['sudo apt install -y default-jre']);
  }
}

async function dnfSetup(devSetup: DevSetupHelper): Promise<void> {
  await devSetup.runShell('Installing dependencies from dnf', [
    `sudo dnf install -y zlib-devel git-lfs fontconfig-devel android-tools unzip`,
  ]);
  if (!checkCommandExists('java')) {
    await devSetup.runShell('Installing Java Runtime Environment', ['sudo dnf install -y java-latest-openjdk']);
  }
}

async function pacmanSetup(devSetup: DevSetupHelper): Promise<void> {
  await devSetup.runShell('Installing dependencies from pacman', [
    `sudo pacman -S --noconfirm zlib git-lfs fontconfig android-tools unzip`,
  ]);
  if (!checkCommandExists('java')) {
    await devSetup.runShell('Installing Java Runtime Environment', ['sudo pacman -S --noconfirm jre-openjdk']);
  }
}

async function zypperSetup(devSetup: DevSetupHelper): Promise<void> {
  await devSetup.runShell('Installing dependencies from zypper', [
    `sudo zypper install -y zlib-devel git-lfs fontconfig-devel android-tools unzip`,
  ]);
  if (!checkCommandExists('java')) {
    await devSetup.runShell('Installing Java Runtime Environment', ['sudo zypper install -y java-openjdk']);
  }
}

async function apkSetup(devSetup: DevSetupHelper): Promise<void> {
  await devSetup.runShell('Installing dependencies from apk', [
    `sudo apk add zlib-dev git-lfs fontconfig-dev android-tools unzip`,
  ]);
  if (!checkCommandExists('java')) {
    await devSetup.runShell('Installing Java Runtime Environment', ['sudo apk add openjdk17-jre']);
  }
}

async function getPackageManager(): Promise<PackageManager> {
  try {
    const result = await spawnCliCommand(
      "grep '^ID=' /etc/os-release | cut -d'=' -f2 | tr -d '\"'",
      undefined,
      'pipe',
      false,
      false,
    );
    const distro = result.stdout.trim().toLowerCase();

    switch (distro) {
      case 'ubuntu':
      case 'debian':
      case 'linuxmint':
      case 'pop':
      case 'elementary':
      case 'zorin':
      case 'kali': {
        return 'apt';
      }
      case 'fedora':
      case 'centos':
      case 'rhel':
      case 'rocky':
      case 'almalinux': {
        return 'dnf';
      }
      case 'arch':
      case 'manjaro':
      case 'endeavouros':
      case 'garuda':
      case 'artix': {
        return 'pacman';
      }
      case 'opensuse':
      case 'opensuse-leap':
      case 'opensuse-tumbleweed':
      case 'suse':
      case 'sles': {
        return 'zypper';
      }
      case 'alpine': {
        return 'apk';
      }
      default: {
        return 'unknown';
      }
    }
  } catch (error) {
    console.error('Error detecting distribution:', error);
    return 'unknown';
  }
}
