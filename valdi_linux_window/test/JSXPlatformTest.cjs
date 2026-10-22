const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { test } = require('node:test');
const ts = require(process.argv[2] || 'typescript');

// Execute the real bootstrap and prototype classes with native/debugging
// services stubbed. Each platform gets an isolated module instance.
const core = path.resolve(__dirname, '../../src/valdi_modules/src/valdi/valdi_core/src');
const bootstrap = process.argv[3] || path.join(core, 'JSXBootstrap.ts');

function loadPlatform(platform) {
  const cache = new Map();
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
      if (name === './BuildType') return { isDevBuild: () => false };
      if (name === 'valdi_core/src/StringCache') return { StringCache: class {} };
      if (name === './debugging/DaemonClientManager') {
        return { DaemonClientManager: class { addListener() {} } };
      }
      return {};
    }
    vm.runInNewContext(output, {
      exports,
      require: requireModule,
      runtime: { getCurrentPlatform: () => platform, internString: value => value, getAttributeId: () => 0 },
    }, { filename: file });
    return exports;
  }
  return load(bootstrap).jsx;
}

test('existing platforms keep their built-in view class mappings', () => {
  for (const [platform, expected] of [[1, 'com.snap.valdi.views.ValdiView'], [2, 'SCValdiView'],
    [3, 'SCValdiView'], [4, 'view']]) {
    assert.equal(loadPlatform(platform).makeNodePrototype('view').viewClass, expected);
  }
});

test('Linux initializes JSX and maps built-ins to SnapDrawing iOS class names', () => {
  const jsx = loadPlatform(5);
  for (const [tag, expected] of [['view', 'SCValdiView'], ['label', 'SCValdiLabel'],
    ['scroll', 'SCValdiScrollView'], ['layout', 'Layout']]) {
    assert.equal(jsx.makeNodePrototype(tag).viewClass, expected);
  }
});

test('desktop custom-view fallback preserves iOS names and desktop overrides', () => {
  for (const platform of [3, 5]) {
    const jsx = loadPlatform(platform);
    assert.equal(jsx.makeNodePrototype('custom-view', { iosClass: 'IOS', androidClass: 'Android',
      macosClass: 'Desktop' }).viewClass, 'Desktop');
    assert.equal(jsx.makeNodePrototype('custom-view', { iosClass: 'IOS', androidClass: 'Android' }).viewClass, 'IOS');
    assert.equal(jsx.makeNodePrototype('custom-view', { androidClass: 'Android' }).viewClass, 'Deferred');
  }
});

test('unknown platform IDs still fail before rendering', () => {
  for (const platform of [0, 6, -1, NaN, '5']) {
    assert.throws(() => loadPlatform(platform), /Unrecognized platform type/);
  }
});
