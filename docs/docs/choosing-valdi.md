# Choosing Valdi

Valdi is a cross-platform native UI framework built around TypeScript and Bazel. It is designed for teams that want to write UI code once and ship true native views on iOS and Android — without a JavaScript bridge at runtime.

This page is a decision guide. It covers what Valdi is good at and honest gaps in the current release.

## Framework Comparison

| | **Valdi** | **React Native** | **Flutter** | **Native iOS/Android** |
|---|---|---|---|---|
| **Language** | TypeScript | JavaScript / TypeScript | Dart | Swift / Kotlin |
| **Renders to** | True native views (no WebView, no JS bridge) | True native views (new arch: JSI, no bridge) | Native views via Skia/Impeller | Platform native |
| **UI model** | Class-based components, side-effect JSX | Function/class components, virtual DOM | Widget tree | UIKit / Jetpack Compose |
| **Hot reload** | `valdi hotreload` — sub-second on-device | Metro bundler — fast | Flutter hot reload — fast | Previews / simulators |
| **Build system** | Bazel (`BUILD.bazel`) | npm / Metro | `pubspec.yaml` + `flutter` CLI | Xcode / Gradle |
| **Typing** | Strong (TypeScript, no `any` in generated APIs) | Optional (TypeScript overlay) | Strong (Dart) | Strong |
| **Ecosystem** | Early-stage; Bazel-first | Large (npm) | Growing (pub.dev) | Platform SDKs |
| **Dev platform** | macOS Apple Silicon (iOS + Android); Linux (Android only) | macOS, Windows, Linux | macOS, Windows, Linux | macOS (iOS); macOS/Windows/Linux (Android) |
| **Target platforms** | iOS, Android, macOS desktop, Web (alpha) | iOS, Android, Web | iOS, Android, Web, Desktop | iOS or Android only |
| **Open source** | Yes (MIT) | Yes (MIT) | Yes (BSD) | Yes |
| **Bazel integration** | First-class | Third-party rules only | Not supported | Via `rules_apple` / `rules_android` |

## When Valdi is a good fit

**You need a strong TypeScript contract between UI and native.** Valdi generates type-safe ObjC/Kotlin bindings directly from TypeScript interface definitions. Every native binding is defined in TypeScript; there is no `NativeModules.MyMethod()` string dispatch.

**You want true native rendering without a JS bridge at runtime.** Valdi compiles TypeScript to bytecode and renders directly to native views — there is no WebView and no JS-to-native bridge like React Native's JSI.


**You are building features for iOS and Android simultaneously.** Valdi modules compile to a single `.valdimodule` archive that runs on both platforms. Platform-specific behavior is isolated to native bindings.

## When another framework is a better fit

**You need Windows or Intel Mac development machines.** The Valdi compiler binary currently ships only for Apple Silicon macOS. Intel Mac and Windows development are not supported yet. Linux is supported for Android-only work. If your engineers are on Windows or Intel Macs and need iOS support, choose React Native or Flutter.

## Current gaps (as of beta-0.0.x)

These are known limitations the Valdi team is actively working on or tracking:

| Gap | Status | Workaround |
|-----|--------|-----------|
| Windows dev support | Not supported | macOS or Linux required |
| WebSocket API | Not available | Use native bindings via Valdi's TypeScript bridge |
| CMake / non-Bazel build | Not available | Bazel required |
| Bazel Central Registry | Not registered | Use `http_archive` with the GitHub release |
| Linux native target | Not available | — |

## Quick decision guide

```
Need iOS support on non-Apple-Silicon Mac? ── Yes ─────────▶ React Native or Flutter
        │
       No
        │
Are you willing to use Bazel? ───────────── No ─────────────▶ React Native or Flutter
        │
       Yes
        │
        ▼
   Valdi is likely a good fit.
   Start with: valdi bootstrap
```

## Further reading

- [Getting Started](./start-install.md) — install and run your first module
- [Valdi for React Developers](./start-from-react.md) — side-by-side React ↔ Valdi reference
- [Migrating from Flutter](./migrate-from-flutter.md) — Flutter concept mapping
- [Migrating from Jetpack Compose](./migrate-from-compose.md) — Compose concept mapping
- [Internals: Renderer](./internals-renderer.md) — how Valdi renders to native views
- [Troubleshooting](./help-troubleshooting.md) — setup and common issues
