# GitHub Actions CI for Valdi CLI

## Workflows

### 1. `test-cli-linux.yml` - Linux Distribution Testing

Tests the CLI on multiple Linux distributions to ensure proper distribution detection and package management.

**Triggers:**
- Pull requests touching CLI source files
- Pushes to main branch
- Manual workflow dispatch

**Test Matrix:**
- Ubuntu 22.04
- Ubuntu 24.04
- Fedora 39
- Fedora 40

**Tests:**
1. **Distribution Detection**
   - Verifies correct detection of distribution type (Debian, RedHat)
   - Validates package manager identification (apt, dnf)
   - Checks distribution name parsing

2. **Package Name Mappings**
   - Tests all common package mappings (git, npm, Java, etc.)
   - Ensures correct package names for each distribution
   - Validates fallback behavior

3. **Install Command Generation**
   - Tests single and multiple package installation commands
   - Verifies correct package manager usage
   - Validates sudo requirement handling

4. **Git-LFS Setup Detection**
   - Tests whether repository setup is needed
   - Validates setup command generation
   - Checks Debian vs RPM script selection

5. **`valdi doctor` Integration**
   - Runs the doctor command on each distribution
   - Verifies it executes without errors
   - Checks distribution-specific fix commands

6. **Unit Tests**
   - Runs Jasmine unit tests for `linuxDistro` utilities
   - Validates ESLint compliance

**Example Run:**

```bash
# Locally test distribution detection (requires Docker)
docker run -it ubuntu:22.04 bash
# Install Node.js, clone repo, run tests

# Or use act to run GitHub Actions locally
act -j test-distribution-detection -P ubuntu-22.04=ubuntu:22.04
```

### 2. `bzl-changes.yml` - Main CI Pipeline (Updated)

**Changes:**
- Added `test-cli-linux` job to test CLI on Linux containers
- Updated path triggers to include `npm_modules/cli/src/**`
- Tests run alongside existing Bazel validation

**Linux Job:**
- Tests CLI on Ubuntu and Fedora
- Validates distribution detection
- Ensures package mappings work correctly

### 3. `publish-npm.yml` - NPM Publishing

**No changes needed** - builds and publishes on Ubuntu, which now benefits from the improved Linux support.

## Running Tests Locally

### Test Distribution Detection

```bash
# On your Linux machine
cd npm_modules/cli
npm install
npm run build

# Test distribution detection
node -e "
  const distro = require('./dist/utils/linuxDistro');
  const detected = distro.detectLinuxDistro();
  console.log('Detected:', detected);
"

# Run unit tests
npx jasmine dist/utils/linuxDistro.spec.js
```

### Test on Multiple Distributions (Docker)

```bash
# Ubuntu 22.04
docker run --rm -v $(pwd):/workspace -w /workspace node:22-slim bash -c "
  apt-get update && apt-get install -y git
  cd npm_modules/cli
  npm install && npm run build
  node -e \"const d = require('./dist/utils/linuxDistro'); console.log(d.detectLinuxDistro())\"
"

# Fedora 39
docker run --rm -v $(pwd):/workspace -w /workspace node:22-slim bash -c "
  dnf install -y git
  cd npm_modules/cli
  npm install && npm run build
  node -e \"const d = require('./dist/utils/linuxDistro'); console.log(d.detectLinuxDistro())\"
"
```

## CI Badge

Add this to your README to show CI status:

```markdown
![Test CLI on Linux](https://github.com/Snapchat/Valdi/actions/workflows/test-cli-linux.yml/badge.svg)
```

## Troubleshooting CI Failures

### Distribution detection fails
- Check `/etc/os-release` file exists in container
- Verify `ID` and `ID_LIKE` fields are present
- Check package manager is in PATH

### Package mappings incorrect
- Update `getCommonPackageMappings()` in `src/utils/linuxDistro.ts`
- Add distribution-specific package names
- Update unit tests to match

### `valdi doctor` fails
- Check if missing dependencies are expected for CI
- Verify error handling works correctly
- Ensure fix commands use correct distribution detection

## Adding New Distributions to CI

To add a new distribution to the test matrix:

1. Edit `.github/workflows/test-cli-linux.yml`
2. Add to the matrix:
   ```yaml
   - distro: new-distro-name
     container: distro:version
     expected_type: debian|redhat|arch|suse
     expected_pm: apt|dnf|pacman|zypper
     expected_name_pattern: "Distro Name"
   ```
3. Ensure the distribution has Node.js 22+ support
4. Test locally with Docker first

## Notes

- **Arch Linux** and **openSUSE** are not in CI due to container availability/complexity
- CI uses Docker containers for isolation
- Tests are read-only - we don't actually install packages in CI
- Manual testing on physical distributions is still recommended for full coverage
