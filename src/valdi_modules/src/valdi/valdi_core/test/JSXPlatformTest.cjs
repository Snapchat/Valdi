const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { test } = require('node:test');
const ts = require(process.argv[2] ||
  path.resolve(__dirname, '../../../../../../compiler/companion/node_modules/typescript'));

// Execute the real bootstrap and prototype classes with native/debugging
// services stubbed. Each platform gets an isolated module instance.
const core = path.resolve(__dirname, '../src');
const bootstrap = process.argv[3] || path.join(core, 'JSXBootstrap.ts');

function loadPlatform(platform) {
  const cache = new Map();
  const runtime = { getCurrentPlatform: () => platform, internString: value => value, getAttributeId: () => 0 };
  function load(file) {
    if (cache.has(file)) return cache.get(file);
    const output = ts.transpileModule(fs.readFileSync(file, 'utf8'), {
      compilerOptions: { target: ts.ScriptTarget.ES2020, module: ts.ModuleKind.CommonJS },
    }).outputText;
    const exports = {};
    cache.set(file, exports);
    function requireModule(name) {
      if (name === './NodePrototype') return load(path.join(core, 'NodePrototype.ts'));
      if (name === './utils/PropertyList') return load(path.join(core, 'utils/PropertyList.ts'));
      if (name === './ValdiRuntimeProvider') return { getValdiRuntime: () => runtime };
      if (name === './BuildType') return { isDevBuild: () => false };
      if (name === 'valdi_core/src/StringCache') return { StringCache: class {} };
      if (name === './debugging/DaemonClientManager') {
        return {
          DaemonClientManager: class {
            addListener() {}
          },
        };
      }
      return {};
    }
    vm.runInNewContext(
      output,
      {
        exports,
        require: requireModule,
        runtime,
      },
      { filename: file },
    );
    return exports;
  }
  return load(bootstrap).jsx;
}

test('existing platforms keep their built-in view class mappings', () => {
  for (const [platform, expected] of [
    [1, 'com.snap.valdi.views.ValdiView'],
    [2, 'SCValdiView'],
    [3, 'SCValdiView'],
    [4, 'view'],
  ]) {
    assert.equal(loadPlatform(platform).makeNodePrototype('view').viewClass, expected);
  }
});

test('Linux initializes JSX and maps built-ins to SnapDrawing iOS class names', () => {
  const jsx = loadPlatform(5);
  for (const [tag, expected] of [
    ['view', 'SCValdiView'],
    ['label', 'SCValdiLabel'],
    ['scroll', 'SCValdiScrollView'],
    ['layout', 'Layout'],
  ]) {
    assert.equal(jsx.makeNodePrototype(tag).viewClass, expected);
  }
});

test('desktop custom-view fallback preserves iOS names and desktop overrides', () => {
  for (const platform of [3, 5]) {
    const jsx = loadPlatform(platform);
    assert.equal(
      jsx.makeNodePrototype('custom-view', { iosClass: 'IOS', androidClass: 'Android', macosClass: 'Desktop' })
        .viewClass,
      'Desktop',
    );
    assert.equal(jsx.makeNodePrototype('custom-view', { iosClass: 'IOS', androidClass: 'Android' }).viewClass, 'IOS');
    assert.equal(jsx.makeNodePrototype('custom-view', { androidClass: 'Android' }).viewClass, 'Deferred');
  }
});

test('Linux custom-view classes take precedence over desktop and iOS fallbacks', () => {
  for (const makeAttributes of [values => ({ ...values }), values => Object.entries(values).flat()]) {
    const jsx = loadPlatform(5);
    const overrides = { linuxClass: 'Linux', macosClass: 'Desktop', iosClass: 'IOS', androidClass: 'Android' };
    for (const [values, expected] of [
      [overrides, 'Linux'],
      [{ linuxClass: 'Linux' }, 'Linux'],
      [{ macosClass: 'Desktop', iosClass: 'IOS' }, 'Desktop'],
      [{ iosClass: 'IOS' }, 'IOS'],
      [{ androidClass: 'Android' }, 'Deferred'],
      [{ linuxClass: '', macosClass: 'Desktop' }, 'Desktop'],
    ]) {
      assert.equal(jsx.makeNodePrototype('custom-view', makeAttributes(values)).viewClass, expected);
    }
  }
});

test('Linux overrides do not change custom-view mappings on existing platforms', () => {
  for (const [platform, expected] of [
    [1, 'Android'],
    [2, 'IOS'],
    [3, 'Desktop'],
    [4, 'custom-view'],
  ]) {
    assert.equal(
      loadPlatform(platform).makeNodePrototype('custom-view', {
        linuxClass: 'Linux',
        macosClass: 'Desktop',
        iosClass: 'IOS',
        androidClass: 'Android',
        webClass: 'Web',
      }).viewClass,
      expected,
    );
  }
});

test('resolved Linux class attributes are removed from the prototype property list', () => {
  for (const attributes of [{ linuxClass: 'Linux', value: 'retained' }, ['linuxClass', 'Linux', 'value', 'retained']]) {
    const prototype = loadPlatform(5).makeNodePrototype('custom-view', attributes);
    assert.equal(prototype.viewClass, 'Linux');
    assert.deepEqual(prototype.attributes, Array.isArray(attributes) ? ['value', 'retained'] : { value: 'retained' });
  }
});

test('unknown platform IDs still fail before rendering', () => {
  for (const platform of [0, 6, -1, NaN, '5']) {
    assert.throws(() => loadPlatform(platform), /Unrecognized platform type/);
  }
});
