import 'jasmine/src/jasmine';
import { isVersionAtLeast } from '../src/CompilerIntrinsics';
import { ValdiRuntime } from '../src/ValdiRuntime';

declare const runtime: ValdiRuntime;

describe('CompilerIntrinsics', () => {
  it('enables placeholder-versioned API guards during development', () => {
    expect(isVersionAtLeast(__PLACEHOLDER__)).toBeTrue();
  });

  it('continues comparing concrete versions with the runtime API version', () => {
    const currentVersion = runtime.apiVersion;

    expect(isVersionAtLeast(0)).toBe(currentVersion >= 0);
    expect(isVersionAtLeast(2147483647)).toBe(currentVersion >= 2147483647);
  });
});
