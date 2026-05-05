import 'ts-jest';
import * as ts from 'typescript';
import { createConsoleLogTransformer } from './ConsoleLogTransformer';
import { trimAllLines } from './utils/StringUtils';

function sanitize(text: string): string {
  return trimAllLines(text);
}

/**
 * Compiles TypeScript source through the ConsoleLogTransformer and returns the output.
 */
function transform(input: string): string {
  const result = ts.transpileModule(input, {
    compilerOptions: {
      target: ts.ScriptTarget.ES2019,
      module: ts.ModuleKind.CommonJS,
      removeComments: false,
    },
    transformers: {
      before: [createConsoleLogTransformer()],
    },
  });
  return sanitize(result.outputText);
}

describe('ConsoleLogTransformer', () => {
  describe('wraps direct console calls', () => {
    it('wraps console.log', () => {
      const result = transform(`console.log("hello");`);
      expect(result).toContain('if (runtime.isLoggingEnabled)');
      expect(result).toContain('console.log("hello")');
    });

    it('wraps console.warn', () => {
      const result = transform(`console.warn("warning");`);
      expect(result).toContain('if (runtime.isLoggingEnabled)');
      expect(result).toContain('console.warn("warning")');
    });

    it('wraps console.error', () => {
      const result = transform(`console.error("error");`);
      expect(result).toContain('if (runtime.isLoggingEnabled)');
      expect(result).toContain('console.error("error")');
    });

    it('wraps console.info', () => {
      const result = transform(`console.info("info");`);
      expect(result).toContain('if (runtime.isLoggingEnabled)');
      expect(result).toContain('console.info("info")');
    });

    it('wraps console.debug', () => {
      const result = transform(`console.debug("debug");`);
      expect(result).toContain('if (runtime.isLoggingEnabled)');
      expect(result).toContain('console.debug("debug")');
    });
  });

  describe('wraps calls with complex arguments', () => {
    it('wraps calls with template literals', () => {
      const input = 'const x = 42;\nconsole.log(`value is ${x}`);';
      const result = transform(input);
      expect(result).toContain('if (runtime.isLoggingEnabled)');
    });

    it('wraps calls with multiple arguments', () => {
      const result = transform(`console.log("a", someObj, 123);`);
      expect(result).toContain('if (runtime.isLoggingEnabled)');
      expect(result).toContain('console.log("a", someObj, 123)');
    });

    it('wraps calls with function call arguments', () => {
      const result = transform(`console.log(JSON.stringify(obj));`);
      expect(result).toContain('if (runtime.isLoggingEnabled)');
      expect(result).toContain('JSON.stringify(obj)');
    });
  });

  describe('wraps multiple console calls in the same file', () => {
    it('wraps each call independently', () => {
      const input = `
        console.log("first");
        console.warn("second");
        console.error("third");
      `;
      const result = transform(input);
      // Should have 3 separate if guards
      const guardCount = (result.match(/runtime\.isLoggingEnabled/g) || []).length;
      expect(guardCount).toBe(3);
    });
  });

  describe('wraps console calls inside functions', () => {
    it('wraps calls inside a function body', () => {
      const input = `
        function doSomething() {
          console.log("inside function");
        }
      `;
      const result = transform(input);
      expect(result).toContain('if (runtime.isLoggingEnabled)');
      expect(result).toContain('console.log("inside function")');
    });

    it('wraps calls inside arrow function with block body', () => {
      const input = `
        const fn = () => {
          console.log("inside arrow");
        };
      `;
      const result = transform(input);
      expect(result).toContain('if (runtime.isLoggingEnabled)');
    });

    it('wraps calls inside class methods', () => {
      const input = `
        class MyClass {
          doWork() {
            console.log("class method");
          }
        }
      `;
      const result = transform(input);
      expect(result).toContain('if (runtime.isLoggingEnabled)');
    });
  });

  describe('handles non-ExpressionStatement console usage', () => {
    it('does not wrap console.error used as a callback reference', () => {
      const input = `promise.catch(console.error);`;
      const result = transform(input);
      expect(result).not.toContain('runtime.isLoggingEnabled');
      expect(result).toContain('promise.catch(console.error)');
    });

    it('does not wrap console.log assigned to a variable', () => {
      const input = `const result = console.log("test");`;
      const result = transform(input);
      expect(result).not.toContain('runtime.isLoggingEnabled');
    });

    it('wraps console.log in expression-bodied arrow', () => {
      const input = `const fn = (x: any) => console.log(x);`;
      const result = transform(input);
      expect(result).toContain('runtime.isLoggingEnabled');
      expect(result).toContain('const fn = (x) => {');
      expect(result).toContain('console.log(x)');
    });

    it('wraps parenthesized console.log in expression-bodied arrow', () => {
      const input = `const fn = (x: any) => (console.log(x));`;
      const result = transform(input);
      expect(result).toContain('runtime.isLoggingEnabled');
      expect(result).toContain('console.log(x)');
    });

    it('does not wrap non-direct console usage in expression-bodied arrow', () => {
      const input = `const fn = (x: any) => doSomething(console.log(x));`;
      const result = transform(input);
      expect(result).not.toContain('runtime.isLoggingEnabled');
      expect(result).toContain('doSomething(console.log(x))');
    });

    it('wraps nested expression-bodied arrow console calls in default parameters', () => {
      const input = `const fn = (x: any = (() => console.log("param"))()) => console.log(x);`;
      const result = transform(input);
      const guardCount = (result.match(/runtime\.isLoggingEnabled/g) || []).length;
      expect(guardCount).toBe(2);
      expect(result).toContain('console.log("param")');
      expect(result).toContain('console.log(x)');
    });
  });

  describe('does NOT wrap non-console methods', () => {
    it('does not wrap console.trace', () => {
      const input = `console.trace("trace");`;
      const result = transform(input);
      expect(result).not.toContain('runtime.isLoggingEnabled');
    });

    it('does not wrap other objects with log method', () => {
      const input = `logger.log("test");`;
      const result = transform(input);
      expect(result).not.toContain('runtime.isLoggingEnabled');
    });

    it('does not wrap runtime.outputLog', () => {
      const input = `runtime.outputLog(2, "test");`;
      const result = transform(input);
      expect(result).not.toContain('runtime.isLoggingEnabled');
    });
  });

  describe('preserves non-console code', () => {
    it('leaves other statements untouched', () => {
      const input = `
        const x = 1;
        console.log("hello");
        const y = 2;
      `;
      const result = transform(input);
      expect(result).toContain('const x = 1');
      expect(result).toContain('const y = 2');
      expect(result).toContain('if (runtime.isLoggingEnabled)');
    });
  });

});
