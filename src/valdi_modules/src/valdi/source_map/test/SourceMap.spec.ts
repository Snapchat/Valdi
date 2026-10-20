import 'jasmine/src/jasmine';
import { SourceMap } from 'source_map/src/SourceMap';
import { GetOrCreateSourceMapFunc, parseStackFrames, symbolicateStackFrames } from 'source_map/src/StackSymbolicator';
import { LineOffsetSourceMap } from '../src/SourceMap';

const sourceMapContent = `eyJ2ZXJzaW9uIjozLCJmaWxlIjoiU291cmNlTWFwRXhhbXBsZS5qcyIsInNvdXJjZVJvb3QiOiIvIiwic291cmNlcyI6WyJzb3VyY2VfbWFwL3Rlc3QvU291cmNlTWFwRXhhbXBsZS50cyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiOzs7QUFRQSxTQUFnQixXQUFXLENBQUMsS0FBVTtJQUNwQyxPQUFPLEtBQUssQ0FBQyxJQUFJLEdBQUcsS0FBSyxDQUFDLEtBQUssQ0FBQztBQUNsQyxDQUFDO0FBRkQsa0NBRUMifQ==`;

describe('SourceMap', () => {
  it('can be parsed', () => {
    const sourceMap = SourceMap.parseFromBase64(sourceMapContent);

    expect(sourceMap.file).toBe('SourceMapExample.js');
    expect(sourceMap.sourceRoot).toBe('/');
    expect(sourceMap.sources).toEqual(['source_map/test/SourceMapExample.ts']);

    expect(sourceMap.lines.length).toBe(7);
  });

  it('can resolve original position', () => {
    const sourceMap = SourceMap.parseFromBase64(sourceMapContent);

    let position = sourceMap.resolve(5, 12, undefined)!;
    expect(position).toBeTruthy();
    expect(position.line).toBe(10);
    expect(position.column).toBe(10);

    position = sourceMap.resolve(5, 25, undefined)!;
    expect(position).toBeTruthy();
    expect(position.line).toBe(10);
    expect(position.column).toBe(23);
    expect(position.fileName).toBe('source_map/test/SourceMapExample.ts');
  });

  it('can resolve original position with offset', () => {
    const sourceMap = SourceMap.parseFromBase64(sourceMapContent);

    const position = sourceMap.resolve(5, 14, undefined)!;
    expect(position).toBeTruthy();
    expect(position.line).toBe(10);
    expect(position.column).toBe(12);
  });

  it('Test LineOffsetSourceMap', () => {
    const sourceMap = new LineOffsetSourceMap(5);
    let position = sourceMap.resolve(10, 0, undefined)!;
    expect(position).toBeTruthy();
    expect(position.line).toBe(5);
    expect(position.column).toBe(0);

    position = sourceMap.resolve(420, 69, undefined)!;
    expect(position).toBeTruthy();
    expect(position.line).toBe(415);
    expect(position.column).toBe(69);

    position = sourceMap.resolve(420, undefined, undefined)!;
    expect(position).toBeTruthy();
    expect(position.line).toBe(415);
    expect(position.column).toBe(0);

    position = sourceMap.resolve(1, 0, undefined)!;
    expect(position).toBeFalsy();
  });
});

function makeGetOrCreateSourceMapFunc(sourceMaps: { [key: string]: SourceMap | undefined }): GetOrCreateSourceMapFunc {
  return (file: string) => {
    return sourceMaps[file];
  };
}

// function makeError(): Error {
//   let error: Error | undefined;
//   try {
//     doSomething(undefined);
//   } catch (err: any) {
//     error = err;
//   }

//   if (!error) {
//     throw new Error('Error not thrown');
//   }

//   injectSourceMap();

//   return error;
// }

