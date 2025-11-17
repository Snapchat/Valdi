# GitHub Workflows

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
2. **Build & Publish**: For each changed package:
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

