# Custom image loaders

Valdi supports loading and displaying `https` and `res` images by default with the `<image>` tag. If you need additional formats or custom image loading behavior, you can build a native custom image loader.

You will need a custom loader for each platform where you intend to load images.

[iOS SCValdiImageLoader]: ../../valdi/src/valdi/ios/SCValdiDefaultImageLoader.m
[Android ValdiImageLoader]: ../../valdi/src/java/com/snap/valdi/imageloading/DefaultValdiImageLoader.kt

Before you create your custom loader, take a look at the existing custom loaders to determine if your functionality has already been implemented: See [iOS][iOS SCValdiImageLoader] and [Android][Android ValdiImageLoader].

## How custom loaders work

The `src` attribute specifies the target of an `<image>`. This is often thought of as a URL but it is just a string. Anything that can be encoded as a string can be used as a `src`: a URL, a b64 encoded blob of something, a db key.

Custom loaders specify `supportedURLSchemes` these are not strictly URL schemes but again just strings. If the prefix of the specified `src` matches a custom loader's `supportedURLSchemes` then Valdi will use that loader to load that image.

`supportedURLSchemes => returns ['my-image-loader']`

Matches: `my-image-loader://url.shaped.thing` `my-image-loader://bG9sd3V0`

`supportedURLSchemes` should be unique to your loader; if there are conflicting strings, the first one registered will be used.

Your custom loader should live in a folder that you own, not Valdi's codebase.

## iOS

On iOS, the loader must conform to the [`SCValdiImageLoader`](../../valdi_core/src/valdi_core/ios/valdi_core/SCValdiImageLoader.h) protocol and implement the following methods (including at least one of the optional methods):

```objc

/**
 Returns the URL schemes that this VideoLoader knows how to load.
 */
- (NSArray<NSString*>*)supportedURLSchemes;

/**
 Returns the request payload object which will be provided to the loadVideoWithRequestPayload:
 method for the given imageURL. The payload can be any Objective-C object. The payload might
 be cached internally by the framework.
 */
- (id)requestPayloadWithURL:(NSURL*)imageURL error:(NSError**)error;

@optional

/**
 Load the image with the given request payload and call the completion when done.
 This method should only be implemented if this SCValdiImageLoader instance supports loading
 URLs into SCValdiImage instances.
 */
- (id<SCValdiCancelable>)loadImageWithRequestPayload:(id)requestPayload
                                             parameters:(SCValdiAssetRequestParameters)parameters
                                             completion:(SCValdiImageLoaderCompletion)completion;

/**
 Load the bytes with the given request payload and call the completion when done.
 This method should only be implemented if this SCValdiImageLoader instance supports loading
 URLs into raw bytes.
 */
- (id<SCValdiCancelable>)loadBytesWithRequestPayload:(id)requestPayload
                                             completion:(SCValdiImageLoaderBytesCompletion)completion;

```

> [!WARNING]
> The returned implementation for `SCValdiCancelable` must be thread-safe as `cancel()` can be called on any thread.

## Android

The loader must conform to [ValdiImageLoader](../../valdi/src/java/com/snap/valdi/utils/ValdiImageLoader.kt) and implement the following methods:

```kotlin

    /**
    Returns the URL schemes supported by this ImageLoader
     */
    fun getSupportedURLSchemes(): List<String>

    /**
     * Returns a RequestPayload that will be provided to the load function of the implementation.
     * The request payload might be cached by the runtime to avoid processing
     * it again. This method can throw a ValdiException if Uri cannot be parsed.
     */
    @Throws(ValdiException::class)
    fun getRequestPayload(url: Uri): Any

    /**
     * Returns the output types that this ValdiAssetLoader supports.
     */
    fun getSupportedOutputTypes(): Int

    /**
    Load the bitmap for the given URL and call the completion when done.
     */
    fun loadImage(requestPayload: Any,
                  options: ValdiImageLoadOptions,
                  completion: ValdiImageLoadCompletion): Disposable?

```

The `getSupportedOutputTypes` method should return a list of [ValdiAssetLoadOutputTypes](../../valdi/src/java/com/snap/valdi/utils/ValdiAssetLoadOptions.kt#L3).

## Byte-exact (raw) bitmap data

Images added to a module's `res/` folder go through the image-asset pipeline: the
compiler generates per-density variants and, on Android, re-encodes them into lossy,
density-scaled WebP drawables (see [The `<image>`](./core-images.md)). That is the
right behavior for decorative UI, but it is *lossy* — it will smear hard pixel edges
into intermediate grays and may resize the source. If you need the **original,
untouched bytes** of an image — for example a small bitmap used as literal per-pixel
data — do **not** put it in `res/`.

Instead, bundle the image as a **`.bin` file in the module's `srcs`** and read its
bytes back verbatim at runtime. `.bin` files are bundled as retrievable *module
entries* — the same mechanism the runtime uses for bundled data — with zero
re-encoding, whereas `res/` files are re-encoded and are not retrievable this way.

### 1. Bundle the image as a `.bin` file (not a `res/` image)

Give the file a `.bin` extension so it opts out of the image pipeline, and keep the real
format visible by naming it `<name>.<realext>.bin` (e.g. `checker_2x2.png.bin`). Place it
under `src/`, add `src/**/*.bin` to the module's `srcs` glob, and enable `inline_assets`
so the bytes are available at runtime:

```bazel
valdi_module(
    name = "my_module",
    srcs = glob([
        "src/**/*.ts",
        "src/**/*.tsx",
        "src/**/*.bin",  # byte-exact data files, bundled verbatim as retrievable entries
    ]),
    inline_assets = True,
    deps = ["//src/valdi_modules/src/valdi/drawing", ...],
    ...
)
```

### 2. Retrieve the bytes byte-exact

`getModuleFileEntryAsBytes` returns the file's exact bytes, on every platform. The path
is relative to the module root and matches the file's location in `srcs`:

```ts
import { getModuleFileEntryAsBytes } from 'valdi_core/src/Valdi';

const bytes = getModuleFileEntryAsBytes('my_module', 'src/patterns/checker_2x2.png.bin');
```

### 3. Turn the bytes into a bitmap — with no re-encoding

Use [`drawing/src/BitmapFactory`](../../src/valdi_modules/src/valdi/drawing/src/BitmapFactory.ts):

- **`decodeBitmap(bytes)`** decodes an encoded image (PNG/WebP) and accepts a
  `Uint8Array` directly. A PNG source decodes losslessly, so the pixels are exactly what
  you bundled. Best when your `.bin` holds an encoded image.
- **`createBitmapWithBuffer(info, buffer)`** wraps a *raw* pixel buffer directly — no
  codec at all. Use this when your `.bin` holds raw pixels (e.g. RGBA8888); you supply
  the `width`/`height`/`rowBytes`.

```ts
import { decodeBitmap } from 'drawing/src/BitmapFactory';

const bitmap = decodeBitmap(bytes);
try {
  // Read pixels back to confirm they are exactly what was bundled.
  const firstPixel = bitmap.accessPixels((view) => [
    view.getUint8(0),
    view.getUint8(1),
    view.getUint8(2),
    view.getUint8(3),
  ]);
} finally {
  // Dispose to release the native bitmap; skipping this leaks it.
  bitmap.dispose();
}
```

A complete, runnable version of this pattern lives in the Hello World example app
(`apps/helloworld/.../hello_world/src/ByteExactImageExample.tsx`), which bundles a
lossless 2×2 black/white PNG as `.bin` and renders the reconstructed pixels on screen.
