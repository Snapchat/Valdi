# The `<image>`

## Introduction

Valdi supports displaying local and remote image assets through the `<image>` element in TSX, and animated images using the `<animatedimage>` element.

The Valdi compiler will automatically generate image variants at the right size for both iOS and Android if there is a variant missing. Bundled images should be added to the `res/` folder in the correct Valdi module. The names of the images should follow the iOS or Android naming conventions (`@2x`/`@3x` for iOS or `drawable-hdpi/` `drawable-mdpi/` etc.. for Android) so that the compiler can know what variant the images represent.

You can specify remote assets with a URL.

## Using images

For local assets in the `res/` folder, the module will expose properties to all the assets available in this directory. Resources should autocomplete in VSCode.

> [!Note]
> The Valdi compiler generates an **Asset Catalog** which includes the dimensions (in pixel-independent units) for each image asset. This allows the runtime to "measure" images during the layout pass without having to actually load the image data, improving performance and reducing memory usage.

You specify the asset a url in the `src` attribute of `<image>` in TSX.

```tsx
// Import the asset catalog in the res folder of the valdi_example module
import res from 'valdi_example/res';
import { contentObjectImageLoaderUrl, NativeContentTypeKey } from 'media/src/util/encryption';
// Out lovely component
export class HelloWorld extends Component {
  onRender() {
    /**
     * We are gonna render our images in a container, so let's make sure our images has:
     *  - a source "src", this can be an URL or an asset
     *  - sizes, make sure height and width are defined, otherwise they might be set to 0!
     *  - optionally, we can "tint" our image by an arbitrary color
     */
    <view padding={10} backgroundColor='lightblue' flexDirection='row' justifyContent='center'>
      {/* You can pass the "emoji" asset to the src of our image */}
      <image src={res.emoji} tint='black' height={48} width={48} margin={10} />
      {/* You can Pass an arbitrary url to the src of our image */}
      <image src='https://placedog.net/500' height={48} width={48} margin={10} />
      {/* Urls are sometimes used for loading BOLT assets */}
      <image src='https://placecats.com/500/500' height={48} width={48} margin={10} />
      {/* BOLT Urls can also be obtained from contentObjects using contentObjectImageLoaderUrl */}
      <image
        src={contentObjectImageLoaderUrl({
            contentObject, nativeContentTypeKey:
            NativeContentTypeKey.COMMUNITIES,
        })}
        height={48}
        width={48}
        margin={10}
      />
    </view>;
  }
}

```

![Screenshot of image components rendering images](./assets/core-images/IMG_1453.jpg)

> Make sure to run `valdi projectsync [--target target]` before trying to import an image that was just added

## Adding an image asset

Let's consider we want to add an image `my_banner@3x.png` in a `onboarding` module. We'd add it to the `res` folder as follows:
```
onboarding/res/my_banner@3x.png
```

If we also had an Android specific variant like `drawable-mdpi/my_banner.webp`, we'd also add it in the `res` folder as follow:
```
onboarding/res/my_banner@3x.png
onboarding/res/drawable-mdpi/my_banner.webp
```

For the image to be useable at runtime, we need run the compiler again which will generate any missing variants. In our example, the compiler will generate a `my_banner@2x.png`, `drawable-hdpi/my_banner.web`, `drawable-xhdpi/my_banner.webp`, `drawable-xxhdpi/my_banner.webp` and `drawable-xxxhdpi/my_banner.webp`.

Make sure the `res` folder is referenced in your module's `BUILD` file.

```bazel

valdi_module(
    name = "onboarding",
    ...
    res = glob([
        "res/**/*.jpeg",
        "res/**/*.jpg",
        "res/**/*.png",
        "res/**/*.svg",
        "res/**/*.webp",
    ]),
    ...
)

```

```bash
# Typical valdi development path
cd /Path/To/Your/App/src/valdi/app_name/src

# Need to regenerate the BUILD.bazel files if adding in a /res folder
bazel build //src/onboarding/onboarding:onboarding

# Compile the typescript code into the native repo
valdi install ios
# or
valdi install android
```

