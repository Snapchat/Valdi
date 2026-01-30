# Getting Started with Valdi

This guide will help you set up your development environment and get started with your first project.

## Prerequisites

### macOS
**Install Xcode** from the [App Store](https://apps.apple.com/us/app/xcode/id497799835) - this is required for iOS development.

### Linux
No prerequisites! The Valdi CLI will install everything you need.

> [!IMPORTANT]
> Windows is not currently supported. Please wait for upcoming updates!


### All Platforms
That's it! The `valdi dev_setup` command handles all other dependencies automatically, including:
- Homebrew (macOS)
- Bazelisk
- Java JDK 17
- Android SDK command-line tools
- Git LFS
- Watchman
- And more...

> [!TIP]
> For manual installation details, see the [macOS](./setup/macos_setup.md) or [Linux](./setup/linux_setup.md) reference guides.

## Installation

### 1. Install the Valdi CLI

```bash
npm install -g @snap/valdi
```

> [!NOTE]
> During installation, you might see warnings about deprecated packages. These can usually be ignored!


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

Valdi provides editor extensions for syntax highlighting, debugging, and device logs during hot reload.

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

### 3. Install Valdi Extensions

Download the extension files from the [latest release](https://github.com/Snapchat/Valdi/releases/latest):
- `valdi-vivaldi.vsix` - Device logs and Valdi language support
- `valdi-debug.vsix` - JavaScript debugger for Valdi apps

**Install the extensions:**
```bash
# For VSCode
code --install-extension /path/to/valdi-vivaldi.vsix
code --install-extension /path/to/valdi-debug.vsix

# For Cursor
cursor --install-extension /path/to/valdi-vivaldi.vsix
cursor --install-extension /path/to/valdi-debug.vsix
```

### 4. Configure TypeScript

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
   - Join our [Discord community](https://discord.gg/uJyNEeYX2U)
   - Check [Troubleshooting Guide](./TROUBLESHOOTING.md)

## Next Steps

Ready to start building? Check out:

- [Getting Started Codelab](https://github.com/Snapchat/Valdi/blob/main/docs/codelabs/getting_started/1-introduction.md)
- [Documentation](https://github.com/Snapchat/Valdi/tree/main/docs#the-basics)
- [API Reference](https://github.com/Snapchat/Valdi/tree/main/docs/api)
- [Command Line Reference](./docs/command-line-references.md)
