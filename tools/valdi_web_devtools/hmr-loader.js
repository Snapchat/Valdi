// @ts-check
/**
 * Valdi Web HMR webpack loader.
 *
 * Appends a tiny bootstrap to every module it processes that:
 *   1. Registers the module's exports with the HMR runtime registry so the
 *      registry can map exported classes to their source moduleId.
 *   2. Hands the module's `module.hot` handle to the registry — but does
 *      NOT self-accept here. Whether the module gets to self-accept is
 *      decided by mountRoot: only modules whose exported class is mounted
 *      as a root get module.hot.accept() called on them.
 *   3. Invokes handleModuleUpdate on re-evaluation so previously-mounted
 *      roots re-render with the new class.
 *
 * Why defer accept? If every module with a function export blindly
 * self-accepted, webpack would stop bubbling at every leaf. Children of a
 * mounted root aren't tracked in the mounts map, so handleModuleUpdate
 * would no-op for their updates and the edit would be silently swallowed.
 * Letting non-root modules go unaccepted bubbles their updates up the
 * dependency graph; if no ancestor root catches the update, the dev-server
 * triggers a full reload (visible change + lost state), which is correct
 * behavior — never silent.
 *
 * The loader matches /\.tsx?$/. The loader runs AFTER ts-loader (webpack
 * applies loaders right-to-left), so by the time we see the source it's
 * already plain JS. We string-append the bootstrap and pass the source
 * map through unchanged — no AST rewriting, easy to read in the bundle,
 * easy to remove.
 */

'use strict';

const HMR_BOOTSTRAP = `
;(function valdiHMRBootstrap() {
  if (typeof module === 'undefined' || !module.hot) return;
  var hmr;
  try {
    hmr = require('valdi-web-devtools/hmr');
  } catch (e) {
    return;
  }
  var exportsObj = module.exports;
  var hasFunctionExport = false;
  if (exportsObj && typeof exportsObj === 'object') {
    for (var k in exportsObj) {
      if (typeof exportsObj[k] === 'function') { hasFunctionExport = true; break; }
    }
  }
  if (!hasFunctionExport) {
    // Pure-data / helper module. Skip registration entirely — webpack
    // bubbles the update to a parent that handles it (or full-reloads).
    return;
  }
  var wasRegistered = hmr.hasModule(module.id);
  hmr.registerModule(module.id, exportsObj);
  hmr.registerModuleHot(module.id, module.hot);
  // Intentionally NOT calling module.hot.accept() here. mountRoot calls
  // accept on the moduleId hosting the mounted class; other modules
  // remain unaccepted and let webpack bubble.
  if (wasRegistered) {
    hmr.handleModuleUpdate(module.id);
  }
})();
`;

module.exports = function valdiHMRLoader(source, map, meta) {
  this.cacheable && this.cacheable();
  const callback = this.async();
  callback(null, source + '\n' + HMR_BOOTSTRAP, map, meta);
};
