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
 * Validates a project name and returns an error message if invalid, or null if valid.
 */
export function validateProjectName(name: string): string | null {
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