import { execSync } from 'child_process';
import fs from 'fs';
import os from 'os';
import path from 'path';
import { ANSI_COLORS } from '../core/constants';
import { checkCommandExists } from '../utils/cliUtils';
import {
  LinuxDistroType,
  buildInstallCommand,
  detectLinuxDistro,
  getCommonPackageMappings,
  getGitLfsRepoSetupCommand,
  getPackageName,
  needsGitLfsRepoSetup,
} from '../utils/linuxDistro';
import { wrapInColor } from '../utils/logUtils';
import { DevSetupHelper, HOME_DIR } from './DevSetupHelper';
import { ANDROID_LINUX_COMMANDLINE_TOOLS } from './versions';

const BAZELISK_URL = 'https://github.com/bazelbuild/bazelisk/releases/download/v1.26.0/bazelisk-linux-amd64';

/**
 * Maps Node.js architecture names to Debian package architecture names.
 * @returns Debian package architecture string (e.g., 'amd64', 'arm64', 'armhf')
 */
function getDebianArchitecture(): string {
  const nodeArch = os.arch();
  
  // Map Node.js arch names to Debian package arch names
  switch (nodeArch) {
    case 'x64': {
      return 'amd64';
    }
    case 'arm64': {
      return 'arm64';
    }
    case 'arm': {
      return 'armhf';
    }
    case 'ia32': {
      return 'i386';
    }
    default: {
      // Fallback to amd64 for unknown architectures
      console.log(
        wrapInColor(
          `Warning: Unknown architecture '${nodeArch}', defaulting to amd64`,
          ANSI_COLORS.YELLOW_COLOR,
        ),
      );
      return 'amd64';
    }
  }
}

