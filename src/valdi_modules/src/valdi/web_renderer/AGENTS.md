# AGENTS.md - Valdi Web Renderer Notes

This file applies to `/src/valdi_modules/src/valdi/web_renderer`.

The web renderer is being rebuilt around a native-like renderer core. Do not treat older web renderer behavior as sacred unless the user explicitly asks for compatibility. The current priority is a clean, fast, C++-inspired architecture that can be completed incrementally.

## Directory Layout

- `src/core/`
  - Renderer primitives: `ViewNode`, `ViewNodeTree`, `ElementClass`, and `Palette`.
  - `ViewNode` owns the DOM element, parent/children links, applied attributes, dirty flags, palette state, lifecycle state, and the minimal applier context.
  - `ViewNodeTree` owns node lookup, root ownership, render batching, scheduled flushes, and palette change integration.
- `src/elements/`
  - One file per concrete `ElementClass` subclass.
  - `ElementClassRegistry.ts` is the central registry mapping Valdi view class aliases to singleton element classes.
  - `ElementClassSupport.ts` contains shared DOM helpers for element classes.
- `src/attributes/`
  - `AttributesApplier.ts` is the stateful attribute owner/resolution/dirty flushing class.
  - `AttributeApplierHelpers.ts` contains typed parser/factory helpers such as number, boolean, CSS length, enum, color, and direct style appliers.
- `src/utils/`
  - Shared low-level utilities such as `IndexedRecord`.

Keep these boundaries intact.

## Element Classes

- Each built-in view type should have its own `ElementClass` subclass file in `src/elements/`.
- Do not add generic catch-all classes such as `BasicElementClass` or `SimpleDivElementClass`.
- Do not use large switch statements on `viewClass` or `attributeName`. Use the registry for view class lookup and the per-class `elementAttributes` record for attribute lookup.
- Do not introduce temporary creation-info objects.
- `ElementClass.createElement()` owns template caching and cloning. Subclasses should implement `protected onCreateElement()` to build one template element, and the base class clones it for each node.
- The registry should construct and wire the concrete element class singletons. Dependent classes should receive class instances they need, for example label/text classes can receive view/textfield classes and reuse their appliers.
- Unknown or incomplete element behavior should be represented in the specific element class with a clear TODO, not hidden in a generic fallback class.

## Attribute Architecture

- `AttributesApplier` owns stored attribute values, style owner values, dirty attribute names, and dirty composites.
- Attribute appliers should be typed on the HTMLElement subtype they operate on and should receive the element directly. Avoid passing `ViewNode` into appliers.
- `AttributeApplierContext` should expose only what appliers truly need. Keep additions narrow and justify each one.
- Every `AttributeApplier` and `CompositeAttribute` must provide `reset`. Reset is not optional; it is called when the resolved value becomes `undefined` or `null`.
- Attribute appliers must not branch behavior based on the attribute name. If two attributes need different reset/apply behavior, use two appliers.
- Missing attributes should warn with node id, element class name, attribute name, and provided value.
- Failed applies should throw locally, be caught by `AttributesApplier`, and log node id, element class name, attribute name, provided value, and error message. Do not let applier failures escape a flush.
- Do not add blind catches. If a promise or callback can fail, log enough context to debug it.
- Avoid hard casts. Prefer typed helpers, narrow element interfaces, and DOM APIs that TypeScript can type-check.

## Attribute Values And Priority

- Direct attributes outrank style attributes.
- `undefined` and `null` remove that owner value and allow lower-priority owners to win again.
- `false` is a real boolean value and must not be treated as removal.
- Style objects are treated as immutable by identity.
- Attribute conflicts are rare. Optimize the common case:
  - store the single owner/value inline in `StoredAttribute`;
  - allocate owner collections only on real conflicts;
  - lazily allocate optional state such as cleanup callbacks and dirty composites.
- Store the resolved `ElementAttribute` lookup inside `StoredAttribute` so each attribute name is resolved once.

## Dirty Update Pass

