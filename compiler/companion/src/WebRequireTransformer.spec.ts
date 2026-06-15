import 'ts-jest';
import * as ts from 'typescript';
import { createWebRequireTransformer } from './WebRequireTransformer';
import { trimAllLines } from './utils/StringUtils';

function sanitize(text: string): string {
  return trimAllLines(text);
}

function transform(input: string): string {
  const result = ts.transpileModule(input, {
    compilerOptions: {
      target: ts.ScriptTarget.ES2019,
      module: ts.ModuleKind.CommonJS,
    },
    transformers: {
      before: [createWebRequireTransformer()],
    },
  });
  return sanitize(result.outputText);
}

const MARKER = '/* @valdi-dynamic */';
const STRIP_START = '/* @valdi-web-strip-start */';
const STRIP_END = '/* @valdi-web-strip-end */';

describe('WebRequireTransformer', () => {
  describe('string literal requires are untouched', () => {
    it('keeps single-arg string require', () => {
      const result = transform(`const x = require("DeviceBridge");`);
      expect(result).toContain('require("DeviceBridge")');
      expect(result).not.toContain(MARKER);
    });

    it('keeps relative path require', () => {
      const result = transform(`const x = require("./utils/helper");`);
      expect(result).toContain('require("./utils/helper")');
      expect(result).not.toContain(MARKER);
    });

    it('keeps template literal require without substitutions', () => {
      const result = transform('const x = require(`DeviceBridge`);');
      expect(result).not.toContain(MARKER);
    });
  });

  describe('variable requires are annotated', () => {
    it('annotates single-arg variable require', () => {
      const result = transform(`const x = require(modulePath);`);
      expect(result).toContain(`${MARKER} require(modulePath)`);
    });

    it('annotates multi-arg variable require preserving all args', () => {
      const result = transform(`const x = require(componentPath, true, true);`);
      expect(result).toContain(`${MARKER} require(componentPath, true, true)`);
    });
  });

  describe('import-generated requires are untouched', () => {
    it('import statement becomes plain require via tsc', () => {
      const result = transform(`import { Component } from "valdi_core/src/Component";\nconsole.log(Component);`);
      expect(result).toContain('require("valdi_core/src/Component")');
      expect(result).not.toContain(MARKER);
    });
  });

  describe('non-require calls are untouched', () => {
    it('ignores other function calls', () => {
      const result = transform(`const x = someFunction("test");`);
      expect(result).not.toContain(MARKER);
    });

    it('ignores require.resolve', () => {
      const result = transform(`const x = require.resolve("path");`);
      expect(result).not.toContain(MARKER);
    });
  });

  describe('string-literal multi-arg requires are wrapped in sentinels', () => {
    it('wraps require with two extra args', () => {
      const result = transform(`const x = require("DeviceBridge", true, true);`);
      expect(result).toContain(`${STRIP_START} require("DeviceBridge", true, true) ${STRIP_END}`);
    });

    it('wraps require with one extra arg', () => {
      const result = transform(`const x = require("Cof", true);`);
      expect(result).toContain(`${STRIP_START} require("Cof", true) ${STRIP_END}`);
    });

    it('does not wrap single-arg requires', () => {
      const result = transform(`const x = require("DeviceBridge");`);
      expect(result).not.toContain(STRIP_START);
      expect(result).not.toContain(STRIP_END);
    });

    it('does not wrap variable-first-arg requires', () => {
      const result = transform(`const x = require(modulePath, true);`);
      expect(result).not.toContain(STRIP_START);
      expect(result).toContain(MARKER);
    });

    it('handles single-quoted strings', () => {
      const result = transform(`const x = require('Cof', true);`);
      expect(result).toContain(`${STRIP_START} require('Cof', true) ${STRIP_END}`);
    });

    it('preserves nested call args between sentinels', () => {
      const result = transform(`const x = require("X", getFlag(), other(1, 2));`);
      expect(result).toContain(`${STRIP_START} require("X", getFlag(), other(1, 2)) ${STRIP_END}`);
    });
  });

  describe('mixed requires in one file', () => {
    it('annotates variable requires and wraps string-literal multi-arg requires', () => {
      const input = `
        const a = require("DeviceBridge");
        const b = require(dynamicPath, true);
        const c = require("Cof", true);
      `;
      const result = transform(input);
      expect(result).toContain('require("DeviceBridge")');
      expect(result).toContain(`${MARKER} require(dynamicPath, true)`);
      expect(result).toContain(`${STRIP_START} require("Cof", true) ${STRIP_END}`);
      // One dynamic annotation, one strip sentinel pair
      expect((result.match(/@valdi-dynamic/g) || []).length).toBe(1);
      expect((result.match(/@valdi-web-strip-start/g) || []).length).toBe(1);
      expect((result.match(/@valdi-web-strip-end/g) || []).length).toBe(1);
    });
  });
});
