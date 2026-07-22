// @ts-check
/**
 * Valdi Web HMR runtime registry.
 *
 * The hmr-loader appends a per-module bootstrap that:
 *   1. Calls registerModule with the module's exports so classToModule
 *      learns which moduleId each exported class came from.
 *   2. Calls registerModuleHot to hand over the module's `module.hot`
 *      handle so the registry can decide LATER whether the module gets
 *      to self-accept HMR updates.
 *   3. Calls handleModuleUpdate on re-evaluation so previously-mounted
 *      roots whose class came from this module get re-rendered.
 *
 * The accept decision is deferred to mountRoot. Only modules that export a
 * class registered as a root mount get module.hot.accept() called on them.
 * Every other module (helpers, child components, leaf views) is left
 * un-accepted so webpack bubbles its update up the dependency graph until
 * either a parent root catches it or the dev-server triggers a full reload.
 *
 * Without this gating, every leaf .tsx/.ts that exported a function would
 * blindly self-accept — webpack would stop bubbling at the leaf, the leaf
 * isn't in the mounts map, handleModuleUpdate would no-op, and the change
 * would be silently swallowed. The mount-only accept policy ensures every
 * edit either re-renders the affected root mount or triggers a full reload,
 * never silently disappears.
 *
 * Phase 1 — state is lost across the swap (same semantics as a manual
 * renderRootComponent re-call). Subtree-level HMR + state preservation are
 * future work.
 */

'use strict';

const modules = new Map();
const mounts = new Map();
const classToModule = new WeakMap();
// moduleId → module.hot handle for any module that hosted the loader
// bootstrap. mountRoot consults this to retroactively call .accept() on
// the moduleId hosting the mounted class.
const moduleHots = new Map();
// moduleIds that have had .accept() called on them at least once. Guards
// against repeat calls (webpack tolerates it; this keeps bookkeeping tidy).
const acceptedModules = new Set();

let nextMountId = 1;

function indexExports(moduleId, exports) {
  if (!exports || typeof exports !== 'object') return;
  for (const name of Object.keys(exports)) {
    const value = exports[name];
    if (typeof value === 'function') {
      classToModule.set(value, moduleId);
    }
  }
}

function findExportName(exports, ctr) {
  if (!exports || typeof exports !== 'object') return null;
  for (const name of Object.keys(exports)) {
    if (exports[name] === ctr) return name;
  }
  return null;
}

function hasModule(moduleId) {
  return modules.has(moduleId);
}

function registerModule(moduleId, exportsObj) {
  modules.set(moduleId, exportsObj);
  indexExports(moduleId, exportsObj);
}

/**
 * Hand over the module's `module.hot` handle so mountRoot can call
 * `.accept()` retroactively when the module is identified as a mount root.
 *
 * @param {string|number} moduleId
 * @param {{accept: (() => void)|undefined}|null|undefined} hot
 */
function registerModuleHot(moduleId, hot) {
  if (!hot || typeof hot.accept !== 'function') return;
  moduleHots.set(moduleId, hot);
}

function acceptModule(moduleId) {
  if (acceptedModules.has(moduleId)) return;
  const hot = moduleHots.get(moduleId);
  if (!hot || typeof hot.accept !== 'function') return;
  hot.accept();
  acceptedModules.add(moduleId);
}

function registerMount(record) {
  if (!record || !record.ctr || typeof record.renderFn !== 'function') return null;
  const moduleId = record.moduleId != null
    ? record.moduleId
    : classToModule.get(record.ctr);
  const mountId = nextMountId++;
  mounts.set(mountId, {
    moduleId,
    exportName: findExportName(modules.get(moduleId), record.ctr),
    ctr: record.ctr,
    renderFn: record.renderFn,
  });
  return mountId;
}

function unregisterMount(mountId) {
  mounts.delete(mountId);
}

