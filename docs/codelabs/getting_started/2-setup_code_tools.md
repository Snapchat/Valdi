# Get the code and set up your tools
These steps should be completed in order, each step assumes you have successfully completed the previous steps.

## Get the code

This codelab uses the `helloworld` app that ships in the Valdi repository, so you'll work from a clone of the repo rather than a project created with `valdi bootstrap`. If you haven't installed the Valdi CLI yet, follow [INSTALL.md](../../INSTALL.md) first.

```
git clone https://github.com/Snapchat/Valdi.git
cd Valdi
```

Run every command in the rest of this codelab from the root of this clone.

## Editor setup
Valdi apps are standard TypeScript/TSX projects. You can use any editor, but the Valdi CLI generates project configuration for VS Code and Cursor, so the steps below assume one of those. No Valdi-specific editor extension is required.

- Install [VS Code](https://code.visualstudio.com/download) or [Cursor](https://cursor.com)
- Launch the editor
- Add the editor's command to your `PATH`
    - Open the Command Palette (**cmd+shift+P**)
    - Type `shell command`
    - Select `> Install 'code' command in PATH` (or `> Install 'cursor' command in PATH`)
For these changes to take effect, restart your terminal or `source` your terminal's config file.

## Generate the project configuration

From the root of the clone, generate the TypeScript project configuration your editor uses for type checking and imports:

```
valdi projectsync
```

Re-run `valdi projectsync` whenever you change dependencies, localization files, or resource files.

## Use the workspace TypeScript version

- Open any `.tsx` file in the repo (for example `apps/helloworld/src/valdi/hello_world/src/HelloWorldApp.tsx`)
- Open the Command Palette (**cmd+shift+P**)
- Select `TypeScript: Select TypeScript Version...`
- Choose `Use Workspace Version`

## Optional: set up your AI assistant

If you use an AI coding assistant, install the Valdi skills so it knows Valdi's component model (Valdi is not React):

```
valdi skills install
```

See [Working with AI Assistants](../../docs/ai-tooling.md) for details.

### [Next >](./3-start_coding.md)
