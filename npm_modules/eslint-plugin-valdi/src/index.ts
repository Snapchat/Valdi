import assignTimerIdRule from './rules/assign-timer-id';
import attributedTextNoArrayAssignmentRule from './rules/attributed-text-no-array-assignment';
import jsxNoLambdaRule from './rules/jsx-no-lambda';
import noImplicitIndexImport from './rules/no-implicit-index-import';
import mutateStateWithoutSetState from './rules/mutate-state-without-set-state';
import noImportFromTestOutsideTestDir from './rules/no-import-from-test-outside-test-dir';
import onlyConstEnum from './rules/only-const-enum';
import noDeclareTestWithoutDescribe from './rules/no-declare-test-without-describe';
import noReactPatterns from './rules/no-react-patterns';

export = {
  rules: {
    'attributed-text-no-array-assignment': attributedTextNoArrayAssignmentRule,
    'jsx-no-lambda': jsxNoLambdaRule,
    'only-const-enum': onlyConstEnum,
    'assign-timer-id': assignTimerIdRule,
    'no-implicit-index-import': noImplicitIndexImport,
    'mutate-state-without-set-state': mutateStateWithoutSetState,
    'no-import-from-test-outside-test-dir': noImportFromTestOutsideTestDir,
    'no-declare-test-without-describe': noDeclareTestWithoutDescribe,
    'no-react-patterns': noReactPatterns,
  },
};
