# The `<custom-view>`

## Introduction

Valdi supports injecting native views inside of an existing valdi feature through the `<custom-view>` element in TSX. This works on all four platforms: iOS (`UIView`), Android (`View`), macOS (`NSView`), and web (DOM elements).

This type of integration is useful when a very complex view (such as a system view) is already implemented natively and we want to re-use platform-specific code instead of writing a new cross-platform component.

## Using a `<custom-view>`

Here we find a simple example on how to inject custom views inside of a Valdi rendered feature.

### TypeScript

```tsx
export interface MyContext {
  myCustomViewFactory: ViewFactory; // This will tell valdi which native view to use
}
export class MyComponent extends Component<{}, MyContext> {
  onRender() {
    <view backgroundColor='lightblue'>
      <custom-view viewFactory={this.context.myCustomViewFactory} myAttribute={42}/>
    </view>
  }
}
```

### Android

```kotlin
// Create a ViewFactory, that will instantiate a MyCustomView under the hood.
val myCustomViewFactory = runtime.createViewFactory(MyCustomView::class.java, {
  context -> MyCustomView(context, this.myViewDependencies)
}, MyCustomViewAttributesBinder(context))

// Pass the viewFactory to the context
val context = MyContext()
context.myCustomViewFactory = myCustomViewFactory

// Create the view with the context
val view = MyComponent.create(runtime, null, context)
```

### iOS

```objectivec
// Create a ViewFactory, that will instantiate a MyCustomView under the hood.
id<SCValdiViewFactory> myCustomViewFactory =
  [runtime makeViewFactoryWithBlock:^UIView *{
    return [[MyCustomView alloc] initWithMyDependencies:self.myViewDependencies];
  } attributesBinder:nil forClass:[MyCustomView class]];

// Pass the viewFactory to the context
MyContext *context = [MyContext new];
context.myCustomViewFactory = myCustomViewFactory;

// Create the view with the context
MyComponent *view = [[MyComponent alloc] initWithRuntime:runtime
                                                  viewModel:nil
                                           componentContext:context];
```

## How to define `MyCustomView`

In the above snippet we injected the view MyCustomView, but we'd need to make sure that View is defined properly to be passed to Valdi.

Declaring a view class is usually not enough. Your custom view class will probably need to be configurable in order to be useful. Configuration of elements is done through attributes. For example a `Label` will expose attributes like `value` to set the text, `font` to change the font etc... If you were creating a `SliderView` element, you may want to expose an attribute like `progress` which could be used to change the slide position.

The process in which iOS/Android will declare the available attributes on a View class is called the _Attributes Binding_. At runtime, whenever Valdi has to inflate a view of a given class for the very first time, it will give an opportunity to iOS/Android code to register the attributes it knows how to handle. This process is lazy and happens only once per view class, regardless of which component is responsible for the inflation of that class.

In some cases, you might want to make a view automatically size itself based on its attributes. If we take the `Label` example again, we want the view representing the `Label` to have a size that changes based on its font, text etc...

To do this, you need to declare a measurer placeholder view instance or explicitly return a size via `setPlaceholderViewMeasureDelegate` or `setViewMeasureDelegate` respectively (see examples below) inside the `bindAttributes` function. Whenever a view of that class needs to be measured, the respective methods defined on the view will be used, preferring `setViewMeasureDelegate` if both are defined, the attributes will be applied to it, and `sizeThatFits:` will be called on iOS or `onMeasure()` on Android.

> [!NOTE]
> The view only needs to be measured if the dimensions of the view cannot be inferred from the view's layout attributes on the TypeScript side. For example the following view does not need to be measured as its width is defined relative to its parent (which would already be measured and known) and the height is set statically to 100.
> ```tsx
>   <view width={'100%'} height={100} />
> ```
>
> In this case, the native measure methods defined above would _not_ be called.

### Android

To register attributes on Android, you need to make a class implementing `com.snap.valdi.attributes.AttributesBinder`. You then register an instance of that class to the `ViewFactory` being created.

Make sure your view also properly implements the `onMeasure()` method.

