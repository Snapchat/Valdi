import 'ts-jest';
import { Workspace } from '../Workspace';
import { compileFile } from './CompileNativeCommand';
import { IRtoString } from './IRtoString';
import { NativeCompilerIR } from './builder/NativeCompilerBuilderIR';
import * as ts from 'typescript';

import { trimAllLines } from '../utils/StringUtils';
import { NativeCompilerOptions } from './NativeCompilerOptions';
import { compileIRsToC } from './NativeCompiler';

function sanitize(text: string): string {
  return trimAllLines(text);
}

function compileAsIRString(
  text: string,
  options: NativeCompilerOptions,
  setupWorkspace: ((workpace: Workspace) => void) | undefined,
  filterIR: (ir: NativeCompilerIR.Base) => boolean,
): string {
  const workspace = new Workspace('/', false, undefined, {
    target: ts.ScriptTarget.ESNext,
    module: ts.ModuleKind.CommonJS,
    lib: ['lib.es2015.d.ts'],
  });

  const filePath = 'file.ts';
  workspace.registerInMemoryFile(filePath, text);
  workspace.addSourceFileAtPath(filePath);
  if (setupWorkspace) {
    setupWorkspace(workspace);
  }
  const irs = compileFile(workspace, true, options, filePath, filePath);

  const filteredIRs = irs.filter(filterIR);
  return IRtoString(filteredIRs).trim();
}

function compileAsC(
  text: string,
  options: NativeCompilerOptions,
  selectFunction: string | undefined,
  setupWorkspace: ((workpace: Workspace) => void) | undefined,
): string {
  const workspace = new Workspace('/', false, undefined, {
    target: ts.ScriptTarget.ESNext,
    module: ts.ModuleKind.CommonJS,
    lib: ['lib.es2015.d.ts'],
  });

  const filePath = 'file.ts';
  workspace.registerInMemoryFile(filePath, text);
  workspace.addSourceFileAtPath(filePath);
  if (setupWorkspace) {
    setupWorkspace(workspace);
  }
  const irs = compileFile(workspace, true, options, filePath, filePath);
  const c_code = compileIRsToC(irs, options).source;

  if (selectFunction) {
    const functionStart = `static tsn_value file_ts_${selectFunction}(tsn_vm *ctx,`;
    const lines = c_code.split('\n');
    const output: string[] = [];
    let inFunction = false;
    let inComment = false;
    for (const line of lines) {
      if (line.startsWith(functionStart) && !line.endsWith(';')) {
        inFunction = true;
      }
      if (line.trimStart().startsWith('/*')) {
        inComment = true;
      }
      if (line.trimStart().startsWith('*/')) {
        inComment = false;
      }
      if (inFunction && !inComment) {
        output.push(line);
      }
      if (line.startsWith('}')) {
        inFunction = false;
      }
    }
    return output.join('\n');
  }
  return c_code;
}

function configuredCompile(
  text: string,
  options: NativeCompilerOptions,
  includeFunctionArguments: boolean,
  includeRetainRelease: boolean,
  includeSlots: boolean,
  includeExceptionHandling: boolean,
  functionsToKeep: string[] | undefined,
  setupWorkspace?: ((workpace: Workspace) => void) | undefined,
): string {
  let inExceptionJumpTarget = false;
  let inRemovedFunction = false;
  return compileAsIRString(text, options, setupWorkspace, (ir) => {
    switch (ir.kind) {
      case NativeCompilerIR.Kind.StartFunction: {
        const typedIR = ir as NativeCompilerIR.StartFunction;
        if (functionsToKeep === undefined || functionsToKeep.includes(typedIR.name)) {
          return true;
        } else {
          inRemovedFunction = true;
          return false;
        }
      }
      case NativeCompilerIR.Kind.EndFunction: {
        if (inRemovedFunction) {
          inRemovedFunction = false;
          return false;
        } else {
          return true;
        }
      }
      case NativeCompilerIR.Kind.Slot:
        return !inRemovedFunction && includeSlots;
      case NativeCompilerIR.Kind.Retain:
        return !inRemovedFunction && includeRetainRelease;
      case NativeCompilerIR.Kind.Free:
        return !inRemovedFunction && includeRetainRelease;
      case NativeCompilerIR.Kind.GetFunctionArg:
        return !inRemovedFunction && includeFunctionArguments;
      case NativeCompilerIR.Kind.BindJumpTarget: {
        const typedIR = ir as NativeCompilerIR.BindJumpTarget;

        inExceptionJumpTarget = typedIR.target.tag === 'exception';
      }
      default: {
        if (inRemovedFunction) {
          return false;
        }

        if (includeExceptionHandling) {
          return true;
        } else {
          return !inExceptionJumpTarget;
        }
      }
    }
  });
}

function compileSimplified(text: string, functionsToKeep?: string[]): string {
  return configuredCompile(
    text,
    { optimizeSlots: false, optimizeVarRefs: true },
    false,
    false,
    false,
    false,
    functionsToKeep,
  );
}