- The tree has one root node. Do not scan all nodes to flush normal updates.
- `ViewNodeTree.flush()` starts at the root and calls the root-to-children update pass.
- Dirty state must propagate upward. There should never be a dirty child with a clean attached ancestor.
- It is valid for detached nodes/subtrees to become dirty, but the dirty state must propagate once that subtree is rooted or moved under an attached root.
- `markNeedsUpdate()` should be a no-op when the node already has an update flag.
- Use a bitfield for update flags so checking whether any update is needed is cheap.
- `setAttribute()` should only mark an update when the resolved attribute value actually changes.
- During render batches, flush at the outermost `endRender()`. Outside render batches, schedule one microtask flush.
- If a flush dirties more work, the tree should continue flushing until the requested work is complete.

## Palette Handling

- Palette state is managed by `ColorPaletteManager` in `src/core/Palette.ts`. Do not add a second global palette state path.
- `ViewNodeTree` owns the palette manager instance for a renderer tree and listens for palette changes.
- `colorPaletteName` should apply by calling `AttributeApplierContext.setColorPalette()`.
- Palette resolution happens during the root-to-children update pass. Pass the starting active palette from the root; do not recompute active palette ad hoc inside nodes.
- A node inherits its parent palette unless it has an override. Moving a node must mark palette dirty so descendants recompute during the update pass.
- Palette mutation and active palette changes may traverse/reapply from the root. This is intentionally O(n) because palette changes are infrequent.
- Color-dependent attributes and composites must be marked dirty when palette resolution changes or a forced palette reapply occurs.

## Composite Attributes

- A composite is color-dependent if any part is color-dependent. Store this on `composite.colorDependent`.
- Composite parts should mark the composite dirty, and the composite should apply once during flush.
- Keep composites small and purposeful. Use them when attribute values depend on each other, for example transform parts.

## Performance Expectations

- Optimize for the common path in rendering and attribute updates.
- Avoid temporary objects in hot paths. Examples:
  - no creation-info object for element creation;
  - no repeated default style object construction for each node; use per-element-class template cloning;
  - no owner-value allocation for the single-owner attribute case;
  - lazy allocation for optional state.
- Prefer plain records or `IndexedRecord` for small hot mutable key/value sets. Use `Map` only when it is actually the right fit.
- Use `IndexedRecord` for dirty name sets and other mutable keyed collections that need efficient `set`, `remove`, `clear`, `keys`, `empty`, and `pop`.
- Do not repeat expensive lookups during flush. Cache per-attribute metadata in stored attribute state.
- Do not add global node scans for normal updates. Root traversal is the update mechanism.
- Keep APIs tight. Public methods should be what other classes actually need; ViewNode internals should stay private.

## Code Style Preferences

- Keep the public renderer/delegate API stable unless the user explicitly asks to change it.
- Rename operations should be complete: folder names, imports, dynamic `customRequire()` paths, test names, and local variables should agree.
- Use precise names. Avoid confusing pairs like `AttributeApplier` and `AttributeAppliers.ts`; prefer names that explain role, such as `AttributeApplierHelpers.ts`.
- Module constants should use `UPPER_SNAKE_CASE`.
- Do not use TypeScript default parameters. Put defaults inside the function body or pass explicit values at call sites.
- Avoid generic error-message parameters passed into helper methods. Prefer specific helper names such as `getNodeOrThrow`.
- Do not add DOM data attributes such as `data-valdi-node-id` unless there is a concrete runtime need.
- If an old view implementation has been replaced by element classes, remove it instead of keeping duplicate paths.
- Do not preserve dead files just because they once had behavior. If only a tiny helper survives, move the helper to an appropriate home and delete the old file.
- Use `interface`, not type-alias object patterns.

## Testing

- Run the focused target after web renderer changes:

```bash
bazel test //src/valdi_modules/src/valdi/web_renderer:test
```

- Unit tests should cover:
  - style/direct priority and fallback;
  - boolean `false` as a real value;
  - dirty update coalescing;
  - detached dirty subtree propagation when attached;
  - root-only flushing, not all-node scanning;
  - palette inheritance, mutation, active palette swap, and subtree overrides;
  - composite coalescing;
  - missing attribute warnings;
  - applier failure logging context;
  - `ViewNodeTree` create/move/root/destroy behavior.
- When visual or integration behavior matters, use the Valdi integration test system and compare against the provided baseline output rather than relying only on unit tests.
