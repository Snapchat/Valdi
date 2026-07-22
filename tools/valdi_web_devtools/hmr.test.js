// @ts-check
/**
 * Unit tests for the HMR runtime registry.
 *
 * Lock in the mount-only-self-accept policy so a future regression
 * doesn't reintroduce silent-swallow for child-module updates:
 *   - Only modules whose exported class is registered as a mount root
 *     should ever see module.hot.accept() called on them.
 *   - Modules whose exports aren't mounted must NOT be accepted, so
 *     webpack bubbles their updates up to a parent root (or full reload).
 *
 * Run via `node --test tools/valdi_web_devtools/hmr.test.js` or via the
 * `:hmr_test` Bazel target.
 */

'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');

function freshRegistry() {
  delete require.cache[require.resolve('./hmr')];
  return require('./hmr');
}

function fakeHot() {
  const calls = [];
  return {
    calls,
    accept: () => { calls.push('accept'); },
  };
}

test('mountRoot accepts the moduleId hosting the mounted class', () => {
  const hmr = freshRegistry();
  class Root {}
  const rootHot = fakeHot();
  hmr.registerModule('root.js', { Root });
  hmr.registerModuleHot('root.js', rootHot);

  let rendered = 0;
  hmr.mountRoot(Root, () => { rendered++; });

  assert.equal(rendered, 1, 'mountRoot invokes renderFn once at mount');
  assert.deepEqual(rootHot.calls, ['accept'], 'mountRoot accepts the root module');
});

test('mountRoot does NOT accept child / helper modules', () => {
  const hmr = freshRegistry();
  class Child {}
  class Root {}
  const childHot = fakeHot();
  const rootHot = fakeHot();
  // Child registers first (deeper in the import graph)
  hmr.registerModule('child.js', { Child });
  hmr.registerModuleHot('child.js', childHot);
  // Root registers after — uses Child as a member but mountRoot is called on Root
  hmr.registerModule('root.js', { Root });
  hmr.registerModuleHot('root.js', rootHot);

  hmr.mountRoot(Root, () => {});

  assert.deepEqual(rootHot.calls, ['accept'], 'root module accepted');
  assert.deepEqual(childHot.calls, [], 'child module NOT accepted — must bubble');
});

test('accept is called at most once per module across repeated mountRoot calls', () => {
  const hmr = freshRegistry();
  class Root {}
  const rootHot = fakeHot();
  hmr.registerModule('root.js', { Root });
  hmr.registerModuleHot('root.js', rootHot);

  hmr.mountRoot(Root, () => {});
  hmr.mountRoot(Root, () => {}); // hypothetical re-mount

  assert.equal(rootHot.calls.length, 1, 'accept called once even on re-mount');
});

test('handleModuleUpdate re-renders mounted root with new class', () => {
  const hmr = freshRegistry();
  class RootV1 {}
  class RootV2 {}
  const rootHot = fakeHot();
  hmr.registerModule('root.js', { Root: RootV1 });
  hmr.registerModuleHot('root.js', rootHot);

  let rendered = null;
  hmr.mountRoot(RootV1, (ctr) => { rendered = ctr; });
  assert.equal(rendered, RootV1);

  // Simulate webpack re-eval: new module body runs, registerModule overwrites.
  hmr.registerModule('root.js', { Root: RootV2 });
  hmr.handleModuleUpdate('root.js');

  assert.equal(rendered, RootV2, 'renderFn re-invoked with the new class');
});

test('handleModuleUpdate is a no-op for non-mount modules', () => {
  const hmr = freshRegistry();
  class Child {}
  hmr.registerModule('child.js', { Child });
  hmr.registerModuleHot('child.js', fakeHot());

  // No mountRoot for Child. Updating it should not throw or warn loudly.
  // (We don't care about console output here, just that the call returns
  // cleanly — the silent-swallow concern is addressed at the loader layer
  // by not self-accepting the child in the first place.)
  hmr.handleModuleUpdate('child.js'); // must not throw
});

test('registerModuleHot ignores invalid hot handles', () => {
  const hmr = freshRegistry();
  class Root {}
  hmr.registerModule('root.js', { Root });

  // No throw for missing/invalid hot
  hmr.registerModuleHot('root.js', null);
  hmr.registerModuleHot('root.js', undefined);
  hmr.registerModuleHot('root.js', {});

  // mountRoot must not throw when no valid hot was registered.
  // Renderer still runs; module just won't self-accept (acceptable —
  // dev-server will reload on edit).
  let rendered = false;
  hmr.mountRoot(Root, () => { rendered = true; });
  assert.equal(rendered, true);
});