This can then be used like before:

```tsx
import res from 'valdi_example/res';
// Translate to rendering: onboarding/res/my_banner@[*].png
<image src={res.myBanner} />
```

## Animated images
The `<animatedimage>` element supports several types of animated images and can display static images as well. Supported formats include [Lottie](https://airbnb.design/lottie) and animated/static WebP, as well as static JPG and PNG.

[factory method]: #todo-factory-method-link
[LottieDemo app]: #todo-lottie-demo-link

Under the hood, `<animatedimage>` uses the AnimatedImage [factory method][] to find the right decoder to use for the given media. Valdi uses a custom Lottie renderer, and then relies on [SkCodec](https://github.com/google/skia/blob/main/include/codec/SkCodec.h) to handle a variety of image formats. This means that other formats supported by SkCodec (such as gif) could be supported by compiling SkCodec with additional codecs.

See the [LottieDemo app][] for examples of how to use `<animatedimage>`.

## Custom image loading

If you need a different data format or additional functionality, you can do that with a [custom image loader](./advanced-images.md).

## Preloading images

The runtime will automatically take care of loading assets represented from the `src` attribute of a `<image>` or `<animatedimage>` element whenever the element is visible within the viewport. You can also trigger the load explicitly from the TypeScript code, which can be used to preload images or retrieve the bytes content of an asset:
```ts

import { Component } from 'valdi_core/src/Component';
import { addAssetLoadObserver, AssetType, AssetOutputType, AssetSubscription, resolveAssetOutputType } from 'valdi_core/src/Asset';

export class ImagePreloader extends Component {
  private loadSubscription?: AssetSubscription;

  onCreate() {
      // Resolve the outputType that the runtime will use for loading images into our current contextId
      // set within the renderer. This will detect whenever the context is using the iOS, Android or SnapDrawing
      // render backend and get us the right AssetOutputType.
      const assetOutputType = resolveAssetOutputType(this.renderer.contextId, AssetType.IMAGE);
      // Eagerly load our image whenever our ImagePreloader is created, without waiting for the runtime to do it for us.
      this.loadSubscription = addAssetLoadObserver('https://mydomain.com/myimage.jpg', (loadedAsset, error) => {
        console.log(`Finished preloading image with error: ${error}`)
      }, assetOutputType);
  }

  onDestroy() {
    this.loadSubscription?.unsubscribe();
    this.loadSubscription = undefined;
  }
}
```

> [!WARNING]
> Please note that the asset load observer must be unsubscribed at some point after it has been added, otherwise the loaded image will stay in the in-memory cache forever. The runtime relies on the presence of observers to detect whether or not an asset is actually used.


## ImageView Callbacks

You can attach `ImageView` specific callbacks (in addition to normal callbacks available on a `View`) to perform logic when an image has been loaded or decoded. There are two available callbacks:

```typescript
type ImageOnAssetLoadCallback = (success: boolean, errorMessage?: string) => void;
type ImageOnImageDecodedCallback = (width: number, height: number) => void;

onAssetLoad?: ImageOnAssetLoadCallback;
onImageDecoded?: ImageOnImageDecodedCallback;
```

A key difference between these two callbacks is that `onAssetLoad` is triggered when the image finishes loading but does not necessarily know its dimensions yet. Conversely, `onImageDecoded` is called after the layout has been calculated, which is reflected in their callback parameters.

> [!NOTE]
> Due to rendering optimizations, views that are too far outside of the parents viewport are not rendered or loaded by default. In such cases, if an image is not preloaded, the `onAssetLoad` and `onImageDecoded` callbacks will not be triggered. This behavior can be bypassed with `limitToViewPort={false}` attribute on a view.
> This may be relevant when an image is in a scroll view or animating images that begins outside of the parent view's viewport.

## Complete API Reference

For a comprehensive list of all properties and methods available on `<image>` and `<animatedimage>` elements, including all callbacks, transforms, and display options, see the [API Reference](../api/api-reference-elements.md#imageview).
