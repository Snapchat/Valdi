const wordSplitRegex = /[\W_]/;

// Bazel and common programming language reserved words that should not be used as project names
const RESERVED_PROJECT_NAMES = new Set([
  // Bazel reserved words
  'test',
  'tests',
  'build',
  'workspace',
  'native',
  'rule',
  'package',
  'glob',
  'select',
  'repository',
  'external',
  'bazel',
  // Common programming language keywords that might cause issues
  'class',
  'function',
  'var',
  'let',
  'const',
  'if',
  'else',
  'for',
  'while',
  'return',
  'import',
  'export',
  'module',
  'require',
  // Other problematic names
  'main',
  'lib',
  'src',
  'bin',
  'data',
  'config',
]);

/**
 * Sanitizes a project name to be safe for use in Bazel rules and generated code.
 * - Replaces dashes with underscores
 * - Removes any characters that are not alphanumeric or underscore
 * - Ensures the name starts with a letter or underscore
 * - Preserves original case (Bazel target names are case-sensitive)
 */
export function sanitizeProjectName(name: string): string {
  // Replace dashes with underscores
  let sanitized = name.replace(/-/g, '_');
  
  // Remove any characters that are not alphanumeric or underscore
  sanitized = sanitized.replace(/[^a-zA-Z0-9_]/g, '');
  
  // Ensure it starts with a letter or underscore
  if (sanitized.length > 0 && /^[0-9]/.test(sanitized)) {
    sanitized = '_' + sanitized;
  }
  
  return sanitized;
}

/**
 * The project name is used as the Bazel module name in `module(name = ...)` in
 * MODULE.bazel, which Bazel requires to 1) only contain lowercase letters (a-z),
 * digits (0-9), dots (.), hyphens (-) and underscores (_), 2) begin with a
 * lowercase letter and 3) end with a lowercase letter or digit.
 */
const BAZEL_MODULE_NAME_REGEX = /^[a-z]([\d._a-z-]*[\da-z])?$/;

export function isValidBazelModuleName(name: string): boolean {
  return BAZEL_MODULE_NAME_REGEX.test(name);
}

export interface ValidateProjectNameOptions {
  /**
   * Also require the sanitized name to be a valid Bazel module name. Only needed
   * when the name becomes the `module(name = ...)` of a new MODULE.bazel (i.e.
   * `valdi bootstrap`). Module and target names created by `valdi new_module`
   * are case-sensitive directory/BUILD target names and do not need this.
   */
  requireBazelModuleName?: boolean;
}

/**
 * Validates a project name and returns an error message if invalid, or null if valid.
 */
export function validateProjectName(name: string, options: ValidateProjectNameOptions = {}): string | null {
  if (!name || name.trim().length === 0) {
    return 'Project name cannot be empty.';
  }
  
  const sanitized = sanitizeProjectName(name);
  
  if (sanitized.length === 0) {
    return 'Project name must contain at least one alphanumeric character.';
  }
  
  // Check if the sanitized name (case-insensitive) is a reserved word
  if (RESERVED_PROJECT_NAMES.has(sanitized.toLowerCase())) {
    return `Project name "${name}" (sanitized to "${sanitized}") is a reserved word and cannot be used. Please choose a different name.`;
  }

  // When the name is used as the Bazel module name in MODULE.bazel, reject anything
  // Bazel would refuse before any files are written.
  if (options.requireBazelModuleName && !isValidBazelModuleName(sanitized)) {
    const suggestion = sanitized.toLowerCase().replace(/^[^a-z]+/, '').replace(/[^\da-z]+$/, '');
    return (
      `Project name "${name}" is not a valid Bazel module name. ` +
      `It must only contain lowercase letters (a-z), digits (0-9), dots (.), hyphens (-) and underscores (_), ` +
      `begin with a lowercase letter and end with a lowercase letter or digit.` +
      (suggestion ? ` Did you mean "${suggestion}"?` : '')
    );
  }

  // Warn if the name was significantly changed during sanitization
  if (sanitized !== name.replace(/-/g, '_')) {
    return `Project name "${name}" contains invalid characters. It will be sanitized to "${sanitized}". Please use only letters, numbers, underscores, and dashes.`;
  }
  
  return null;
}

export function toPascalCase(str: string): string {
    const words = str.trim().toLowerCase().split(wordSplitRegex);
    return words.reduce((acc, curr) => {
        const [firstChar, ...rest] = curr;
        const pascalCaseWord = firstChar?.toUpperCase()?.concat(rest.join('')) ?? '';
        return acc + pascalCaseWord;
    }, '');
}

export function toSnakeCase(str: string): string {
    const words = str.trim().toLowerCase().split(wordSplitRegex);
    return words.join('_');
}