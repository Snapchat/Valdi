# Getting Started with Valdi

> **Install the Valdi CLI from npm.** This guide assumes you install the CLI with `npm install -g @snap/valdi`. If you are contributing to Valdi itself, see [DEV_SETUP.md](DEV_SETUP.md) instead.

This guide will help you set up your development environment and get started with your first project.

## Prerequisites

### macOS
**Install Xcode** from the [App Store](https://apps.apple.com/us/app/xcode/id497799835) - this is required for iOS development.

### Linux
No prerequisites! The Valdi CLI will install everything you need.

### All Platforms
That's it! The `valdi dev_setup` command handles all other dependencies automatically, including:
- Homebrew (macOS)
- Bazelisk
- Java JDK 17
- Android SDK command-line tools
- Git LFS
- Watchman
- Shell autocomplete, JDK symlink (macOS), Android env vars, and platform-specific extras (e.g. ios-webkit-debug-proxy on macOS; adb, zlib/fontconfig dev libs on Linux)

> [!TIP]
> For manual installation details, see the [macOS](./setup/macos_setup.md) or [Linux](./setup/linux_setup.md) reference guides.

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
> The first build may take several minutes as it sets up the bazel WORKSPACE.

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
   - **macOS:** Ensure Xcode is installed and configured (`sudo xcode-select -s /Applications/Xcode.app`)
   - **All platforms:** Ensure you have a stable internet connection for downloading dependencies

3. **Review detailed setup guides:**
   - [macOS Setup Reference](./setup/macos_setup.md)
   - [Linux Setup Reference](./setup/linux_setup.md)

4. **Get help:**
   - Ask in [GitHub Discussions](https://github.com/Snapchat/Valdi/discussions)
   - Check [Troubleshooting Guide](./TROUBLESHOOTING.md)

## Next Steps

Ready to start building? Check out:

- [Getting Started Codelab](https://github.com/Snapchat/Valdi/blob/main/docs/codelabs/getting_started/1-introduction.md)
- [Documentation](https://github.com/Snapchat/Valdi/tree/main/docs#the-basics)
- [API Reference](https://github.com/Snapchat/Valdi/tree/main/docs/api)
- [Command Line Reference](./docs/command-line-references.md)
