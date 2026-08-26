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
- `debugger-performance.js`: renderer trace and Hermes CPU profile controls.
- `debugger-providers.js`: provider discovery plus read-only Storage and SQL inspector rendering.
- `debugger-settings.js`: published debug-setting discovery, rendering, validation, and mutation requests.
- `debugger-actions.js`: UI actions, command prompt handling, auto-refresh, and externally driven debugger actions.
- `debugger-session.js`: `sessionStorage` restore/persist for reload-friendly debugger state.
- `debugger-bootstrap.js`: DOM event wiring and boot sequence.
- `devtools-panel.html`, `devtools-panel.css`, and `devtools-panel.js`: the focused Chromium Elements, Console, and bounded Performance panel embedded by the generated extension.

Scripts are loaded as classic browser scripts in the order listed in
`index.html`. There is no module loader or bundler for this frontend; shared
functions and variables are intentionally global within the page.

## Server Contract

The local HTTP server lives in `../src/debugger/server.ts`. It serves these
assets and proxies debugger requests to Valdi daemon or Hermes endpoints.

Important routes:

- `/api/status`: preserves the legacy daemon-port and hot reload proxy status contract.
- `/api/snapshot`: fetches the selected target's view tree and preview data.
- `/api/runtime-logs` and `/api/runtime-logs/stream`: read and stream target logs.
- `/api/debugger/state`, `/api/debugger/events`, and `/api/debugger/actions`: keep the browser UI and external agents in sync.
- `/api/input`: validates and forwards bounded input requests to the selected Valdi target.
- `/api/performance/trace/*`: start, stop, capture, and export native renderer traces.
- `/api/debugger/providers` and `/api/debugger/providers/request`: discover and query target-owned debugger providers.
- `/api/debugger/settings`: discover and update target-published debug settings.
- `/api/performance/profile/*`: list Hermes contexts and capture CPU profiles.
- `/api/devtools/targets`: returns a fresh, bounded registry of native Valdi, explicit web-preview, and JavaScript-proxy targets.
- `/api/devtools/target`: resolves either one opaque native target ID or the exact configured inspected Chromium page identity.
- `/api/devtools/snapshot`, `/api/devtools/highlight`, and `/api/devtools/evaluate`: proxy the explicit web debugger bridge contract through loopback CDP.
- `/api/devtools/performance/snapshot` and `/api/devtools/performance/trace/*`: sample the exact web preview and record one bounded global Chromium trace without changing the daemon/Hermes `/api/performance/*` routes.

Renderer tracing uses the runtime debugger protocol and the existing native
trace recorder. Captures are process-wide: the selected context is the capture
target used to reach the runtime, not the origin assigned to every event.
One-shot captures are limited to 15 seconds so the debugger handler can retain
the result before the native recorder's independent safety timeout, and
exported JSON can be opened in Perfetto. Native bounds report dropped event
counts, and retained timeout/retry results expire after one minute.
Hermes CPU profiling uses the existing inspector transport.

The native and synthetic previews forward capability, query, tap, focus, text,
key, and scroll requests through the selected target's bounded input contract.
Web-renderer inspection uses the first-party bridge exposed as
`window.__VALDI_WEB_DEBUGGER__` with `getSnapshot()`, `highlightNode()`, and
`clearHighlight()`. The DevTools panel proxies inspection through the exact
configured loopback Chromium target.

Web-preview performance requests require the exact `sessionId`, `inspectedUrl`,
and per-tab `targetNonce`; incomplete, stale, or cross-tab identities fail
closed. The recorder normalizes CDP events incrementally and retains at most
10,000 events with 2 KiB UTF-8 trace names. One-shot captures run for their
requested duration from 100 milliseconds through 15 seconds; manually started
recordings have a 15-second watchdog. An undelivered completed result is kept
for one minute. The complete response is limited
to 4 MiB and contains one normalized trace list plus export metadata—never a
duplicate raw or Perfetto event list. The panel shows at most 120 graph samples,
120 timeline rows, and 12 grouped summary rows. Its only trace filters are
Valdi, Browser, and All; Chrome Trace JSON is assembled only when exported.

Every `/api/devtools/targets` descriptor declares an `identityMode`.
`target-id` is used by native Valdi and waiting proxy records; only an
attachable `target-id` target using the `valdi-daemon` transport can be resolved
or snapshotted through `targetId`. `inspected-page` is reserved for the explicit
Chromium web preview, which must use the complete `inspectedUrl` plus
`targetNonce` identity for target resolution and the complete `sessionId`,
`inspectedUrl`, and `targetNonce` tuple for snapshots and performance. An
inspected-page target is never attachable through its public ID, and identity
modes cannot be mixed in one request.

