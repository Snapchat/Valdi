import 'jasmine';
import { sanitizeProjectName, toPascalCase, toSnakeCase, validateProjectName } from './stringUtils';

describe('stringUtils', () => {
    it('converts strings to pascal case', () => {
      const testCases: [string, string][] = [['hello', 'Hello'], ['hello world', 'HelloWorld'], ['My New Module', 'MyNewModule'], ['my_new_module', 'MyNewModule']];
      testCases.forEach(([input, expected]) => {
        expect(toPascalCase(input)).toBe(expected);
      });
    });

    it('converts strings to snake case', () => {
      const testCases: [string, string][] = [['hello', 'hello'], ['hello world', 'hello_world']];
      testCases.forEach(([input, expected]) => {
        expect(toSnakeCase(input)).toBe(expected);
      });
    });

    describe('sanitizeProjectName', () => {
      it('replaces dashes with underscores', () => {
        expect(sanitizeProjectName('my-project')).toBe('my_project');
        expect(sanitizeProjectName('my-cool-app')).toBe('my_cool_app');
      });

      it('preserves original case', () => {
        expect(sanitizeProjectName('MyProject')).toBe('MyProject');
        expect(sanitizeProjectName('MY_PROJECT')).toBe('MY_PROJECT');
        expect(sanitizeProjectName('testNewModule')).toBe('testNewModule');
      });

      it('removes invalid characters', () => {
        expect(sanitizeProjectName('my project!')).toBe('myproject');
        expect(sanitizeProjectName('my@project#')).toBe('myproject');
        expect(sanitizeProjectName('my.project')).toBe('myproject');
      });

      it('prefixes with underscore if starts with number', () => {
        expect(sanitizeProjectName('123project')).toBe('_123project');
        expect(sanitizeProjectName('7-eleven')).toBe('_7_eleven');
      });

      it('handles mixed cases', () => {
        expect(sanitizeProjectName('My-Cool_Project123')).toBe('My_Cool_Project123');
      });

      it('handles already valid names', () => {
        expect(sanitizeProjectName('my_project')).toBe('my_project');
        expect(sanitizeProjectName('myproject')).toBe('myproject');
        expect(sanitizeProjectName('my_project_123')).toBe('my_project_123');
      });
    });

    describe('validateProjectName', () => {
      it('rejects empty names', () => {
        expect(validateProjectName('')).toBeTruthy();
        expect(validateProjectName('   ')).toBeTruthy();
      });

      it('rejects reserved words (case-insensitive)', () => {
        expect(validateProjectName('test')).toContain('reserved word');
        expect(validateProjectName('Test')).toContain('reserved word');
        expect(validateProjectName('TEST')).toContain('reserved word');
        expect(validateProjectName('build')).toContain('reserved word');
        expect(validateProjectName('workspace')).toContain('reserved word');
        expect(validateProjectName('native')).toContain('reserved word');
        expect(validateProjectName('package')).toContain('reserved word');
      });

      it('accepts valid names with mixed case', () => {
        expect(validateProjectName('my_project')).toBeNull();
        expect(validateProjectName('myproject')).toBeNull();
        expect(validateProjectName('my_cool_app')).toBeNull();
        expect(validateProjectName('project123')).toBeNull();
        expect(validateProjectName('MyProject')).toBeNull();
        expect(validateProjectName('testNewModule')).toBeNull();
      });

      it('accepts names with dashes (they will be sanitized)', () => {
        expect(validateProjectName('my-project')).toBeNull();
        expect(validateProjectName('my-cool-app')).toBeNull();
      });

      it('rejects names with only invalid characters', () => {
        expect(validateProjectName('!!!')).toBeTruthy();
        expect(validateProjectName('...')).toBeTruthy();
        expect(validateProjectName('@#$')).toBeTruthy();
      });

      it('handles names that start with numbers', () => {
        // These are valid because sanitization will prefix with underscore
        expect(validateProjectName('123project')).toBeNull();
      });
    });
});

