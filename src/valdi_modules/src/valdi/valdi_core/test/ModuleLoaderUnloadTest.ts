import 'jasmine/src/jasmine';
import { ModuleLoader } from 'valdi_core/src/ModuleLoader';

for (const worklist of [false, true]) {
  describe(`ModuleLoader.unloadAllUnused (worklist=${worklist})`, () => {
    let loader: ModuleLoader;
    let graph: { [path: string]: string[] };
    let evaluations: { [path: string]: number };

    beforeEach(() => {
      graph = {};
      evaluations = {};
      loader = new ModuleLoader(
        (path, require, module) => {
          evaluations[path] = (evaluations[path] ?? 0) + 1;
          module.exports = { path };
          for (const dependency of graph[path] ?? []) {
            require(dependency, true);
          }
          return undefined;
        },
        undefined,
        undefined,
        true,
        worklist,
      );
    });

    it('retains roots and shared dependencies, then unloads and reloads them', () => {
      graph = {
        'test/root': ['test/left', 'test/right', 'test/left'],
        'test/left': ['test/shared'],
        'test/right': ['test/shared'],
      };
      loader.load('test/root', true);
      loader.load('test/unused', true);
      expect(loader.unloadAllUnused(['test/./root'])).toEqual(['test/unused']);
      expect(loader.getDependents('test/shared').sort()).toEqual(['test/left', 'test/right']);
      expect(loader.unloadAllUnused([]).sort()).toEqual(Object.keys(graph).concat('test/shared').sort());
      expect(loader.unloadAllUnused([])).toEqual([]);
      expect(loader.load('test/root', true).path).toEqual('test/root');
      expect(evaluations['test/shared']).toEqual(2);
    });

    it('preserves cycles and their dependencies while removing unrelated leaves', () => {
      graph = { 'test/a': ['test/b'], 'test/b': ['test/a', 'test/dependency'] };
      loader.load('test/a', true);
      loader.load('test/unused', true);
      expect(loader.unloadAllUnused([])).toEqual(['test/unused']);
      expect(loader.isLoaded('test/a')).toBe(true);
      expect(loader.isLoaded('test/dependency')).toBe(true);
      expect(loader.unload(['test/a'], false, false).sort()).toEqual(['test/a', 'test/b']);
      expect(loader.unloadAllUnused([])).toEqual(['test/dependency']);
    });

    it('unloads lazy modules and can evaluate them after reloading', () => {
      loader = new ModuleLoader(
        () => {
          evaluations['test/lazy'] = (evaluations['test/lazy'] ?? 0) + 1;
          return undefined;
        },
        undefined,
        undefined,
        false,
        worklist,
      );
      loader.load('test/lazy');
      expect(evaluations['test/lazy']).toBeUndefined();
      expect(loader.unloadAllUnused([])).toEqual(['test/lazy']);
      loader.load('test/lazy', true);
      expect(evaluations['test/lazy']).toEqual(1);
    });

    it('handles imports during disposal without unloading newly used dependencies', () => {
      graph = { 'test/first': ['test/dependency'] };
      loader.load('test/first', true);
      loader.onHotReload({ path: 'test/first' }, 'test/observed', () => {});
      // Removing a registered callback executes indexOf during module disposal.
      const callbacks = (loader as any).hotReloadCallbacks['test/observed'];
      callbacks.indexOf = () => {
        graph['test/new'] = ['test/dependency'];
        loader.load('test/new', true);
        return -1;
      };
      expect(loader.unloadAllUnused(['test/new']).sort()).toEqual(['test/first']);
      expect(loader.getDependents('test/dependency')).toEqual(['test/new']);
      expect(loader.unloadAllUnused([]).sort()).toEqual(['test/dependency', 'test/new']);
    });

    it('includes dependencies released by a nested unload during disposal', () => {
      graph = { 'test/second': ['test/dependency'] };
      loader.load('test/first', true);
      loader.load('test/second', true);
      loader.onHotReload({ path: 'test/first' }, 'test/observed', () => {});
      const callbacks = (loader as any).hotReloadCallbacks['test/observed'];
      callbacks.indexOf = () => {
        loader.unload(['test/second'], false, false);
        return -1;
      };
      expect(loader.unloadAllUnused([]).sort()).toEqual(['test/dependency', 'test/first']);
      expect(loader.isLoaded('test/dependency')).toBe(false);
    });
  });
}
