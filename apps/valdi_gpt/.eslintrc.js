
module.exports = {
  root: true,
  parser: '@typescript-eslint/parser',
  parserOptions: {
    tsconfigRootDir: __dirname,
    project: ['./src/valdi/_configs/eslint.tsconfig.json'],
  },
  plugins: ['@typescript-eslint', 'unused-imports', 'rxjs', 'prettier', '@snap/eslint-plugin-valdi'],
  extends: [
    'eslint:recommended',
    'plugin:@typescript-eslint/recommended',
    'plugin:import/recommended',
    'plugin:import/typescript',
    'prettier',
  ],
  rules: { 
    '@snap/valdi/attributed-text-no-array-assignment': 'error',
    '@snap/valdi/jsx-no-lambda': 'error',
    '@snap/valdi/assign-timer-id': 'error',
    '@snap/valdi/only-const-enum': 'off',
    '@snap/valdi/no-implicit-index-import': 'error',
    '@snap/valdi/mutate-state-without-set-state': 'error',
    '@snap/valdi/no-import-from-test-outside-test-dir': 'error',
    '@snap/valdi/no-declare-test-without-describe': 'error', 
  },
};
