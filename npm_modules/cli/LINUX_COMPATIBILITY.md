# Linux Distribution Compatibility

This document describes how the Valdi CLI handles different Linux distributions.

## Overview

The Valdi CLI (`valdi doctor` and `valdi dev_setup` commands) now supports multiple Linux distributions beyond just Debian/Ubuntu. The CLI automatically detects your distribution and uses the appropriate package manager and package names.

## Supported Distributions

### Tier 1 Support (Fully Tested)
- **Debian-based**: Debian, Ubuntu, Linux Mint, Pop!_OS, etc.
  - Package manager: `apt`
  - Tested on: Ubuntu 22.04, Debian 12

### Tier 2 Support (Community Tested)
- **Red Hat-based**: Fedora, RHEL, CentOS, Rocky Linux, AlmaLinux
  - Package manager: `dnf` or `yum`
  - Notes: Some packages (like watchman) may require EPEL repository

- **Arch-based**: Arch Linux, Manjaro, EndeavourOS
  - Package manager: `pacman`
  - Notes: Some packages may be in AUR

- **SUSE-based**: openSUSE, SLES
  - Package manager: `zypper`

## How Distribution Detection Works

The CLI detects your distribution using the following methods (in order):

1. **`/etc/os-release`**: Reads the standard Linux distribution information file
   - Uses `ID` and `ID_LIKE` fields to determine distribution family
   - Extracts distribution name and version

2. **Package Manager Detection**: Falls back to detecting available package managers
   - Checks for `apt-get`, `dnf`, `yum`, `pacman`, or `zypper`

3. **Unknown Distributions**: If detection fails, provides manual installation instructions

## Package Mappings

Different distributions use different package names for the same software. The CLI includes mappings for common dependencies:

| Dependency | Debian/Ubuntu | Red Hat/Fedora | Arch Linux | openSUSE |
|------------|---------------|----------------|------------|----------|
| Java 17 JDK | `openjdk-17-jdk` | `java-17-openjdk-devel` | `jdk17-openjdk` | `java-17-openjdk-devel` |
| Android Debug Bridge | `adb` | `android-tools` | `android-tools` | `android-tools` |
| Fontconfig Dev | `libfontconfig1-dev` | `fontconfig-devel` | `fontconfig` | `fontconfig-devel` |
| Zlib Dev | `zlib1g-dev` | `zlib-devel` | `zlib` | `zlib-devel` |
| Git LFS | `git-lfs` | `git-lfs` | `git-lfs` | `git-lfs` |
| Watchman | `watchman` | `watchman` | `watchman` | `watchman` |

## Commands

### `valdi dev_setup`

The `dev_setup` command automatically installs required dependencies for your distribution:

```bash
valdi dev_setup
```

**What it does:**
1. Detects your Linux distribution
2. Installs the following dependencies:
   - Node.js/npm (if not installed)
   - Java 17 JDK
   - Git LFS
   - Watchman (for hot reload)
   - Android Debug Bridge (adb)
   - Development libraries (fontconfig, zlib)
   - Additional distribution-specific packages
3. Downloads and installs Bazelisk
4. Sets up Android SDK with command-line tools
5. Configures environment variables in your shell RC file

**Distribution-specific behavior:**

- **Debian/Ubuntu**: 
  - Sets up packagecloud repository for git-lfs
  - Installs `libtinfo5` for Android command-line tools compatibility

- **Red Hat/Fedora**:
  - Sets up packagecloud RPM repository for git-lfs
  - Installs `ncurses-compat-libs` for Android tools
  - Shows warning if EPEL repository is needed for watchman

- **Arch Linux**:
  - Uses standard repositories (most packages available)
  - May require AUR helper for some packages

- **openSUSE**:
  - Uses zypper to install packages

### `valdi doctor`

The `doctor` command checks your development environment and provides distribution-specific fix commands:

```bash
valdi doctor
```

