# Valdi Debugger Frontend

This directory is the packaged browser UI served by `valdi debugger`.
It is intentionally local-only, dependency-free, and small enough to ship with
the CLI package.

## File Map

- `index.html`: static shell and DOM anchors for the debugger UI.
- `debugger.css`: themes, layout, controls, preview, inspector, and responsive styles.
- `debugger-state.js`: shared state, DOM references, constants, and action parameter helpers.
- `debugger-api.js`: fetch helpers, action stream, and development reload stream.
- `debugger-tree-model.js`: transport-neutral hierarchy identity, traversal, lookup, and path helpers shared by both frontends.
- `debugger-model.js`: snapshot normalization, bounds, issues, and standalone selection helpers.
- `debugger-preview-html.js`: inert HTML projection of the hot-reloaded snapshot tree.
- `debugger-render.js`: header, target list, tree, preview overlay, inspector, and export rendering.
- `debugger-runtime.js`: target discovery, snapshots, runtime log streaming, heap, and copy/export helpers.
- `debugger-performance.js`: Hermes CPU profile controls.
- `debugger-actions.js`: UI actions, command prompt handling, auto-refresh, and externally driven debugger actions.
- `debugger-session.js`: `sessionStorage` restore/persist for reload-friendly debugger state.
- `debugger-bootstrap.js`: DOM event wiring and boot sequence.
- `devtools-panel.html`, `devtools-panel.css`, and `devtools-panel.js`: the focused Chromium Elements and Console panel embedded by the generated extension.

Scripts are loaded as classic browser scripts in the order listed in
`index.html`. There is no module loader or bundler for this frontend; shared
functions and variables are intentionally global within the page.

## Server Contract

The local HTTP server lives in `../src/debugger/server.ts`. It serves these
assets and proxies debugger requests to Valdi daemon or Hermes endpoints.

Important routes:

- `/api/status`: probes daemon targets and hot reload proxy state.
- `/api/snapshot`: fetches the selected target's view tree and preview data.
- `/api/runtime-logs` and `/api/runtime-logs/stream`: read and stream target logs.
- `/api/debugger/state`, `/api/debugger/events`, and `/api/debugger/actions`: keep the browser UI and external agents in sync.
- `/api/performance/profile/*`: list Hermes contexts and capture CPU profiles.
- `/api/devtools/target`: matches the inspected Chromium page to the exact configured preview origin and path.
- `/api/devtools/snapshot`, `/api/devtools/highlight`, and `/api/devtools/evaluate`: proxy the explicit web debugger bridge contract through loopback CDP.

Renderer tracing is intentionally not part of this foundation. It requires the
separate runtime and native renderer-instrumentation stack; land that stack
before adding renderer trace routes or controls to this debugger. Hermes CPU
profiling uses the existing inspector transport and has no such prerequisite.
Target input forwarding and data/network provider tabs should likewise land
with their runtime-side contracts and end-to-end tests rather than as inactive
browser-only surfaces.
Web-renderer inspection depends on the target page explicitly exposing
`window.__VALDI_WEB_DEBUGGER__` with `getSnapshot()`, `highlightNode()`, and
`clearHighlight()`. The renderer-side adapter is intentionally outside this
CLI/DevTools core.

Detailed debugger snapshots explicitly opt in to component ViewModel and state
serialization. That data can be sensitive, is bounded by a per-field and
whole-tree character budget, and is never included in ordinary `valdi inspect
tree` requests. Auto-refresh starts disabled so serialization remains a
deliberate local debugging action. The server rejects non-loopback Host,
Origin, and cross-site browser API requests.

The normal debugger document and every other static asset use
`frame-ancestors 'none'` plus `X-Frame-Options: DENY`. Only
`/devtools-panel.html` permits a Chromium extension ancestor; executable
DevTools routes additionally require same-origin JSON requests.

## Development Loop

For an installed CLI:

```bash
valdi debugger
```

For local CLI development, build the CLI, then run the built entrypoint:

```bash
cd npm_modules/cli
npm run build
node dist/index.js debugger --host 127.0.0.1 --port 8765
```

For a manually launched Owl/Chromium web preview:

```bash
node dist/index.js debugger \
  --web-preview-url http://127.0.0.1:8080/index.html \
  --chromium-debugging-port 9222
```

Start Owl/Chromium with the printed `--remote-debugging-port` and
`--load-extension` values, then open the exact opted-in preview URL printed by
the command. Target matching removes only the injected `valdiDebugger` and
`valdiDevTools` parameters, then requires the same origin, pathname, and
remaining query parameters.

The synthetic native-tree preview never auto-loads projected HTTP(S) image,
video, CSS background, or WebView resources. Only `data:` and `blob:` media are
assigned; WebView contents are represented by an inert placeholder.

The debugger server watches `.html`, `.css`, and `.js` files in this directory
and emits `/api/dev-events`; the browser reloads itself when these files change.
If you add a new static asset type, update the server MIME map and watcher.

Session state is persisted in `sessionStorage` under
`valdi.debugger.session.v1`, so normal debugger refreshes should preserve the
active section, selected target/node, filters, expanded tree nodes, and capture
settings.

## Validation

For frontend-only changes:

```bash
for file in npm_modules/cli/debugger/debugger*.js; do node -c "$file" || exit 1; done
git diff --check
```

For changes that touch the server or CLI TypeScript:

```bash
cd npm_modules/cli
npm run build
node_modules/.bin/tsc --noEmit --project tsconfig.dist.json
```

When validating the full ordered browser bundle, concatenate the scripts in the
same order as `index.html` and parse the result with `new Function(...)`.
