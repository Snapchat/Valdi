# Troubleshooting

## Installation

Run `valdi doctor` first: it checks your environment and suggests fixes. For build, hot reload and runtime problems, see the more detailed [help & troubleshooting page](./docs/help-troubleshooting.md). If you're still stuck, search the web for the exact error output, then ask in [GitHub Discussions](https://github.com/Snapchat/Valdi/discussions).

These are common errors and how to resolve them.

<details id="xcode-27-libcxx">
    <summary><code>The selected platform is no longer supported by libc++</code></summary>

You're building with Xcode 27 using a Valdi version from before Xcode 27 support. The full error looks like:

```
error: "The selected platform is no longer supported by libc++." [-Werror,-W#warnings]
```

Update your project to a Valdi version with Xcode 27 support. On macOS 26 you can instead build with Xcode 26.5 and make sure Bazel uses it (see the next entry); macOS 27 can't run Xcode 26.5.
</details>

<a id="bazel-uses-the-wrong-xcode-version"></a>
<details>
    <summary>Bazel uses the wrong Xcode version</summary>

Bazel finds Xcode installs through macOS LaunchServices, not `xcode-select`, so with several Xcodes installed it may pick a different one. You'll see a line like `No default Xcode version is set with 'xcode-select'; picking ':version27_0_0_...'` in the build output.

To fix it:

1. Select the Xcode you want, for example `sudo xcode-select -s /Applications/Xcode-26.5.app` (adjust the path)
2. Clear Bazel's cached Xcode configuration: `bazel clean --expunge`
3. If Bazel still picks the wrong version, pin it for a build: `--xcode_version=26.5`, or add `build --xcode_version=26.5` to your project's `.bazelrc`
</details>

<details>
    <summary><code>nvm not found</code> (if you installed Node.js with nvm)</summary>

It's very likely either:

- The post install instructions for `nvm` were missed, use `brew info nvm` and follow the instructions for updating your shell environment (e.g. `.zshrc`, `.bashrc`)

```
You should create NVM's working directory if it doesn't exist:
mkdir ~/.nvm

Add the following to your shell profile e.g. ~/.profile or ~/.zshrc:
export NVM_DIR="$HOME/.nvm"
[ -s "/opt/homebrew/opt/nvm/nvm.sh" ] && \. "/opt/homebrew/opt/nvm/nvm.sh"  # This loads nvm
[ -s "/opt/homebrew/opt/nvm/etc/bash_completion.d/nvm" ] && \. "/opt/homebrew/opt/nvm/etc/bash_completion.d/nvm"  # This loads nvm bash_completion
```

- The shell you are currently using has not loaded the updated profile, either: close the terminal shell and open a new one - `source` the updated profile (e.g. `source ~/.zshrc`)
</details>

<details>
    <summary><code>Swap Space</code></summary>

Bazel eats a lot of memory, if you see Java memory errors, you don't have enough swap space.

8GB should be enough for an Android build of the hello world app.

</details>
