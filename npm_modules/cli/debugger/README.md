# Valdi Debugger Frontend

This directory is the packaged browser UI served by `valdi debugger`.
It is intentionally local-only, dependency-free, and small enough to ship with
the CLI package.

## File Map

- `index.html`: static shell and DOM anchors for the debugger UI.
- `debugger.css`: themes, layout, controls, preview, inspector, and responsive styles.
- `debugger-state.js`: shared state, DOM references, constants, and action parameter helpers.
- `debugger-api.js`: fetch helpers, action stream, and development reload stream.
- `debugger-model.js`: snapshot normalization, tree traversal, bounds, issues, and selection helpers.
- `debugger-preview-html.js`: inert HTML projection of the hot-reloaded snapshot tree.
- `debugger-render.js`: header, target list, tree, preview overlay, inspector, and export rendering.
- `debugger-runtime.js`: target discovery, snapshots, runtime log streaming, heap, and copy/export helpers.
- `debugger-performance.js`: Hermes CPU profile controls.
- `debugger-actions.js`: UI actions, command prompt handling, auto-refresh, and externally driven debugger actions.
- `debugger-session.js`: `sessionStorage` restore/persist for reload-friendly debugger state.
- `debugger-bootstrap.js`: DOM event wiring and boot sequence.

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

Renderer tracing is intentionally not part of this foundation. It requires the
separate runtime and native renderer-instrumentation stack; land that stack
before adding renderer trace routes or controls to this debugger. Hermes CPU
profiling uses the existing inspector transport and has no such prerequisite.
Target input forwarding and data/network provider tabs should likewise land
with their runtime-side contracts and end-to-end tests rather than as inactive
browser-only surfaces.
Web-renderer inspection should land together with its first-party bridge rather
than expose an inert preview flag from this foundation.

Detailed debugger snapshots explicitly opt in to component ViewModel and state
serialization. That data can be sensitive, is bounded by a per-field and
whole-tree character budget, and is never included in ordinary `valdi inspect
tree` requests. Auto-refresh starts disabled so serialization remains a
deliberate local debugging action. The server rejects non-loopback Host,
Origin, and cross-site browser API requests.

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
