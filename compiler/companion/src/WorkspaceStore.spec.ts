import { getCompilationCacheVersion } from './cache/CachingWorkspaceFactory';
import { ILogger } from './logger/ILogger';
import { WorkspaceStore } from './WorkspaceStore';

const logger: ILogger = {
  debug: undefined,
  info: undefined,
  warn: undefined,
  error: undefined,
};

describe('WorkspaceStore', () => {
  it('keeps the native API minimum scoped to each workspace', () => {
    const store = new WorkspaceStore(logger, undefined, false);

    const unchecked = store.createWorkspace(undefined);
    const baselineZero = store.createWorkspace(0);
    const higherBaseline = store.createWorkspace(7);

    expect(store.getUncachedWorkspace(unchecked.workspaceId).nativeApiMinVersion).toBeUndefined();
    expect(store.getUncachedWorkspace(baselineZero.workspaceId).nativeApiMinVersion).toBe(0);
    expect(store.getUncachedWorkspace(higherBaseline.workspaceId).nativeApiMinVersion).toBe(7);

    store.destroyAllWorkspaces();
  });

  it('rejects invalid native API minimums', () => {
    const store = new WorkspaceStore(logger, undefined, false);

    expect(() => store.createWorkspace(-1)).toThrow('nativeApiMinVersion must be an integer between 0 and 2147483647');
    expect(() => store.createWorkspace(1.5)).toThrow('nativeApiMinVersion must be an integer between 0 and 2147483647');
    expect(() => store.createWorkspace(2147483648)).toThrow(
      'nativeApiMinVersion must be an integer between 0 and 2147483647',
    );
  });

  it('versions compilation caches by native API minimum', () => {
    expect(getCompilationCacheVersion(undefined)).toBe('3');
    expect(getCompilationCacheVersion(0)).toBe('3/native-api-min-version-0');
    expect(getCompilationCacheVersion(7)).toBe('3/native-api-min-version-7');
  });
});
