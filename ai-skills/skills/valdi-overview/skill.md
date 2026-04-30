# Valdi Open Source - Cursor Rules

## ⚠️ Open Source Project

This is an open source project. Never commit secrets, API keys, or proprietary information.

## 🚨 CRITICAL: This is NOT React!

Valdi uses TSX/JSX syntax but is **fundamentally different from React**. 

**Common AI mistakes:**
- ❌ Suggesting `useState`, `useEffect`, `useContext` (don't exist!)
- ❌ Functional components (don't exist!)
- ❌ `this.props` (should be `this.viewModel`)
- ❌ `markNeedsRender()`, `onMount()`, `onUpdate()` (wrong names/don't exist!)

**Correct Valdi:**
- ✅ `class MyComponent extends StatefulComponent`
- ✅ `state = {}` + `this.setState()`
- ✅ `this.viewModel` for props
- ✅ `onCreate()`, `onViewModelUpdate()`, `onDestroy()` lifecycle

## 📦 AI Skills

Install Valdi skills for your AI tool to get context-specific guidance:

```bash
npm install -g @snap/valdi
valdi skills install
```

Skills available (`valdi skills list`):

| Skill | Coverage |
|-------|----------|
| `valdi-tsx` | TSX component patterns, lifecycle, styling |
| `valdi-setup` | Module BUILD.bazel, tsconfig, hot reload |
| `valdi-async` | CancelablePromise, HTTPClient, lifecycle safety |
| `valdi-perf` | ViewModel stability, createReusableCallback, Style interning |
| `valdi-component-tests` | elementKeyFind, tapNodeWithKey, discriminated unions |
| `valdi-ios` | Swift/ObjC platform bridging |
| `valdi-android` | Kotlin platform bridging |
| `valdi-bazel` | Build rules, platform builds |
| `valdi-compiler` | Compiler pipeline internals |
| `valdi-cpp-runtime` | C++ runtime and renderer |
| `valdi-polyglot-module` | Cross-platform polyglot APIs, web polyglot entry pattern |
| `valdi-custom-view` | Native view integration, viewFactory |

## Quick Commands

```bash
bazel build //...          # Build everything
bazel test //...           # Run all tests
valdi install ios          # Build & install iOS app
valdi hotreload            # Start hot reload
```

## More Information

- **Comprehensive guide**: `/AGENTS.md`
- **AI tooling**: `/docs/docs/ai-tooling.md`
- **Support**: `/SUPPORT.md`
- **Discord**: https://discord.gg/uJyNEeYX2U
