//
//  HermesUtils.cpp
//  valdi-hermes
//
//  Created by Simon Corsin on 9/27/23.
//

#include "valdi/hermes/HermesUtils.hpp"
#include "valdi/hermes/HermesJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/Utils/RefCountableAutoreleasePool.hpp"
#include "valdi_core/cpp/Constants.hpp"

#include "hermes/VM/NativeState.h"

namespace Valdi::Hermes {

JSFunctionData::JSFunctionData(HermesJavaScriptContext& jsContext, const Valdi::Ref<Valdi::JSFunction>& function)
    : jsContext(jsContext), function(function) {}
JSFunctionData::~JSFunctionData() = default;

HermesNativeClassData::HermesNativeClassData(HermesJavaScriptContext& jsContext,
                                             const Ref<JSNativeClassData>& nativeClass)
    : jsContext(jsContext), nativeClass(nativeClass) {}

HermesNativeClassData::~HermesNativeClassData() = default;

HermesNativeClassFunctionData::HermesNativeClassFunctionData(HermesJavaScriptContext& jsContext,
                                                             const Ref<JSNativeClassData>& nativeClass,
                                                             const StringBox& name)
    : jsContext(jsContext),
      nativeClass(nativeClass),
      referenceInfo(nativeClass->makeMemberReferenceInfo(name)) {}

HermesNativeClassFunctionData::~HermesNativeClassFunctionData() = default;

void freeNativeState(hermes::vm::GC& /*gc*/, hermes::vm::NativeState* nativeState) {
    if (nativeState != nullptr) {
        RefCountableAutoreleasePool::release(nativeState->context());
    }
}

void freeContext(void* context) {
    RefCountableAutoreleasePool::release(context);
}

RefCountable* getOwnNativeStateContext(hermes::vm::Runtime& runtime,
                                       hermes::vm::Handle<hermes::vm::JSObject> object) {
    hermes::vm::NamedPropertyDescriptor descriptor;
    if (!hermes::vm::JSObject::getOwnNamedDescriptor(
            object,
            runtime,
            hermes::vm::Predefined::getSymbolID(hermes::vm::Predefined::InternalPropertyNativeState),
            descriptor)) {
        return nullptr;
    }

    auto value = hermes::vm::JSObject::getNamedSlotValueUnsafe(*object, runtime, descriptor);
    if (!value.isObject()) {
        return nullptr;
    }
    auto* objectValue = value.getObject(runtime);
    if (!hermes::vm::vmisa<hermes::vm::NativeState>(objectValue)) {
        return nullptr;
    }
    return reinterpret_cast<RefCountable*>(hermes::vm::vmcast<hermes::vm::NativeState>(objectValue)->context());
}

hermes::vm::ExecutionStatus setNativeState(hermes::vm::Runtime& runtime,
                                           hermes::vm::Handle<hermes::vm::JSObject> object,
                                           const Ref<RefCountable>& data) {
    auto nativeState =
        runtime.makeHandle(hermes::vm::NativeState::create(runtime, unsafeBridgeRetain(data.get()), &freeNativeState));
    auto result = hermes::vm::JSObject::defineOwnProperty(
        object,
        runtime,
        hermes::vm::Predefined::getSymbolID(hermes::vm::Predefined::InternalPropertyNativeState),
        hermes::vm::DefinePropertyFlags::getNewNonEnumerableFlags(),
        nativeState);
    return result.getStatus();
}

static hermes::vm::ExecutionStatus onJsCallError(hermes::vm::Runtime& runtime,
                                                 Valdi::JSExceptionTracker& exceptionTracker) {
    auto exception = exceptionTracker.getExceptionAndClear();
    return runtime.setThrownValue(HermesJavaScriptContext::toHermesValue(exception.get()));
}

static inline bool handleInterrupt(IJavaScriptContext& jsContext, JSExceptionTracker& exceptionTracker) {
    if (VALDI_UNLIKELY(jsContext.interruptRequested()) && jsContext.onInterrupt()) {
        exceptionTracker.onError("JavaScript execution terminated");
        return true;
    }
    return false;
}

hermes::vm::CallResult<hermes::vm::HermesValue> callTrampoline(void* context,
                                                               hermes::vm::Runtime& runtime,
                                                               hermes::vm::NativeArgs args) {
    auto* functionData = unsafeBridgeUnretained<JSFunctionData>(context);

    auto thisRef = functionData->jsContext.toJSValueRef(args.getThisArg());
    JSValueRef outArguments[args.getArgCount()];
    for (size_t i = 0; i < args.getArgCount(); i++) {
        outArguments[i] = functionData->jsContext.toJSValueRef(args.getArg(i));
    }

    JSExceptionTracker exceptionTracker(functionData->jsContext);
    JSFunctionNativeCallContext callContext(functionData->jsContext,
                                            &outArguments[0],
                                            args.getArgCount(),
                                            exceptionTracker,
                                            functionData->function->getReferenceInfo());
    callContext.setThisValue(thisRef.get());

    if (handleInterrupt(functionData->jsContext, exceptionTracker)) {
        return onJsCallError(runtime, exceptionTracker);
    }

    auto result = (*functionData->function)(callContext);

    if (VALDI_LIKELY(exceptionTracker)) {
        return HermesJavaScriptContext::toHermesValue(result.get());
    } else {
        return onJsCallError(runtime, exceptionTracker);
    }
}

hermes::vm::CallResult<hermes::vm::HermesValue> callNativeClassInstanceMember(
    void* context, hermes::vm::Runtime& runtime, hermes::vm::NativeArgs args) {
    auto functionData = unsafeBridge<HermesNativeClassFunctionData>(context);
    auto& jsContext = functionData->jsContext;

    auto argumentCount = args.getArgCount();
    JSValueRef arguments[argumentCount];
    for (size_t i = 0; i < argumentCount; i++) {
        arguments[i] = jsContext.toJSValueRef(args.getArg(i));
    }

    JSExceptionTracker exceptionTracker(jsContext);
    JSFunctionNativeCallContext callContext(jsContext,
                                            argumentCount == 0 ? nullptr : arguments,
                                            argumentCount,
                                            exceptionTracker,
                                            functionData->referenceInfo);
    auto thisValue = jsContext.toJSValueRef(args.getThisArg());
    callContext.setThisValue(thisValue.get());

    auto thisObject = args.dyncastThis<hermes::vm::JSObject>();
    auto instanceData =
        thisObject ? Ref(dynamic_cast<JSNativeClassInstanceData*>(getOwnNativeStateContext(runtime, thisObject))) :
                     Ref<JSNativeClassInstanceData>();
    if (instanceData == nullptr || instanceData->getNativeClass() != functionData->nativeClass) {
        return runtime.raiseTypeError("Native class member called with an incompatible receiver");
    }

    if (handleInterrupt(jsContext, exceptionTracker)) {
        return onJsCallError(runtime, exceptionTracker);
    }

    auto result = functionData->callback(instanceData->getOpaque().get(), callContext);
    if (!exceptionTracker) {
        return onJsCallError(runtime, exceptionTracker);
    }
    return HermesJavaScriptContext::toHermesValue(result.get());
}

hermes::vm::CallResult<hermes::vm::HermesValue> callNativeClassStaticMember(
    void* context, hermes::vm::Runtime& runtime, hermes::vm::NativeArgs args) {
    auto functionData = unsafeBridge<HermesNativeClassFunctionData>(context);
    auto& jsContext = functionData->jsContext;

    auto argumentCount = args.getArgCount();
    JSValueRef arguments[argumentCount];
    for (size_t i = 0; i < argumentCount; i++) {
        arguments[i] = jsContext.toJSValueRef(args.getArg(i));
    }

    JSExceptionTracker exceptionTracker(jsContext);
    JSFunctionNativeCallContext callContext(jsContext,
                                            argumentCount == 0 ? nullptr : arguments,
                                            argumentCount,
                                            exceptionTracker,
                                            functionData->referenceInfo);
    auto thisValue = jsContext.toJSValueRef(args.getThisArg());
    callContext.setThisValue(thisValue.get());

    if (handleInterrupt(jsContext, exceptionTracker)) {
        return onJsCallError(runtime, exceptionTracker);
    }

    auto result = functionData->callback(functionData->nativeClass->getOpaque().get(), callContext);
    if (!exceptionTracker) {
        return onJsCallError(runtime, exceptionTracker);
    }
    return HermesJavaScriptContext::toHermesValue(result.get());
}

hermes::vm::CallResult<hermes::vm::PseudoHandle<hermes::vm::JSObject>> createNativeClassObject(
    hermes::vm::Runtime& runtime, hermes::vm::Handle<hermes::vm::JSObject> prototype, void* /*context*/) {
    return hermes::vm::JSObject::create(runtime, prototype);
}

hermes::vm::CallResult<hermes::vm::HermesValue> callNativeClassConstructor(void* context,
                                                                           hermes::vm::Runtime& runtime,
                                                                           hermes::vm::NativeArgs args) {
    auto classData = unsafeBridge<HermesNativeClassData>(context);
    if (!args.isConstructorCall()) {
        return runtime.raiseTypeError("Native class constructor must be called with new");
    }

    auto newTargetValue = args.getNewTargetHandle();
    auto newTargetObject = hermes::vm::vmisa<hermes::vm::JSObject>(newTargetValue.get()) ?
                               hermes::vm::Handle<hermes::vm::JSObject>::vmcast(newTargetValue) :
                               hermes::vm::Runtime::makeNullHandle<hermes::vm::JSObject>();
    auto* newTargetClassData =
        newTargetObject ? dynamic_cast<HermesNativeClassData*>(getOwnNativeStateContext(runtime, newTargetObject)) :
                          nullptr;
    if (newTargetClassData != classData.get()) {
        return runtime.raiseTypeError("Native class subclassing is not supported");
    }
    auto constructor = classData->nativeClass->getConstructor();
    if (constructor == nullptr) {
        return runtime.raiseTypeError("Native class cannot be constructed from JavaScript");
    }

    auto& jsContext = classData->jsContext;
    auto argumentCount = args.getArgCount();
    JSValueRef arguments[argumentCount];
    for (size_t i = 0; i < argumentCount; i++) {
        arguments[i] = jsContext.toJSValueRef(args.getArg(i));
    }

    JSExceptionTracker exceptionTracker(jsContext);
    const auto& referenceInfo = classData->nativeClass->getConstructorReferenceInfo();
    JSFunctionNativeCallContext callContext(
        jsContext, argumentCount == 0 ? nullptr : arguments, argumentCount, exceptionTracker, referenceInfo);
    auto thisValue = jsContext.toJSValueRef(args.getThisArg());
    callContext.setThisValue(thisValue.get());

    if (handleInterrupt(jsContext, exceptionTracker)) {
        return onJsCallError(runtime, exceptionTracker);
    }

    auto nativeOpaque = constructor(classData->nativeClass->getOpaque().get(), callContext);
    if (exceptionTracker && nativeOpaque == nullptr) {
        exceptionTracker.onError("Native class constructor returned a null opaque object");
    }
    if (!exceptionTracker) {
        return onJsCallError(runtime, exceptionTracker);
    }

    auto thisObject = args.dyncastThis<hermes::vm::JSObject>();
    auto instanceData = makeShared<JSNativeClassInstanceData>(classData->nativeClass, nativeOpaque);
    if (setNativeState(runtime, thisObject, instanceData) == hermes::vm::ExecutionStatus::EXCEPTION) {
        return hermes::vm::ExecutionStatus::EXCEPTION;
    }
    return args.getThisArg();
}

void ByteBufferOStream::write_impl(const char* Ptr, size_t Size) {
    _output.append(Ptr, Ptr + Size);
}

uint64_t ByteBufferOStream::current_pos() const {
    return static_cast<uint64_t>(_output.size());
}

ByteBufferOStream::ByteBufferOStream(ByteBuffer& output) : _output(output) {}
ByteBufferOStream::~ByteBufferOStream() = default;

BytesView ByteBufferOStream::toBytesView() {
    flush();
    return _output.toBytesView();
}

} // namespace Valdi::Hermes
