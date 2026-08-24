#include "valdi/jsbridge/JavaScriptBridge.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptBridge.hpp"

#if VALDI_HAS_QUICKJS
#include "valdi/quickjs/QuickJSJavaScriptContextFactory.hpp"
#endif

#if VALDI_HAS_JSCORE
#include "valdi/jscore/JavaScriptCoreContextFactory.hpp"
#endif

#if VALDI_HAS_V8
#include "valdi/v8/V8JavaScriptContextFactory.hpp"
#endif

#if VALDI_HAS_HERMES
#include "valdi/hermes/HermesJavaScriptContextFactory.hpp"
#endif

using namespace snap::valdi_core;
namespace Valdi {

static Valdi::IJavaScriptBridge* getAuto();

static Valdi::IJavaScriptBridge* getQuickJS() {
#if VALDI_HAS_QUICKJS
    static auto* kBridge = new ValdiQuickJS::QuickJSJavaScriptContextFactory();
    return kBridge;
#else
    SC_ABORT("Requested QuickJS VM, which is not available on this build type");
    return nullptr;
#endif
}

static Valdi::IJavaScriptBridge* getJSCore() {
#if VALDI_HAS_JSCORE
    static auto* kBridge = new ValdiJSCore::JavaScriptCoreContextFactory();
    return kBridge;
#else
    SC_ABORT("Requested JSCore VM, which is not available on this build type");
    return nullptr;
#endif
}

static Valdi::IJavaScriptBridge* getV8() {
#if VALDI_HAS_V8
    static auto* kBridge = new Valdi::V8::V8JavaScriptContextFactory();
    return kBridge;
#else
    SC_ABORT("Requested V8 VM, which is not available on this build type");
    return nullptr;
#endif
}

static Valdi::IJavaScriptBridge* getHermes() {
#if VALDI_HAS_HERMES
    static auto* kBridge = new Valdi::Hermes::HermesJavaScriptContextFactory();
    return kBridge;
#else
    SC_ABORT("Requested Hermes VM, which is not available on this build type");
    return nullptr;
#endif
}

static Valdi::IJavaScriptBridge* getAuto() {
#if VALDI_JS_ENGINE_DEFAULT_HERMES && VALDI_HAS_HERMES
    return getHermes();
#elif VALDI_JS_ENGINE_DEFAULT_QUICKJS && VALDI_HAS_QUICKJS
    return getQuickJS();
#elif VALDI_JS_ENGINE_DEFAULT_JSCORE && VALDI_HAS_JSCORE
    return getJSCore();
#elif VALDI_HAS_JSCORE
    return getJSCore();
#elif VALDI_HAS_QUICKJS
    return getQuickJS();
#elif VALDI_HAS_HERMES
    return getHermes();
#else
#error "No vm is available for the build."
#endif
}

bool JavaScriptBridge::isAvailable(JavaScriptEngineType type) {
    switch (type) {
        case JavaScriptEngineType::Auto:
            return true;
        case JavaScriptEngineType::JSCore:
#if VALDI_HAS_JSCORE
            return true;
#else
            return false;
#endif
        case JavaScriptEngineType::QuickJS:
#if VALDI_HAS_QUICKJS
            return true;
#else
            return false;
#endif
        case JavaScriptEngineType::V8:
#if VALDI_HAS_V8
            return true;
#else
            return false;
#endif
        case JavaScriptEngineType::Hermes:
#if VALDI_HAS_HERMES
            return true;
#else
            return false;
#endif
        default:
            return false;
    }
}

Valdi::IJavaScriptBridge* JavaScriptBridge::get(JavaScriptEngineType type) {
    switch (type) {
        case JavaScriptEngineType::Auto: {
            return getAuto();
        }
        case JavaScriptEngineType::JSCore: {
            return getJSCore();
        }
        case JavaScriptEngineType::QuickJS: {
            return getQuickJS();
        }
        case JavaScriptEngineType::V8: {
            return getV8();
        }
        case JavaScriptEngineType::Hermes: {
            return getHermes();
        }
        default: {
            SC_ABORT("Invalid JS engine VM provided");
            return nullptr;
        }
    }
}
} // namespace Valdi
