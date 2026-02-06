# View Recycling

## Introduction

To achieve fast inflation and fast scrolling performance, Valdi recycles all the native views (`UIView` and `android.view.View` and its subclasses). Whenever a view needs to be destroyed, either because a `render-if` toggled to false, or it was part of `for-each` branch which needs to be removed, or the whole component simply needs to be destroyed, Valdi will perform the following actions:
- The view will be cleared out of all its attributes, this means that all the attributes's reset methods get called on it. This will make the view back to a neutral state. In order to have consistent rendering, it is important that any attributes applied to a View at the platform level can be reverted.
- On Android, if the View implements `ValdiRecyclableView`, `prepareForRecycling()` will be called and if the view returns false, the view will be simply discarded instead of recycled.
- On iOS, if the View overrides `willEnqueueIntoValdiPool`, it will be called and if the view returns false, the view will also be discarded.
- If the methods above returned true, the View will be placed into a dedicated view pool keyed by its class name.

> [!Note]
> The recycling logic is managed by **View Factories**. Each view class has a corresponding `ViewFactory` that maintains its own pool of reusable view instances. This ensures that view inflation is as efficient as possible across the entire application.

Later on, whenever Valdi needs to create a view of a certain view class, it will first look up in the view pool if there is one pooled view available. If there is it will use it, otherwise a new instance will be created.

The view pool is _global_, meaning that if there was a previously destroyed Valdi screen, any subsequent opening Valdi screen will inflate faster, as long as they have a fair amount of common views. Because Valdi is designed around composition, most native view trees should have very few different kind of view classes, and as such the number of underlying dedicated view pools should be low and the hit rates on those pools should be high.

## Impact on performance

This approach aims to make inflation latency as low as possible. It allows patterns that are easy to understand like `for-each`, `render-if` to function in an efficient way. Without it, toggling a `render-if` could mean creating/destroying a lot of views potentially. Thanks to the pooling, once the view pool is cached most of the work being done from Valdi is about reconfiguring views.

## Scrolling

An additional major benefit is its relationship to the `limitToViewport` attribute (set to `true` by default).

Views that are not visible on screen and have this attribute enabled won't be inflated. When the views become visible they become automatically inflated. This is a common operation when displaying a list of items in a `ScrollView`. With the view pools, doing this is quickly becomes a cheap process.

The combination of `limitToViewport` with the internal automatic view recycling and `for-each` is how Valdi allows you to display gigantic list of items in a `ScrollView`. Other frameworks, including `UIKit` and the `Android SDK` typically require you to use a `UICollectionView` or `RecyclerView` and implement a bunch of specific interfaces. They handle the recycling at the cell level for you, but they require you to properly cleanup the cell yourself before it gets to the pool. It is a very common source of bugs when cleanup is not handled correctly. 

Making complex layouts with those APIs is also often non trivial. Valdi solves those problems by managing the cleanup of the views automatically, and by not having to use a dedicated component for managing scrollable content that should be using cell recycling. You just use Flexbox to layout your view tree, even for infinitely scrollable content.

## Implementing view recycling on your custom View

- On Android, implement `ValdiRecyclableView` and return `true` in `prepareForRecycling()`. You can do any necessary cleanup in that method as well. Most of the time you shouldn't have anything to do in this method. Don't reset the attributes yourself, this will be done automatically by calling the reset methods.
- On iOS, override `willEnqueueIntoValdiPool` and return `true`. Same comments apply from Android above.
- That's it, your view is now recyclable for every consumers of it!

### Note on custom attributes

Make sure that for any attributes you declare in your view, you implement the reset methods and you are able to rollback the attribute exactly as it was before. Failing to do this means that the view might be enqueued in the pool dirty, and this could have side effects like indeterministic rendering if the view ends up being used somewhere else, on a different component or screens.

Additionally, try to make your attributes apply and reset methods fast. if the view is recycled and re-used when scrolling, it will have to call your attribute handlers to configure your view and thus will have an impact on performance if those methods are slow.
