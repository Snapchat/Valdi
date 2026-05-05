# Native CollectionViews

## Introduction

When using the `@ExportModel` on a component, Valdi generates Objective-C, Swift, and Kotlin View classes that act as entry points from iOS/Android into your Valdi component in TypeScript. Your component can represent an entire screen, or it can represent a smaller UI component as part of an existing iOS/Android view hierarchy. For instance, it can represent a cell inside a `UITableView` or `RecyclerView` scrollable list.

Creating an entry point view can be an expensive operation depending on the complexity of your TypeScript component. It is thus best to avoid creating root views when scrolling, for example when integrating a Valdi view inside an iOS's `UITableViewCell` or inside an Android's `RecyclerView` item view. We will look at two options that you can use to efficiently integrate a Valdi view inside an existing iOS/Android view hierarchy that uses `UITableView`, `UICollectionView` or `RecyclerView`.

### Update view models on scroll

The first option is to create your entry point view inside the view instance representing your cell or item, and provide it the right view model for the item it represents. You might have something like `SCMyTableViewCell`, meant to represent a cell inside the `UITableView`. Inside `SCMyTableViewCell`, you can create an instance of your Valdi view with no view models initially, and whenever `SCMyTableViewCell` is being displayed, you can pass the view model that contains the data that your Valdi component should display. When scrolling, `UITableView`/`RecyclerView` will ask you to populate the UI, and you can implement those calls by resolving the view model to display and pass it to the Valdi view. The number of root components alive in this approach will be equal to the number of visible cells, as the components are re-used during scrolling.

This option is probably natural for many developers, as it is a common pattern on iOS and Android. It is however not necessarily efficient performance wise, as every time the view model is passed to the component, the framework will have to:
- Convert the view model to TypeScript.
- Trigger a render pass.
- Evaluate your render function and apply all the changes.
- Calculate the layout.
- Populate the UI.

### Attach root view of an existing component on scroll

The second option is to create your components ahead of time, before your `UITableView`/`RecyclerView` is even displayed, and attach a root view to your component whenever it becomes visible. When `UITableView`/`RecyclerView` is asking to populate the UI, instead of resolving the view model for your cell, you'd resolve the _component_ and provide it a view on which the UI should be displayed on. Instead of holding an array of data that represents what you want to display, you'd hold an array of Valdi components. You pass the data once to the components, when you are creating them, and those components will hold the data instead of holding it in native code (Objective-C/Swift/Kotlin). This approach allows the framework to cache more things under the hood, like the layout calculation, and will reduce work that needs to happen during scrolling. The trade-off is that the number of root components alive will be equivalent to the number of total cells in the whole scrollable area. This is often not a problem unless each cell has a complex hierarchy and you have multiple thousands of cells to display.

#### Creating a component without UI

You can create a component without UI by creating a `ValdiContext` instance directly, without a root view. To do this, you can use `-[SCValdiRuntime createContextWithViewClass:viewModel:componentContext:]` on iOS or `Runtime.createValdiContext` on Android.

Let's consider this component:
```ts
// @ViewModel
// @ExportModel({ios: 'SCMyCellItemViewModel', android: 'com.snap.valdi.MyCellItemViewModel'})
interface ViewModel {
  label: string;
}

// @ExportModel({ios: 'SCMyCellItemView', android: 'com.snap.valdi.MyCellItemView'})
export class MyCellItem extends Component<ViewModel> {
  // Render code...
}
```

You can create a `ValdiContext` for this component as follow:

iOS:
```objectivec
// Get the runtime instance
id<SCValdiRuntime> runtime = /* ...*/;

// Create view model
SCMyCellItemViewModel *viewModel = [SCMyCellItemViewModel new];
viewModel.label = @"MyLabel";

// Create the component
id<SCValdiContextProtocol> valdiContext = [runtime createContextWithViewClass:[SCMyCellItemView class] viewModel:viewModel componentContext:nil];
// Store the context
```

Android:
```java (works better than kotlin syntax highlighting)
// Get the runtime instance
val runtime: ICValdiRuntime

// Create the view model
val viewModel = MyCellItemViewModel()
viewModel.label = "MyLabel"

// Create the component
runtime.createValdiContext(MyCellItemView.componentPath, viewModel, null, null) { valdiContext ->
  // Store the context
}
```

This will asynchronously trigger the creation of your TypeScript component. There won't be any UI displayed yet, but the framework will still respond to the render calls and will store the render output. All those functions are thread safe and can be called in a background thread. On iOS, the component will be destroyed when the `ValdiContext` is deallocated. On Android, it will be destroyed when `destroy()` is explicitly called on the `ValdiContext` instance.

#### Populating the UI

To populate the UI on your component, you need to provide a root view to the `ValdiContext` instance. You can create an empty `ValdiView` or `SCValdiView` in your `SCMyTableViewCell`, and set it as the root view to the context:

iOS:
```objectivec
- (void)populateUIForCell:(SCMyTableViewCell *)cell atRow:(NSUInteger)row
{
  // Resolve the ValdiContext
  id<SCValdiContextProtocol> valdiContext = _valdiContexts[row];
  // Attach the UI
  valdiContext.rootView = cell.valdiViewHost;
}
```

Android:
```java (works better than kotlin syntax highlighting)
fun populateUI(cell: MyRecyclerViewItem, row: Int) {
  // Resolve the ValdiContext
  val valdiContext = valdiContexts[row]
  // Attach the UI
  valdiContext.rootView = cell.valdiViewHost
}
```

With this approach, we completely dissociate the UI from the Valdi components. We use our components as objects that process their view models and internally resolve the elements. Whenever the UI is actually needed, we attach a view host to display it.

#### Measuring the cell

`UITableView`/`RecyclerView` requires the consumer to provide the size for the cells before it can layout the elements. Once you've created the `ValdiContext` instance, you can measure it by using `-[SCValdiContext measureLayoutWithMaxSize:direction:]` or `ValdiContext.measureLayout()`:

iOS:
```objectivec
- (CGSize)sizeForItemAtRow:(NSUInteger)row
{
  // Resolve the ValdiContext
  id<SCValdiContextProtocol> valdiContext = _valdiContexts[row];
  // Measure the layout
  CGSize maxSize = CGSizeMake(self.view.bounds.size.width, CGFLOAT_MAX)
  return [valdiView measureLayoutWithMaxSize:maxSize direction:SCValdiLayoutDirectionLTR];
}
```

Android:
```java (works better than kotlin syntax highlighting)
- (Double)heightForItew(row: Int) {
  // Resolve the ValdiContext
  val valdiContext = valdiContexts[row]
  // Measure the layout
  val maxWidth = View.MeasureSpec.makeMeasureSpec(view.width, View.MeasureSpec.AT_MOST)
  val maxHeight = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED)
  return valdiContext.measureLayout(maxWidth, maxHeight).height
}
```

Note that since Valdi components render asynchronously, you might have to wait until the render completes before being able to return a size. For this you can use the `waitUntilAllUpdatesCompleted` method:

iOS:
```objectivec
  // Resolve the ValdiContext
  id<SCValdiContextProtocol> valdiContext = _valdiContexts[row];

  [valdiContext waitUntilRenderCompletedWithCompletion:^{
    // Updates have completed
  }];
```

Android:
```java (works better than kotlin syntax highlighting)
  // Resolve the ValdiContext
  val valdiContext = valdiContexts[row]
  valdiContext.waitUntilAllUpdatesCompleted {
    // Updates have completed
  }
```
