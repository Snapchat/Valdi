# GitHub Workflows

## Testing workflows locally

You can run GitHub Actions locally in two ways.

### 1. Run the script directly (recommended when a job is script-based)

Many workflows just run a script. Run that script on your machine with the same env (Node, Java, Bazel, etc.):

| Workflow / job | Local command (from repo root) |
|----------------|----------------------------------|
| **Release Test** | `./tools/ci/release_test.sh` |
| **Bazel smoke test** | `./tools/ci/bootstrap_app.sh` (set `OPEN_SOURCE_DIR=$(pwd)`, `PROJECT_ROOT=/tmp/valdi_app`) |

No Docker required; same as CI except for runner OS (e.g. macOS for iOS build).

### 2. Use [act](https://github.com/nektos/act) to run the workflow in Docker

[act](https://github.com/nektos/act) runs GitHub Actions locally using Docker images for the runner environment.

**Install (macOS):** `brew install act`

**List workflows and jobs:**
```bash
act -l
```

**Run a specific workflow (e.g. Release Test):**
```bash
act release-test -j release-test
```

**Run a specific workflow by event (e.g. push):**
```bash
act push -W .github/workflows/release-test.yml -j release-test
```

**Limitations:**

- **macOS runner**: `act` runs jobs in Linux containers by default. The Release Test and Bazel smoke jobs use `runs-on: macos-latest` (for Xcode / iOS build). To run those as in CI you either:
  - Run the script directly on your Mac (see above), or
  - Use `act` with a macOS image if your act version supports it (experimental).
- **Secrets**: Use `act -s NPM_TOKEN=...` (or a `.secrets` file) for workflows that need secrets; they are not pulled from GitHub.
- **Services / caches**: Some actions (e.g. `actions/cache`, `actions/checkout`) work in act; others may differ from GitHub.

For the **Release Test** workflow, running `./tools/ci/release_test.sh` on a Mac is the closest to CI and usually the easiest.

---

## Release Test (Public GitHub)

The `release-test.yml` workflow verifies that the **bleeding edge (main branch)** of the public GitHub Valdi and Valdi_Widgets repos can be bootstrapped, built, and tested. Run it before cutting a release to answer: "if we cut a release now, will things fail?"

### What it does

1. Builds the Valdi CLI from source in this repo
2. Bootstraps a new app **without** a local Valdi path (uses `--valdiVersion=main --valdiWidgetsVersion=main`, i.e. bleeding edge from `https://github.com/Snapchat/Valdi` and `Valdi_Widgets`)
3. Builds the iOS app and runs the module test

### When it runs

- **Manual**: Actions → "Release Test (Public GitHub)" → Run workflow
- **On release**: When a GitHub release is published
- **On version tags**: Push `v*` or `beta-*` (e.g. `v1.0.1`, `beta-0.0.2`)
- **On PR**: When bootstrap/release-test files change (`npm_modules/cli` bootstrap, `tools/ci/release_test.sh`, or this workflow)

### Running locally

From the repo root (open_source):

```bash
./tools/ci/release_test.sh
```

Requires Node, Java 17, Bazel (or Bazelisk), and watchman. On macOS, the script builds the iOS app; set `SKIP_BUILD=1` to only run bootstrap + unit test. The script uses bleeding edge (main) by default; the workflow does the same.

---

## NPM Package Publishing

The `publish-npm.yml` workflow automatically publishes npm packages to the public npm registry when their `package.json` files are updated.

### Packages

This workflow handles publishing for:
- **@snap/valdi** (`npm_modules/cli/`) - CLI tools for Valdi development (available as `valdi` command)
- **@snap/eslint-plugin-valdi** (`npm_modules/eslint-plugin-valdi/`) - ESLint rules for Valdi

### Trigger Conditions

The workflow runs when:
1. Changes are pushed to `main` or `master` branch
2. The changes include modifications to `npm_modules/*/package.json`
3. Manual trigger via workflow_dispatch

### How It Works

1. **Detect Changes**: Determines which package.json files were modified
2. **Test CLI (before publish only)**: For `@snap/valdi`, two jobs run first. Both must succeed before publish:
   - **test-cli** (Ubuntu): install, build, smoke tests (`valdi --version`, `--help`, `bootstrap --help`, `doctor --help`).
   - **test-cli-build** (macOS): full release test — bootstrap an app from bleeding edge (main), build the iOS app, run the module test (`./tools/ci/release_test.sh`). Ensures the CLI can bootstrap and build before we publish.
3. **Build & Publish**: For each changed package:
   - Checks out the code
   - Sets up Node.js 20
   - Installs dependencies with `npm ci`
   - Builds the package with `npm run build`
   - Publishes to npm registry with `npm publish --access public`

### Setup Requirements

#### NPM Token

You must configure an `NPM_TOKEN` secret in your GitHub repository:

1. **Create an NPM Access Token**:
   - Log in to [npmjs.com](https://www.npmjs.com/)
   - Go to Account Settings → Access Tokens
   - Click "Generate New Token" → "Classic Token"
   - Select "Automation" type
   - Copy the generated token

2. **Add Secret to GitHub**:
   - Go to your GitHub repository
   - Navigate to Settings → Secrets and variables → Actions
   - Click "New repository secret"
   - Name: `NPM_TOKEN`
   - Value: Paste your npm access token
   - Click "Add secret"

#### Package Publishing Permissions

Ensure the npm account associated with the token has:
- Publishing rights for the `@snap` organization (for both `@snap/valdi` and `@snap/eslint-plugin-valdi`)

### Usage

To publish a new version of a package:

1. Update the version in the package's `package.json`:
   ```bash
   cd npm_modules/cli  # or eslint-plugin-valdi
   npm version patch   # or minor, major
   ```

2. Commit and push the changes:
   ```bash
   git add package.json
   git commit -m "Bump @snap/valdi version to X.Y.Z"
   git push origin main
   ```

3. The workflow will automatically:
   - Detect the package.json change
   - Build the package
   - Publish it to npm

### Manual Trigger

You can also manually trigger the workflow:
1. Go to Actions tab in GitHub
2. Select "Publish NPM Packages" workflow
3. Click "Run workflow"
4. Select the branch and click "Run workflow"

Note: Manual triggers will attempt to publish all packages, so ensure versions have been updated to avoid npm publish errors.

### Troubleshooting

- **401 Unauthorized**: Check that the `NPM_TOKEN` secret is correctly configured
- **403 Forbidden**: Ensure the npm account has publishing permissions for the package
- **Version already exists**: Update the version number in package.json before publishing
- **Build failures**: Check that the package builds successfully locally before pushing