```kotlin
import com.snap.valdi.attributes.RegisterAttributesBinder
import com.snap.valdi.attributes.AttributesBinder


class MyCustomView(context: Context) : FrameLayout(context) {}

// Make sure to add the @RegisterAttributesBinder annotation so that
// the Valdi runtime can find it.
@RegisterAttributesBinder
class MyCustomViewAttributesBinder(context: Context): AttributesBinder<MyCustomView> {

  override val viewClass: Class<MyCustomView>
    get() = MyCustomView::class.java

    private fun applyMyAttribute(
      view: MyCustomView,
      attributeValue: Double,
      animator: ValdiAnimator?
    ) {
      view.myAttribute = attributeValue
    }

    private fun resetMyAttribute(
      view: MyCustomView,
      animator: ValdiAnimator?
    ) {
      view.myAttribute = 0
    }

    override fun bindAttributes(
      attributesBindingContext: AttributesBindingContext<MyCustomView>
    ) {
      // Define an attribute
      attributesBindingContext.bindDoubleAttribute(
        "myAttribute",
        false, // this attribute will not invalidate the layout of the view
        this::applyMyAttribute, // called when a new attribute value is set on the view
        this::resetMyAttribute // called when the attribute becomes undefined
      )

      // This is optional and will make the view measureable
      attributesBindingContext.setPlaceholderViewMeasureDelegate(lazy {
          ValdiDatePicker(context).apply {
              layoutParams = ViewGroup.LayoutParams(
                      ViewGroup.LayoutParams.WRAP_CONTENT,
                      ViewGroup.LayoutParams.WRAP_CONTENT
              )
          }
      })

      // This is optional and will make the view measureable
      attributesBindingContext.setMeasureDelegate(object : MeasureDelegate {
          override fun onMeasure(
              attributes: ViewLayoutAttributes,
              widthMeasureSpec: Int,
              heightMeasureSpec: Int,
              isRightToLeft: Boolean
          ): MeasuredSize {
              return MeasuredSize(1000, 1000)
          }
      })
    }
}
```

The semantics are the same as for iOS. We declared to Valdi that we know how to handle the attribute `myAttribute`, we are expecting it to be a double, and we provided callbacks to apply and reset the attribute on the view.

The second parameter of `bindDoubleAttribute` is used to tell Valdi whether the intrinsic size of your view might change if the attribute changes. This is useful for attributes like `font` on a `Label`, because changing the `font` means the `Label` could end up being bigger. Whenever an attribute changes with `invalidateLayoutOnChange` set to true, a layout calculation will always be triggered at some point.

### iOS

To register attributes on iOS, you can override this class method `+(void)bindAttributes:(SCValdiAttributesBindingContext *)` in your custom view class. The given attributes binding context provides convenience methods to register the attributes and get a callback when the attribute needs to be applied or removed from a view instance.

Make sure your view also properly implements the `sizeThatFits:` method.

```objectivec
@implementation MyCustomView {}

- (void)valdi_applyMyAttribute:(double)attributeValue
{
  self.myAttribute = attributeValue;
}

+ (void)bindAttributes:(SCValdiAttributesBindingContext *)bindingContext
{
  // Define an attribute
  [bindingContext bindAttribute:@"myAttribute"
       invalidateLayoutOnChange:NO
                withDoubleBlock:^(MyCustomView *myCustomView,
                                  double attributeValue,
                                  SCValdiAnimator *animator) {
    // called when a new attribute value is set on the view
    [myCustomView valdi_applyMyAttribute:attributeValue];
  }
                  resetBlock:^(MyCustomView *myCustomView,
                               SCValdiAnimator *animator) {
    // called when the attribute becomes undefined
    [myCustomView valdi_applyMyAttribute:0];
  }];

  // This is optional and will make the view measureable
  [attributesBinder setPlaceholderViewMeasureDelegate:^UIView *{
      return [MyCustomView new];
  }];

  // This is optional and will make the view measureable (will be prioritized if setPlaceholderViewMeasureDelegate is also defined)
  [attributesBinder setMeasureDelegate:^CGSize(id<SCValdiViewLayoutAttributes> attributes, CGSize maxSize,
                                                           UITraitCollection *traitCollection) {
      CGSize newSize = CGSizeMake(1000, 1000);
      return newSize;
  }];

}

@end
```