Registry reads are snapshots, not leases: each list or target-ID resolution
rediscovers the current endpoints and rejects removed or replaced identities.
Discovery reads existing companion-owned ADB forwards and loopback proxy
metadata without creating or replacing forwarding state. It retains at most 8
ADB forwards, 10 daemon endpoints, 16 clients per endpoint, 64 contexts per
client, 128 proxy records, and 256 final targets; daemon fan-out is limited to
4 workers. Configured web-preview URLs are capped at 4,096 UTF-8 bytes and their
derived names at 256 bytes. Proxy responses are capped at 512 KiB and the final
serialized `/api/devtools/targets` response is independently capped at 512 KiB.
The legacy `/api/status` route intentionally keeps its prior port probing and
forwarding behavior and does not run registry discovery.

The DevTools panel has two mutually exclusive identity modes. An extension-opened
inspected page keeps using its exact `sessionId`, `inspectedUrl`, and
`targetNonce` tuple and never requests the target registry. A directly opened
panel receives one opaque `targetId`, discovers at most 256 registry entries,
and never falls back to the first, only, attached, or newly discovered target.
Only attachable `target-id` entries using the `valdi-daemon` transport and
advertising both `components` and `snapshot` can be selected. Inspected-page,
waiting, and unsupported entries remain visible with a disabled explanation.
Direct snapshot and capability-supported tool requests send only the selected
opaque `targetId`.

Target changes close the old Console stream before installing the replacement,
invalidate outstanding snapshot, highlight, Console, and Performance work, and
clear all target-owned presentation. Console and Performance tabs are enabled
only when the selected descriptor advertises their capability. A target change
is blocked while a Performance operation or recording owns the selected target;
if registry removal forces a detach, the exact previous Performance owner stays
available solely so the recording can be stopped and retrieved. Registry
removal never selects another target automatically.

The Data section discovers target-owned providers through a generic custom
message contract. The persistence module registers its bounded web snapshot as
the `persistent-store` Storage provider and reports it unavailable on platforms
whose native binding does not expose that inspector. SQL, Network, and
key-value integrations remain unavailable unless a runtime provider registers
them; the UI never synthesizes sample data.

Runtime adapters cross the provider boundary with an already serialized JSON
object document, not an arbitrary object graph. They should call
`createDebuggerProviderOwner(module, 'stable/adapter/module/key')`, register
their provider through that owner, and return
`createDebuggerProviderResult(JSON.stringify(snapshot))` from `handleRequest`.
Core caps action documents at 48 KiB of UTF-8, validates their depth,
collection sizes, strings, and total value count, and then enforces 128 KiB on
the complete serialized custom-response body including metadata. Owners bind
to the creating module object's hot-reload callback automatically, including
webpack modules where `module.path` is absent. Adapters call
`owner.dispose()` only when stopping before a reload. A newer registration for
an existing provider ID permanently invalidates the older registration. The
PersistentStore adapter projects known data properties into a deterministic
43 KiB document before calling this helper. On native runtimes the owner
observes `module.path`; the explicit stable key is replacement identity only.
Web runtimes fall back to observing that stable key because webpack does not
provide `module.path`.

Published settings are application-owned controls registered only in debug
runtimes. Values are limited to declared toggle, select, text, and number
settings; the server does not expose arbitrary runtime property mutation.

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
the command. Target matching ignores the reserved development parameters
`valdiDebugger`, `valdiDevTools`, and the optional, explicitly requested
`valdiTrace` parameter, then requires the same origin, pathname, and remaining
query parameters. The debugger command does not enable tracing by default.

The synthetic native-tree preview never auto-loads projected HTTP(S) image,
video, CSS background, or WebView resources. Only `data:` and `blob:` media are
assigned; WebView contents are represented by an inert placeholder.

The debugger server watches `.html`, `.css`, and `.js` files in this directory
and emits `/api/dev-events`; the browser reloads itself when these files change.
If you add a new static asset type, update the server MIME map and watcher.

Session state is persisted in `sessionStorage` under
`valdi.debugger.session.v1`, so normal debugger refreshes should preserve the
active section, selected target/node, filters, expanded tree nodes, and capture
settings, plus the active provider and published-settings group. Provider and
settings payloads are cleared when the selected target changes.

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
