# Getting Started with Valdi

> **Install the Valdi CLI from npm.** This guide assumes you install the CLI with `npm install -g @snap/valdi`. If you are contributing to Valdi itself, see [DEV_SETUP.md](DEV_SETUP.md) instead.

This guide will help you set up your development environment and get started with your first project.

## Prerequisites

### All Platforms
**Install [Node.js](https://nodejs.org/) 18 or later** (22 recommended; it's what Valdi's CI uses). You need `npm` to install the Valdi CLI. On macOS you can use `brew install node`.

### macOS
**Install Xcode 26.5 or 27** (from [Apple Developer downloads](https://developer.apple.com/download/all/) or the [App Store](https://apps.apple.com/us/app/xcode/id497799835)). It's required for iOS development.

> [!IMPORTANT]
> **Supported toolchain:** Valdi is tested with **Xcode 26.5** on **macOS 26** and **Xcode 27** on **macOS 27**. A project created with an older Valdi release needs to update Valdi to build with Xcode 27 (see [Troubleshooting](./TROUBLESHOOTING.md#xcode-27-libcxx)). If you have several Xcode versions installed, see [Troubleshooting](./TROUBLESHOOTING.md#bazel-uses-the-wrong-xcode-version) to make sure Bazel picks the one you want.

### Linux
No other prerequisites. The Valdi CLI installs everything else.

### What `valdi dev_setup` installs
- **macOS:** Homebrew (if missing), then `npm`, `bazelisk`, `openjdk@17`, `temurin`, `watchman`, `ios-webkit-debug-proxy` and `android-platform-tools` via Homebrew
- **Linux:** Bazelisk (into `~/.valdi/bin`), Java JDK 17, Watchman, adb and build libraries (e.g. zlib/fontconfig dev packages) via your distro's package manager
- **Both:** shell autocomplete

You don't need to install the Android SDK or NDK yourself: Bazel downloads them during the build.

> [!WARNING]
> `valdi dev_setup` changes your machine: it asks for `sudo` (for package installs and, on macOS, to symlink the JDK into `/Library/Java/JavaVirtualMachines`) and appends `PATH`, `JAVA_HOME` and Homebrew lines to your shell profile (`~/.zprofile` and `~/.zshrc`, or `~/.bashrc`). To do these steps by hand instead, follow the [macOS](./setup/macos_setup.md) or [Linux](./setup/linux_setup.md) reference guides.

## Installation

### 1. Install the Valdi CLI

```bash
npm install -g @snap/valdi
```

### 2. Set Up Your Development Environment

```bash
# Set up your development environment (installs all dependencies)
valdi dev_setup

# Verify everything is working
valdi doctor
```

> [!NOTE]
> The first time you run `valdi dev_setup`, it will download and install several gigabytes of dependencies. This may take 10-20 minutes depending on your internet connection.

> [!TIP]
> **Contributing to Valdi?** If you're developing Valdi itself (not just using it), clone the repository and install from source:
> ```bash
> git clone git@github.com:Snapchat/Valdi.git
> cd Valdi/npm_modules/cli/
> npm run cli:install
> ```

## Creating Your First Project

The best way to start a new project is to bootstrap it using the Valdi CLI. The bootstrap command will create all of the necessary directories, source, and configuration files.

### 1. Bootstrap a New Project

```bash
# Create and enter your project directory
mkdir my_project
cd my_project

# Initialize a new Valdi project
valdi bootstrap
```

This will create all necessary files for a new Valdi project in your current directory.

### 2. Run Your Project

Choose your target platform and install dependencies:

```bash
# For iOS
valdi install ios

# For Android
valdi install android
```

> [!NOTE]
> The first build may take several minutes while Bazel downloads and builds dependencies.

### Versions

The CLI and the framework are versioned separately. The CLI is the `@snap/valdi` npm package (e.g. `1.2.0`). The framework is released as tags on GitHub (e.g. `beta-0.2.0`). `valdi bootstrap` pins your project to the latest framework release; pass `--valdiVersion=<tag>` (or `--valdiVersion=main` for the development branch) to choose a different one. See the [releases page](https://github.com/Snapchat/Valdi/releases) for available tags.

### 3. Enable Hot Reloading

Once your app is running in a simulator or emulator, start the hot reloader to see your changes in real-time:

```bash
valdi hotreload
```

## VSCode/Cursor Setup (Optional but Recommended)

Valdi apps are standard TypeScript/TSX projects, so no Valdi-specific editor extensions are required — syntax highlighting and type checking come from the workspace TypeScript version (configured below). Device logs and debugging are handled by the CLI and standard tooling:

- **Device logs:** `valdi log`
- **Debugging:** the JavaScript runtime is debuggable over the Chrome DevTools Protocol; no custom debugger extension is needed. CDP debugging requires the Hermes engine, which isn't the default. See the [Hermes Debugger guide](./docs/workflow-hermes-debugger.md) to switch to it and attach the debugger.

### 1. Install VSCode or Cursor

- **VSCode**: Download from [code.visualstudio.com](https://code.visualstudio.com/download)
- **Cursor**: Download from [cursor.com](https://cursor.com)

### 2. Add Shell Command to PATH

For **VSCode**:
- Launch VSCode
- Open Command Palette (Cmd+Shift+P or Ctrl+Shift+P)
- Type `shell command` and select `> Install 'code' command in PATH`
- Restart your terminal

For **Cursor**:
- Launch Cursor
- Open Command Palette (Cmd+Shift+P or Ctrl+Shift+P)
- Type `shell command` and select `> Install 'cursor' command in PATH`
- Restart your terminal

### 3. Configure TypeScript

After creating your first Valdi project:
- Open any TypeScript file (.tsx) in your project
- Press `Cmd+Shift+P` (or Ctrl+Shift+P)
- Select "TypeScript: Select TypeScript Version..."
- Choose `Use Workspace Version`

> [!IMPORTANT]
> Selecting the workspace TypeScript version is crucial for proper development and cannot be automated.

## Project Synchronization

When you make changes to any of the following:

- Dependencies
- Localization files
- Resource files

Run this command to update your project configuration:

```bash
valdi projectsync
```

## Troubleshooting

If you encounter any issues during setup:

1. **Run diagnostics:**
   ```bash
   valdi doctor
   ```
   This will check your environment and provide specific fix suggestions.

2. **Check prerequisites:**
   - **macOS:** Ensure Xcode 26.5 or 27 is installed and selected (`sudo xcode-select -s /Applications/Xcode.app`, adjusting the path to wherever it's installed)
   - **All platforms:** Ensure you have a stable internet connection for downloading dependencies

3. **Review detailed setup guides:**
   - [macOS Setup Reference](./setup/macos_setup.md)
   - [Linux Setup Reference](./setup/linux_setup.md)

4. **Get help:**
   - Ask in [GitHub Discussions](https://github.com/Snapchat/Valdi/discussions)
   - Check the [Troubleshooting Guide](./TROUBLESHOOTING.md) and the more detailed [help & troubleshooting page](./docs/help-troubleshooting.md)

## Next Steps

Ready to start building? Check out:

- [Getting Started Codelab](./codelabs/getting_started/1-introduction.md)
- [Documentation](https://github.com/Snapchat/Valdi/tree/main/docs#the-basics)
- [API Reference](https://github.com/Snapchat/Valdi/tree/main/docs/api)
- [Command Line Reference](./docs/command-line-references.md)
