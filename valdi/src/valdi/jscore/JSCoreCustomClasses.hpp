//
//  JSCoreCustomClasses.hpp
//  ValdiIOS
//
//  Created by Simon Corsin on 5/31/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi/runtime/JavaScript/JavaScriptTypes.hpp"
#include "valdi/runtime/JavaScript/JSNativeClassData.hpp"
#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#include <JavaScriptCore/JavaScriptCore.h>

namespace ValdiJSCore {

JSClassRef getNativeFunctionClassRef();
JSClassRef getWrappedObjectClassRef();
JSClassRef getNativeClassInstanceMemberClassRef();
JSClassRef getNativeClassStaticMemberClassRef();

class JavaScriptCoreContext;

struct JSFunctionData : public Valdi::SimpleRefCountable {
public:
    JavaScriptCoreContext& jsContext;
    Valdi::Ref<Valdi::JSFunction> function;

    JSFunctionData(JavaScriptCoreContext& jsContext, const Valdi::Ref<Valdi::JSFunction>& function);
    ~JSFunctionData() override;
};

JSFunctionData* getAttachedJsFunctionData(JSObjectRef objectRef);
Valdi::RefCountable* getAttachedWrappedObject(JSObjectRef objectRef);

struct NativeClassFunctionData final : public Valdi::SimpleRefCountable {
    NativeClassFunctionData(JavaScriptCoreContext& jsContext,
                            const Valdi::Ref<Valdi::JSNativeClassData>& nativeClass);

    NativeClassFunctionData(JavaScriptCoreContext& jsContext,
                            const Valdi::Ref<Valdi::JSNativeClassData>& nativeClass,
                            const Valdi::StringBox& name);

    JavaScriptCoreContext& jsContext;
    Valdi::Ref<Valdi::JSNativeClassData> nativeClass;
    Valdi::ReferenceInfo referenceInfo;
    Valdi::JSClassCallback callback = nullptr;
};

NativeClassFunctionData* getAttachedNativeClassFunctionData(JSObjectRef objectRef);

} // namespace ValdiJSCore
