import 'jasmine';
import {
  type LinuxDistroInfo,
  LinuxDistroType,
  buildInstallCommand,
  getCommonPackageMappings,
  getGitLfsRepoSetupCommand,
  getPackageName,
  needsGitLfsRepoSetup,
} from './linuxDistro';

describe('linuxDistro', () => {
  describe('getPackageName', () => {
    it('returns Debian package name for Debian distribution', () => {
      const packageMap = {
        debian: 'openjdk-17-jdk',
        redhat: 'java-17-openjdk-devel',
        unknown: 'openjdk-17',
      };

      const distro: LinuxDistroInfo = {
        type: LinuxDistroType.DEBIAN,
        name: 'Ubuntu',
        packageManager: { name: 'apt', installCommand: 'apt-get install', requiresSudo: true },
      };

      const result = getPackageName(packageMap, distro);

      expect(result).toBe('openjdk-17-jdk');
    });

    it('returns RedHat package name for RedHat distribution', () => {
      const packageMap = {
        debian: 'openjdk-17-jdk',
        redhat: 'java-17-openjdk-devel',
        unknown: 'openjdk-17',
      };

      const distro: LinuxDistroInfo = {
        type: LinuxDistroType.REDHAT,
        name: 'Fedora',
        packageManager: { name: 'dnf', installCommand: 'dnf install', requiresSudo: true },
      };

      const result = getPackageName(packageMap, distro);

      expect(result).toBe('java-17-openjdk-devel');
    });

    it('returns unknown fallback for distributions without specific mapping', () => {
      const packageMap = {
        debian: 'openjdk-17-jdk',
        unknown: 'openjdk-17',
      };

      const distro: LinuxDistroInfo = {
        type: LinuxDistroType.ARCH,
        name: 'Arch Linux',
        packageManager: { name: 'pacman', installCommand: 'pacman -S', requiresSudo: true },
      };

      const result = getPackageName(packageMap, distro);

      expect(result).toBe('openjdk-17');
    });
  });

  describe('buildInstallCommand', () => {
    it('builds install command for Debian', () => {
      const distro: LinuxDistroInfo = {
        type: LinuxDistroType.DEBIAN,
        name: 'Ubuntu',
        packageManager: { name: 'apt', installCommand: 'apt-get install', requiresSudo: true },
      };

      const result = buildInstallCommand(['git', 'npm', 'watchman'], distro);

      expect(result).toBe('sudo apt-get install git npm watchman');
    });

    it('builds install command for RedHat', () => {
      const distro: LinuxDistroInfo = {
        type: LinuxDistroType.REDHAT,
        name: 'Fedora',
        packageManager: { name: 'dnf', installCommand: 'dnf install', requiresSudo: true },
      };

      const result = buildInstallCommand(['git', 'npm'], distro);

      expect(result).toBe('sudo dnf install git npm');
    });

    it('builds install command for Arch', () => {
      const distro: LinuxDistroInfo = {
        type: LinuxDistroType.ARCH,
        name: 'Arch Linux',
        packageManager: { name: 'pacman', installCommand: 'pacman -S', requiresSudo: true },
      };

      const result = buildInstallCommand(['git', 'npm'], distro);

      expect(result).toBe('sudo pacman -S git npm');
    });

    it('returns comment for unknown distribution', () => {
      const distro: LinuxDistroInfo = {
        type: LinuxDistroType.UNKNOWN,
        name: 'Unknown',
        packageManager: { name: 'unknown', installCommand: '', requiresSudo: true },
      };

      const result = buildInstallCommand(['git', 'npm'], distro);

      expect(result).toContain('Unable to determine package manager');
      expect(result).toContain('git, npm');
    });
  });

  describe('needsGitLfsRepoSetup', () => {
    it('returns true for Debian distributions', () => {
      const distro: LinuxDistroInfo = {
        type: LinuxDistroType.DEBIAN,
        name: 'Ubuntu',
        packageManager: { name: 'apt', installCommand: 'apt-get install', requiresSudo: true },
      };

      expect(needsGitLfsRepoSetup(distro)).toBe(true);
    });

    it('returns false for Arch distributions', () => {
      const distro: LinuxDistroInfo = {
        type: LinuxDistroType.ARCH,
        name: 'Arch Linux',
        packageManager: { name: 'pacman', installCommand: 'pacman -S', requiresSudo: true },
      };

      expect(needsGitLfsRepoSetup(distro)).toBe(false);
    });
  });

  describe('getGitLfsRepoSetupCommand', () => {
    it('returns Debian script for Debian distributions', () => {
      const distro: LinuxDistroInfo = {
        type: LinuxDistroType.DEBIAN,
        name: 'Ubuntu',
        packageManager: { name: 'apt', installCommand: 'apt-get install', requiresSudo: true },
      };

      const result = getGitLfsRepoSetupCommand(distro);

      expect(result).toContain('script.deb.sh');
    });

    it('returns RPM script for RedHat distributions', () => {
      const distro: LinuxDistroInfo = {
        type: LinuxDistroType.REDHAT,
        name: 'Fedora',
        packageManager: { name: 'dnf', installCommand: 'dnf install', requiresSudo: true },
      };

      const result = getGitLfsRepoSetupCommand(distro);

      expect(result).toContain('script.rpm.sh');
    });

    it('returns null for distributions with git-lfs in standard repos', () => {
      const distro: LinuxDistroInfo = {
        type: LinuxDistroType.ARCH,
        name: 'Arch Linux',
        packageManager: { name: 'pacman', installCommand: 'pacman -S', requiresSudo: true },
      };

      const result = getGitLfsRepoSetupCommand(distro);

      expect(result).toBeNull();
    });
  });

  describe('getCommonPackageMappings', () => {
    it('returns package mappings for common dependencies', () => {
      const mappings = getCommonPackageMappings();

      expect(mappings['git']).toBeDefined();
      expect(mappings['git-lfs']).toBeDefined();
      expect(mappings['npm']).toBeDefined();
      expect(mappings['openjdk-17']).toBeDefined();
      expect(mappings['watchman']).toBeDefined();
      expect(mappings['adb']).toBeDefined();
      expect(mappings['fontconfig']).toBeDefined();
      expect(mappings['zlib']).toBeDefined();
    });

    it('has platform-specific mappings for Java', () => {
      const mappings = getCommonPackageMappings();
      const javaMappings = mappings['openjdk-17'];

      expect(javaMappings).toBeDefined();
      if (javaMappings) {
        expect(javaMappings.debian).toBe('openjdk-17-jdk');
        expect(javaMappings.redhat).toBe('java-17-openjdk-devel');
        expect(javaMappings.arch).toBe('jdk17-openjdk');
        expect(javaMappings.suse).toBe('java-17-openjdk-devel');
      }
    });

    it('has platform-specific mappings for adb', () => {
      const mappings = getCommonPackageMappings();
      const adbMappings = mappings['adb'];

      expect(adbMappings).toBeDefined();
      if (adbMappings) {
        expect(adbMappings.debian).toBe('adb');
        expect(adbMappings.redhat).toBe('android-tools');
        expect(adbMappings.arch).toBe('android-tools');
        expect(adbMappings.suse).toBe('android-tools');
      }
    });
  });
});
