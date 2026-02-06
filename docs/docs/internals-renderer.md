# Valdi Renderer Internals

The Valdi Renderer is responsible for managing the lifecycle of elements and components, performing efficient diffing, and synchronizing the UI state with the native runtime.

## Core Concepts

### VirtualNode Tree vs. Element Tree
- **VirtualNode Tree**: A TypeScript-only hierarchy containing both **Components** (logic) and **Elements** (UI primitives).
- **Element Tree**: A filtered version of the VirtualNode tree containing only native elements. This tree is synchronized with the C++ runtime.

### Node Prototypes
The Valdi compiler generates a **Prototype** for every TSX expression. Prototypes act as blueprints, allowing the Renderer to identify nodes across render passes without expensive lookups.

## The Diffing Algorithm

Valdi uses a stack-based, incremental diffing algorithm that runs *during* the render pass. This minimizes object churn and allows for early exits.

### Stack-Based Rendering
Push operations can be nested. The Renderer keeps a stack to know what is the node being rendered and their ancestors.

### Key Resolution
Valdi generates stable keys for nodes based on their Prototype and position. 
- **Automatic Keys**: O(1) in most cases. If a conflict occurs (e.g., multiple identical prototypes), an incrementing sequence is used.
- **Explicit Keys**: Recommended for loops (e.g., `<label key={item.id} />`) to ensure nodes are correctly moved rather than mutated when the list order changes.

### Bypassing Rendering
To maximize performance, the Renderer implements a **Stop Gap Rule**:
- If a component's **ViewModel** (props) has not changed since the last render, the `onRender()` call is bypassed entirely.
- This prevents a change at the root from cascading through the entire application tree.

## Algorithm Complexity

The Renderer is optimized for fast incremental renders. Almost all operations are constant time unless changes are detected:

| Operation | Complexity |
| :--- | :--- |
| **Generating Key** | O(1) in most cases (up to O(n) on key conflict) |
| **Diffing ViewModels** | O(1) |
| **Resolving Children** | O(n) initial, O(1) incremental (no change), O(n+k) incremental (with change) |

## Advanced Optimization: Slot Re-rendering

One of Valdi's most powerful optimizations is the ability to re-render **Slots** independently of their parent component.

### The Problem
In traditional frameworks, if a parent passes new children to a child component, the child component must re-render to incorporate them.

### The Valdi Solution
The Renderer tracks the exact location where a slot was evaluated in the element tree. When the parent re-renders:

1. The Renderer identifies the slot's parent node and its siblings.
2. It "replays" the slot's render function at that specific location.
3. The child component itself is **not** re-rendered, preserving its internal state and avoiding unnecessary work.

## Element Synchronization

The `RendererDelegate` (specifically `JSXRendererDelegate`) translates VirtualNode changes into native commands.

### Efficient Updates
The Renderer computes the minimal set of operations to sync the native tree:
- **Insert/Remove**: Standard O(n) operations.
- **Move**: Detected by comparing the current children array with the previous one.
- **Attribute Updates**: Constant time O(1) updates. Valdi does not detect "removed" attributes; they must be explicitly set to `undefined`.

### Marshalling Optimizations
- **Interned Attributes**: Common attribute names (e.g., "backgroundColor") are interned as integers to avoid string conversion overhead.
- **Style Interning**: `Style<>` objects are interned lazily. Reusing style objects in TypeScript allows the bridge to pass a single integer ID instead of the full style dictionary.

## Error Handling & Boundaries

The Renderer wraps all component lifecycle methods in `try-catch` blocks to ensure UI stability.

### Error Recovery
If a render fails:
1. The Renderer unwinds the stack to the failing component.
2. It searches for the nearest ancestor implementing `onError()`.
3. In debug mode, the `DefaultErrorBoundary` catches these errors and displays a full-screen error report.