describe('Symbolicator', () => {
  it('can parse QuickJS stackframes', () => {
    const stack = `
    at doSomething (source_map/test/SourceMapExample:6)
    at makeError (source_map/test/SourceMap.spec:31)
    at call (native)
    at <anonymous> (jasmine/src/jasmine:6423)
    `;

    const stackFrames = parseStackFrames(stack);
    expect(stackFrames).toEqual([
      { line: 6, column: undefined, name: 'doSomething', fileName: 'source_map/test/SourceMapExample' },
      { line: 31, column: undefined, name: 'makeError', fileName: 'source_map/test/SourceMap.spec' },
      { line: undefined, column: undefined, name: 'call', fileName: 'native' },
      { line: 6423, column: undefined, name: '<anonymous>', fileName: 'jasmine/src/jasmine' },
    ]);
  });

  it('can parse JSCore stackframes', () => {
    const stack = `
    doSomething@source_map/test/SourceMapExample:6:12
    makeError@source_map/test/SourceMap.spec:31
    [native code]
    jasmine/src/jasmine:6423
    `;

    const stackFrames = parseStackFrames(stack);
    expect(stackFrames).toEqual([
      { line: 6, column: 12, name: 'doSomething', fileName: 'source_map/test/SourceMapExample' },
      { line: 31, column: undefined, name: 'makeError', fileName: 'source_map/test/SourceMap.spec' },
      { line: undefined, column: undefined, name: undefined, fileName: '[native_code]' },
      { line: 6423, column: undefined, name: undefined, fileName: 'jasmine/src/jasmine' },
    ]);
  });

  it('can symbolicate stack frames', () => {
    const stack = `
    at doSomething (source_map/test/SourceMapExample:5)
    at makeError (source_map/test/SourceMap.spec:31)
    at call (native)
    at <anonymous> (jasmine/src/jasmine:6423)
    `;

    const stackFrames = parseStackFrames(stack);

    const getOrCreateSourceMapFunc = makeGetOrCreateSourceMapFunc({
      'source_map/test/SourceMapExample': SourceMap.parseFromBase64(sourceMapContent),
      'source_map/test/SourceMap.spec': new SourceMap(undefined, undefined, [], [], [], []),
    });

    const symbolicatedStackFrames = symbolicateStackFrames(stackFrames, getOrCreateSourceMapFunc);

    expect(symbolicatedStackFrames).toEqual([
      { line: 10, column: 3, name: 'doSomething', fileName: 'source_map/test/SourceMapExample.ts' },
      { line: 31, column: undefined, name: 'makeError', fileName: 'source_map/test/SourceMap.spec' },
      { line: undefined, column: undefined, name: 'call', fileName: 'native' },
      { line: 6423, column: undefined, name: '<anonymous>', fileName: 'jasmine/src/jasmine' },
    ]);
  });

  it('continues symbolicating stack frames when SourceMap lookup throws', () => {
    const stack = `
    at missingSourceMap (source_map/test/Missing:5)
    at doSomething (source_map/test/SourceMapExample:5)
    `;

    const stackFrames = parseStackFrames(stack);
    const consoleErrorSpy = spyOn(console, 'error');
    const getOrCreateSourceMapFunc: GetOrCreateSourceMapFunc = file => {
      if (file === 'source_map/test/Missing') {
        throw new Error('Could not load module');
      }
      return SourceMap.parseFromBase64(sourceMapContent);
    };

    const symbolicatedStackFrames = symbolicateStackFrames(stackFrames, getOrCreateSourceMapFunc);

    expect(symbolicatedStackFrames).toEqual([
      { line: 5, column: undefined, name: 'missingSourceMap', fileName: 'source_map/test/Missing' },
      { line: 10, column: 3, name: 'doSomething', fileName: 'source_map/test/SourceMapExample.ts' },
    ]);
    expect(consoleErrorSpy).toHaveBeenCalledWith(
      'Failed to symbolicate stack frame source_map/test/Missing: Could not load module',
    );
  });

  // Disabling this test for now as it doesn't currently work with QuickJS and minification on.
  // QuickJS doesn't provide the column number on thrown exceptions which prevents to resolve
  // the symbols on minified files since those files are all laid out on a single line.
  // it('can symbolicate errors', () => {
  //   const error = makeError();

  //   expect(error.stack).not.toContain('source_map/test/SourceMapExample.ts:10:3');

  //   const symbolicatedStack = symbolicateStack(error!.stack!);

  //   expect(symbolicatedStack).toContain('source_map/test/SourceMapExample.ts:10:3');
  // });
});
