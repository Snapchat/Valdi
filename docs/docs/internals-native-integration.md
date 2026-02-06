# Native Integration Internals

Valdi provides a robust abstraction layer for managing native UI hierarchies across iOS, Android, and Skia. This document explores the internal mechanisms that enable high-performance native view management.

## View Abstraction

The C++ runtime interacts with native views through the `View` and `ViewFactory` abstract classes. This allows the core logic to remain platform-agnostic while the actual UI is rendered using UIKit (iOS), Android Views, or Skia.

### `View` Class
A `View` represents a single piece of the user interface. It wraps a platform-specific view (e.g., `UIView` or `android.view.View`) and provides a uniform interface for:
- **Hierarchy Management**: Inserting, removing, and reordering child views.
- **Geometry**: Setting the frame (position and size) relative to the parent.
- **Measurement**: Calculating the view's intrinsic size (e.g., for labels or images).
- **Attributes**: Applying and resetting key-value properties.

### `ViewFactory` Class
The `ViewFactory` is responsible for creating and recycling `View` instances.
- **Pooling**: To minimize the overhead of view inflation, `ViewFactory` maintains a pool of reusable view instances.
- **Measurer Placeholder**: Each factory provides a specialized view instance used exclusively for measuring elements that are not currently visible.
- **Attribute Binding**: A factory is associated with a set of `BoundAttributes` that it supports.

## `IViewManager` Interface

The `IViewManager` is the primary interface implemented by a UI backend. It serves as a registry and resolver for the runtime.

### Key Responsibilities
1. **View Class Resolution**: Resolves a `ViewFactory` for a given view class name (e.g., "label", "image", or a custom class).
2. **Attribute Discovery**: When a view class is first used, the runtime queries the `IViewManager` for all attributes supported by that class and its hierarchy.
3. **Hierarchy Introspection**: Provides the class hierarchy for a view class, allowing Valdi to merge attributes from parent classes (e.g., a `Label` inherits attributes from `View`).

## Attribute Binding System

Attribute binding is the process of registering and applying properties to native views. This system is designed to be efficient and type-safe.

### Binding Declaration
When a view class registers an attribute, it specifies:
- **Expected Type**: (e.g., Int, Double, String, Style). This allows the runtime to pass values directly without expensive boxing/unboxing.
- **Apply Callback**: The logic that mutates the native view to reflect the new attribute value.
- **Reset Callback**: The logic that restores the view to its default state when an attribute is removed.
- **Layout Impact**: Whether changing the attribute should mark the layout as dirty (e.g., changing a label's font size).

### Cross-Platform Consistency
Valdi ensures that attributes behave identically across all supported platforms. The core layout attributes (Flexbox) are handled by the Yoga engine, while view-specific attributes (colors, opacity, etc.) are implemented by each platform's integration layer.

## Custom View Integration

The `<custom-view>` element allows developers to inject arbitrary native views into a Valdi tree.

1. **Class Mapping**: Valdi can instantiate views by their platform-specific class names using reflection (Android) or class lookup (iOS).
2. **Factory Injection**: For more control, a `ViewFactory` can be passed via the `ComponentContext`, allowing for custom initialization and dependency injection.
3. **Lazy Registration**: Attribute binding for custom views happens lazily—only when the view class is first encountered at runtime.