In this example we declared to Valdi that we know how to handle the attribute `myAttribute`, we are expecting it to be a double, and we provided callbacks to apply and reset the attribute on the view.

The `animator` parameter is passed whenever the attribute change is expected to be animated. `SCValdiAnimator` provides methods to animate a change using `CoreAnimation`. If your attribute is not animatable, it is fine to ignore it.

The `invalidateLayoutOnChange` parameter is used to tell Valdi whether the intrinsic size of your view might change if the attribute changes. This is useful for attributes like `font` on a `Label`, because changing the `font` means the `Label` could end up being bigger. Whenever an attribute changes with `invalidateLayoutOnChange` set to true, a layout calculation will always be triggered at some point.

#### How to bind `Observable` to iOS view:

<!-- TODO: Link to widget source code -->

[observable]: #replace-with-public-url

Let's say your attribute is called `observable`.
- At the TSX level, pass the `observable` attribute like `observable={convertObservableToBridgeObservable(yourObservable)}` ([see the method][observable])
- At the iOS level, use the `bindAttribute withUntypedBlock:` for name `observable`:
  ```objectivec
  SCObservable<NSString *>observable;
  SCValdiMarshallerScoped(marshaller, {
    NSInteger objectIndex = SCValdiMarshallerPushUntyped  (marshaller, untypedInstance);
    SCValdiBridgeObservable *bridgeObservable =   [SCValdiMarshallableObjectRegistryGetSharedInstance()   unmarshallObjectOfClass:[SCValdiBridgeObservable class]   fromMarshaller:marshaller atIndex:objectIndex];
    observable = [bridgeObservable toSCObservable];
  });
  ```

## Native View or Component?

In some cases, you might be able to implement a view class as a Valdi component itself. For example a `SliderView`.

It is very likely than writing the slider as a component would have been easier and allowed the same level of functionality. Here are some reasons which you might want to use a native view class instead:

- The functionality you want to provide is difficult to do in Valdi.

  For example if you wanted to actually draw an image buffer on the screen or draw the glyphs from a text, it will have to use the platform specific APIs on iOS/Android. As such an `Image` or `Label` native view class will do the job better than a Valdi component.
- There already exist a view class which provides the level of functionality you need.
- The view class has performance heavy logic that would be slow to evaluate in a JavaScript engine.

## Using a class mapping

> [!NOTE]
> In most cases it is easier and safer to use a `ViewFactory` instead

Alternatively in TSX, you can use an arbitrary view class in your render template by using the `<custom-view>` element and pass it the platform-specific class names.

In order for the view class to be instantiable by Valdi, it needs to have a working `initWithFrame:` initializer in iOS/macOS, and `init(context: Context)` in Android. If the view class does not have those constructors, you can create your own view class that wraps the native view class you want to use. If the view class ultimately needs additional native dependencies to be constructed, the Valdi way to provide them is to use attributes.

If a view class is designed to be re-usable across multiple features, consider abstracting out the native view class inside a component.

### Platform class attributes

The `<custom-view>` element supports four platform-specific class attributes:

| Attribute | Platform | Example |
|-----------|----------|---------|
| `iosClass` | iOS | `'SliderView'` |
| `androidClass` | Android | `'com.snap.valdi.SliderView'` |
| `macosClass` | macOS | `'SliderView'` |
| `webClass` | Web | `'slider-view'` |

**Platform fallthrough rules:**
- **macOS** falls through to `iosClass` when `macosClass` is not specified. This means if your iOS and macOS views share the same class, you only need `iosClass`.
- **Web** uses `webClass` to look up a registered factory in the `WebViewClassRegistry`.
- You only need to specify the platforms your app targets.

### TypeScript

```tsx
export interface SliderViewModel {
  progress: number;
}
export class Slider extends Component<SliderViewModel> {
  onRender() {
    <custom-view
      iosClass='SliderView'
      androidClass='com.snap.valdi.SliderView'
      macosClass='MacOSSliderView'
      webClass='slider-view'
      progress={this.viewModel.progress}
    />
  }
}
```

