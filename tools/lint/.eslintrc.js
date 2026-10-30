// Style rules for Valdi module code (src/valdi_modules and the sample apps under apps/).
//
// These are the same rules Valdi modules are held to when they are imported and built alongside other
// Valdi apps, so a pull request that passes this lint should not pick up new style failures on import.
//
// Run from the repository root:
//   ./tools/ci/lint_modules.sh            # all modules and apps
//   ./tools/ci/lint_modules.sh <files>    # specific files
//   ./tools/ci/lint_modules.sh --fix      # apply auto-fixes
//
// Framework modules that predate the type-aware rules run with a relaxed set (RELAXED_RULES below),
// matching how they have always been linted. New modules and apps get the full set.

// Glob patterns below are relative to the repository root, which is the working directory
// tools/ci/lint_modules.sh runs ESLint from.
const MODULES_ROOT = 'src/valdi_modules/src/valdi';

const DEFAULT_RULES = {
  'no-control-regex': 'off',
  'no-useless-escape': 'off',
  'eqeqeq': 'error',
  'import/no-unresolved': 'off',
  'import/no-duplicates': 'error',
  'import/no-named-as-default': 'error',
  'import/order': ['warn', { alphabetize: { order: 'asc' } }],
  'unused-imports/no-unused-imports': 'warn',
  '@typescript-eslint/no-empty-function': 'off',
  '@typescript-eslint/no-explicit-any': 'error',
  '@typescript-eslint/no-namespace': 'off',
  '@typescript-eslint/no-non-null-assertion': 'off',
  '@typescript-eslint/ban-ts-comment': [
    'error',
    {
      'ts-ignore': 'allow-with-description',
      'minimumDescriptionLength': 10,
    },
  ],
  '@typescript-eslint/no-inferrable-types': 'off',
  '@typescript-eslint/no-empty-interface': 'off',
  '@typescript-eslint/no-implied-eval': 'error',
  '@typescript-eslint/ban-types': ['error', { extendDefaults: true, types: { '{}': false } }],
  '@typescript-eslint/member-ordering': 'warn',
  '@typescript-eslint/explicit-function-return-type': ['error', { allowExpressions: true }],
  '@typescript-eslint/no-unsafe-return': 'error',
  '@typescript-eslint/no-unused-vars': 'error',
  '@typescript-eslint/no-unsafe-member-access': 'error',
  '@typescript-eslint/no-unsafe-call': 'error',
  '@typescript-eslint/no-misused-promises': 'error',
  '@typescript-eslint/no-unnecessary-type-assertion': 'error',
  '@typescript-eslint/no-unsafe-assignment': 'error',
  '@typescript-eslint/no-floating-promises': 'error',
  '@typescript-eslint/await-thenable': 'error',
  '@typescript-eslint/naming-convention': [
    'warn',
    {
      selector: 'default',
      format: ['camelCase'],
      leadingUnderscore: 'allow',
      trailingUnderscore: 'allow',
    },
    {
      selector: 'variable',
      format: ['camelCase', 'UPPER_CASE', 'PascalCase'],
      leadingUnderscore: 'allow',
      trailingUnderscore: 'allow',
    },
    {
      selector: 'typeLike',
      format: ['PascalCase'],
    },
    {
      selector: 'enumMember',
      format: ['UPPER_CASE', 'PascalCase'],
    },
    {
      selector: 'typeProperty',
      format: ['camelCase', 'snake_case'],
    },
    {
      selector: 'objectLiteralProperty',
      modifiers: ['requiresQuotes'],
      format: null,
    },
    {
      selector: 'objectLiteralProperty',
      format: ['snake_case', 'camelCase', 'UPPER_CASE'],
    },
    {
      selector: 'import',
      format: ['camelCase', 'PascalCase'],
    },
  ],
  'rxjs/no-sharereplay': 'error',
  'rxjs/no-async-subscribe': 'error',
  'rxjs/no-ignored-observable': 'error',
  'rxjs/no-ignored-subscription': 'error',
  'rxjs/no-nested-subscribe': 'error',
  'rxjs/no-unbound-methods': 'error',
  'prettier/prettier': 'warn',
  '@snap/valdi/attributed-text-no-array-assignment': 'error',
  '@snap/valdi/jsx-no-lambda': 'error',
  '@snap/valdi/assign-timer-id': 'error',
  '@snap/valdi/only-const-enum': 'off',
  '@snap/valdi/no-implicit-index-import': 'error',
  '@snap/valdi/mutate-state-without-set-state': 'error',
  '@snap/valdi/no-import-from-test-outside-test-dir': 'error',
  '@snap/valdi/no-declare-test-without-describe': 'error',
};

// Rules turned off for framework modules that were written before the type-aware rules existed.
const RELAXED_RULES = {
  'eqeqeq': 'off',
  '@typescript-eslint/member-ordering': 'off',
  '@typescript-eslint/no-explicit-any': 'off',
  '@typescript-eslint/no-unused-vars': 'off',
  '@typescript-eslint/explicit-function-return-type': 'off',
  '@typescript-eslint/no-unsafe-return': 'off',
  '@typescript-eslint/no-unsafe-member-access': 'off',
  '@typescript-eslint/no-unsafe-call': 'off',
  '@typescript-eslint/no-misused-promises': 'off',
  '@typescript-eslint/no-unnecessary-type-assertion': 'off',
  '@typescript-eslint/no-unsafe-assignment': 'off',
  '@typescript-eslint/naming-convention': 'off',
};

const RELAXED_MODULES = [
  'bridge_observables',
  'coreutils',
  'foundation',
  'persistence',
  'source_map',
  'types',
  'valdi_core',
  'valdi_protobuf',
  'valdi_standalone',
  'valdi_test',
];

module.exports = {
  root: true,
  parser: '@typescript-eslint/parser',
  parserOptions: {
    tsconfigRootDir: __dirname,
    project: ['./tsconfig.json'],
  },
  ignorePatterns: [
    '**/node_modules/**',
    '**/proto.d.ts',
    // Vendored or generated code.
    `${MODULES_ROOT}/jasmine/**`,
    `${MODULES_ROOT}/valdi_rxjs/**`,
    `${MODULES_ROOT}/valdi_core/src/tslib.d.ts`,
  ],
  plugins: ['@typescript-eslint', 'unused-imports', 'rxjs', 'prettier', '@snap/valdi'],
  extends: [
    'eslint:recommended',
    'plugin:@typescript-eslint/recommended',
    'plugin:import/recommended',
    'plugin:import/typescript',
    'prettier',
  ],
  rules: { ...DEFAULT_RULES },
  overrides: [
    {
      files: RELAXED_MODULES.map(m => `${MODULES_ROOT}/${m}/**/*.{ts,tsx}`),
      rules: { ...RELAXED_RULES },
    },
    {
      // Native element typings mirror platform attribute names and types.
      files: [`${MODULES_ROOT}/valdi_tsx/src/NativeTemplateElements.d.ts`],
      rules: {
        '@typescript-eslint/naming-convention': 'off',
        '@typescript-eslint/no-explicit-any': 'off',
      },
    },
    {
      files: [`${MODULES_ROOT}/**/*.d.ts`],
      rules: { '@snap/valdi/only-const-enum': 'error' },
    },
  ],
};
