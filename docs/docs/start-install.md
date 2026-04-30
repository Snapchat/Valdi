# Getting Started with Valdi

> **Get the Valdi CLI from npm.** Run `npm install -g @snap/valdi` and use the commands below. If you're contributing to Valdi itself, see [DEV_SETUP.md](../DEV_SETUP.md).

## Prerequisites

**macOS only:** Install [Xcode from the App Store](https://apps.apple.com/us/app/xcode/id497799835)

Everything else is handled automatically by `valdi dev_setup`!

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
- Reach out on [Discord](https://discord.gg/uJyNEeYX2U) if you get stuck
