# Getting Started with Valdi

> **Get the Valdi CLI from npm.** Run `npm install -g @snap/valdi` and use the commands below. If you're contributing to Valdi itself, see [DEV_SETUP.md](../DEV_SETUP.md).

## Prerequisites

- **All platforms:** [Node.js](https://nodejs.org/) 18 or later (22 recommended)
- **macOS:** Xcode 26.5 or 27; see [Prerequisites](../INSTALL.md#prerequisites).

`valdi dev_setup` installs the rest (it asks for `sudo` and edits your shell profile; see the [Installation Guide](../INSTALL.md#what-valdi-dev_setup-installs) for details).

## Quick Start

```bash
# 1. Install the Valdi CLI
npm install -g @snap/valdi

# 2. Set up your development environment
valdi dev_setup

# 3. Verify everything works
valdi doctor

# 4. Create your first project
mkdir my_project && cd my_project
valdi bootstrap
valdi install ios  # or android
```

## Full Installation Guide

For detailed setup instructions, VSCode configuration, and troubleshooting, see the [Installation Guide](../INSTALL.md).

## Help!

- Run `valdi doctor` to diagnose issues
- Check the [Troubleshooting Guide](../TROUBLESHOOTING.md)
- Ask in [GitHub Discussions](https://github.com/Snapchat/Valdi/discussions) if you get stuck
