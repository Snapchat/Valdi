import { JSEvalResult, JsEvaluator, ModuleLoader, RequireFunc, SourceMapFactory } from 'valdi_core/src/ModuleLoader';
import { StringMap } from 'coreutils/src/StringMap';
import 'jasmine/src/jasmine';
import { ISourceMap, MappedPosition } from 'source_map/src/ISourceMap';

type JsModuleFunc = (require: RequireFunc, module: any, exports: any) => void;

function makeEvaluator(cb: (path: string) => void): JsEvaluator {
  return (path, require, module, exports) => {
    cb(path);
    return undefined;
  };
}

describe('ModuleLoader', () => {
  it('loads module lazily', () => {
    let loaded = false;
    const loader = new ModuleLoader(
      makeEvaluator(() => {
        loaded = true;
      }),
      undefined,
      undefined,
    );

    const module = loader.load('whatever');

    expect(loaded).toBeFalsy();

    module['something'];

    expect(loaded).toBeTruthy();
  });

  it('loads module eagerly in commonjs module', () => {
    let loaded = false;
    const loader = new ModuleLoader(
      makeEvaluator(() => {
        loaded = true;
      }),
      undefined,
      undefined,
      true,
    );

    const module = loader.load('whatever');

    expect(loaded).toBeTruthy();
  });

  it('can load modules recursively', () => {
    let module1LoadCount = 0;
    let module1AtModule2Import: any;
    let module1AtModule3Import: any;

    const module1: JsModuleFunc = (require, module, exports) => {
      module1LoadCount++;
      exports.before = true;
      require('./module2');
      exports.after = true;
      require('./module3');
    };

    const module2: JsModuleFunc = (require, module, exports) => {
      const imported = require('./module1');
      module1AtModule2Import = { ...imported };
    };

    const module3: JsModuleFunc = (require, module, exports) => {
      const imported = require('./module1');
      module1AtModule3Import = { ...imported };
    };

    const moduleByPath: StringMap<JsModuleFunc> = {
      'my_module/src/module1': module1,
      'my_module/src/module2': module2,
      'my_module/src/module3': module3,
    };

    const loader = new ModuleLoader(
      (path, require, module, exports): JSEvalResult => {
        const moduleFunc = moduleByPath[path];
        if (!moduleFunc) {
          throw new Error(`Unrecognized module ${path}`);
        }
        moduleFunc(require, module, exports);
        return undefined;
      },
      undefined,
      undefined,
      true,
    );

    loader.load('my_module/src/module1');

    expect(module1LoadCount).toBe(1);
    expect(module1AtModule2Import).toEqual({ before: true });
    expect(module1AtModule3Import).toEqual({ before: true, after: true });
  });

  it('load modules only once', () => {
    const loadRequests: string[] = [];
    const loader = new ModuleLoader(
      makeEvaluator(path => {
        loadRequests.push(path);
      }),
      undefined,
      undefined,
    );

    expect(loadRequests).toEqual([]);

    loader.load('my_module/src/File1')[0];
    expect(loadRequests).toEqual(['my_module/src/File1']);

    // Test load the module again through absolute path
    loader.load('my_module/src/File1')[0];
    // Test loading through absolute path with relative segments
    loader.load('my_module/src/../src/File1')[0];
    // Test loading relative through another module
    loader.resolveRequire('other_module/src/File')('../../my_module/src/File1')[0];

    expect(loadRequests).toEqual(['my_module/src/File1']);
  });

  it('can preload modules', () => {
    let module1LoadCount = 0;
    let module2LoadCount = 0;
    let module3LoadCount = 0;

    const module1: JsModuleFunc = (require, module, exports) => {
      module1LoadCount++;
      require('./module2');
    };

    const module2: JsModuleFunc = (require, module, exports) => {
      module2LoadCount++;
      require('./module3');
    };

    const module3: JsModuleFunc = (require, module, exports) => {
      module3LoadCount++;
      require('./module1');
    };

    const moduleByPath: StringMap<JsModuleFunc> = {
      'my_module/src/module1': module1,
      'my_module/src/module2': module2,
      'my_module/src/module3': module3,
    };

    const loader = new ModuleLoader(
      (path, require, module, exports): JSEvalResult => {
        const moduleFunc = moduleByPath[path];
        if (!moduleFunc) {
          throw new Error(`Unrecognized module ${path}`);
        }
        moduleFunc(require, module, exports);
        return undefined;
      },
      undefined,
      undefined,
      false,
    );

    expect(module1LoadCount).toBe(0);
    expect(module2LoadCount).toBe(0);
    expect(module3LoadCount).toBe(0);

    loader.preload('my_module/src/module1', 0);

    expect(module1LoadCount).toBe(1);
    expect(module2LoadCount).toBe(0);
    expect(module3LoadCount).toBe(0);

    loader.preload('my_module/src/module1', 1);

    expect(module1LoadCount).toBe(1);
    expect(module2LoadCount).toBe(1);
    expect(module3LoadCount).toBe(0);

    loader.preload('my_module/src/module1', 1);

    expect(module1LoadCount).toBe(1);
    expect(module2LoadCount).toBe(1);
    expect(module3LoadCount).toBe(0);

    loader.preload('my_module/src/module1', 2);

    expect(module1LoadCount).toBe(1);
    expect(module2LoadCount).toBe(1);
    expect(module3LoadCount).toBe(1);

    loader.preload('my_module/src/module1', 10);

    expect(module1LoadCount).toBe(1);
    expect(module2LoadCount).toBe(1);
    expect(module3LoadCount).toBe(1);
  });

  it('can resolve absolute path', () => {
    let askedPath: string | undefined;
    const loader = new ModuleLoader(
      makeEvaluator(path => {
        askedPath = path;
      }),
      undefined,
      undefined,
    );

    loader.load('hello/tiny/village/../../world')[''];

    expect(askedPath).toBe('hello/world');
  });

  it('can resolve relative path', () => {
    let askedPath: string | undefined;
    const loader = new ModuleLoader(
      makeEvaluator(path => {
        askedPath = path;
      }),
      undefined,
      undefined,
    );

    const require = loader.resolveRequire('my_module/src/MyFile');
    require('./MyFile2')[0];

    expect(askedPath).toBe('my_module/src/MyFile2');

    require('./directory/Nested')[0];

    expect(askedPath).toBe('my_module/src/directory/Nested');

    require('../../other_module/File')[0];

    expect(askedPath).toBe('other_module/File');
  });

  it('resolve root modules to valdi_core', () => {
    let askedPath: string | undefined;
    const loader = new ModuleLoader(
      makeEvaluator(path => {
        askedPath = path;
      }),
      undefined,
      undefined,
    );

    loader.load('Root')[0];

    expect(askedPath).toBe('valdi_core/src/Root');

    loader.load('NotRoot/../NowRoot')[0];

    expect(askedPath).toBe('valdi_core/src/NowRoot');
  });

  it('stores dependencies', () => {
    const loader = new ModuleLoader(
      makeEvaluator(path => {}),
      undefined,
      undefined,
    );

    const modulePath = 'module/src/File1';

    const require = loader.resolveRequire(modulePath);

    expect(loader.getDependencies(modulePath)).toEqual([]);

    require('./File2');

    expect(loader.getDependencies(modulePath)).toEqual(['module/src/File2']);

    // Dependency should be recorded only once
    require('./File2');
    expect(loader.getDependencies(modulePath)).toEqual(['module/src/File2']);

    require('../src2/File3');

    expect(loader.getDependencies(modulePath)).toEqual(['module/src/File2', 'module/src2/File3']);

    require('valdi_core/src/Device');

    expect(loader.getDependencies(modulePath)).toEqual([
      'module/src/File2',
      'module/src2/File3',
      'valdi_core/src/Device',
    ]);
  });

  it('stores dependents', () => {
    const loader = new ModuleLoader(
      makeEvaluator(path => {}),
      undefined,
      undefined,
    );

    const modulePath = 'module/src/Common';
    loader.load(modulePath);

    expect(loader.getDependents(modulePath)).toEqual([]);

    loader.resolveRequire('module/src/File1')('./Common');

    expect(loader.getDependents(modulePath)).toEqual(['module/src/File1']);

    // Dependent should be recorded only once
    loader.resolveRequire('module/src/File1')('./Common');

    expect(loader.getDependents(modulePath)).toEqual(['module/src/File1']);

    loader.resolveRequire('module/src/File3')('./Common');

    expect(loader.getDependents(modulePath)).toEqual(['module/src/File1', 'module/src/File3']);
  });

  it('providesADifferentStackOnEachFailedLoad', () => {
    const loader = new ModuleLoader(
      makeEvaluator(() => {
        throw new Error('You cannot load');
      }),
      undefined,
      undefined,
    );

    const result = loader.load('somemodule');

    let error: Error | undefined;
    try {
      result['something'];
    } catch (err: any) {
      error = err;
    }
    let error2: Error | undefined;
    try {
      result['something'];
    } catch (err: any) {
      error2 = err;
    }

    expect(error).toBeTruthy();
    expect(error2).toBeTruthy();

    expect(error?.message).toBe(error2?.message);
    expect(error?.stack).not.toEqual(error2?.stack);
  });

  it('preloadsValdiModuleOnImport', () => {
    const preloadedValdiModules: string[] = [];
    const valdiModulePreloader = (name: string) => {
      preloadedValdiModules.push(name);
    };

    const loader = new ModuleLoader(
      makeEvaluator(path => {}),
      undefined,
      valdiModulePreloader,
    );

    expect(preloadedValdiModules).toEqual([]);

    const module = loader.load('module/src/File1');

    expect(preloadedValdiModules).toEqual(['module']);

    const module2 = loader.load('module/src/File2');

    // Should not have changed given that we already preloaded 'module'
    expect(preloadedValdiModules).toEqual(['module']);

    const module3 = loader.load('new_module/src/File1');

    expect(preloadedValdiModules).toEqual(['module', 'new_module']);
  });

  it('resolves SourceMap', () => {
    const sourceMapContentByFile: StringMap<string> = {
      'module_a/src/FileA': 'hello',
      'module_a/src/FileB': 'world',
    };
    const sourceMapByContent: StringMap<ISourceMap> = {
      hello: {
        resolve(line, column, name): MappedPosition | undefined {
          return undefined;
        },
      },
    };

    const sourceMapCalls: string[] = [];
    const sourceMapContentCalls: (string | undefined)[] = [];

    const loader = new ModuleLoader(
      makeEvaluator(path => {}),
      (path: string) => {
        sourceMapCalls.push(path);
        return sourceMapContentByFile[path];
      },
      undefined,
    );

    const sourceMapFactory: SourceMapFactory = (sourceMapContent: string | undefined): ISourceMap | undefined => {
      if (!sourceMapContent) {
        return undefined;
      }

      sourceMapContentCalls.push(sourceMapContent);
      if (sourceMapContent) {
        return sourceMapByContent[sourceMapContent];
      }
      return undefined;
    };

    const sourceMap = loader.getOrCreateSourceMap('module_a/src/FileA', sourceMapFactory);
    expect(sourceMap).toBe(sourceMapByContent.hello);

    expect(sourceMapCalls).toEqual(['module_a/src/FileA']);
    expect(sourceMapContentCalls).toEqual(['hello']);

    // Calling it again should return same object and not call the source maps again
    const sourceMap2 = loader.getOrCreateSourceMap('module_a/src/FileA', sourceMapFactory);
    expect(sourceMap2).toBe(sourceMap);

    expect(sourceMapCalls).toEqual(['module_a/src/FileA']);
    expect(sourceMapContentCalls).toEqual(['hello']);

    const sourceMap3 = loader.getOrCreateSourceMap('module_a/src/FileB', sourceMapFactory);
    expect(sourceMap3).toBeUndefined();

    expect(sourceMapCalls).toEqual(['module_a/src/FileA', 'module_a/src/FileB']);
    expect(sourceMapContentCalls).toEqual(['hello', 'world']);

    // Calling it again shouldn't resolve the source map again even if it previously failed
    const sourceMap4 = loader.getOrCreateSourceMap('module_a/src/FileB', sourceMapFactory);
    expect(sourceMap4).toBeUndefined();

    expect(sourceMapCalls).toEqual(['module_a/src/FileA', 'module_a/src/FileB']);
    expect(sourceMapContentCalls).toEqual(['hello', 'world']);

    // When not source map content attached, should not provide source map

    const sourceMap5 = loader.getOrCreateSourceMap('module_a/src/FileC', sourceMapFactory);
    expect(sourceMap5).toBeUndefined();

    expect(sourceMapCalls).toEqual(['module_a/src/FileA', 'module_a/src/FileB', 'module_a/src/FileC']);
    expect(sourceMapContentCalls).toEqual(['hello', 'world']);

    // Calling it again shouldn't resolve the source map again

    const sourceMap6 = loader.getOrCreateSourceMap('module_a/src/FileC', sourceMapFactory);
    expect(sourceMap6).toBeUndefined();

    expect(sourceMapCalls).toEqual(['module_a/src/FileA', 'module_a/src/FileB', 'module_a/src/FileC']);
    expect(sourceMapContentCalls).toEqual(['hello', 'world']);
  });

  it('propagates import failures while resolving SourceMap', () => {
    const loader = new ModuleLoader(
      makeEvaluator(path => {
        throw new Error(`Could not import ${path}`);
      }),
      undefined,
      undefined,
    );

    expect(() => loader.getOrCreateSourceMap('module_a/src/FileA', () => undefined)).toThrowError(
      'Could not import module_a/src/FileA',
    );
  });
});
