#pragma once

/**
 * This header adds a set of macros to detect the platform we're targeting for the build.
 * SC_ANDROID for Android
 * SC_IOS for iOS
 * SC_IPHONE_SIMULATOR for iPhone simulator
 * SC_DESKTOP for macos and desktop/server linux
 * SC_DESKTOP_LINUX for desktop/server linux
 * SC_DESKTOP_MACOS for desktop macos
 * SC_WASM for wasm
 * SC_SNAPOS_NDK for the SnapOS NDK (Spectacles lens); these builds also target linux, so
 *   SC_DESKTOP_LINUX is set too and they classify as desktop-linux
 */

#if defined(__APPLE__)

#include "TargetConditionals.h"
#if TARGET_IPHONE_SIMULATOR
#define SC_IPHONE_SIMULATOR 1
#define SC_IOS 1
#elif TARGET_OS_IPHONE
#define SC_IOS 1
#else
#define SC_DESKTOP 1
#define SC_DESKTOP_MACOS 1
#endif

#elif defined(ANDROID_WITH_JNI)
#define SC_ANDROID 1
#elif defined(EMSCRIPTEN)
#define SC_WASM 1
#elif defined(__linux__)
#define SC_DESKTOP 1
#define SC_DESKTOP_LINUX 1
#endif // defined(__APPLE__)

// We need this guard because this file is used in pure Objective-C which doesn't support constexpr.
#ifdef __cplusplus

namespace snap {

// Supported operating systems
enum class Platform { Android, Ios, MacOs, DesktopLinux, Wasm };

#if defined(SC_ANDROID)
constexpr Platform kPlatform = Platform::Android;
#elif defined(SC_IOS)
constexpr Platform kPlatform = Platform::Ios;
#elif defined(SC_DESKTOP_MACOS)
constexpr Platform kPlatform = Platform::MacOs;
#elif defined(SC_WASM)
constexpr Platform kPlatform = Platform::Wasm;
#else
constexpr Platform kPlatform = Platform::DesktopLinux;
#endif

///
/// Set of helper functions for use in constexpr functions and `if` blocks.
/// Provides cleaner alternative to #ifdef macros in some cases.
///
/// For example:
/// if constexpr (snap::isIos()) {
///     callIosSpecificFunction();
/// else {
///     callGenericFunction();
/// }
///

constexpr bool isAndroid() {
    return kPlatform == Platform::Android;
}

constexpr bool isIos() {
    return kPlatform == Platform::Ios;
}

constexpr bool isMacOs() {
    return kPlatform == Platform::MacOs;
}

constexpr bool isDesktopLinux() {
    return kPlatform == Platform::DesktopLinux;
}

constexpr bool isApple() {
    return isIos() || isMacOs();
}

constexpr bool isWasm() {
    return kPlatform == Platform::Wasm;
}

constexpr bool isDesktop() {
    return isMacOs() || isDesktopLinux();
}

// True for the on-device Spectacles lens native library, built against the SnapOS NDK.
// This is a narrower classification within isDesktop(): the SnapOS NDK targets linux, so
// these builds also satisfy isDesktopLinux()/isDesktop(). Driven by the SC_SNAPOS_NDK build
// define; kPlatform stays DesktopLinux so nothing that keys off isDesktopLinux() changes.
constexpr bool isSnapOsNdk() {
#if defined(SC_SNAPOS_NDK)
    return true;
#else
    return false;
#endif
}

// Based off of the C standard library definition of NDEBUG (not debug).
// TODO: We should not use this flag and should instead drive off of build flavor!
constexpr bool isSystemDebug() {
#if defined(NDEBUG)
    return false;
#else
    return true;
#endif
}

} // namespace snap

#endif