If your macOS view uses the same class as iOS, omit `macosClass`:

```tsx
// macOS will automatically use 'SliderView' (the iosClass)
<custom-view
  iosClass='SliderView'
  androidClass='com.snap.valdi.SliderView'
  progress={this.viewModel.progress}
/>
```

### Android

```java (works better than kotlin syntax highlighting)
// We can then create the view
val view = MyFeature.create(runtime, null, null)
```

> [!Warning]
> The Valdi runtime will use the JVM's reflection APIs to resolve the view class name. The given Android class name needs to be stable and not change on release builds for this mechanism to work. On Android Snapchat's release builds, the R8 optimizer is used to mangle and rename classes, which can break custom views referred by their class names in Valdi. You can use the `@Keep` annotation in Kotlin or a configured `proguard-rules.pro` file to enforce that the view class is not renamed and removed from the apk.

### iOS

```objectivec
// When we create the slider-view, we'd need to define the attribute binding
@implementation SliderView {}
+ (void)bindAttributes:(SCValdiAttributesBindingContext *)bindingContext
{
  // Define my slider's attributes
}
@end

// We can then create the view
MyFeature *view = [[MyFeature alloc] initWithRuntime:runtime
                                              viewModel:nil
                                       componentContext:nil];
```

### macOS

macOS custom views work the same as iOS. If your macOS and iOS views share a class name (which is common since both use Objective-C), the `iosClass` fallthrough handles it automatically. Define a separate `macosClass` only when the macOS view is different:

```objectivec
// macOS-specific view (e.g., using NSPopUpButton instead of UIPickerView)
@implementation MacOSSliderView {}
+ (void)bindAttributes:(SCValdiAttributesBindingContext *)bindingContext
{
  // Define macOS-specific attributes
}
@end
```

### Web

Web custom views use a factory registration pattern. Register a factory function that creates a DOM element and optionally returns an attribute handler:

```typescript
// In your web polyglot module (e.g., web/src/MyWebViews.ts)

interface AttributeHandler {
  changeAttribute(name: string, value: unknown): void;
}

type ViewFactory = (container: HTMLElement) => AttributeHandler;

function createSliderFactory(): ViewFactory {
  return (container: HTMLElement): AttributeHandler => {
    const slider = document.createElement('input');
    slider.type = 'range';
    container.appendChild(slider);

    return {
      changeAttribute(name: string, value: unknown): void {
        if (name === 'progress' && typeof value === 'number') {
          slider.value = String(value * 100);
        }
      },
    };
  };
}

// Class names must match the webClass attributes in the corresponding TSX components.
export const webPolyglotViews: Record<string, ViewFactory> = {
  'slider-view': createSliderFactory(),
};
```

The factory function receives a container DOM element and returns an object with a `changeAttribute(name, value)` method to receive attribute updates from the Valdi renderer.

To register web factories, create a `ts_project` (never a `filegroup`) and add it as `web_deps` in your `BUILD.bazel`. The `ts_project` requires `transpiler = "tsc"` and a dedicated `web/tsconfig.json`:

```python
load("@aspect_rules_ts//ts:defs.bzl", "ts_project")

ts_project(
    name = "my_web_views",
    srcs = glob([
        "web/**/*.ts",
        "src/**/*.d.ts",       # include if web code imports module type declarations
    ], exclude = [
        "web/**/*.d.ts",       # avoid TS5055 output collision with composite
    ]),
    allow_js = True,
    composite = True,
    transpiler = "tsc",
    tsconfig = "web/tsconfig.json",
)

valdi_module(
    name = "my_module",
    # ...
    web_deps = [":my_web_views"],
)
```

The `web/tsconfig.json` should be standalone (not extending the module-level tsconfig):

```json
{
  "compilerOptions": {
    "target": "ES2016",
    "module": "commonjs",
    "strict": true,
    "composite": true,
    "allowJs": true,
    "lib": ["dom", "ES2019"]
  }
}
```
