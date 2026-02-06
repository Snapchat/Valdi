import fs from 'fs';
import { checkCommandExists } from './cliUtils';

/**
 * Detected Linux distribution type
 */
export enum LinuxDistroType {
  DEBIAN = 'debian', // Debian, Ubuntu, Mint, Pop!_OS, etc.
  REDHAT = 'redhat', // RHEL, Fedora, CentOS, Rocky Linux, etc.
  ARCH = 'arch', // Arch Linux, Manjaro, EndeavourOS, etc.
  SUSE = 'suse', // openSUSE, SLES, etc.
  UNKNOWN = 'unknown',
}

/**
 * Package manager information for a Linux distribution
 */
export interface PackageManager {
  /** Name of the package manager */
  name: string;
  /** Command to install packages (without sudo) */
  installCommand: string;
  /** Whether this package manager requires sudo */
  requiresSudo: boolean;
  /** Command to update package database */
  updateCommand?: string;
}

/**
 * Linux distribution information
 */
export interface LinuxDistroInfo {
  /** Distribution type */
  type: LinuxDistroType;
  /** Distribution name (e.g., "Ubuntu", "Fedora") */
  name: string;
  /** Distribution version (if available) */
  version?: string;
  /** Package manager information */
  packageManager: PackageManager;
}

/**
 * Infer distribution keys from the enum, excluding 'unknown'
 */
type Distros = Exclude<`${LinuxDistroType}`, 'unknown'>;

/**
 * Package name mappings for different distributions
 * Keys are automatically inferred from LinuxDistroType enum
 */
export type PackageNameMap = {
  [K in Distros]?: string;
} & {
  unknown: string; // Fallback for unknown distributions
};

/**
 * Detects the current Linux distribution and package manager
 */