function handleModuleUpdate(moduleId) {
  const newExports = modules.get(moduleId);
  if (!newExports) return;

  for (const [mountId, record] of mounts) {
    if (record.moduleId !== moduleId) continue;

    const newCtr = record.exportName != null ? newExports[record.exportName] : null;
    if (!newCtr || typeof newCtr !== 'function') {
      console.warn(
        `[valdi-hmr] mount ${mountId}: export '${record.exportName}' missing in new module ${moduleId}; skipping re-render`,
      );
      continue;
    }

    try {
      record.renderFn(newCtr);
      record.ctr = newCtr;
      classToModule.set(newCtr, moduleId);
      console.log(`[valdi-hmr] re-rendered mount ${mountId} (${record.exportName})`);
    } catch (err) {
      console.error(`[valdi-hmr] re-render failed for mount ${mountId}:`, err);
    }
  }
}

/**
 * Mount a Valdi component as a renderer root with transparent HMR support.
 *
 * The caller supplies a `renderFn(ctr)` callback that knows how to render the
 * component (including allocating a fresh ComponentPrototype each call —
 * Valdi treats same-prototype calls as no-ops, so re-renders need a new id).
 *
 * Initial mount: calls renderFn(ctr) immediately. Re-render on HMR update:
 * the registry calls renderFn(newCtr) with the swapped-in class.
 *
 * @param {Function} ctr - Component constructor (initial class).
 * @param {(ctr: Function) => void} renderFn - Renders ctr into the live UI.
 * @returns {number|null} Mount id (opaque) or null if registration failed.
 */
function mountRoot(ctr, renderFn) {
  renderFn(ctr);
  const mountId = registerMount({ ctr, renderFn });
  // Tell webpack the module that exports this class is allowed to
  // self-accept HMR updates. Modules that aren't mount roots never call
  // hot.accept(), so their edits bubble up the dependency graph until
  // either an ancestor root catches them or the dev-server full-reloads.
  const moduleId = classToModule.get(ctr);
  if (moduleId != null) acceptModule(moduleId);
  return mountId;
}

/**
 * Auto-attach HMR to a renderer class. Wraps
 * `RendererClass.prototype.renderRootComponent` so every root mount is
 * registered with the HMR runtime without the caller passing a renderFn
 * callback. Consumers write plain `renderer.renderRootComponent(...)`; HMR
 * intercepts, remembers the mount, and replays with the swapped-in class
 * on module update.
 *
 * Prod bundles never reach this function — call it inside a
 * `if (module.hot) { ... }` guard in your dev-only setup, and it becomes
 * dead code webpack strips.
 *
 * @param {Function} RendererClass - The renderer whose renderRootComponent
 *   method should be wrapped (e.g. ValdiWebRenderer).
 * @param {{newPrototype: () => any}} opts - Factory that returns a fresh
 *   ComponentPrototype on each replay. Valdi treats same-prototype calls as
 *   no-ops, so re-renders must allocate a new prototype id.
 */
function attachHmr(RendererClass, opts) {
  if (!RendererClass || !RendererClass.prototype) return;
  if (RendererClass.prototype.__valdiHmrAttached) return;
  const newPrototype = opts && opts.newPrototype;
  if (typeof newPrototype !== 'function') {
    throw new Error('attachHmr: opts.newPrototype factory is required');
  }

  const orig = RendererClass.prototype.renderRootComponent;
  if (typeof orig !== 'function') return;

  RendererClass.prototype.renderRootComponent = function(ctr, prototype, viewModel, context) {
    const result = orig.call(this, ctr, prototype, viewModel, context);
    const rendererRef = this;
    registerMount({
      ctr,
      renderFn: (newCtr) => {
        orig.call(rendererRef, newCtr, newPrototype(), viewModel, context);
      },
    });
    const moduleId = classToModule.get(ctr);
    if (moduleId != null) acceptModule(moduleId);
    return result;
  };
  RendererClass.prototype.__valdiHmrAttached = true;
}

module.exports = {
  hasModule,
  registerModule,
  registerModuleHot,
  registerMount,
  unregisterMount,
  handleModuleUpdate,
  mountRoot,
  attachHmr,
  // exported for tests
  _internals: { moduleHots, mounts, acceptedModules },
};
