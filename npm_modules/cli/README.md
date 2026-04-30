# Valdi CLI

The Valdi CLI tool provides helpful commands for setting up your environment, creating projects, building applications, and managing your Valdi workflow.

## For Users

The CLI is published to npm as `@snap/valdi`:

```bash
# Install globally
npm install -g @snap/valdi

# Set up your development environment
valdi dev_setup

# Verify your setup
valdi doctor

# Get help
valdi --help
```

### Create Your First App

After setting up your environment, create a new Valdi project:

```bash
# Create a new directory for your project
mkdir my_valdi_app
cd my_valdi_app

# Initialize the project
valdi bootstrap

# Build and install on iOS
valdi install ios

# Or build and install on Android
valdi install android

# Start hot reload for development
valdi hotreload
```

Now you can edit your TypeScript files and see changes instantly on your device!

For complete documentation, see:
- [Command Line Reference](https://github.com/Snapchat/Valdi/blob/main/docs/docs/command-line-references.md)
- [Installation Guide](https://github.com/Snapchat/Valdi/blob/main/docs/INSTALL.md)
- [Troubleshooting](https://github.com/Snapchat/Valdi/blob/main/docs/TROUBLESHOOTING.md) — run `valdi doctor` to diagnose issues, or ask on [Discord](https://discord.gg/uJyNEeYX2U)

### Key Commands

**`valdi dev_setup`** - Automated environment setup
- Installs all required dependencies (Bazel, Node.js, Java JDK 17, Android SDK, etc.)
- Configures environment variables and PATH
- Initializes Git LFS
- Sets up shell autocomplete
- Platform-specific: macOS (Homebrew, Xcode) or Linux (apt packages)

**`valdi doctor`** - Environment diagnostics
- Validates Node.js, Bazel, Java, Android SDK installations
- Checks Git LFS initialization
- Verifies shell autocomplete configuration
- Checks VSCode/Cursor extensions (warns if missing)
- macOS: Validates Xcode installation
- Supports `--framework` mode for additional checks
- Supports `--fix` to auto-repair issues
- Supports `--json` for CI/CD integration

**`valdi bootstrap`** - Project initialization
- Creates a new Valdi project in the current directory
- Sets up BUILD.bazel, WORKSPACE, package.json, and source files
- Supports multiple application templates (Hello World, Counter, etc.)
- Options: `-y` (skip confirmation), `-n` (project name), `-t` (application type), `-l` (local Valdi path), `-c` (clean directory first). Run `valdi bootstrap --help` for all options.

**`valdi install <platform>`** - Build and install
- Builds and installs app to connected device/simulator
- Platforms: `ios`, `android`, `macos`

**`valdi hotreload`** - Development server
- Enables instant hot reload during development
- Watches for file changes and updates app in milliseconds

**`valdi skills`** - AI assistant skills
- Installs Valdi context files into Claude Code, Cursor, or GitHub Copilot so AI tools generate correct Valdi code instead of React patterns
- `valdi skills install` — auto-detects installed AI tools and installs all skills
- `valdi skills list` — show available skills and install status per agent
- `valdi skills update` — re-install already-installed skills from the bundled package
- `valdi skills create` — scaffold a new skill (run from within the Valdi repo)

**Other commands:** `valdi build <platform>` (build without installing), `valdi package <platform>` (create distributable app), `valdi export <platform>` (export library for native apps), `valdi test` (run tests), `valdi lint check` / `valdi lint format` (lint and format code), `valdi log` (stream device logs), `valdi projectsync` (sync VS Code project and native bindings), `valdi completion` (shell autocomplete setup). Use `valdi <command> --help` for options.

For complete command documentation, see [Command Line Reference](https://github.com/Snapchat/Valdi/blob/main/docs/docs/command-line-references.md).

### Creating New Modules

```sh
valdi new_module

# Create module without prompts (specify template to skip the prompt)
valdi new_module my_new_module --skip-checks --template=ui_component

# Help
$ valdi new_module --help
valdi new_module [module-name]


******************************************
Valdi Module Creation Guide
******************************************

Requirements for Valdi module names:
- May contain: A-Z, a-z, 0-9, '-', '_', '.'
- Must start with a letter.

Recommended Directory Structure:
my_application/          # Root directory of your application
├── WORKSPACE            # Bazel Workspace
├── BUILD.bazel          # Bazel build
└── modules/
    ├── module_a/
    │   ├── BUILD.bazel
    │   ├── android/     # Native Android sources (Kotlin)
    │   ├── ios/         # Native iOS sources (Objective-C)
    │   ├── macos/       # Native macOS sources (Objective-C)
    │   ├── web/         # Web sources (TypeScript, compiled by tsc)
    │   ├── cpp/         # Native C++ sources
    │   └── src/         # Valdi sources
    │       └── ModuleAComponent.tsx
    ├── module_b/
        ├── BUILD.bazel
    │   ├── res/         # Image and font resources
    │   ├── strings/     # Localizable strings
        └── src/
            └── ModuleBComponent.tsx

For more comprehensive details, refer to the core-module documentation:
https://github.com/Snapchat/Valdi/blob/main/docs/docs/core-module.md

******************************************


Positionals:
  module-name  Name of the Valdi module.

Options:
  --debug        Run with debug logging                    [boolean] [default: false]
  --version      Show version number                       [boolean]
  --help         Show help                                 [boolean]
  --skip-checks  Skips confirmation prompts.               [boolean]
  --template     Module template to use (skips the prompt). One of: ui_component, polyglot_bridge_module, polyglot_view_module  [string]
```

## For Contributors

This section is for developers working on the Valdi CLI itself.

### Prerequisites

Set your npm registry when working on this module:

```sh
npm config set registry https://registry.npmjs.org/
```

### Development Setup

Install dependencies:

```sh
npm install
```

### Development

Run the CLI:

```sh
npm run main
```

# Pass in command line arguments

```sh
npm run main bootstrap -- --confirm-bootstrap
```

Build JavaScript output to `./dist`:

```sh
npm run build
```

Develop with hot reload:

```sh
npm run watch
node ./dist/index.js
node ./dist/index.js bootstrap --confirm-bootstrap
```

Show the help menu:

```sh
node ./dist/index.js new_module --help
```

Run unit tests:

```sh
npm test
```

Install the `valdi` command locally:

```sh
npm run cli:install
```
