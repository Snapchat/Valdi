//
//  QuickJSUtils.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/19/19.
//

#pragma once

#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JSNativeClassData.hpp"
#include "valdi/runtime/Utils/RefCountableAutoreleasePool.hpp"
#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"

#include <quickjs/quickjs.h>

#include <memory>

namespace ValdiQuickJS {

class QuickJSJavaScriptContext;

struct JSClassDefWithId {
    JSClassID classID;
    JSClassDef classDef;
};

const JSClassDefWithId* getBridgedFunctionClassDef();
const JSClassDefWithId* getWrappedObjectClassDef();
const JSClassDefWithId* getWeakRefFinalizerClassDef();
const JSClassDefWithId* getNativeClassConstructorClassDef();
const JSClassDefWithId* getNativeClassInstanceMemberClassDef();
const JSClassDefWithId* getNativeClassStaticMemberClassDef();

struct NativeClassFunctionData final : public Valdi::SimpleRefCountable {
    NativeClassFunctionData(const Valdi::Ref<Valdi::JSNativeClassData>& nativeClass,
                            const Valdi::StringBox& name);

    Valdi::Ref<Valdi::JSNativeClassData> nativeClass;
    Valdi::ReferenceInfo referenceInfo;
    Valdi::JSClassCallback callback = nullptr;
};

inline Valdi::JSValue toValdiJSValue(const JSValue& value) {
    return Valdi::JSValue(value);
}

inline JSValue fromValdiJSValue(const Valdi::JSValue& jsValue) {
    return jsValue.bridge<JSValue>();
}

inline Valdi::JSPropertyName toValdiJSPropertyName(const JSAtom& value) {
    return Valdi::JSPropertyName(value);
}

inline JSAtom fromValdiJSPropertyName(const Valdi::JSPropertyName& value) {
    return value.bridge<JSAtom>();
}

inline void attachValdiJSContext(JSContext* context, QuickJSJavaScriptContext* quickJsContext) {
    JS_SetRuntimeOpaque(JS_GetRuntime(context), quickJsContext);
}

inline QuickJSJavaScriptContext* getValdiJSContext(JSContext* context) {
    return reinterpret_cast<QuickJSJavaScriptContext*>(JS_GetRuntimeOpaque(JS_GetRuntime(context)));
}

inline QuickJSJavaScriptContext* getValdiJSContext(JSRuntime* rt) {
    return reinterpret_cast<QuickJSJavaScriptContext*>(JS_GetRuntimeOpaque(rt));
}

inline void setObjectCallable(const JSValue& value, const Valdi::Ref<Valdi::JSFunction>& callable) {
    auto* opaque = JS_GetOpaque(value, getBridgedFunctionClassDef()->classID);
    if (opaque != nullptr) {
        Valdi::RefCountableAutoreleasePool::release(opaque);
    }

    JS_SetOpaque(value, Valdi::unsafeBridgeRetain(callable.get()));
}

inline Valdi::JSFunction* getObjectCallable(const JSValue& value) {
    auto* opaque = JS_GetOpaque(value, getBridgedFunctionClassDef()->classID);

    return dynamic_cast<Valdi::JSFunction*>(reinterpret_cast<Valdi::RefCountable*>(opaque));
}

void setObjectWrappedObject(const JSValue& value, const Valdi::Ref<Valdi::RefCountable>& wrappedObject);
Valdi::Ref<Valdi::RefCountable> getObjectWrappedObject(const JSValue& value);

void setNativeClassConstructorData(const JSValue& value, const Valdi::Ref<Valdi::JSNativeClassData>& nativeClass);
Valdi::Ref<Valdi::JSNativeClassData> getNativeClassConstructorData(const JSValue& value);

void setNativeClassFunctionData(const JSValue& value, const Valdi::Ref<NativeClassFunctionData>& functionData);

size_t weakReferenceIdFromJSWeakReferenceFinalizer(const JSValue& jsWeakReferenceFinalizer);
void setWeakReferenceIdToJSWeakReferenceFinalizer(const JSValue& jsWeakReferenceFinalizer, size_t weakReferenceId);

} // namespace ValdiQuickJS
