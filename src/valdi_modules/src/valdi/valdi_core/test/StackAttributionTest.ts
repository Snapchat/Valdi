import 'jasmine/src/jasmine';
import { parseStackFrames } from 'source_map/src/StackSymbolicator';
import { bundleNameFromStackFrames } from 'valdi_core/src/StackAttribution';
import { symbolicate } from 'valdi_core/src/Symbolicator';

describe('bundleNameFromStackFrames', () => {
  it('names the innermost feature frame below native and framework frames', () => {
    const frames = parseStackFrames(
      [
        '    at <anonymous> ([native_code])',
        '    at <object>.next (send_to/src/SendToRootComponent.tsx:1547:16)',
        '    at ConsumerObserver.next (valdi_rxjs/src/Subscriber.ts:161:25)',
        '    at <anonymous> (people_rx/src/ObservableFriendCore.ts:83:5)',
      ].join('\n'),
    );

    expect(bundleNameFromStackFrames(frames)).toBe('send_to');
  });

  it('returns undefined when the stack never leaves framework code', () => {
    const frames = parseStackFrames(
      [
        '    at makeConfiguredCallback (valdi_core/src/FunctionUtils.ts:57:12)',
        '    at withGlobalNativeRefs (valdi_core/src/NativeReferences.ts:57:3)',
        '    at Subscriber._next (valdi_rxjs/src/Subscriber.ts:119:22)',
      ].join('\n'),
    );

    expect(bundleNameFromStackFrames(frames)).toBeUndefined();
  });

  it('ignores frames whose file is not a module path', () => {
    const frames = parseStackFrames(
      ['    at call (native)', '    at <anonymous> (<eval>:1:1)', 'anonymous@[native code]'].join('\n'),
    );

    expect(bundleNameFromStackFrames(frames)).toBeUndefined();
  });

  it('reads JavaScriptCore frames', () => {
    const frames = parseStackFrames(
      ['next@send_to/src/SendToRootComponent:1547:16', '_next@valdi_rxjs/src/Subscriber:119:22'].join('\n'),
    );

    expect(bundleNameFromStackFrames(frames)).toBe('send_to');
  });
});

describe('symbolicate', () => {
  it('names the bundle of the innermost feature frame on the symbolicated error', () => {
    const error = new Error('boom');
    error.stack = [
      '    at <object>.next (send_to/src/SendToRootComponent:1547:16)',
      '    at Subscriber._next (valdi_rxjs/src/Subscriber:119:22)',
    ].join('\n');

    const symbolicated = symbolicate(error);

    expect(symbolicated.stackBundleName).toBe('send_to');
    expect(symbolicated.originalStack).toBe(error.stack);
  });

  it('leaves stackBundleName unset when no feature frame is present', () => {
    const error = new Error('boom');
    error.stack = '    at Subscriber._next (valdi_rxjs/src/Subscriber:119:22)';

    expect(symbolicate(error).stackBundleName).toBeUndefined();
  });
});