**Features:**
- Detects your distribution automatically
- Provides correct package manager commands for your distribution
- Shows distribution-specific warnings (e.g., EPEL repository for RHEL)
- Validates all required tools are installed

**Example output on Fedora:**

```
Running Valdi environment diagnostics...
Detected distribution: Fedora Linux (dnf)

✓ Node.js installation
✗ Java installation
  • Java not found in PATH
  Fix: sudo dnf install java-17-openjdk-devel
```

## Troubleshooting

### Watchman on Red Hat/Fedora

Watchman may not be available in the default repositories. Enable EPEL:

```bash
# Fedora
sudo dnf install epel-release

# RHEL/CentOS
sudo yum install epel-release
```

Then install watchman:

```bash
sudo dnf install watchman
# or
sudo yum install watchman
```

### Packages in AUR (Arch Linux)

Some packages may only be available in the AUR. Use an AUR helper like `yay` or `paru`:

```bash
yay -S watchman
```

### Unknown Distribution

If your distribution is not automatically detected, you'll see:

```
Unable to detect Linux distribution.
Please manually install the following dependencies and re-run this command:
  - git
  - git-lfs
  - npm (Node.js)
  - openjdk-17-jdk (or equivalent Java 17 JDK)
  - watchman
  - adb (Android Debug Bridge)
  - fontconfig development libraries
  - zlib development libraries
```

Consult your distribution's package manager documentation for the correct package names.

### Distribution Detection Issues

If distribution detection fails or provides incorrect results:

1. Check that `/etc/os-release` exists and is readable:
   ```bash
   cat /etc/os-release
   ```

2. Verify your package manager is in PATH:
   ```bash
   which apt-get  # or dnf, yum, pacman, zypper
   ```

3. Report the issue with your distribution details:
   - Distribution name and version
   - Output of `cat /etc/os-release`
   - Available package manager

## Contributing

To add support for a new distribution:

1. Add package mappings in `src/utils/linuxDistro.ts`:
   ```typescript
   // In getCommonPackageMappings()
   'your-package': {
     debian: 'debian-package-name',
     redhat: 'redhat-package-name',
     arch: 'arch-package-name',
     suse: 'suse-package-name',
     fallback: 'generic-package-name',
   }
   ```

2. Add distribution detection logic in `detectLinuxDistro()` if needed

3. Test on the target distribution:
   ```bash
   valdi dev_setup
   valdi doctor
   ```

4. Update this documentation with your findings

## Technical Details

### Files Modified
- `src/utils/linuxDistro.ts` - Distribution detection and package mapping utilities
- `src/setup/linuxSetup.ts` - Linux-specific dev_setup implementation
- `src/commands/doctor.ts` - Environment diagnostics with distribution-aware fix commands

### Distribution Detection API

```typescript
import { detectLinuxDistro, buildInstallCommand, getPackageName } from './utils/linuxDistro';

// Detect distribution
const distro = detectLinuxDistro();
console.log(distro.name); // e.g., "Ubuntu"
console.log(distro.type); // e.g., LinuxDistroType.DEBIAN
console.log(distro.packageManager.name); // e.g., "apt"

// Build install command
const cmd = buildInstallCommand(['git', 'npm', 'watchman'], distro);
console.log(cmd); // e.g., "sudo apt-get install git npm watchman"

// Get package name for current distribution
const packageMappings = getCommonPackageMappings();
const javaPackage = getPackageName(packageMappings['openjdk-17'], distro);
console.log(javaPackage); // e.g., "openjdk-17-jdk" on Ubuntu
```

## See Also

- [Valdi Installation Guide](https://github.com/Snapchat/Valdi/blob/main/docs/INSTALL.md)
- [Development Setup Guide](https://github.com/Snapchat/Valdi/blob/main/docs/DEV_SETUP.md)
- [Contributing Guidelines](https://github.com/Snapchat/Valdi/blob/main/CONTRIBUTING.md)
