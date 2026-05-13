# Releasing to iOS App Store and Google Play Store

## Introduction

This document explains how to build a Valdi application and submit it to the iOS App Store and Google Play Store.

## Prequisites

You should have a `valdi_application()` target that represents your Valdi application in a `BUILD.bazel`. For example:
```py
valdi_application(
    name = "hello_world",
    ios_bundle_id = "com.snap.valdi.helloworld",
    root_component_path = "SampleApps@hello_world/src/HelloWorldApp",
    title = "Valdi Hello World",
    deps = ["//apps/helloworld/src/valdi/hello_world"],
    version = "1.0.5"
)
```

For iOS, you should have a distribution certificate and an App Store provisioning profile associated with the App ID you want to use.

For Android, TODO.

## Submitting to iOS App Store

In your `valdi_application()` target, specify the production provisioning profile using the `ios_provisioning_profile` attribute, and the app icons that your application should use using the `ios_app_icons` attribute. You should also specify if your app targets iPhone, iPad or both using the `ios_families` attribute. Lastly, you should specify the `ios_bundle_id` that your app will use, which should match the bundle id of your provisioning profile.

```py
valdi_application(
    name = "hello_world",
    ios_bundle_id = "com.snap.valdi.helloworld",
    ios_provisioning_profile = "prod.mobileprovision",
    ios_app_icons = glob(["app_icons/ios/**"]),
    ios_families = ["iphone"], # Can also be "ipad"
)
```
In the example above, it expects a `prod.mobileprovision` file to be in the same directory where the `BUILD.bazel` file containing the `valdi_application()` lives. The `ios_app_icons` must references an `xcassets` folder containing a `.appiconset` folder with all the icons in it. For example:
```sh
ls app_icons/ios/Assets.xcassets/AppIcon.appiconset
1024.png	120.png		152.png		167.png		180.png		20.png		29.png		40.png		58.png		60.png		80.png		87.png		Contents.json
```

You can also generate app icons from a single source image at build time:

```py
load(
    "//bzl/valdi:valdi_application.bzl",
    "valdi_application",
    "valdi_application_icons",
)

valdi_application(
    name = "hello_world",
    icons = valdi_application_icons(src = "app_icon.png"),
)
```

If platform-specific source images are needed, use the platform icon macros directly:

```py
load(
    "//bzl/valdi:valdi_application.bzl",
    "valdi_application",
    "valdi_android_application_icons",
    "valdi_ios_application_icons",
    "valdi_macos_application_icons",
)

valdi_application(
    name = "hello_world",
    ios_app_icons = valdi_ios_application_icons(src = "ios_icon.png"),
    android_app_icons = valdi_android_application_icons(
        src = "android_icon.png",
        round_src = "android_round_icon.png",
    ),
    macos_app_icons = valdi_macos_application_icons(src = "macos_icon.png"),
)
```

Build your app in release mode using the `valdi package` command:
```sh
valdi package ios --build_config release --output_path hello_world.ipa
```

Once the build is done, you can upload the .ipa file to the App Store. You can follow the instructions by [Apple here](https://developer.apple.com/help/app-store-connect/manage-builds/upload-builds/), but please note that since we are building the application without Xcode, we will have to use a different tool than Xcode for uploading the ipa. The [Transporter app](https://apps.apple.com/us/app/transporter/id1450874784?mt=12), which is an official application from Apple, works well. You can drag and drop the ipa into Transporter, upload it, and you're done!

## Submitting to Google Play Store

In your `valdi_application()` target, specify the list of Android resource files to include in your application using the `android_resource_files` attribute. These should contain the app icons, and potentially a theme for the main activity so that a splash background is shown when the application starts. You can then specify which app icon or activity theme to use using the `android_app_icon_name`, `android_app_round_icon_name` and `android_activity_theme_name` attributes.

```py
valdi_application(
    name = "hello_world",
    android_app_icon_name = "app_icon",
    android_resource_files = glob(["app_assets/android/**"]),
    android_activity_theme_name = "Theme.MyApp.Launch",
)
```

Here is the contents of `app_assets/android` for this example:
```sh
ls -R app_assets/android
drawable-nodpi	mipmap-hdpi	mipmap-mdpi	mipmap-xhdpi	mipmap-xxhdpi	mipmap-xxxhdpi	values

app_assets/android/drawable-nodpi:
splash.png

app_assets/android/mipmap-hdpi:
app_icon.png

app_assets/android/mipmap-mdpi:
app_icon.png

app_assets/android/mipmap-xhdpi:
app_icon.png

app_assets/android/mipmap-xxhdpi:
app_icon.png

app_assets/android/mipmap-xxxhdpi:
app_icon.png

app_assets/android/values:
colors.xml	themes.xml
```

The `valdi_application()` rule will generate the `AndroidManifest.xml` file depending on the attributes that arae being passed in. If you want complete control over the Android manifest, you can use the `android_app_manifest` attribute and pass a target or a file name that contains the manifest. Your provided manifest should set as its main activity `<package>.StartActivity`, where package is the value passed from the `android_package` attribute (which defaults to `com.snap.valdi` is not passed). If your package is `com.company.product`, then main activity should be `com.company.product.StartActivty`.

Once you have setup your resources, build your app in release mode using the `valdi package` command:
```sh
valdi package android --build_config release --output_path hello_world.apk
```

If the `valdi` CLI asks you to choose between an unsigned and a signed apk, choose the unsigned one.
You can then submit the `apk` to the Google Play Store.