describe('NativeCompiler', () => {
  it('supports var ref', () => {
    const result = configuredCompile(
      `
      let value = 42;
      value = 43;
      `,
      { optimizeSlots: false, optimizeVarRefs: false },
      true,
      false,
      false,
      false,
      undefined,
    );

    expect(result).toBe(
      `
function_begin 'file_ts__module_init__'
newvref 'exports' @4
getfnarg 2 @3
setvref @3 @4
newvref 'value' @5
storeint 42 @6
setvref @6 @5
storeint 43 @7
setvref @7 @5
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('can optimize var refs', () => {
    const result = configuredCompile(
      `
      let value = 42;
      value = 43;
      `,
      { optimizeSlots: false, optimizeVarRefs: true },
      true,
      false,
      false,
      false,
      undefined,
    );

    expect(result).toBe(
      `
function_begin 'file_ts__module_init__'
getfnarg 2 @3
storeint 42 @6
assign @6 @5
storeint 43 @7
assign @7 @5
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('can optimize var refs and slots', () => {
    const result = configuredCompile(
      `
    let value = 42;
    value = 43;
    `,
      { optimizeSlots: true, optimizeVarRefs: true },
      true,
      true,
      true,
      false,
      undefined,
    );

    expect(result).toBe(
      `
function_begin 'file_ts__module_init__'
slot 'retval' @1
slot 'object' @2
slot 'number' @3
slot 'number' @4
getfnarg 2 @2
storeint 42 @3
assign @3 @4
storeint 43 @3
assign @3 @4
jump 'return'
release @2
retain @1
jumptarget 'return'
release @2
function_end @1
    `.trim(),
    );
  });

  it('reconciles types on captured variable refs', () => {
    const result = configuredCompile(
      `
      declare function build(cb: () => void): void;

      let array: string[] | undefined;
      build(() => {
          array = [];
      });

      const arrayLength = array.length;
    `,
      { optimizeSlots: true, optimizeVarRefs: true },
      true,
      true,
      false,
      false,
      ['file_ts__module_init__'],
    );

    expect(result).toBe(
      `
function_begin 'file_ts__module_init__'
getfnarg 2 @2
newvref 'array' @3
setvref @4 @3
release @2
getglobal @2
getprop @2 'build' @5
release @2
newarrowfn 'file_ts_anon' [ @3 ] @2
call @5 @5 [ @2 ] @6
release @5
loadvref @3 @5
release @2
getprop @5 'length' @2
release @6
assign @2 @6
retain @6
jump 'return'
release @5
retain @1
jumptarget 'return'
release @6
release @5
release @3
release @2
function_end @1
    `.trim(),
    );
  });

  it('can optimize var refs of function arguments', () => {
    const result = configuredCompile(
      `
      function myFn(value: number): number {
        const value2 = value;
        return value2;
      }
      `,
      { optimizeSlots: false, optimizeVarRefs: true },
      true,
      false,
      false,
      false,
      ['file_ts_myFn'],
    );

    expect(result).toBe(
      `
function_begin 'file_ts_myFn'
getfnarg 0 @4
assign @4 @5
assign @5 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('supports int literals', () => {
    const result = compileSimplified(`const value = 42;`);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 42 @6
assign @6 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('supports long literals', () => {
    const result = compileSimplified(`const value = 123456789012;`);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storelong 123456789012 @6
assign @6 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('supports double literals', () => {
    const result = compileSimplified(`const value = 42.5;`);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storedouble 42.5 @6
assign @6 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('supports double with scientific notation', () => {
    const result = compileSimplified(`const value = 17976931348623157e292;`);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storedouble 1.7976931348623157e+308 @6
assign @6 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('supports string literals', () => {
    const result = compileSimplified(`const value = 'Hello World';`);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getmoduleconst 0 @6
assign @6 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('supports variable', () => {
    const result = compileSimplified(`
        let value = 42;
        let value2 = 43;
        value = value2;
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 42 @7
assign @7 @5
storeint 43 @8
assign @8 @6
assign @6 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles function', () => {
    const result = configuredCompile(
      `
        function myFunction(input: number): number {
          return input;
        }
        `,
      { optimizeSlots: false, optimizeVarRefs: true },
      true,
      false,
      false,
      false,
      undefined,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getfnarg 2 @3
newfn 'file_ts_myFunction' 'myFunction' [ ] @6
assign @6 @5
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_myFunction'
getfnarg 0 @4
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles unary operators', () => {
    const result = compileSimplified(`
        let value = 1;
        let value2: any = +value;
        value2 = -value;
        value2++;
        value2--;
        value2 = ~value;
        value2 = !value;
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 1 @7
assign @7 @5
plus @5 @9
assign @9 @6
neg @5 @11
assign @11 @6
assign @6 @13
inc @6 @14
assign @14 @6
assign @6 @16
dec @6 @17
assign @17 @6
bitwisenot @5 @19
assign @19 @6
logicalnot @5 @21
assign @21 @6
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });
  it('compiles arith binary operators', () => {
    const result = compileSimplified(`
        let value = 1;
        let value2 = 42;
        let value3 = value + value2;
        value3 = value - value2;
        value3 = value * value2;
        value3 = value / value2;
        value3 = value % value2;
        value3 = value ** value2;
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 1 @8
assign @8 @5
storeint 42 @9
assign @9 @6
add @5 @6 @12
assign @12 @7
sub @5 @6 @15
assign @15 @7
mult @5 @6 @18
assign @18 @7
div @5 @6 @21
assign @21 @7
mod @5 @6 @24
assign @24 @7
exp @5 @6 @27
assign @27 @7
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles shift binary operators', () => {
    const result = compileSimplified(`
        let value = 1;
        let value2 = 42;
        let value3 = value >> value2;
        value3 = value << value2;
        value3 = value >>> value2;
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 1 @8
assign @8 @5
storeint 42 @9
assign @9 @6
rightshift @5 @6 @12
assign @12 @7
leftshift @5 @6 @15
assign @15 @7
urightshift @5 @6 @18
assign @18 @7
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles bitwise binary operators', () => {
    const result = compileSimplified(`
        let value = 1;
        let value2 = 42;
        let value3 = value ^ value2;
        value3 = value & value2;
        value3 = value | value2;
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 1 @8
assign @8 @5
storeint 42 @9
assign @9 @6
xor @5 @6 @12
assign @12 @7
and @5 @6 @15
assign @15 @7
or @5 @6 @18
assign @18 @7
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles assignment operators', () => {
    const result = compileSimplified(`
        let value = 1;
        value += 1;
        value -= 2;
        value *= 3;
        value /= 4;
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 1 @6
assign @6 @5
storeint 1 @7
add @5 @7 @9
assign @9 @5
storeint 2 @10
sub @5 @10 @12
assign @12 @5
storeint 3 @13
mult @5 @13 @15
assign @15 @5
storeint 4 @16
div @5 @16 @18
assign @18 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles comma operator', () => {
    const result = compileSimplified(`
      let value1 = 1;
      let value2 = 2;
      let value3 = 3;

      let result = 0;
      result = value1 = 3, value2 = 2, value3 = 1;
    `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 1 @9
assign @9 @5
storeint 2 @10
assign @10 @6
storeint 3 @11
assign @11 @7
storeint 0 @12
assign @12 @8
storeint 1 @13
assign @13 @7
storeint 2 @14
assign @14 @6
storeint 3 @15
assign @15 @5
assign @15 @8
jump 'return'
jumptarget 'return'
function_end @1
    `.trim(),
    );
  });

  it('compiles assignment expression operators', () => {
    const result = compileSimplified(`
        let value = 1;
        let value2 = value += 1;
        value2 = value -= 1;
        value2 = value *= 1;
        value2 = value /= 1;
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 1 @7
assign @7 @5
storeint 1 @8
add @5 @8 @10
assign @10 @5
assign @10 @6
storeint 1 @11
sub @5 @11 @13
assign @13 @5
assign @13 @6
storeint 1 @14
mult @5 @14 @16
assign @16 @5
assign @16 @6
storeint 1 @17
div @5 @17 @19
assign @19 @5
assign @19 @6
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles instancesof', () => {
    const result = compileSimplified(`
        let value: any = 1;
        let value2 = value instanceof value.prototype;
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 1 @7
assign @7 @5
getprop @5 'prototype' @9
instanceof @5 @9 @11
assign @11 @6
jump 'return'
jumptarget 'return'
function_end @1
              `.trim(),
    );
  });

  it('compiles typeof', () => {
    const result = compileSimplified(`
        let value = 1;
        let value2 = typeof value;
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 1 @7
assign @7 @5
typeof @5 @9
assign @9 @6
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles get property', () => {
    const result = compileSimplified(`
        let value = 'hello';
        const value2 = value.length;
        const value3 = value['length'];
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getmoduleconst 0 @8
assign @8 @5
getprop @5 'length' @10
assign @10 @6
getmoduleconst 1 @12
getpropvalue @5 @12 @13
assign @13 @7
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles get property index', () => {
    const result = compileSimplified(`
        let value = [42, 43];
        const value2 = value[1];
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newarray @7
storeint 0 @8
storeint 42 @9
setpropv @7 @8 @9
inc @8 @10
storeint 43 @11
setpropv @7 @10 @11
inc @10 @12
assign @7 @5
storeint 1 @14
getpropvalue @5 @14 @15
assign @15 @6
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles object literals', () => {
    const result = compileSimplified(`
        const value = {};
        const value2 = {hello: value};
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @7
assign @7 @5
newobject @8
setprop @8 'hello' @5
assign @8 @6
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles object literals with int key', () => {
    const result = compileSimplified(`
        const value = {};
        const value2 = {1: value, 2: false};
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @7
assign @7 @5
newobject @8
storeint 1 @9
setpropv @8 @9 @5
storeint 2 @11
storebool false @12
setpropv @8 @11 @12
assign @8 @6
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('comiles object literal with string key', () => {
    const result = compileSimplified(`
        const value = {'hello': 'world'};
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @6
getmoduleconst 0 @7
getmoduleconst 1 @8
setpropv @6 @7 @8
assign @6 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('comiles object literal with method', () => {
    const result = compileSimplified(
      `
        const value = {
          name(): string {
            return 'Hello World';
          }
        };
        `,
      ['file_ts__module_init__'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @6
newfn 'file_ts_value_name' 'name' [ ] @7
setprop @6 'name' @7
assign @6 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles array literals', () => {
    const result = compileSimplified(`
        const value = [];
        const value2 = [value];
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newarray @7
assign @7 @5
newarray @8
storeint 0 @9
setpropv @8 @9 @5
inc @9 @11
assign @8 @6
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles array spread operator', () => {
    const result = compileSimplified(`
        const value = [];
        const value2 = [...value];
        const value3 = [...value, 42];
        const value4 = [42, ...value];
        const value5 = [42, ...value, 43];
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newarray @10
assign @10 @5
newarray @11
storeint 0 @12
copyprops @5 @11
add @12 @14 @15
assign @11 @6
newarray @16
storeint 0 @17
copyprops @5 @16
add @17 @19 @20
storeint 42 @21
setpropv @16 @20 @21
inc @20 @22
assign @16 @7
newarray @23
storeint 0 @24
storeint 42 @25
setpropv @23 @24 @25
inc @24 @26
copyprops @5 @23
add @26 @28 @29
assign @23 @8
newarray @30
storeint 0 @31
storeint 42 @32
setpropv @30 @31 @32
inc @31 @33
copyprops @5 @30
add @33 @35 @36
storeint 43 @37
setpropv @30 @36 @37
inc @36 @38
assign @30 @9
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles object spread operator', () => {
    const result = compileSimplified(`
        const value = {};
        const value2 = {...value};
        const value3 = {...value, prop: 42};
        const value4 = {propA: 42, ...value};
        const value5 = {propA: 42, ...value, propB: 43};
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @10
assign @10 @5
newobject @11
copyprops @5 @11
assign @11 @6
newobject @14
copyprops @5 @14
storeint 42 @17
setprop @14 'prop' @17
assign @14 @7
newobject @18
storeint 42 @19
setprop @18 'propA' @19
copyprops @5 @18
assign @18 @8
newobject @22
storeint 42 @23
setprop @22 'propA' @23
copyprops @5 @22
storeint 43 @26
setprop @22 'propB' @26
assign @22 @9
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles for of loops', () => {
    const result = compileSimplified(`
        const array = [42];
        let current = 0;
        for (const item of array) {
          current += item;
        }
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newarray @7
storeint 0 @8
storeint 42 @9
setpropv @7 @8 @9
inc @8 @10
assign @7 @5
storeint 0 @11
assign @11 @6
iterator @5 @13
jumptarget 'cond'
iteratornext @13 @14
branch @13 'body' 'exit'
jumptarget 'body'
assign @14 @15
add @6 @15 @18
assign @18 @6
jump 'cond'
jumptarget 'exit'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles for of loops with no variable', () => {
    const result = compileSimplified(`
        const array = [42];
        let current = 0;
        let item: number;
        for (item of array) {
          current += item;
        }
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newarray @8
storeint 0 @9
storeint 42 @10
setpropv @8 @9 @10
inc @9 @11
assign @8 @5
storeint 0 @12
assign @12 @6
assign @13 @7
iterator @5 @15
jumptarget 'cond'
iteratornext @15 @16
branch @15 'body' 'exit'
jumptarget 'body'
add @6 @7 @19
assign @19 @6
jump 'cond'
jumptarget 'exit'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles for loops', () => {
    const result = compileSimplified(`
        const array = [42];
        let current = 0;
        for (let i = 0; i < array.length; i++) {
          current += array[i];
        }
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newarray @7
storeint 0 @8
storeint 42 @9
setpropv @7 @8 @9
inc @8 @10
assign @7 @5
storeint 0 @11
assign @11 @6
storeint 0 @13
assign @13 @12
jumptarget 'cond'
getprop @5 'length' @15
lt @12 @15 @17
branch @17 'body' 'exit'
jumptarget 'body'
getpropvalue @5 @12 @20
add @6 @20 @22
assign @22 @6
assign @12 @24
inc @12 @25
assign @25 @12
jump 'cond'
jumptarget 'exit'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles for in loops', () => {
    expect(() => {
      const result = compileSimplified(`
          const object = {};
          const allKeys: string[] = [];
          for (const key in object) {
            allKeys.push(key);
          }
          `);

      expect(result).toEqual(
        `
function_begin 'file_ts__module_init__'
newarray @6
storeint 0 @7
storeint 42 @8
setpropv @6 @7 @8
inc @7 @9
assign @6 @5
storeint 0 @11
assign @11 @10
storeint 0 @13
assign @13 @12
jumptarget 'cond'
getprop @5 'length' @16
lt @12 @16 @17
branch @17 'body' 'exit'
jumptarget 'body'
getpropvalue @5 @12 @20
add @10 @20 @22
assign @22 @10
assign @12 @24
inc @12 @25
assign @25 @12
jump 'cond'
jumptarget 'exit'
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
      );
    }).toThrow(); // not implemented yet
  });

  it('compiles continue statement', () => {
    const result = compileSimplified(
      `
    function process(input: number[]): number {
      let output = 0;
      for (let i = 0; i < input.length; i++) {
        if (i % 2 === 0) {
          continue;
        }
        output += input[i];
      }


      return output;
    }
        `,
      ['file_ts_process'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_process'
storeint 0 @6
assign @6 @5
storeint 0 @8
assign @8 @7
jumptarget 'cond'
getprop @4 'length' @10
lt @7 @10 @12
branch @12 'body' 'exit'
jumptarget 'body'
jumptarget 'condition'
storeint 0 @13
storeint 2 @14
mod @7 @14 @16
eqstrict @16 @13 @17
branch @17 'true' 'false'
jumptarget 'true'
jump 'cond'
jump 'exit'
jumptarget 'false'
jumptarget 'exit'
getpropvalue @4 @7 @20
add @5 @20 @22
assign @22 @5
assign @7 @24
inc @7 @25
assign @25 @7
jump 'cond'
jumptarget 'exit'
assign @5 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles while loops', () => {
    const result = compileSimplified(`
        let current = 0;
        while (current < 0) {
          current++;
        }
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 0 @6
assign @6 @5
jumptarget 'cond'
storeint 0 @7
lt @5 @7 @9
branch @9 'body' 'exit'
jumptarget 'body'
assign @5 @11
inc @5 @12
assign @12 @5
jump 'cond'
jumptarget 'exit'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compile do while loops', () => {
    const result = compileSimplified(`
        let current = 0;
        do {
          current++;
        } while (current < 0);
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 0 @6
assign @6 @5
jumptarget 'body'
assign @5 @8
inc @5 @9
assign @9 @5
jumptarget 'cond'
storeint 0 @10
lt @5 @10 @12
branch @12 'body' 'exit'
jumptarget 'exit'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles function calls', () => {
    const result = configuredCompile(
      `
        function multiply(left: number, right: number): number {
          return left * right;
        }
        multiply(2, 4);
        `,
      { optimizeSlots: false, optimizeVarRefs: true },
      false,
      false,
      false,
      false,
      ['file_ts__module_init__'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newfn 'file_ts_multiply' 'multiply' [ ] @6
assign @6 @5
storeint 2 @8
storeint 4 @9
call @5 @5 [ @8, @9 ] @10
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles lambda', () => {
    const result = compileSimplified(`
        declare function reduce(input: number[], initial: number, acc: (cur: number, input: number) => number): number;

        reduce([1, 2, 3], 0, (c, i) => c + i);
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getglobal @5
getprop @5 'reduce' @6
newarray @7
storeint 0 @8
storeint 1 @9
setpropv @7 @8 @9
inc @8 @10
storeint 2 @11
setpropv @7 @10 @11
inc @10 @12
storeint 3 @13
setpropv @7 @12 @13
inc @12 @14
storeint 0 @15
newarrowfn 'file_ts_anon' [ ] @16
call @6 @6 [ @7, @15, @16 ] @17
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_anon'
add @5 @6 @9
assign @9 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles lambda with immutable captured values', () => {
    const result = compileSimplified(`
        declare function reduce(input: number[], initial: number, acc: (cur: number, input: number) => number): number;

        const additional = 10;
        reduce([1, 2, 3], 0, (c, i) => c + i + additional);
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref 'additional' @5
storeint 10 @6
setvref @6 @5
getglobal @7
getprop @7 'reduce' @8
newarray @9
storeint 0 @10
storeint 1 @11
setpropv @9 @10 @11
inc @10 @12
storeint 2 @13
setpropv @9 @12 @13
inc @12 @14
storeint 3 @15
setpropv @9 @14 @15
inc @14 @16
storeint 0 @17
newarrowfn 'file_ts_anon' [ @5 ] @18
call @8 @8 [ @9, @17, @18 ] @19
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_anon'
getclosurearg 0 @7
loadvref @7 @8
add @5 @6 @11
add @11 @8 @12
assign @12 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles lambda with mutable captured values', () => {
    const result = compileSimplified(`
        declare function reduce(input: number[], initial: number, acc: (cur: number, input: number) => number): number;

        let additional = 10;
        reduce([1, 2, 3], 0, (c, i) => {
          const result = c + i + additional;
          additional++;
          return result;
        });
        additional++;
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref 'additional' @5
storeint 10 @6
setvref @6 @5
getglobal @7
getprop @7 'reduce' @8
newarray @9
storeint 0 @10
storeint 1 @11
setpropv @9 @10 @11
inc @10 @12
storeint 2 @13
setpropv @9 @12 @13
inc @12 @14
storeint 3 @15
setpropv @9 @14 @15
inc @14 @16
storeint 0 @17
newarrowfn 'file_ts_anon' [ @5 ] @18
call @8 @8 [ @9, @17, @18 ] @19
loadvref @5 @20
assign @20 @21
inc @20 @22
setvref @22 @5
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_anon'
getclosurearg 0 @8
loadvref @8 @9
add @5 @6 @12
add @12 @9 @13
assign @13 @7
loadvref @8 @14
assign @14 @15
inc @14 @16
setvref @16 @8
assign @7 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles lambda with nested immutable captured values', () => {
    const result = compileSimplified(`
        declare function visit(input: number[], getVisitor: () => (input: number) => number);

        const additional = 10;
        visit([1, 2, 3], () => {
          return (i) => i + additional;
        })
        `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref 'additional' @5
storeint 10 @6
setvref @6 @5
getglobal @7
getprop @7 'visit' @8
newarray @9
storeint 0 @10
storeint 1 @11
setpropv @9 @10 @11
inc @10 @12
storeint 2 @13
setpropv @9 @12 @13
inc @12 @14
storeint 3 @15
setpropv @9 @14 @15
inc @14 @16
newarrowfn 'file_ts_anon' [ @5 ] @17
call @8 @8 [ @9, @17 ] @18
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_anon'
getclosurearg 0 @3
newarrowfn 'file_ts_anon_0' [ @3 ] @4
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_anon_0'
getclosurearg 0 @5
loadvref @5 @6
add @4 @6 @8
assign @8 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles self referencing functions', () => {
    const result = configuredCompile(
      `
      function add(input: number, max: number): number {
        return input < max ? add(input + 1, max) : input;
      }
      `,
      { optimizeSlots: true, optimizeVarRefs: true },
      false,
      false,
      false,
      false,
      undefined,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref 'add' @3
newfn 'file_ts_add' 'add' [ @3 ] @2
setvref @2 @3
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_add'
getclosurearg 0 @2
jumptarget 'condition'
lt @3 @4 @5
branch @5 'condition_true' 'condition_false'
jumptarget 'condition_true'
loadvref @2 @6
storeint 1 @7
add @3 @7 @8
call @6 @6 [ @8, @4 ] @9
assign @9 @4
jump 'exit'
jumptarget 'condition_false'
assign @3 @4
jump 'exit'
jumptarget 'exit'
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
            `.trim(),
    );
  });

  it('compiles for of loops', () => {
    const result = compileSimplified(
      `
      function add(items: number[]): number {
        let output = 0;
        for (const item of items) {
          output += item;
        }

        return output;
      }
        `,
      ['file_ts_add'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_add'
storeint 0 @6
assign @6 @5
iterator @4 @8
jumptarget 'cond'
iteratornext @8 @9
branch @8 'body' 'exit'
jumptarget 'body'
assign @9 @10
add @5 @10 @13
assign @13 @5
jump 'cond'
jumptarget 'exit'
assign @5 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
    `.trim(),
    );
  });

  it('compiles for in loops', () => {
    const result = compileSimplified(
      `
      function add(items: number[]): number {
        let output = 0;
        for (const k in items) {
          output += items[k];
        }

        return output;
      }
        `,
      ['file_ts_add'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_add'
storeint 0 @6
assign @6 @5
keysiterator @4 @8
jumptarget 'cond'
keysiteratornext @8 @9
branch @8 'body' 'exit'
jumptarget 'body'
assign @9 @10
getpropvalue @4 @10 @13
add @5 @13 @15
assign @15 @5
jump 'cond'
jumptarget 'exit'
assign @5 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
    `.trim(),
    );
  });

  it('compiles namespace', () => {
    const result = compileSimplified(
      `
        export namespace MyNamespace {
          export const VALUE = 42;
        }
        `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @6
assign @6 @5
setprop @3 'MyNamespace' @6
storeint 42 @9
assign @9 @8
setprop @5 'VALUE' @9
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles class', () => {
    const result = compileSimplified(
      `
      class MyClass {
      }
        `,
      undefined,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newclass 'file_ts_MyClass' 'MyClass' @6 [ ] @7
assign @7 @5
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_MyClass'
assign @3 @4
storeexception @5
new @3 [ ] @6
assign @6 @4
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles class with initialized property', () => {
    const result = compileSimplified(
      `
      class MyClass {
        value = 42;
      }
        `,
      ['file_ts_MyClass'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_MyClass'
assign @3 @4
storeexception @5
new @3 [ ] @6
assign @6 @4
storeint 42 @9
setprop @4 'value' @9
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles class with static initialized property', () => {
    const result = compileSimplified(
      `
      class MyClass {
        static value = 42;
      }
        `,
      ['file_ts__module_init__'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newclass 'file_ts_MyClass' 'MyClass' @6 [ ] @7
assign @7 @5
storeint 42 @8
setprop @7 'value' @8
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles class with method', () => {
    const result = compileSimplified(
      `
      class MyClass {
        sayHello(): void {

        }
      }
        `,
      ['file_ts__module_init__'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newclass 'file_ts_MyClass' 'MyClass' @6 [ ] @7
assign @7 @5
newfn 'file_ts_MyClass_sayHello' 'sayHello' [ ] @8
setclassmethod @7 'sayHello' @8
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles class with static method', () => {
    const result = compileSimplified(
      `
      class MyClass {
        static sayHello(): void {

        }
      }
        `,
      ['file_ts__module_init__'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newclass 'file_ts_MyClass' 'MyClass' @6 [ ] @7
assign @7 @5
newfn 'file_ts_MyClass_sayHello' 'sayHello' [ ] @8
setclassstaticmethod @7 'sayHello' @8
jump 'return'
jumptarget 'return'
function_end @1
    `.trim(),
    );
  });

  it('compiles class with getter', () => {
    const result = compileSimplified(
      `
      class MyClass {
        get value(): string {
          return this.toString();
        }
      }
        `,
      ['file_ts__module_init__', 'file_ts_MyClass__get__value'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newclass 'file_ts_MyClass' 'MyClass' @6 [ ] @7
assign @7 @5
newfn 'file_ts_MyClass__get__value' 'value' [ ] @8
setclassgetter @7 'value' @8
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_MyClass__get__value'
storethis @3
assign @3 @4
getprop @4 'toString' @6
call @6 @4 [ ] @7
assign @7 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles class with setter', () => {
    const result = compileSimplified(
      `
      class MyClass {
        private _value?: string;
        set value(v: string) {
          this._value = v;
        }
      }
        `,
      ['file_ts__module_init__', 'file_ts_MyClass__set__value'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newclass 'file_ts_MyClass' 'MyClass' @6 [ ] @7
assign @7 @5
newfn 'file_ts_MyClass__set__value' 'value' [ ] @8
setclasssetter @7 'value' @8
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_MyClass__set__value'
storethis @6
assign @6 @7
setprop @7 '_value' @4
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles class with static getter', () => {
    const result = compileSimplified(
      `
      let value = 'Hello World';
      class MyClass {
        static get value(): string {
          return value;
        }
      }
        `,
      ['file_ts__module_init__', 'file_ts_MyClass__get__value'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref 'value' @5
getmoduleconst 0 @7
setvref @7 @5
newclass 'file_ts_MyClass' 'MyClass' @8 [ ] @9
assign @9 @6
newfn 'file_ts_MyClass__get__value' 'value' [ @5 ] @10
setclassstaticgetter @9 'value' @10
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_MyClass__get__value'
getclosurearg 0 @3
loadvref @3 @4
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles class with static setter', () => {
    const result = compileSimplified(
      `
      let value = 'Hello World';
      class MyClass {
        static set value(v: string) {
          value = v;
        }
      }
        `,
      ['file_ts__module_init__', 'file_ts_MyClass__set__value'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref 'value' @5
getmoduleconst 0 @7
setvref @7 @5
newclass 'file_ts_MyClass' 'MyClass' @8 [ ] @9
assign @9 @6
newfn 'file_ts_MyClass__set__value' 'value' [ @5 ] @10
setclassstaticsetter @9 'value' @10
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_MyClass__set__value'
getclosurearg 0 @6
setvref @4 @6
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles class with getter/setter from value', () => {
    const result = compileSimplified(
      `
      const key = Symbol();
      class MyClass {
        get [key]() {
          return undefined;
        }

        set [key](v) {
        }
      }
        `,
      ['file_ts__module_init__'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getglobal @7
getprop @7 'Symbol' @8
call @8 @8 [ ] @9
assign @9 @5
newclass 'file_ts_MyClass' 'MyClass' @10 [ ] @11
assign @11 @6
newfn 'file_ts_MyClass__get__anon' 'anonymous' [ ] @13
setclassgetterv @11 @5 @13
newfn 'file_ts_MyClass__set__anon' 'anonymous' [ ] @15
setclasssetterv @11 @5 @15
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles class with constructor', () => {
    const result = compileSimplified(
      `
      class MyClass {
        value: number;

        constructor(value: number) {
          this.value = value;
        }
      }
        `,
      ['file_ts_MyClass'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_MyClass'
assign @3 @4
storeexception @7
new @3 [ ] @8
assign @8 @4
setprop @4 'value' @6
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('initializes class property first before evaluating constructor block', () => {
    const result = compileSimplified(
      `
      class MyClass {
        value = 42;

        constructor(value: number) {
          this.value = value;
        }
      }
        `,
      ['file_ts_MyClass'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_MyClass'
assign @3 @4
storeexception @7
new @3 [ ] @8
assign @8 @4
storeint 42 @11
setprop @4 'value' @11
setprop @4 'value' @6
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles class with parameter property', () => {
    const result = compileSimplified(
      `
      class MyClass {
        constructor(readonly value: number) {
        }
      }
        `,
      ['file_ts_MyClass'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_MyClass'
assign @3 @4
storeexception @7
new @3 [ ] @8
assign @8 @4
setprop @4 'value' @6
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles class with inheritance', () => {
    const result = compileSimplified(
      `
      class Parent {
        value: number;

        constructor(value: number) {
          this.value = value;
        }
      }

      class Child extends Parent {
        value2 = 42;

        constructor(value: number) {
          super(value);

          this.value++;
        }
      }
        `,
      ['file_ts__module_init__', 'file_ts_Child'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newclass 'file_ts_Parent' 'Parent' @7 [ ] @8
assign @8 @5
newclass 'file_ts_Child' 'Child' @5 [ ] @10
assign @10 @6
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_Child'
assign @3 @4
getsuperctor @7
storeexception @10
new @7 [ @6 ] @11
assign @11 @4
storeint 42 @14
setprop @4 'value2' @14
getprop @4 'value' @16
assign @16 @17
inc @16 @18
assign @18 @16
setprop @4 'value' @18
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('respects class init ordering', () => {
    const result = compileSimplified(
      `
      class Parent {
        value: number;

        constructor(value: number) {
          this.value = value;
        }
      }

      class Child extends Parent {
        value = 42;
      }
        `,
      ['file_ts_Child'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_Child'
assign @3 @4
getsuperctor @5
storeexception @6
getfnarguments 0 @7
newv @5 @7 @8
assign @8 @4
storeint 42 @11
setprop @4 'value' @11
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles class expression', () => {
    const result = compileSimplified(
      `
      const MyClass = class {
        value: number;

        constructor(value: number) {
          this.value = value;
        }
      }

      const test = new MyClass(42);
        `,
      ['file_ts__module_init__'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newclass 'file_ts_MyClass_anon' 'anonymous' @7 [ ] @8
assign @8 @5
storeint 42 @10
new @5 [ @10 ] @11
assign @11 @6
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles directly initialized class expression', () => {
    const result = compileSimplified(
      `
      const test = new (class {
        value: number;

        constructor(value: number) {
          this.value = value;
        }
      })(42);
        `,
      ['file_ts__module_init__'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newclass 'file_ts_test_anon' 'anonymous' @6 [ ] @7
storeint 42 @8
new @7 [ @8 ] @9
assign @9 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('class expression can reference self', () => {
    const result = compileSimplified(
      `
      const MyClass = class MyName {
        static lastValue?: number;

        constructor(value: number) {
          MyName.lastValue = value;
          MyClass.lastValue = value;
        }
      }
        `,
      undefined,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref 'MyClass' @5
newvref 'MyName' @6
newclass 'file_ts_MyClass_MyName' 'MyName' @7 [ @6, @5 ] @8
setvref @8 @6
setvref @8 @5
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_MyClass_MyName'
getclosurearg 0 @11
getclosurearg 1 @14
assign @3 @4
storeexception @7
new @3 [ ] @8
assign @8 @4
loadvref @11 @12
setprop @12 'lastValue' @6
loadvref @14 @15
setprop @15 'lastValue' @6
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles nested lambda accessing this', () => {
    const result = compileSimplified(
      `function myFunction(this: any): void {
        const fn = () => {
          return this.value;
        }
      }
        `,
      ['file_ts_myFunction', 'file_ts_myFunction_fn_anon'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_myFunction'
newvref 'this' @5
storethis @4
setvref @4 @5
newarrowfn 'file_ts_myFunction_fn_anon' [ @5 ] @6
assign @6 @3
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_myFunction_fn_anon'
getclosurearg 0 @3
loadvref @3 @4
getprop @4 'value' @5
assign @5 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
    `.trim(),
    );
  });

  it('compiles regex', () => {
    const result = compileSimplified(`
    const regex = /-/g;
    `);

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getglobal @6
getprop @6 'RegExp' @7
getmoduleconst 0 @8
getmoduleconst 1 @9
new @7 [ @8, @9 ] @10
assign @10 @5
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles object binding pattern', () => {
    const result = compileSimplified(
      `
    function myFunction(input: any): any {
      const {title, subtitle} = input;
      return title;
    }
    `,
      ['file_ts_myFunction'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_myFunction'
getprop @4 'title' @8
assign @8 @5
getprop @4 'subtitle' @9
assign @9 @6
assign @5 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles object binding pattern with rename', () => {
    const result = compileSimplified(
      `
    function myFunction(input: any): any {
      const {TITLE: title} = input;
      return title;
    }
    `,
      ['file_ts_myFunction'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_myFunction'
getprop @4 'TITLE' @7
assign @7 @5
assign @5 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles object binding pattern with rest', () => {
    const result = compileSimplified(
      `
    function myFunction(input: any): any {
      const {title, ...rest} = input;
      return rest;
    }
    `,
      ['file_ts_myFunction'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_myFunction'
getprop @4 'title' @8
assign @8 @5
newobject @9
copyprops @4 @9 [ 'title' ]
assign @9 @6
assign @6 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles array binding pattern', () => {
    const result = compileSimplified(
      `
    function myFunction(input: string[]): any {
      const [title, subtitle] = input;
      return title;
    }
    `,
      ['file_ts_myFunction'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_myFunction'
storeint 0 @8
getpropvalue @4 @8 @9
assign @9 @5
storeint 1 @10
getpropvalue @4 @10 @11
assign @11 @6
assign @5 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles array binding pattern with rest', () => {
    const result = compileSimplified(
      `
    function myFunction(input: string[]): any {
      const [title, ...subtitle] = input;
      return subtitle;
    }
    `,
      ['file_ts_myFunction'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_myFunction'
storeint 0 @8
getpropvalue @4 @8 @9
assign @9 @5
getprop @4 'slice' @10
storeint 1 @11
call @10 @4 [ @11 ] @12
assign @12 @6
assign @6 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles binding pattern with default value', () => {
    const result = compileSimplified(
      `
    function myFunction(input: any): any {
      const {title, subtitle = 'Nice'} = input;
      return subtitle;
    }
    `,
      ['file_ts_myFunction'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_myFunction'
getprop @4 'title' @8
assign @8 @5
getprop @4 'subtitle' @9
jumptarget 'condition'
nestrict @9 @11 @12
branch @12 'condition_true' 'condition_false'
jumptarget 'condition_true'
assign @9 @10
jump 'exit'
jumptarget 'condition_false'
getmoduleconst 0 @13
assign @13 @10
jump 'exit'
jumptarget 'exit'
assign @10 @6
assign @6 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles binding pattern in method arguments', () => {
    const result = compileSimplified(
      `

      const lambda: (items: string[]) => string = ([first, second]) => {
        return first + second;
      };
    `,
      ['file_ts_lambda_anon'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_lambda_anon'
storeint 0 @6
getpropvalue @5 @6 @7
assign @7 @3
storeint 1 @8
getpropvalue @5 @8 @9
assign @9 @4
add @3 @4 @12
assign @12 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles nested binding pattern', () => {
    const result = compileSimplified(
      `
    function myFunction(input: any): any {
      const {a, b: {c: d}} = input;
      return d;
    }
    `,
      ['file_ts_myFunction'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_myFunction'
getprop @4 'b' @8
getprop @8 'c' @9
assign @9 @6
getprop @4 'a' @10
assign @10 @5
assign @6 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles void expression', () => {
    const result = compileSimplified(
      `
      const test = void {};
    `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @6
assign @7 @5
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles type assertion', () => {
    const result = compileSimplified(
      `
      const test = <any> {};
    `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @6
assign @6 @5
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles arguments parameter', () => {
    const result = compileSimplified(
      `
      function countArguments(): number {
        return arguments.length;
      }
    `,
      ['file_ts_countArguments'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_countArguments'
getfnarguments 0 @4
assign @4 @3
getprop @3 'length' @6
assign @6 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles spread arguments in calls', () => {
    const result = compileSimplified(
      `
      declare function join(...strs: string[]): string;

      const items = ['Hello', 'World'];
      const result = join(...items);
    `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newarray @7
storeint 0 @8
getmoduleconst 0 @9
setpropv @7 @8 @9
inc @8 @10
getmoduleconst 1 @11
setpropv @7 @10 @11
inc @10 @12
assign @7 @5
getglobal @13
getprop @13 'join' @14
newarray @15
storeint 0 @16
copyprops @5 @15
add @16 @18 @19
callv @14 @14 @15 @20
assign @20 @6
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles spread arguments in new expression', () => {
    const result = compileSimplified(
      `
      declare class Components {
        constructor(initial: string, ...rest: string[]);
      }

      const components = new Components('Prefix', ...['Hello', 'World']);
    `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getglobal @6
getprop @6 'Components' @7
newarray @8
storeint 0 @9
getmoduleconst 0 @10
setpropv @8 @9 @10
inc @9 @11
newarray @12
storeint 0 @13
getmoduleconst 1 @14
setpropv @12 @13 @14
inc @13 @15
getmoduleconst 2 @16
setpropv @12 @15 @16
inc @15 @17
copyprops @12 @8
add @11 @18 @19
newv @7 @8 @20
assign @20 @5
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles spread arguments in function', () => {
    const result = compileSimplified(
      `
      declare class Components {
        constructor(initial: string, ...rest: string[]);
      }

      function join(separator: string, ...rest: string[]): string {
        return rest.join(separator);
      }
    `,
      ['file_ts_join'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_join'
getfnarguments 1 @6
assign @6 @4
getprop @4 'join' @8
call @8 @4 [ @5 ] @10
assign @10 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('respects scope in variables', () => {
    const result = compileSimplified(
      `
      {
        const value = 42;
      }
      {
        const value = 43;
      }
    `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 42 @6
assign @6 @5
storeint 43 @8
assign @8 @7
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles delete expression', () => {
    const result = compileSimplified(
      `
      const obj: any = {title: {name: 'Hello'}};
      delete obj.title.name;
    `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @6
newobject @7
getmoduleconst 0 @8
setprop @7 'name' @8
setprop @6 'title' @7
assign @6 @5
getprop @5 'title' @10
delprop @10 'name' @11
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles function with default parameter value', () => {
    const result = compileSimplified(
      `
      function mutate(animated: boolean = false): boolean {
        return animated;
      }
    `,
      ['file_ts_mutate'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_mutate'
jumptarget 'condition'
nestrict @4 @6 @7
branch @7 'condition_true' 'condition_false'
jumptarget 'condition_true'
assign @4 @5
jump 'exit'
jumptarget 'condition_false'
storebool false @8
assign @8 @5
jump 'exit'
jumptarget 'exit'
assign @5 @3
assign @3 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles export declaration', () => {
    const result = configuredCompile(
      `
      const VALUE = 42;
      export { VALUE };
    `,
      { optimizeSlots: false, optimizeVarRefs: true },
      true,
      false,
      false,
      false,
      undefined,
      undefined,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getfnarg 2 @3
storeint 42 @6
assign @6 @5
setprop @3 'VALUE' @5
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles export declaration binding', () => {
    const result = configuredCompile(
      `
      export { VALUE } from './other';
    `,
      { optimizeSlots: false, optimizeVarRefs: true },
      true,
      false,
      false,
      false,
      undefined,
      (workspace) => {
        workspace.registerInMemoryFile(
          'other.ts',
          `
        export const VALUE = 42;
        `,
        );
      },
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getfnarg 0 @5
getfnarg 2 @3
getmoduleconst 0 @8
call @5 @9 [ @8 ] @10
getprop @10 'VALUE' @11
setprop @3 'VALUE' @11
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles import module expression within lambda', () => {
    const result = configuredCompile(
      `
      const getValue = () => import('./other');
    `,
      { optimizeSlots: false, optimizeVarRefs: true },
      true,
      false,
      false,
      false,
      undefined,
      (workspace) => {
        workspace.registerInMemoryFile(
          'other.ts',
          `
        export const VALUE = 42;
        `,
        );
      },
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref 'require' @7
getfnarg 0 @6
setvref @6 @7
getfnarg 2 @3
newarrowfn 'file_ts_getValue_anon' [ @7 ] @8
assign @8 @5
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_getValue_anon'
getclosurearg 0 @3
loadvref @3 @4
getmoduleconst 0 @5
call @4 @6 [ @5 ] @7
getglobal @8
getprop @8 'Promise' @9
getprop @9 'resolve' @10
call @10 @9 [ @7 ] @11
assign @11 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles optional element access expression', () => {
    const result = compileSimplified(
      `
      declare const global: any;
      const value = global.errorHandlersByContextId?.[42];
    `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
jumptarget 'optional_chain'
getglobal @7
getprop @7 'global' @8
getprop @8 'errorHandlersByContextId' @9
jumptarget 'value_condition'
nullable_branch @9 'value_true' 'value_false'
jumptarget 'value_true'
storeint 42 @11
getpropvalue @9 @11 @12
assign @12 @10
jump 'value_exit'
jumptarget 'value_false'
assign @13 @10
jump 'value_exit'
jumptarget 'value_exit'
jumptarget 'optional_chain_exit'
assign @10 @5
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles optional property access expression within if', () => {
    const result = compileSimplified(
      `
      let component: any = {};
      if (component?.onError) {
        component.onError();
      } else {
        component = {};
      }
    `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @6
assign @6 @5
jumptarget 'condition'
jumptarget 'optional_chain'
jumptarget 'value_condition'
nullable_branch @5 'value_true' 'value_false'
jumptarget 'value_true'
getprop @5 'onError' @10
assign @10 @9
jump 'value_exit'
jumptarget 'value_false'
assign @11 @9
jump 'value_exit'
jumptarget 'value_exit'
assign @9 @7
jumptarget 'optional_chain_exit'
branch @7 'true' 'false'
jumptarget 'true'
getprop @5 'onError' @13
call @13 @5 [ ] @14
jump 'exit'
jumptarget 'false'
newobject @15
assign @15 @5
jumptarget 'exit'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles optional property access expression within call expression', () => {
    const result = compileSimplified(
      `
      let component: any = {};
      component.ref?.onError();
    `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @6
assign @6 @5
jumptarget 'optional_chain'
getprop @5 'ref' @9
jumptarget 'value_condition'
nullable_branch @9 'value_true' 'value_false'
jumptarget 'value_true'
getprop @9 'onError' @11
call @11 @9 [ ] @12
assign @12 @10
jump 'value_exit'
jumptarget 'value_false'
assign @13 @10
jump 'value_exit'
jumptarget 'value_exit'
assign @10 @7
jumptarget 'optional_chain_exit'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('compiles default export', () => {
    const result = compileSimplified(
      `
      const TEST = 42;
      export default TEST;
    `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 42 @6
assign @6 @5
setprop @3 'default' @5
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles default export on function', () => {
    const result = compileSimplified(
      `
      export default function myFn() {};
    `,
      ['file_ts__module_init__'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newfn 'file_ts_myFn' 'myFn' [ ] @6
assign @6 @5
setprop @3 'default' @6
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles default export as assignment', () => {
    const result = compileSimplified(
      `
      const TEST = 42;
      export = TEST;
    `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 42 @6
assign @6 @5
setprop @8 'exports' @5
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('compiles import equal statement', () => {
    const result = compileSimplified(
      `
        declare namespace Outer {
          export namespace Nested {
            export const VALUE: number;
          }
        }

        import VALUE = Outer.Nested.VALUE;
        const result = VALUE * 2;
        `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getglobal @7
getprop @7 'Outer' @8
getprop @8 'Nested' @9
getprop @9 'VALUE' @10
assign @10 @5
storeint 2 @11
mult @5 @11 @13
assign @13 @6
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles default import statement', () => {
    const result = configuredCompile(
      `
      import resources from './res';

      resources.value = 42;
    `,
      { optimizeSlots: false, optimizeVarRefs: true },
      false,
      false,
      false,
      false,
      undefined,
      (workspace) => {
        workspace.registerInMemoryFile(
          'res.ts',
          `
          const obj = {
            value: 0
          };
          export default obj;
        `,
        );
      },
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getmoduleconst 0 @10
call @7 @11 [ @10 ] @12
assign @12 @6
storeint 42 @5
getprop @6 'default' @14
setprop @14 'value' @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('does not emit import for type import', () => {
    const result = configuredCompile(
      `
      import { Number } from './Number';

      const nb: Number = 42;
    `,
      { optimizeSlots: false, optimizeVarRefs: true },
      false,
      false,
      false,
      false,
      undefined,
      (workspace) => {
        workspace.registerInMemoryFile(
          'Number.ts',
          `
          export type Number = number;
        `,
        );
      },
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 42 @6
assign @6 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('does not emit export for type export', () => {
    const result = configuredCompile(
      `
      export { Number } from './Number';
    `,
      { optimizeSlots: false, optimizeVarRefs: true },
      false,
      false,
      false,
      false,
      undefined,
      (workspace) => {
        workspace.registerInMemoryFile(
          'Number.ts',
          `
          export type Number = number;
        `,
        );
      },
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('can use imported symbols in functions', () => {
    const result = configuredCompile(
      `
      import { VALUE } from './Config';

      function resolve(): () => number {
        return () => VALUE;
      }
    `,
      { optimizeSlots: false, optimizeVarRefs: true },
      false,
      false,
      false,
      false,
      undefined,
      (workspace) => {
        workspace.registerInMemoryFile(
          'Config.ts',
          `
          export const VALUE = 42;
        `,
        );
      },
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref '$imported_module' @6
getmoduleconst 0 @10
call @7 @11 [ @10 ] @12
setvref @12 @6
newfn 'file_ts_resolve' 'resolve' [ @6 ] @13
assign @13 @5
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_resolve'
getclosurearg 0 @3
newarrowfn 'file_ts_resolve_anon' [ @3 ] @4
assign @4 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_resolve_anon'
getclosurearg 0 @3
loadvref @3 @4
getprop @4 'VALUE' @5
assign @5 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('compiles in operator', () => {
    const result = compileSimplified(
      `
      const obj: any = {};
      const result = 'hello' in obj;
    `,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @7
assign @7 @5
getmoduleconst 0 @9
in @9 @5 @10
assign @10 @6
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('respects var scoping rules', () => {
    const result = compileSimplified(
      `
      function count(items: number[]): number {
        for (var i = 0; i < items.length; i++) {}

        return i;
      }
        `,
      ['file_ts_count'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_count'
storeint 0 @6
assign @6 @5
jumptarget 'cond'
getprop @4 'length' @8
lt @5 @8 @10
branch @10 'body' 'exit'
jumptarget 'body'
assign @5 @12
inc @5 @13
assign @13 @5
jump 'cond'
jumptarget 'exit'
assign @5 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('allows var reassignment', () => {
    const result = compileSimplified(
      `
      var p = 42;
      var p: number;
        `,
    );

    // TODO(simon): This is not compliant to the spec, the 'var' shouldn't technically
    // be re-assigned to undefined in this case.
    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 42 @6
assign @6 @5
assign @7 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('respects initialization order', () => {
    /**
     * Order should be:
     * - Var refs are initialized
     * - Functions are initialized
     * - Rest of the code is evaluated
     */
    const result = configuredCompile(
      `
        const myConst = 42;
        var myVar = 41;

        class MyClass {

        }

        function MyFunction() {
          MyClass.toString();
        }
    `,
      { optimizeSlots: false, optimizeVarRefs: false },
      false,
      false,
      false,
      false,
      ['file_ts__module_init__'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref 'exports' @4
newvref 'myVar' @6
setvref @3 @4
newvref 'myConst' @5
newvref 'MyClass' @7
newvref 'MyFunction' @8
newfn 'file_ts_MyFunction' 'MyFunction' [ @7 ] @9
setvref @9 @8
storeint 42 @10
setvref @10 @5
storeint 41 @11
setvref @11 @6
newclass 'file_ts_MyClass' 'MyClass' @12 [ ] @13
setvref @13 @7
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('allows function args to override closures', () => {
    const result = configuredCompile(
      `
      let myObject = {};
      function myFn(myObject: any) {
        myObject.value = 42;
      }

    `,
      { optimizeSlots: true, optimizeVarRefs: true },
      true,
      false,
      false,
      false,
      ['file_ts_myFn'],
    );

    expect(result).toEqual(
      `
function_begin 'file_ts_myFn'
getfnarg 0 @2
storeint 42 @3
setprop @2 'value' @3
jump 'return'
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('replaces const enum values by their constants', () => {
    const result = configuredCompile(
      `
      import { Enum } from './Enum';

      const value = Enum.VALUE;
    `,
      { optimizeSlots: false, optimizeVarRefs: true },
      false,
      false,
      false,
      false,
      undefined,
      (workspace) => {
        workspace.registerInMemoryFile(
          'Enum.d.ts',
          `
          export const enum Enum {
            VALUE = 42,
          };
        `,
        );
      },
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 42 @6
assign @6 @5
jump 'return'
jumptarget 'return'
function_end @1
        `.trim(),
    );
  });

  it('supports try finally', () => {
    const result = configuredCompile(
      `
      const obj: any = {};
      try {
        obj.inTry = true;
      } finally {
        obj.inFinally = true;
      }
      obj.inOuter = true;
    `,
      { optimizeSlots: true, optimizeVarRefs: true },
      true,
      false,
      false,
      true,
      undefined,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getfnarg 2 @2
newobject @2
assign @2 @3
jumptarget 'try'
storebool true @4
setprop @3 'inTry' @4
jump 'finally'
jumptarget 'finally'
storebool true @4
setprop @3 'inFinally' @4
checkexception 'exception'
storebool true @4
setprop @3 'inOuter' @4
jump 'return'
jumptarget 'exception'
storeexception @2
assign @2 @1
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('supports try catch', () => {
    const result = configuredCompile(
      `
      const obj: any = {};
      try {
        obj.inTry = true;
      } catch {
        obj.inCatch = true;
      }
      obj.inOuter = true;
    `,
      { optimizeSlots: true, optimizeVarRefs: true },
      true,
      false,
      false,
      true,
      undefined,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getfnarg 2 @2
newobject @2
assign @2 @3
jumptarget 'try'
storebool true @4
setprop @3 'inTry' @4
jump 'finally'
jumptarget 'catch'
getexception @2
storebool true @4
setprop @3 'inCatch' @4
jump 'finally'
jumptarget 'finally'
storebool true @4
setprop @3 'inOuter' @4
jump 'return'
jumptarget 'exception'
storeexception @2
assign @2 @1
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('supports try catch finally', () => {
    const result = configuredCompile(
      `
      const obj: any = {};
      try {
        obj.inTry = true;
      } catch {
        obj.inCatch = true;
      } finally {
        obj.inFinally = true;
      }
      obj.inOuter = true;
    `,
      { optimizeSlots: true, optimizeVarRefs: true },
      true,
      false,
      false,
      true,
      undefined,
    );

    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getfnarg 2 @2
newobject @2
assign @2 @3
jumptarget 'try'
storebool true @4
setprop @3 'inTry' @4
jump 'finally'
jumptarget 'catch'
getexception @2
storebool true @4
setprop @3 'inCatch' @4
jump 'finally'
jumptarget 'finally'
storebool true @4
setprop @3 'inFinally' @4
checkexception 'exception'
storebool true @4
setprop @3 'inOuter' @4
jump 'return'
jumptarget 'exception'
storeexception @2
assign @2 @1
jumptarget 'return'
function_end @1
          `.trim(),
    );
  });

  it('optimize null checks', () => {
    const c_code = compileAsC(
      `
      function foo(v: any): boolean {
        return v == undefined;
      }
      `,
      { optimizeSlots: true, optimizeVarRefs: true, optimizeNullChecks: true },
      'foo',
      undefined,
    );
    expect(c_code.includes('tsn_is_undefined_or_null')).toBeTruthy();
    expect(c_code.includes('tsn_op_eq')).toBeFalsy();
  });

  it('optimize boolean conditions', () => {
    const c_code = compileAsC(
      `
      function foo(x: number, y: number): number {
        if (x < y) {
          return y - x;
        } else {
          return x - y;
        }
      }
      `,
      { optimizeSlots: true, optimizeVarRefs: true },
      'foo',
      undefined,
    );
    expect(c_code.includes('tsn_truthy_bool')).toBeTruthy();
    expect(c_code.includes('tsn_to_bool')).toBeFalsy();
  });

  it('batch set properties', () => {
    const result = configuredCompile(
      `
      const o = {x: 1, y: 2, z: 3};
      `,
      { optimizeSlots: true, optimizeVarRefs: true, mergeSetProperties: true },
      false,
      false,
      false,
      false,
      undefined,
    );
    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newobject @2
storeint 1 @3
storeint 2 @4
storeint 3 @5
setprops @2 [ 'x', 'y', 'z' ] [ @3, @4, @5 ]
assign @2 @6
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('optimize var ref loading', () => {
    const result = configuredCompile(
      `
      const o = 1;
      const x = o + o;
      `,
      { optimizeSlots: true, optimizeVarRefs: false, optimizeVarRefLoads: true },
      false,
      false,
      false,
      false,
      undefined,
    );
    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref 'exports' @2
setvref @3 @2
newvref 'o' @2
newvref 'x' @4
storeint 1 @5
setvref @5 @2
add @5 @5 @6
setvref @6 @4
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });
  it('optimize var ref loading alias invalidating', () => {
    const result = configuredCompile(
      `
      const o = 1;
      let x = o + 1;
      if (1 < 2) {
        x = o + 2;
      }
      `,
      { optimizeSlots: true, optimizeVarRefs: false, optimizeVarRefLoads: true },
      false,
      false,
      false,
      false,
      undefined,
    );
    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref 'exports' @2
setvref @3 @2
newvref 'o' @2
newvref 'x' @4
storeint 1 @5
setvref @5 @2
storeint 1 @6
add @5 @6 @7
setvref @7 @4
jumptarget 'condition'
storeint 2 @5
storeint 1 @6
lt @6 @5 @8
branch @8 'true' 'false'
jumptarget 'true'
storeint 2 @7
loadvref @2 @5
add @5 @7 @6
setvref @6 @4
jump 'exit'
jumptarget 'false'
jumptarget 'exit'
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('optimize assignments', () => {
    const result = configuredCompile(
      `
      const o1 = 1;
      const o2 = o1;
      `,
      { optimizeSlots: true, optimizeVarRefs: true, optimizeAssignments: true },
      false,
      false,
      false,
      false,
      undefined,
    );
    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storeint 1 @3
assign @3 @4
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('self-ref retainable assignments', () => {
    const result = configuredCompile(
      `
      let a = 'hello';
      let b = 'world';
      b += a;
      `,
      { optimizeSlots: true, optimizeVarRefs: true, optimizeAssignments: true },
      false,
      false,
      false,
      false,
      undefined,
    );
    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
getmoduleconst 0 @2
assign @2 @3
getmoduleconst 1 @2
assign @2 @4
add @4 @3 @2
assign @2 @4
jump 'return'
jumptarget 'return'
function_end @1`.trim(),
    );
  });

  it('constant folding', () => {
    const result = configuredCompile(
      `
      const PI = 3.14;
      const TwoPISquare = 2 * PI * PI;
      `,
      { optimizeSlots: true, optimizeVarRefs: true, foldConstants: true },
      false,
      false,
      false,
      false,
      undefined,
    );
    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
storedouble 3.14 @3
assign @3 @4
storedouble 19.7192 @3
assign @3 @4
jump 'return'
jumptarget 'return'
function_end @1
      `.trim(),
    );
  });

  it('constant folding in closure', () => {
    const result = configuredCompile(
      `
      const PI = 3.14;
      function foo() {
        const TwoPISquare = 2 * PI * PI;
      }
      `,
      { optimizeSlots: true, optimizeVarRefs: true, foldConstants: true },
      false,
      false,
      false,
      false,
      undefined,
    );
    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newvref 'PI' @3
newfn 'file_ts_foo' 'foo' [ @3 ] @2
assign @2 @4
storedouble 3.14 @5
setvref @5 @3
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_foo'
storedouble 19.7192 @2
assign @2 @3
jump 'return'
jumptarget 'return'
function_end @1 
      `.trim(),
    );
  });

  it('generator', () => {
    const result = configuredCompile(
      `
      function* foo() {
      let a = 42;
      {
        let a = 12
        yield a;
      }
      return a;
      }
      `,
      { optimizeSlots: true, optimizeVarRefs: true, foldConstants: true },
      false,
      false,
      false,
      false,
      undefined,
    );
    expect(result).toEqual(
      `
function_begin 'file_ts__module_init__'
newfn 'file_ts_foo' 'foo' [ ] @2
assign @2 @3
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_foo'
newarrowfn 'file_ts_foo_anon' [ ] @2
generator @2 @3
assign @3 @1
jump 'return'
jump 'return'
jumptarget 'return'
function_end @1

function_begin 'file_ts_foo_anon'
reentry 'resume_point'
storeint 42 @2
assign @2 @3
storeint 12 @2
assign @2 @4
setresumepoint 'resume_point'
assign @4 @1
jump 'yield'
jumptarget 'resume_point'
resume @5
assign @3 @1
jump 'return'
jump 'return'
jumptarget 'return'
setresumepoint 'end_of_func'
jumptarget 'yield'
function_end @1
      `.trim(),
    );
  });
});
