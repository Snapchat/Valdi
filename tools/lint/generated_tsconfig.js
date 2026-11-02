// Makes the TypeScript that the Valdi compiler generates for each module (res.ts, Strings.d.ts,
// ClientSQL sources) visible to the lint, so files that import it type-check.
//
// Reads the generated file paths on stdin, one per line, relative to open_source/. They come from the
// <module>_projectsync Bazel targets, the same outputs `valdi projectsync` wires into editor tsconfigs:
//
//   bazel-out/<config>/bin/<package>/.valdi_build/projectsync/generated_ts/<module>/<file>
//
// Each file is copied to .generated/<package>/<file>, which mirrors where the module's own sources
// expect it. tsconfig.generated.json then overlays that tree on the sources: rootDirs for relative
// imports (`../res`) and an extra "*" path for module imports (`valdi_test/src/Strings`).
//
// Called by tools/ci/lint_modules.sh; .eslintrc.js picks the config up through VALDI_LINT_TSCONFIG.

const fs = require('fs');
const path = require('path');
const ts = require('typescript');

const OPEN_SOURCE = path.resolve(__dirname, '../..');
const OVERLAY = path.join(__dirname, '.generated');
const GENERATED = /^.+?\/bin\/(.+?)\/\.valdi_build\/projectsync\/generated_ts\/[^/]+\/(.+)$/;

const base = ts.readConfigFile(path.join(__dirname, 'tsconfig.json'), ts.sys.readFile);
if (base.error) {
  console.error(ts.flattenDiagnosticMessageText(base.error.messageText, '\n'));
  process.exit(1);
}
const basePaths = base.config.compilerOptions.paths;

fs.rmSync(OVERLAY, { recursive: true, force: true });
const files = fs.readFileSync(0, 'utf8').split('\n').filter(Boolean);
for (const file of files) {
  const match = GENERATED.exec(file);
  if (!match) {
    console.error(`Not a projectsync generated_ts output: ${file}`);
    process.exit(1);
  }
  const [, packageDir, relativePath] = match;
  const destination = path.join(OVERLAY, packageDir, relativePath);
  fs.mkdirSync(path.dirname(destination), { recursive: true });
  fs.copyFileSync(path.join(OPEN_SOURCE, file), destination);
}

// paths are relative to baseUrl (open_source/); rootDirs are relative to this directory.
const overlayFromBase = path.relative(OPEN_SOURCE, OVERLAY);
const config = {
  extends: './tsconfig.json',
  compilerOptions: {
    paths: {
      ...basePaths,
      '*': [...basePaths['*'], ...basePaths['*'].map(p => `${overlayFromBase}/${p}`)],
    },
    rootDirs: ['../..', './.generated'],
  },
};

fs.writeFileSync(path.join(__dirname, 'tsconfig.generated.json'), `${JSON.stringify(config, null, 2)}\n`);
console.log(`Overlaid ${files.length} generated TypeScript file(s) for the lint.`);