export async function linuxSetup(): Promise<void> {
  const devSetup = new DevSetupHelper();

  // Detect Linux distribution
  const distro = detectLinuxDistro();
  console.log(
    wrapInColor(
      `Detected distribution: ${distro.name} (${distro.packageManager.name})`,
      ANSI_COLORS.BLUE_COLOR,
    ),
  );

  if (distro.type === LinuxDistroType.UNKNOWN) {
    console.log();
    console.log(wrapInColor('Unable to detect Linux distribution.', ANSI_COLORS.YELLOW_COLOR));
    console.log(
      wrapInColor(
        'Please manually install the following dependencies and re-run this command:',
        ANSI_COLORS.YELLOW_COLOR,
      ),
    );
    console.log('  - git');
    console.log('  - git-lfs');
    console.log('  - npm (Node.js)');
    console.log('  - openjdk-17-jdk (or equivalent Java 17 JDK)');
    console.log('  - watchman');
    console.log('  - adb (Android Debug Bridge)');
    console.log('  - fontconfig development libraries');
    console.log('  - zlib development libraries');
    console.log();
    console.log(
      wrapInColor(
        'For manual installation instructions, see: https://github.com/Snapchat/Valdi/blob/main/docs/INSTALL.md',
        ANSI_COLORS.BLUE_COLOR,
      ),
    );
    throw new Error('Unable to proceed with automatic setup on unknown Linux distribution');
  }

  const packageMappings = getCommonPackageMappings();

  // Setup git-lfs repository if needed (only for Debian/Ubuntu and RHEL-based)
  const repoSetupCommand = getGitLfsRepoSetupCommand(distro);
  if (repoSetupCommand && needsGitLfsRepoSetup(distro)) {
    try {
      await devSetup.runShell('Setting up git-lfs repository', [repoSetupCommand]);
    } catch {
      console.log(
        wrapInColor(
          `Warning: git-lfs repository setup failed. Will try to install git-lfs from standard repositories.`,
          ANSI_COLORS.YELLOW_COLOR,
        ),
      );
    }
  }

  // Build list of packages to install
  const packagesToInstall: string[] = [];

  // Always needed packages
  if (!checkCommandExists('npm')) {
    packagesToInstall.push(getPackageName(packageMappings['npm']!, distro));
  }
  if (!checkCommandExists('java')) {
    packagesToInstall.push(getPackageName(packageMappings['openjdk-17']!, distro));
  }
  if (!checkCommandExists('git-lfs')) {
    packagesToInstall.push(getPackageName(packageMappings['git-lfs']!, distro));
  }
  if (!checkCommandExists('watchman')) {
    packagesToInstall.push(getPackageName(packageMappings['watchman']!, distro));
  }
  if (!checkCommandExists('adb')) {
    packagesToInstall.push(getPackageName(packageMappings['adb']!, distro));
  }

  // Development libraries
  packagesToInstall.push(
    getPackageName(packageMappings['fontconfig']!, distro),
    getPackageName(packageMappings['zlib']!, distro),
  );

  // Install all packages
  if (packagesToInstall.length > 0) {
    const installCommand = buildInstallCommand(packagesToInstall, distro);
    
    try {
      await devSetup.runShell(`Installing dependencies via ${distro.packageManager.name}`, [installCommand]);
    } catch {
      console.log();
      console.log(wrapInColor('Some packages failed to install.', ANSI_COLORS.YELLOW_COLOR));
      console.log(wrapInColor('Common issues:', ANSI_COLORS.YELLOW_COLOR));
      
      if (distro.type === LinuxDistroType.REDHAT) {
        console.log('  - watchman may require EPEL repository on RHEL-based systems');
        console.log('    Run: sudo dnf install epel-release (or sudo yum install epel-release)');
      } else if (distro.type === LinuxDistroType.ARCH) {
        console.log('  - Some packages may be in the AUR');
        console.log('    Consider using an AUR helper like yay or paru');
      }
      
      console.log();
      console.log(wrapInColor('Continuing with setup...', ANSI_COLORS.YELLOW_COLOR));
    }
  } else {
    console.log(wrapInColor('All required packages are already installed.', ANSI_COLORS.GREEN_COLOR));
  }

  await devSetup.setupGitLfs();

  await devSetup.setupShellAutoComplete();

  // Install libtinfo5 for Debian-based systems (needed for some Android tools)
  if (distro.type === LinuxDistroType.DEBIAN) {
    const libtinfoInstalled = checkCommandExists('dpkg') && 
      (() => {
        try {
          execSync('dpkg -l libtinfo5', { stdio: 'ignore' });
          return true;
        } catch {
          return false;
        }
      })();

    if (!libtinfoInstalled) {
      try {
        const arch = getDebianArchitecture();
        const version = '6.3-2ubuntu0.1';
        const packageName = `libtinfo5_${version}_${arch}.deb`;
        const packageUrl = `https://security.ubuntu.com/ubuntu/pool/universe/n/ncurses/${packageName}`;
        const tempPath = path.join(HOME_DIR, `.valdi/tmp/${packageName}`);
        
        // Ensure temp directory exists
        const tempDir = path.dirname(tempPath);
        if (!fs.existsSync(tempDir)) {
          fs.mkdirSync(tempDir, { recursive: true });
        }
        
        console.log(
          wrapInColor(
            `Downloading libtinfo5 for ${arch} architecture...`,
            ANSI_COLORS.BLUE_COLOR,
          ),
        );
        
        // Download using DevSetupHelper (more robust than wget)
        await devSetup.downloadToPath(packageUrl, tempPath);
        
        // Install the package
        await devSetup.runShell('Installing libtinfo5 (required for Android tools)', [
          `sudo apt install -y ${tempPath}`,
        ]);
        
        // Clean up
        fs.unlinkSync(tempPath);
      } catch (error) {
        console.log(
          wrapInColor(
            'Warning: libtinfo5 installation failed. Some Android command-line tools may not work.',
            ANSI_COLORS.YELLOW_COLOR,
          ),
        );
        console.log(
          wrapInColor(
            `Details: ${error instanceof Error ? error.message : 'Unknown error'}`,
            ANSI_COLORS.YELLOW_COLOR,
          ),
        );
        console.log(wrapInColor('You may need to install it manually if you encounter issues.', ANSI_COLORS.YELLOW_COLOR));
      }
    }
  } else if (distro.type === LinuxDistroType.REDHAT) {
    // For RHEL-based systems, try to install ncurses-compat-libs if available
    try {
      await devSetup.runShell('Installing ncurses compatibility libraries (for Android tools)', [
        buildInstallCommand(['ncurses-compat-libs'], distro),
      ]);
    } catch {
      console.log(
        wrapInColor(
          'Warning: ncurses-compat-libs installation failed. Some Android command-line tools may not work.',
          ANSI_COLORS.YELLOW_COLOR,
        ),
      );
    }
  }

  const bazeliskPathSuffix = '.valdi/bin/bazelisk';
  const bazeliskTargetPath = path.join(HOME_DIR, bazeliskPathSuffix);
  
  if (fs.existsSync(bazeliskTargetPath)) {
    console.log(wrapInColor('Bazelisk already installed.', ANSI_COLORS.GREEN_COLOR));
  } else {
    await devSetup.downloadToPath(BAZELISK_URL, bazeliskTargetPath);

    // Add executable permission to the downloaded binary
    const stats = fs.statSync(bazeliskTargetPath);
    fs.chmodSync(bazeliskTargetPath, stats.mode | 0o111);
  }

  await devSetup.writeEnvVariablesToRcFile([{ name: 'PATH', value: `"$HOME/.valdi/bin:$PATH"` }]);

  await devSetup.setupAndroidSDK(ANDROID_LINUX_COMMANDLINE_TOOLS);

  devSetup.onComplete();
}