export function detectLinuxDistro(): LinuxDistroInfo {
  // Try to read /etc/os-release (most modern distributions)
  if (fs.existsSync('/etc/os-release')) {
    try {
      const osRelease = fs.readFileSync('/etc/os-release', 'utf8');
      const lines = osRelease.split('\n');
      const info: { [key: string]: string } = {};

      for (const line of lines) {
        const match = line.match(/^([^=]+)=(.*)$/);
        if (match && match[1] && match[2]) {
          // Remove quotes from value
          info[match[1]] = match[2].replaceAll(/^["']|["']$/g, '');
        }
      }

      const id = info['ID']?.toLowerCase() ?? '';
      const idLike = info['ID_LIKE']?.toLowerCase() ?? '';
      const name = info['NAME'] ?? 'Linux';
      const version = info['VERSION_ID'];

      // Detect distribution type based on ID and ID_LIKE
      if (id.includes('debian') || id.includes('ubuntu') || idLike.includes('debian') || idLike.includes('ubuntu')) {
        const result: LinuxDistroInfo = {
          type: LinuxDistroType.DEBIAN,
          name,
          packageManager: getPackageManager(LinuxDistroType.DEBIAN),
        };
        if (version) result.version = version;
        return result;
      } else if (
        id.includes('fedora') ||
        id.includes('rhel') ||
        id.includes('centos') ||
        id.includes('rocky') ||
        idLike.includes('fedora') ||
        idLike.includes('rhel')
      ) {
        const result: LinuxDistroInfo = {
          type: LinuxDistroType.REDHAT,
          name,
          packageManager: getPackageManager(LinuxDistroType.REDHAT),
        };
        if (version) result.version = version;
        return result;
      } else if (id.includes('arch') || id.includes('manjaro') || idLike.includes('arch')) {
        const result: LinuxDistroInfo = {
          type: LinuxDistroType.ARCH,
          name,
          packageManager: getPackageManager(LinuxDistroType.ARCH),
        };
        if (version) result.version = version;
        return result;
      } else if (id.includes('suse') || idLike.includes('suse')) {
        const result: LinuxDistroInfo = {
          type: LinuxDistroType.SUSE,
          name,
          packageManager: getPackageManager(LinuxDistroType.SUSE),
        };
        if (version) result.version = version;
        return result;
      }
    } catch {
      // Fall through to legacy detection
    }
  }

  // Legacy detection methods
  if (checkCommandExists('apt-get')) {
    return {
      type: LinuxDistroType.DEBIAN,
      name: 'Debian/Ubuntu',
      packageManager: getPackageManager(LinuxDistroType.DEBIAN),
    };
  } else if (checkCommandExists('dnf')) {
    return {
      type: LinuxDistroType.REDHAT,
      name: 'Fedora/RHEL',
      packageManager: getPackageManager(LinuxDistroType.REDHAT),
    };
  } else if (checkCommandExists('yum')) {
    return {
      type: LinuxDistroType.REDHAT,
      name: 'RHEL/CentOS',
      packageManager: getPackageManager(LinuxDistroType.REDHAT),
    };
  } else if (checkCommandExists('pacman')) {
    return {
      type: LinuxDistroType.ARCH,
      name: 'Arch Linux',
      packageManager: getPackageManager(LinuxDistroType.ARCH),
    };
  } else if (checkCommandExists('zypper')) {
    return {
      type: LinuxDistroType.SUSE,
      name: 'openSUSE/SLES',
      packageManager: getPackageManager(LinuxDistroType.SUSE),
    };
  }

  // Unknown distribution
  return {
    type: LinuxDistroType.UNKNOWN,
    name: 'Unknown Linux',
    packageManager: {
      name: 'unknown',
      installCommand: '',
      requiresSudo: true,
    },
  };
}

/**
 * Gets the package manager for a specific distribution type
 */
function getPackageManager(distroType: LinuxDistroType): PackageManager {
  switch (distroType) {
    case LinuxDistroType.DEBIAN: {
      return {
        name: 'apt',
        installCommand: 'apt-get install',
        requiresSudo: true,
        updateCommand: 'apt-get update',
      };
    }
    case LinuxDistroType.REDHAT: {
      // Prefer dnf over yum
      if (checkCommandExists('dnf')) {
        return {
          name: 'dnf',
          installCommand: 'dnf install',
          requiresSudo: true,
        };
      } else {
        return {
          name: 'yum',
          installCommand: 'yum install',
          requiresSudo: true,
        };
      }
    }
    case LinuxDistroType.ARCH: {
      return {
        name: 'pacman',
        installCommand: 'pacman -S',
        requiresSudo: true,
        updateCommand: 'pacman -Sy',
      };
    }
    case LinuxDistroType.SUSE: {
      return {
        name: 'zypper',
        installCommand: 'zypper install',
        requiresSudo: true,
      };
    }
    default: {
      return {
        name: 'unknown',
        installCommand: '',
        requiresSudo: true,
      };
    }
  }
}

/**
 * Gets the appropriate package name for the current distribution
 * Uses type-safe indexing based on the LinuxDistroType enum
 */
export function getPackageName(packageMap: PackageNameMap, distro = detectLinuxDistro()): string {
  return packageMap[distro.type] ?? packageMap.unknown;
}

/**
 * Builds an install command for packages on the current distribution
 */
export function buildInstallCommand(packages: string[], distro = detectLinuxDistro()): string {
  if (distro.type === LinuxDistroType.UNKNOWN) {
    return `# Unable to determine package manager. Please install manually: ${packages.join(', ')}`;
  }

  const { packageManager } = distro;
  const sudo = packageManager.requiresSudo ? 'sudo ' : '';

  return `${sudo}${packageManager.installCommand} ${packages.join(' ')}`;
}

/**
 * Gets the package name mappings for common dependencies
 */
export function getCommonPackageMappings(): { [key: string]: PackageNameMap } {
  return {
    git: {
      unknown: 'git',
    },
    'git-lfs': {
      unknown: 'git-lfs',
    },
    npm: {
      unknown: 'npm',
    },
    'openjdk-17': {
      debian: 'openjdk-17-jdk',
      redhat: 'java-17-openjdk-devel',
      arch: 'jdk17-openjdk',
      suse: 'java-17-openjdk-devel',
      unknown: 'openjdk-17-jdk',
    },
    watchman: {
      debian: 'watchman',
      redhat: 'watchman', // May need EPEL repository
      arch: 'watchman',
      suse: 'watchman',
      unknown: 'watchman',
    },
    adb: {
      debian: 'adb',
      redhat: 'android-tools',
      arch: 'android-tools',
      suse: 'android-tools',
      unknown: 'adb',
    },
    fontconfig: {
      debian: 'libfontconfig1-dev',
      redhat: 'fontconfig-devel',
      arch: 'fontconfig',
      suse: 'fontconfig-devel',
      unknown: 'fontconfig',
    },
    zlib: {
      debian: 'zlib1g-dev',
      redhat: 'zlib-devel',
      arch: 'zlib',
      suse: 'zlib-devel',
      unknown: 'zlib',
    },
  };
}

/**
 * Checks if git-lfs can be installed directly or needs repository setup
 */
export function needsGitLfsRepoSetup(distro = detectLinuxDistro()): boolean {
  // Only Debian/Ubuntu-based systems need the packagecloud repository setup
  // Other distributions usually have git-lfs in their standard repos
  return distro.type === LinuxDistroType.DEBIAN;
}

/**
 * Gets the shell command to setup git-lfs repository for the current distribution
 */
export function getGitLfsRepoSetupCommand(distro = detectLinuxDistro()): string | null {
  switch (distro.type) {
    case LinuxDistroType.DEBIAN: {
      return 'curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | sudo bash';
    }
    case LinuxDistroType.REDHAT: {
      return 'curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.rpm.sh | sudo bash';
    }
    default: {
      // Most other distributions have git-lfs in their standard repositories
      return null;
    }
  }
}
