//
//  HermesJavaScriptContext.cpp
//  valdi-hermes
//
//  Created by Simon Corsin on 9/27/23.
//

// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)

#include "valdi/hermes/HermesJavaScriptContext.hpp"
#include "valdi/hermes/HermesJavaScriptCompiler.hpp"
#include "valdi/hermes/HermesUtils.hpp"

#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JSNativeClassData.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"

#include "valdi_core/cpp/Text/UTF16Utils.hpp"
#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueUtils.hpp"

#include "hermes/VM/NativeState.h"
#include "hermes/VM/PropertyAccessor.h"

#ifdef HERMES_ENABLE_DEBUGGER
#include "valdi/hermes/HermesDebuggerServer.hpp"
#endif

#if defined(HERMES_API)
#include "hermes/hermes.h"
#endif

namespace Valdi::Hermes {

constexpr double kManagedValuesOccupancyRatio = 0.75;
constexpr double kManagedValuesSizingWeight = 0.5;

class JSFunctionBase : public JSFunction {
public:
    explicit JSFunctionBase(const char* name)
        : _referenceInfo(ReferenceInfoBuilder().withProperty(StringCache::makeStringFromCLiteral(name)).build()) {}
    ~JSFunctionBase() override = default;

    const ReferenceInfo& getReferenceInfo() const final {
        return _referenceInfo;
    }

private:
    ReferenceInfo _referenceInfo;
};

class UnhandledPromiseCallback : public JSFunctionBase {
public:
    UnhandledPromiseCallback() : JSFunctionBase("onhandledPromiseCallback") {}

    JSValueRef operator()(JSFunctionNativeCallContext& callContext) noexcept final {
        return callContext.getContext().newUndefined();
    }
};

static hermes::vm::RuntimeConfig makeRuntimeConfig() {
    return hermes::vm::RuntimeConfig::Builder()
        // 256kb stack size to be consistent with quickjs - Hermes's default is 128kb
        .withMaxNumRegisters(256 * 1024)
        .withMicrotaskQueue(true)
        .withEnableHermesInternal(true)
        .withES6Promise(true)
        .withES6Class(true)
        .withEnableBlockScoping(true)
        .build();
}

HermesJavaScriptContext::HermesJavaScriptContext(JavaScriptTaskScheduler* taskScheduler,
                                                 [[maybe_unused]] ILogger& logger)
    : IJavaScriptContext(taskScheduler),
      _logger(logger),
      _managedValues(kManagedValuesOccupancyRatio, kManagedValuesSizingWeight) {
#if defined(HERMES_API)
    _jsiRuntime = facebook::hermes::makeHermesRuntime(makeRuntimeConfig());
    _runtime = _jsiRuntime->getVMRuntimeUnsafe();
#else
    _sharedRuntime = hermes::vm::Runtime::create(makeRuntimeConfig());
    _runtime = _sharedRuntime.get();
#endif

    _runtime->addCustomRootsFunction([this](hermes::vm::GC*, hermes::vm::RootAcceptor& acceptor) {
        this->_managedValues.forEach(
            [&acceptor](ManagedHermesValue& managedHermesValue) { acceptor.accept(managedHermesValue.get()); });
    });
}

HermesJavaScriptContext::~HermesJavaScriptContext() {
#ifdef HERMES_ENABLE_DEBUGGER
    if (_debuggerRuntimeId != kRuntimeIdUndefined) {
        auto* debuggerService = HermesDebuggerServer::getInstance(_logger);
        if (debuggerService != nullptr) {
            debuggerService->getRegistry()->remove(_debuggerRuntimeId);
        }
    }
#endif

    setListener(nullptr);
    _emptyArrayBuffer = JSValueRef();
    _int8ArrayCtor = JSValueRef();
    _int16ArrayCtor = JSValueRef();
    _int32ArrayCtor = JSValueRef();
    _uint8ArrayCtor = JSValueRef();
    _uint8ClampedArrayCtor = JSValueRef();
    _uint16ArrayCtor = JSValueRef();
    _uint32ArrayCtor = JSValueRef();
    _float32ArrayCtor = JSValueRef();
    _float64ArrayCtor = JSValueRef();

    prepareForTeardown();
}

void HermesJavaScriptContext::onInitialize(JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);

    // Make sure undefined is initialized
    newUndefined();

    auto emptyArrayBuffer = _runtime->makeHandle(hermes::vm::JSArrayBuffer::create(
        *_runtime, hermes::vm::Handle<hermes::vm::JSObject>::vmcast(&_runtime->arrayBufferPrototype)));
    auto status = hermes::vm::JSArrayBuffer::createDataBlock(*_runtime, emptyArrayBuffer, 0);
    if (!checkException(status, exceptionTracker)) {
        return;
    }

    _emptyArrayBuffer = toJSValueRef(emptyArrayBuffer.getHermesValue());

    auto globalObject = getGlobalObject(exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    _int8ArrayCtor = getConstructorFromObject(globalObject.get(), "Int8Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _int16ArrayCtor = getConstructorFromObject(globalObject.get(), "Int16Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _int32ArrayCtor = getConstructorFromObject(globalObject.get(), "Int32Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _uint8ArrayCtor = getConstructorFromObject(globalObject.get(), "Uint8Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _uint8ClampedArrayCtor = getConstructorFromObject(globalObject.get(), "Uint8ClampedArray", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _uint16ArrayCtor = getConstructorFromObject(globalObject.get(), "Uint16Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _uint32ArrayCtor = getConstructorFromObject(globalObject.get(), "Uint32Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _float32ArrayCtor = getConstructorFromObject(globalObject.get(), "Float32Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _float64ArrayCtor = getConstructorFromObject(globalObject.get(), "Float64Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    initializePromiseRejectionTracker(exceptionTracker);
}

void HermesJavaScriptContext::initializePromiseRejectionTracker(JSExceptionTracker& exceptionTracker) {
    auto global = getGlobalObject(exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    auto hermesInternal = hermes::vm::JSObject::getNamedOrIndexed(
        toJSObject(global.get(), exceptionTracker),
        *_runtime,
        hermes::vm::Predefined::getSymbolID(hermes::vm::Predefined::HermesInternal));
    if (!checkException(hermesInternal.getStatus(), exceptionTracker)) {
        return;
    }

    auto hermesInternalObject = toJSValueRef(hermesInternal.getValue().getHermesValue());

    auto unhandledPromiseCallback = newFunction(makeShared<UnhandledPromiseCallback>(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    auto enablePromiseRejectionTrackerParams = newObject(exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    IJavaScriptContext::setObjectProperty(
        enablePromiseRejectionTrackerParams.get(), "allRejections", newBool(true).get(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    IJavaScriptContext::setObjectProperty(
        enablePromiseRejectionTrackerParams.get(), "onUnhandled", unhandledPromiseCallback.get(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    auto enablePromiseRejectionTrackerFn = IJavaScriptContext::getObjectProperty(
        hermesInternalObject.get(), "enablePromiseRejectionTracker", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    JSFunctionCallContext callContext(*this, &enablePromiseRejectionTrackerParams, 1, exceptionTracker);
    callObjectAsFunction(enablePromiseRejectionTrackerFn.get(), callContext);

    if (!exceptionTracker) {
        return;
    }
}

JSValueRef HermesJavaScriptContext::getGlobalObject(JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto global = _runtime->getGlobal();
    return toJSValueRef(global.getHermesValue());
}

bool HermesJavaScriptContext::supportsPreCompilation() const {
#ifdef HERMESVM_LEAN
    return false;
#else
    return true;
#endif
}

BytesView HermesJavaScriptContext::preCompile(const std::string_view& script,
                                              const std::string_view& sourceFilename,
                                              JSExceptionTracker& exceptionTracker) {
#ifdef HERMESVM_LEAN
    exceptionTracker.onError("Hermes not compiled with compilation enabled");
    return BytesView();
#else
    hermes::vm::GCScope gcScope(*_runtime);
    auto formattedScript = Valdi::formatJsModule(script);

    auto serializedBytecode =
        HermesJavaScriptCompiler::get()->compileToSerializedBytecode(formattedScript, sourceFilename, exceptionTracker);

    if (!exceptionTracker) {
        return BytesView();
    }

    return Valdi::makePreCompiledJsModule(serializedBytecode.data(), serializedBytecode.size());
#endif
}

JSValueRef HermesJavaScriptContext::evaluatePreCompiled(const BytesView& script,
                                                        const std::string_view& sourceFilename,
                                                        JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);

    auto bytecode = HermesJavaScriptCompiler::deserializeBytecode(script, exceptionTracker);
    if (!exceptionTracker) {
        return newUndefined();
    }

    hermes::vm::RuntimeModuleFlags runtimeModuleFlags;
    auto evaluateResult = _runtime->runBytecode(std::move(bytecode),
                                                runtimeModuleFlags,
                                                llvh::StringRef(sourceFilename.data(), sourceFilename.length()),
                                                hermes::vm::Runtime::makeNullHandle<hermes::vm::Environment>(),
                                                hermes::vm::Handle<>(&_runtime->global_));

    return checkCallAndGetValue(evaluateResult, exceptionTracker);
}

JSValueRef HermesJavaScriptContext::evaluate(const std::string& script,
                                             const std::string_view& sourceFilename,
                                             JSExceptionTracker& exceptionTracker) {
#ifdef HERMESVM_LEAN
    exceptionTracker.onError("Hermes not compiled with compilation enabled");
    return JSValueRef();
#else
    hermes::vm::GCScope gcScope(*_runtime);

    auto compiledBytecode = HermesJavaScriptCompiler::get()->compile(script, sourceFilename, exceptionTracker);
    if (!exceptionTracker) {
        return newUndefined();
    }

    auto jsSourceURL = llvh::StringRef(reinterpret_cast<const char*>(sourceFilename.data()), sourceFilename.size());

    hermes::vm::RuntimeModuleFlags runtimeModuleFlags;
    auto evaluateResult = _runtime->runBytecode(std::shared_ptr<hermes::hbc::BCProvider>(std::move(compiledBytecode)),
                                                runtimeModuleFlags,
                                                jsSourceURL,
                                                hermes::vm::Runtime::makeNullHandle<hermes::vm::Environment>(),
                                                hermes::vm::Handle<>(&_runtime->global_));

    return checkCallAndGetValue(evaluateResult, exceptionTracker);
#endif
}

hermes::vm::Handle<hermes::vm::SymbolID> HermesJavaScriptContext::newSymbolIDFromString(const std::string_view& str) {
    auto result = hermes::vm::StringPrimitive::createEfficient(
        *_runtime,
        llvh::makeArrayRef(reinterpret_cast<const uint8_t*>(str.data()), str.size()),
        /* ignoreInputErrors */ true);

    auto symbolId =
        hermes::vm::stringToSymbolID(*_runtime, hermes::vm::createPseudoHandle(result.getValue().getString()));

    return std::move(symbolId.getValue());
}

JSPropertyNameRef HermesJavaScriptContext::newPropertyName(const std::string_view& str) {
    hermes::vm::GCScope gcScope(*_runtime);

    auto symbolId = newSymbolIDFromString(str);

    return toPropertyNameRef(symbolId);
}

StringBox HermesJavaScriptContext::propertyNameToString(const JSPropertyName& propertyName) {
    hermes::vm::GCScope gcScope(*_runtime);

    auto symbolId = toSymbolID(propertyName);
    auto str = _runtime->getStringPrimFromSymbolID(symbolId.get());

    auto stringView = hermes::vm::StringPrimitive::createStringView(*_runtime, _runtime->makeHandle(str));

    if (stringView.isASCII()) {
        return StringCache::getGlobal().makeString(stringView.castToCharPtr(), stringView.length());
    } else {
        return StringCache::getGlobal().makeStringFromUTF16(stringView.castToChar16Ptr(), stringView.length());
    }
}

JSValueRef HermesJavaScriptContext::newObject(JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);

    auto result = hermes::vm::JSObject::create(*_runtime);
    return toJSValueRefUncached(result.getHermesValue());
}

JSValueRef HermesJavaScriptContext::newFunction(const Ref<JSFunction>& callable, JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);

    auto functionName = newSymbolIDFromString(callable->getReferenceInfo().getName().toStringView());

    auto functionData = makeShared<JSFunctionData>(*this, callable);

    auto functionResult = hermes::vm::FinalizableNativeFunction::createWithoutPrototype(
        *_runtime, unsafeBridgeRetain(functionData.get()), &callTrampoline, &freeContext, functionName.get(), 0);

    if (!checkException(functionResult.getStatus(), exceptionTracker)) {
        return newUndefined();
    }

    return toJSValueRefUncached(functionResult.getValue());
}

JSValueRef HermesJavaScriptContext::newWeakRef(const JSValue& object, JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);

    auto jsObject = toJSObject(object, exceptionTracker);
    if (!exceptionTracker) {
        return newUndefined();
    }

    auto prototype = hermes::vm::vmcast<hermes::vm::JSObject>(_runtime->weakRefPrototype);

    auto weakRef = hermes::vm::JSWeakRef::create(*_runtime, _runtime->makeHandle(prototype));
    weakRef->setTarget(*_runtime, std::move(jsObject));

    return toJSValueRefUncached(weakRef.getHermesValue());
}

JSValueRef HermesJavaScriptContext::derefWeakRef(const JSValue& weakRef, JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);

    const auto* hermesValue = toPinnedHermesValue(weakRef);

    if (!hermes::vm::vmisa<hermes::vm::JSWeakRef>(*hermesValue)) {
        exceptionTracker.onError("Not a WeakReference");
        return newUndefined();
    }

    auto jsWeakRef = hermes::vm::Handle<hermes::vm::JSWeakRef>::vmcast(hermesValue);

    return toJSValueRef(jsWeakRef->deref(*_runtime));
}

JSValueRef HermesJavaScriptContext::onNewNull() {
    return toJSValueRefUncached(_runtime->getNullValue().getHermesValue());
}

JSValueRef HermesJavaScriptContext::onNewUndefined() {
    auto value = toJSValueRefUncached(_runtime->getUndefinedValue().getHermesValue());
    _undefinedPinnedValue = toPinnedHermesValue(value.get());
    return value;
}

JSValueRef HermesJavaScriptContext::onNewBool(bool boolean) {
    return toJSValueRefUncached(_runtime->getBoolValue(boolean).getHermesValue());
}

JSValueRef HermesJavaScriptContext::onNewNumber(int32_t number) {
    switch (number) {
        case 0:
            return toJSValueRefUncached(_runtime->getZeroValue().getHermesValue());
        case 1:
            return toJSValueRefUncached(_runtime->getOneValue().getHermesValue());
        default:
            return toJSValueRefUncached(hermes::vm::HermesValue::encodeTrustedNumberValue(static_cast<double>(number)));
    }
}

JSValueRef HermesJavaScriptContext::onNewNumber(double number) {
    return toJSValueRefUncached(hermes::vm::HermesValue::encodeUntrustedNumberValue(number));
}

JSValueRef HermesJavaScriptContext::newStringUTF8(const std::string_view& str, JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);

    auto result = hermes::vm::StringPrimitive::createEfficient(
        *_runtime,
        llvh::makeArrayRef(reinterpret_cast<const uint8_t*>(str.data()), str.size()),
        /* ignoreInputErrors */ true);

    return checkCallAndGetValue(result, exceptionTracker);
}

JSValueRef HermesJavaScriptContext::newStringUTF16(const std::u16string_view& str,
                                                   JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);

    auto result = hermes::vm::StringPrimitive::createEfficient(
        *_runtime, llvh::makeArrayRef(reinterpret_cast<const char16_t*>(str.data()), str.size()));

    return checkCallAndGetValue(result, exceptionTracker);
}

JSValueRef HermesJavaScriptContext::newArray(size_t initialSize, JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);

    auto result = hermes::vm::JSArray::create(*_runtime, initialSize, initialSize);

    if (checkException(result.getStatus(), exceptionTracker)) {
        return toJSValueRef(result.getValue().getHermesValue());
    } else {
        return newUndefined();
    }
}

JSValueRef HermesJavaScriptContext::newArrayBuffer(const BytesView& buffer, JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);

    if (buffer.empty()) {
        return _emptyArrayBuffer;
    } else {
        auto arrayBuffer = _runtime->makeHandle(hermes::vm::JSArrayBuffer::create(
            *_runtime, hermes::vm::Handle<hermes::vm::JSObject>::vmcast(&_runtime->arrayBufferPrototype)));

        auto status = hermes::vm::JSArrayBuffer::setExternalDataBlock(*_runtime,
                                                                      arrayBuffer,
                                                                      const_cast<uint8_t*>(buffer.data()),
                                                                      buffer.size(),
                                                                      unsafeBridgeRetain(buffer.getSource().get()),
                                                                      &freeNativeState);
        if (!checkException(status, exceptionTracker)) {
            return newUndefined();
        }

        return toJSValueRef(arrayBuffer.getHermesValue());
    }
}

JSValueRef HermesJavaScriptContext::newTypedArrayFromConstructor(const JSValueRef& ctor,
                                                                 const JSValue& arrayBuffer,
                                                                 JSExceptionTracker& exceptionTracker) {
    auto parameter = JSValueRef::makeUnretained(*this, arrayBuffer);

    JSFunctionCallContext callContext(*this, &parameter, 1, exceptionTracker);
    return callObjectAsConstructor(ctor.get(), callContext);
}

JSValueRef HermesJavaScriptContext::newTypedArrayFromArrayBuffer(const TypedArrayType& type,
                                                                 const JSValue& arrayBuffer,
                                                                 JSExceptionTracker& exceptionTracker) {
    switch (type) {
        case Valdi::Int8Array:
            return newTypedArrayFromConstructor(_int8ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Int16Array:
            return newTypedArrayFromConstructor(_int16ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Int32Array:
            return newTypedArrayFromConstructor(_int32ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Uint8Array:
            return newTypedArrayFromConstructor(_uint8ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Uint8ClampedArray:
            return newTypedArrayFromConstructor(_uint8ClampedArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Uint16Array:
            return newTypedArrayFromConstructor(_uint16ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Uint32Array:
            return newTypedArrayFromConstructor(_uint32ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Float32Array:
            return newTypedArrayFromConstructor(_float32ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Float64Array:
            return newTypedArrayFromConstructor(_float64ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::ArrayBuffer:
            return Valdi::JSValueRef::makeRetained(*this, arrayBuffer);
    }
}

JSValueRef HermesJavaScriptContext::newWrappedObject(const Ref<RefCountable>& wrappedObject,
                                                     JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);

    auto result = _runtime->makeHandle(hermes::vm::JSObject::create(*_runtime));
    auto nativeState = _runtime->makeHandle(
        hermes::vm::NativeState::create(*_runtime, unsafeBridgeRetain(wrappedObject.get()), &freeNativeState));

    auto setPropertyResult = hermes::vm::JSObject::defineOwnProperty(
        result,
        *_runtime,
        hermes::vm::Predefined::getSymbolID(hermes::vm::Predefined::InternalPropertyNativeState),
        hermes::vm::DefinePropertyFlags::getNewNonEnumerableFlags(),
        nativeState);

    if (!checkException(setPropertyResult.getStatus(), exceptionTracker)) {
        return newUndefined();
    }

    return toJSValueRefUncached(result.getHermesValue());
}

static hermes::vm::DefinePropertyFlags toHermesDataPropertyFlags(bool writable, bool enumerable, bool configurable) {
    hermes::vm::DefinePropertyFlags flags;
    flags.setEnumerable = 1;
    flags.enumerable = enumerable;
    flags.setWritable = 1;
    flags.writable = writable;
    flags.setConfigurable = 1;
    flags.configurable = configurable;
    flags.setValue = 1;
    return flags;
}

static hermes::vm::DefinePropertyFlags toHermesAccessorPropertyFlags(bool hasGetter,
                                                                     bool hasSetter,
                                                                     bool enumerable,
                                                                     bool configurable) {
    hermes::vm::DefinePropertyFlags flags;
    flags.setEnumerable = 1;
    flags.enumerable = enumerable;
    flags.setConfigurable = 1;
    flags.configurable = configurable;
    flags.setGetter = hasGetter;
    flags.setSetter = hasSetter;
    return flags;
}

JSValueRef HermesJavaScriptContext::newNativeClass(const Ref<RefCountable>& classOpaque,
                                                   const JSClassDefinition& classDefinition,
                                                   JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);

    auto nativeClass =
        makeShared<JSNativeClassData>(classDefinition.getName(), classOpaque, classDefinition.getConstructor());
    auto backendClass = makeShared<HermesNativeClassData>(*this, nativeClass);
    auto prototype = _runtime->makeHandle(hermes::vm::JSObject::create(*_runtime));
    auto constructor = _runtime->makeHandle(hermes::vm::NativeConstructor::create(
        *_runtime,
        hermes::vm::Handle<hermes::vm::JSObject>::vmcast(&_runtime->functionPrototype),
        backendClass.get(),
        &callNativeClassConstructor,
        0,
        &createNativeClassObject,
        hermes::vm::CellKind::JSObjectKind));

    auto className = newSymbolIDFromString(classDefinition.getName().toStringView());
    auto defineConstructorStatus = hermes::vm::Callable::defineNameLengthAndPrototype(
        constructor, *_runtime, className.get(), 0, prototype, hermes::vm::Callable::WritablePrototype::No, false);
    if (!checkException(defineConstructorStatus, exceptionTracker)) {
        return newUndefined();
    }
    if (!checkException(setNativeState(*_runtime, constructor, backendClass), exceptionTracker)) {
        return newUndefined();
    }

    auto createFunction = [&](const Ref<HermesNativeClassFunctionData>& functionData,
                              auto call,
                              std::string_view name) -> hermes::vm::Handle<hermes::vm::Callable> {
        auto functionName = newSymbolIDFromString(name);
        auto result =
            hermes::vm::FinalizableNativeFunction::createWithoutPrototype(*_runtime,
                                                                          unsafeBridgeRetain(functionData.get()),
                                                                          call,
                                                                          &freeContext,
                                                                          functionName.get(),
                                                                          0);
        if (!checkException(result.getStatus(), exceptionTracker)) {
            return hermes::vm::Runtime::makeNullHandle<hermes::vm::Callable>();
        }
        return _runtime->makeHandle<hermes::vm::Callable>(result.getValue());
    };

    auto defineDataProperty = [&](hermes::vm::Handle<hermes::vm::JSObject> object,
                                  std::string_view name,
                                  hermes::vm::Handle<> value,
                                  bool writable,
                                  bool enumerable,
                                  bool configurable) -> bool {
        auto propertyName = newSymbolIDFromString(name);
        auto result =
            hermes::vm::JSObject::defineOwnProperty(object,
                                                    *_runtime,
                                                    propertyName.get(),
                                                    toHermesDataPropertyFlags(writable, enumerable, configurable),
                                                    value,
                                                    hermes::vm::PropOpFlags().plusThrowOnError());
        return checkException(result.getStatus(), exceptionTracker) && result.getValue();
    };

    auto defineAccessorProperty = [&](hermes::vm::Handle<hermes::vm::JSObject> object,
                                      std::string_view name,
                                      hermes::vm::Handle<hermes::vm::Callable> getter,
                                      hermes::vm::Handle<hermes::vm::Callable> setter,
                                      bool enumerable,
                                      bool configurable) -> bool {
        auto propertyName = newSymbolIDFromString(name);
        auto accessor = _runtime->makeHandle<hermes::vm::PropertyAccessor>(
            hermes::vm::PropertyAccessor::create(*_runtime, getter, setter));
        auto result = hermes::vm::JSObject::defineOwnProperty(
            object,
            *_runtime,
            propertyName.get(),
            toHermesAccessorPropertyFlags(
                static_cast<bool>(getter), static_cast<bool>(setter), enumerable, configurable),
            accessor,
            hermes::vm::PropOpFlags().plusThrowOnError());
        return checkException(result.getStatus(), exceptionTracker) && result.getValue();
    };

    auto constructorObject = hermes::vm::Handle<hermes::vm::JSObject>::vmcast(constructor);
    for (const auto& entry : classDefinition.getEntries()) {
        auto object = entry.isClassMember() ? constructorObject : prototype;
        auto memberCallback =
            entry.isClassMember() ? &callNativeClassStaticMember : &callNativeClassInstanceMember;
        switch (entry.getKind()) {
            case JSClassEntryKind::Method: {
                if (entry.getMethodCallback() == nullptr) {
                    exceptionTracker.onError("Native class method callback cannot be null");
                    return newUndefined();
                }
                const auto& propertyName = entry.getName();
                auto functionData = makeShared<HermesNativeClassFunctionData>(*this, nativeClass, propertyName);
                functionData->callback = entry.getMethodCallback();
                auto function = createFunction(functionData, memberCallback, propertyName.toStringView());
                if (!exceptionTracker || !defineDataProperty(object,
                                                             propertyName.toStringView(),
                                                             function,
                                                             entry.isWritable(),
                                                             entry.isEnumerable(),
                                                             entry.isConfigurable())) {
                    return newUndefined();
                }
                break;
            }
            case JSClassEntryKind::Constant:
                if (!defineDataProperty(object,
                                        entry.getName().toStringView(),
                                        toHandle(entry.getValue().get()),
                                        entry.isWritable(),
                                        entry.isEnumerable(),
                                        entry.isConfigurable())) {
                    return newUndefined();
                }
                break;
            case JSClassEntryKind::Accessor: {
                if (entry.getGetterCallback() == nullptr && entry.getSetterCallback() == nullptr) {
                    exceptionTracker.onError("Native class accessor must have a getter or setter");
                    return newUndefined();
                }
                auto getter = hermes::vm::Runtime::makeNullHandle<hermes::vm::Callable>();
                auto setter = hermes::vm::Runtime::makeNullHandle<hermes::vm::Callable>();
                const auto& propertyName = entry.getName();
                if (entry.getGetterCallback() != nullptr) {
                    auto functionData = makeShared<HermesNativeClassFunctionData>(*this, nativeClass, propertyName);
                    functionData->callback = entry.getGetterCallback();
                    getter = createFunction(functionData, memberCallback, propertyName.toStringView());
                }
                if (exceptionTracker && entry.getSetterCallback() != nullptr) {
                    auto functionData = makeShared<HermesNativeClassFunctionData>(*this, nativeClass, propertyName);
                    functionData->callback = entry.getSetterCallback();
                    setter = createFunction(functionData, memberCallback, propertyName.toStringView());
                }
                if (!exceptionTracker ||
                    !defineAccessorProperty(object,
                                            propertyName.toStringView(),
                                            getter,
                                            setter,
                                            entry.isEnumerable(),
                                            entry.isConfigurable())) {
                    return newUndefined();
                }
                break;
            }
        }
    }

    return toJSValueRefUncached(constructor.getHermesValue());
}

JSValueRef HermesJavaScriptContext::newObjectFromNativeClass(const Ref<RefCountable>& opaque,
                                                             const JSValue& cls,
                                                             JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto clsObject = toJSObject(cls, exceptionTracker);
    if (!exceptionTracker) {
        return newUndefined();
    }
    auto backendClass = Ref(dynamic_cast<HermesNativeClassData*>(getOwnNativeStateContext(*_runtime, clsObject)));
    if (backendClass == nullptr) {
        exceptionTracker.onError("Value is not a native class");
        return newUndefined();
    }
    if (opaque == nullptr) {
        exceptionTracker.onError("Native class opaque object cannot be null");
        return newUndefined();
    }

    auto prototypeResult = hermes::vm::JSObject::getNamedOrIndexed(
        clsObject,
        *_runtime,
        hermes::vm::Predefined::getSymbolID(hermes::vm::Predefined::prototype));
    if (!checkException(prototypeResult.getStatus(), exceptionTracker)) {
        return newUndefined();
    }
    auto prototypeValue = prototypeResult.getValue().getHermesValue();
    if (!prototypeValue.isObject()) {
        exceptionTracker.onError("Native class prototype is not an object");
        return newUndefined();
    }
    auto prototype = _runtime->makeHandle<hermes::vm::JSObject>(prototypeValue);

    auto object = _runtime->makeHandle(hermes::vm::JSObject::create(*_runtime, prototype));
    auto instanceData = makeShared<JSNativeClassInstanceData>(backendClass->nativeClass, opaque);
    if (!checkException(setNativeState(*_runtime, object, instanceData), exceptionTracker)) {
        return newUndefined();
    }
    return toJSValueRefUncached(object.getHermesValue());
}

JSValueRef HermesJavaScriptContext::getObjectProperty(const JSValue& object,
                                                      const JSPropertyName& propertyName,
                                                      JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto jsObject = toJSObject(object, exceptionTracker);
    if (!jsObject) {
        return newUndefined();
    }

    auto symbolId = toSymbolID(propertyName);
    auto result = hermes::vm::JSObject::getNamedOrIndexed(std::move(jsObject), *_runtime, symbolId.get());

    if (!checkException(result.getStatus(), exceptionTracker)) {
        return newUndefined();
    }

    return toJSValueRef(result.getValue().getHermesValue());
}

JSValueRef HermesJavaScriptContext::getObjectPropertyForIndex(const JSValue& object,
                                                              size_t index,
                                                              JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto jsObject = toJSObject(object, exceptionTracker);
    if (!jsObject) {
        return newUndefined();
    }

    auto result = hermes::vm::JSObject::getComputed_RJS(
        std::move(jsObject),
        *_runtime,
        _runtime->makeHandle(hermes::vm::HermesValue::encodeUntrustedNumberValue(index)));

    if (!checkException(result.getStatus(), exceptionTracker)) {
        return newUndefined();
    }

    return toJSValueRef(result.getValue().getHermesValue());
}

bool HermesJavaScriptContext::hasObjectProperty(const JSValue& object, const JSPropertyName& propertyName) {
    hermes::vm::GCScope gcScope(*_runtime);
    const auto* hermesValue = toPinnedHermesValue(object);

    if (!hermesValue->isObject()) {
        return false;
    }

    auto symbolId = toSymbolID(propertyName);
    auto result = hermes::vm::JSObject::hasNamedOrIndexed(
        hermes::vm::Handle<::hermes::vm::JSObject>::vmcast(hermesValue), *_runtime, symbolId.get());

    if (result.getStatus() == hermes::vm::ExecutionStatus::EXCEPTION) {
        _runtime->clearThrownValue();
        return false;
    }

    return result.getValue();
}

void HermesJavaScriptContext::setObjectProperty(const JSValue& object,
                                                const JSPropertyName& propertyName,
                                                const JSValue& propertyValue,
                                                bool enumerable,
                                                JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto jsObject = toJSObject(object, exceptionTracker);
    if (!jsObject) {
        return;
    }

    auto symbolId = toSymbolID(propertyName);

    if (enumerable) {
        auto result = hermes::vm::JSObject::putNamedOrIndexed(std::move(jsObject),
                                                              *_runtime,
                                                              symbolId.get(),
                                                              toHandle(propertyValue),
                                                              hermes::vm::PropOpFlags().plusThrowOnError());

        checkException(result.getStatus(), exceptionTracker);
    } else {
        auto result =
            hermes::vm::JSObject::defineOwnProperty(std::move(jsObject),
                                                    *_runtime,
                                                    symbolId.get(),
                                                    hermes::vm::DefinePropertyFlags::getNewNonEnumerableFlags(),
                                                    toHandle(propertyValue));
        checkException(result.getStatus(), exceptionTracker);
    }
}

void HermesJavaScriptContext::setObjectProperty(const JSValue& object,
                                                const JSValue& propertyName,
                                                const JSValue& propertyValue,
                                                bool enumerable,
                                                JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto jsObject = toJSObject(object, exceptionTracker);
    if (!jsObject) {
        return;
    }

    auto result = hermes::vm::JSObject::putComputed_RJS(
        std::move(jsObject), *_runtime, toHandle(propertyName), toHandle(propertyValue));

    checkException(result.getStatus(), exceptionTracker);
}

void HermesJavaScriptContext::setObjectPropertyIndex(const JSValue& object,
                                                     size_t index,
                                                     const JSValue& value,
                                                     JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto jsObject = toJSObject(object, exceptionTracker);
    if (!jsObject) {
        return;
    }

    auto result = hermes::vm::JSObject::putComputed_RJS(
        std::move(jsObject),
        *_runtime,
        _runtime->makeHandle(hermes::vm::HermesValue::encodeTrustedNumberValue(index)),
        toHandle(value));

    checkException(result.getStatus(), exceptionTracker);
}

hermes::vm::CallResult<hermes::vm::PseudoHandle<hermes::vm::HermesValue>> HermesJavaScriptContext::doCall(
    hermes::vm::ScopedNativeCallFrame& callFrame,
    const hermes::vm::Handle<hermes::vm::Callable>& handle,
    JSFunctionCallContext& callContext) {
    if (callFrame.overflowed()) {
        return _runtime->raiseStackOverflow(hermes::vm::Runtime::StackOverflowKind::NativeStack);
    }

    for (size_t i = 0; i < callContext.getParameterSize(); i++) {
        callFrame->getArgRef(static_cast<int32_t>(i)) = toHermesValue(callContext.getParameters()[i].get());
    }

    return hermes::vm::Callable::call(handle, *_runtime);
}

JSValueRef HermesJavaScriptContext::callObjectAsFunction(const JSValue& object, JSFunctionCallContext& callContext) {
    hermes::vm::GCScope gcScope(*_runtime);
    const auto* hermesValue = toPinnedHermesValue(object);

    if (!hermes::vm::vmisa<hermes::vm::Callable>(*hermesValue)) {
        callContext.getExceptionTracker().onError("Value is not a function");
        return newUndefined();
    }

    auto handle = hermes::vm::Handle<hermes::vm::Callable>::vmcast(hermesValue);

    auto thisValue = toHermesValue(callContext.getThisValue());
    hermes::vm::ScopedNativeCallFrame callFrame(*_runtime,
                                                static_cast<uint32_t>(callContext.getParameterSize()),
                                                handle.getHermesValue(),
                                                hermes::vm::HermesValue::encodeUndefinedValue(),
                                                thisValue.isUndefined() ? _runtime->getGlobal().getHermesValue() :
                                                                          thisValue);

    auto callResult = doCall(callFrame, handle, callContext);

    if (!checkException(callResult.getStatus(), callContext.getExceptionTracker())) {
        return newUndefined();
    }

    return toJSValueRef(callResult->get());
}

JSValueRef HermesJavaScriptContext::callObjectAsConstructor(const JSValue& object, JSFunctionCallContext& callContext) {
    hermes::vm::GCScope gcScope(*_runtime);
    const auto* hermesValue = toPinnedHermesValue(object);

    if (!hermes::vm::vmisa<hermes::vm::Callable>(*hermesValue)) {
        callContext.getExceptionTracker().onError("Value is not a function");
        return newUndefined();
    }

    auto handle = hermes::vm::Handle<hermes::vm::Callable>::vmcast(hermesValue);

    auto thisRes = hermes::vm::Callable::createThisForConstruct_RJS(handle, *_runtime);
    auto objHandle = _runtime->makeHandle<hermes::vm::JSObject>(std::move(*thisRes));

    hermes::vm::ScopedNativeCallFrame callFrame(*_runtime,
                                                static_cast<uint32_t>(callContext.getParameterSize()),
                                                handle.getHermesValue(),
                                                handle.getHermesValue(),
                                                objHandle.getHermesValue());

    auto callResult = doCall(callFrame, handle, callContext);

    if (!checkException(callResult.getStatus(), callContext.getExceptionTracker())) {
        return newUndefined();
    }

    auto resultValue = callResult->get();
    return toJSValueRef(resultValue.isObject() ? resultValue : objHandle.getHermesValue());
}

void HermesJavaScriptContext::visitObjectPropertyNames(const JSValue& object,
                                                       JSExceptionTracker& exceptionTracker,
                                                       IJavaScriptPropertyNamesVisitor& visitor) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto jsObject = toJSObject(object, exceptionTracker);
    if (!jsObject) {
        return;
    }

    uint32_t beginIndex;
    uint32_t endIndex;
    auto result = hermes::vm::getForInPropertyNames(*_runtime, std::move(jsObject), beginIndex, endIndex);
    if (!checkException(result.getStatus(), exceptionTracker)) {
        return;
    }

    const auto& propertyNames = *result;
    for (uint32_t i = beginIndex; i < endIndex; i++) {
        hermes::vm::GCScope innerGCScope(*_runtime);

        auto name = propertyNames->at(*_runtime, i);

        JSPropertyNameRef propertyName;

        if (name.isString()) {
            auto symbolId = hermes::vm::stringToSymbolID(*_runtime, hermes::vm::createPseudoHandle(name.getString()));
            if (!checkException(symbolId.getStatus(), exceptionTracker)) {
                return;
            }
            propertyName = toPropertyNameRef(symbolId.getValue());
        } else if (name.isNumber()) {
            auto strResult = hermes::vm::toString_RJS(*_runtime, _runtime->makeHandle(name));
            if (!checkException(strResult.getStatus(), exceptionTracker)) {
                return;
            }
            auto symbolId = hermes::vm::stringToSymbolID(*_runtime, std::move(strResult.getValue()));
            if (!checkException(symbolId.getStatus(), exceptionTracker)) {
                return;
            }

            propertyName = toPropertyNameRef(symbolId.getValue());
        }

        if (!propertyName.empty()) {
            if (!visitor.visitPropertyName(*this, object, propertyName.get(), exceptionTracker)) {
                return;
            }
        }
    }
}

static hermes::vm::CallResult<hermes::vm::PseudoHandle<hermes::vm::StringPrimitive>> toString(
    hermes::vm::Runtime& runtime, const hermes::vm::HermesValue& value) {
    return hermes::vm::toString_RJS(runtime, runtime.makeHandle(value));
}

StringBox HermesJavaScriptContext::valueToString(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto result = toString(*_runtime, toHermesValue(value));

    if (!checkException(result.getStatus(), exceptionTracker)) {
        return StringBox();
    }

    auto stringView =
        hermes::vm::StringPrimitive::createStringView(*_runtime, _runtime->makeHandle(std::move(result.getValue())));

    if (stringView.isASCII()) {
        return StringCache::getGlobal().makeString(stringView.castToCharPtr(), stringView.length());
    } else {
        return StringCache::getGlobal().makeStringFromUTF16(stringView.castToChar16Ptr(), stringView.length());
    }
}

Ref<StaticString> HermesJavaScriptContext::valueToStaticString(const JSValue& value,
                                                               JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto result = toString(*_runtime, toHermesValue(value));

    if (!checkException(result.getStatus(), exceptionTracker)) {
        return nullptr;
    }

    auto stringView =
        hermes::vm::StringPrimitive::createStringView(*_runtime, _runtime->makeHandle(std::move(result.getValue())));
    if (stringView.isASCII()) {
        return StaticString::makeUTF8(stringView.castToCharPtr(), stringView.length());
    } else {
        return StaticString::makeUTF16(stringView.castToChar16Ptr(), stringView.length());
    }
}

bool HermesJavaScriptContext::valueToBool(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto hermesValue = toHermesValue(value);
    if (hermesValue.isUndefined() || hermesValue.isNull()) {
        return false;
    } else if (hermesValue.isNumber()) {
        return hermesValue.getNumber() != 0;
    } else if (hermesValue.isBool()) {
        return hermesValue.getBool();
    } else if (hermesValue.isString()) {
        return hermesValue.getString()->getSize() != 0;
    } else {
        return true;
    }
}

double HermesJavaScriptContext::valueToDouble(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto hermesValue = toHermesValue(value);
    if (hermesValue.isUndefined() || hermesValue.isNull()) {
        return 0.0;
    } else if (hermesValue.isNumber()) {
        return hermesValue.getNumber();
    } else if (hermesValue.isBool()) {
        return hermesValue.getBool() ? 1.0 : 0.0;
    } else if (hermesValue.isString()) {
        auto stringView =
            hermes::vm::StringPrimitive::createStringView(*_runtime, _runtime->makeHandle(hermesValue.getString()));
        std::string utf8;
        if (stringView.isASCII()) {
            utf8 += std::string_view(stringView.castToCharPtr(), stringView.length());
            ;
        } else {
            auto converted = Valdi::utf16ToUtf8(stringView.castToChar16Ptr(), stringView.length());
            utf8 += std::string_view(converted.first, converted.second);
        }

        return atof(utf8.c_str()); // NOLINT(cert-err34-c)
    } else {
        return 0.0;
    }
}

int32_t HermesJavaScriptContext::valueToInt(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    return static_cast<int32_t>(valueToDouble(value, exceptionTracker));
}

static bool canReceiveNativeState(hermes::vm::JSObject* object) {
    return !object->isProxyObject() && !object->isHostObject();
}

Ref<RefCountable> HermesJavaScriptContext::valueToWrappedObject(const JSValue& value,
                                                                JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto* hermesValue = toPinnedHermesValue(value);
    if (!hermesValue->isObject()) {
        return nullptr;
    }

    auto jsObject = hermes::vm::Handle<hermes::vm::JSObject>::vmcast(hermesValue);
    if (!canReceiveNativeState(jsObject.get())) {
        return nullptr;
    }

    auto result = hermes::vm::JSObject::getNamedOrIndexed(
        std::move(jsObject),
        *_runtime,
        hermes::vm::Predefined::getSymbolID(hermes::vm::Predefined::InternalPropertyNativeState));

    if (result.getStatus() == hermes::vm::ExecutionStatus::EXCEPTION) {
        _runtime->clearThrownValue();
        return nullptr;
    }

    if (!hermes::vm::vmisa<hermes::vm::NativeState>(result.getValue().getHermesValue())) {
        return nullptr;
    }

    auto* nativeState = hermes::vm::vmcast<hermes::vm::NativeState>(result.getValue().get());

    return unwrapNativeClassInstanceData(unsafeBridge<RefCountable>(nativeState->context()));
}

Ref<JSFunction> HermesJavaScriptContext::valueToFunction(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto* hermesValue = toPinnedHermesValue(value);
    if (!hermes::vm::vmisa<hermes::vm::FinalizableNativeFunction>(*hermesValue)) {
        return nullptr;
    }

    auto* functionContext = unsafeBridgeUnretained<RefCountable>(
        hermes::vm::vmcast<hermes::vm::FinalizableNativeFunction>(*hermesValue)->getContext());
    auto* functionData = dynamic_cast<JSFunctionData*>(functionContext);
    if (functionData == nullptr) {
        return nullptr;
    }

    return functionData->function;
}

static TypedArrayType getTypedArrayType(const hermes::vm::PinnedHermesValue& hermesValue) {
    if (hermes::vm::vmisa<hermes::vm::Uint8Array>(hermesValue)) {
        return Uint8Array;
    } else if (hermes::vm::vmisa<hermes::vm::Float32Array>(hermesValue)) {
        return Float32Array;
    } else if (hermes::vm::vmisa<hermes::vm::Float64Array>(hermesValue)) {
        return Float64Array;
    } else if (hermes::vm::vmisa<hermes::vm::Uint8ClampedArray>(hermesValue)) {
        return Uint8ClampedArray;
    } else if (hermes::vm::vmisa<hermes::vm::Int8Array>(hermesValue)) {
        return Int8Array;
    } else if (hermes::vm::vmisa<hermes::vm::Int16Array>(hermesValue)) {
        return Int16Array;
    } else if (hermes::vm::vmisa<hermes::vm::Int32Array>(hermesValue)) {
        return Int32Array;
    } else if (hermes::vm::vmisa<hermes::vm::Uint16Array>(hermesValue)) {
        return Uint16Array;
    } else if (hermes::vm::vmisa<hermes::vm::Uint32Array>(hermesValue)) {
        return Uint32Array;
    } else {
        SC_ABORT("Unrecognized TypedArray type");
        return Uint8Array;
    }
}

JSTypedArray HermesJavaScriptContext::valueToTypedArray(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    auto* hermesValue = toPinnedHermesValue(value);

    if (hermes::vm::vmisa<hermes::vm::JSArrayBuffer>(*hermesValue)) {
        auto handle = hermes::vm::Handle<hermes::vm::JSArrayBuffer>::vmcast(hermesValue);
        if (!handle->attached()) {
            return JSTypedArray();
        }

        auto* data = handle->getDataBlock(*_runtime);
        auto size = handle->size();

        return JSTypedArray(TypedArrayType::ArrayBuffer, data, size, JSValueRef::makeUnretained(*this, value));
    } else if (hermes::vm::vmisa<hermes::vm::JSTypedArrayBase>(*hermesValue)) {
        auto handle = hermes::vm::Handle<hermes::vm::JSTypedArrayBase>::vmcast(hermesValue);
        if (!handle->attached(*_runtime)) {
            return JSTypedArray();
        }

        auto type = getTypedArrayType(*hermesValue);
        auto* data = handle->begin(*_runtime);
        auto size = static_cast<size_t>(handle->getByteLength());
        auto* arrayBuffer = handle->getBuffer(*_runtime);

        return JSTypedArray(
            type, data, size, toJSValueRef(hermes::vm::HermesValue::encodeObjectValue(arrayBuffer, *_runtime)));
    } else {
        exceptionTracker.onError("Not a TypedArray or ArrayBuffer");
        return JSTypedArray();
    }
}

static bool hasNativeState(hermes::vm::Runtime& runtime, const hermes::vm::Handle<hermes::vm::JSObject>& jsObject) {
    if (!canReceiveNativeState(jsObject.get())) {
        return false;
    }

    auto callResult = hermes::vm::JSObject::hasNamed(
        jsObject, runtime, hermes::vm::Predefined::getSymbolID(hermes::vm::Predefined::InternalPropertyNativeState));
    if (callResult.getStatus() == hermes::vm::ExecutionStatus::EXCEPTION) {
        runtime.clearThrownValue();
        return false;
    }

    return callResult.getValue();
}

static bool isInstanceOf(hermes::vm::Runtime& runtime,
                         const hermes::vm::Handle<hermes::vm::JSObject>& jsObject,
                         const hermes::vm::PinnedHermesValue* ctor) {
    auto callResult = hermes::vm::instanceOfOperator_RJS(runtime, jsObject, runtime.makeHandle(*ctor));

    if (callResult.getStatus() == hermes::vm::ExecutionStatus::EXCEPTION) {
        runtime.clearThrownValue();
        return false;
    }

    return callResult.getValue();
}

ValueType HermesJavaScriptContext::getValueType(const JSValue& value) {
    auto* hermesValue = toPinnedHermesValue(value);
    if (hermesValue->isUndefined()) {
        return ValueType::Undefined;
    } else if (hermesValue->isNull()) {
        return ValueType::Null;
    } else if (hermesValue->isNumber()) {
        return ValueType::Double;
    } else if (hermesValue->isBool()) {
        return ValueType::Bool;
    } else if (hermesValue->isString()) {
        return ValueType::StaticString;
    } else if (hermesValue->isObject()) {
        hermes::vm::GCScope gcScope(*_runtime);

        if (hermes::vm::vmisa<hermes::vm::Callable>(*hermesValue)) {
            return ValueType::Function;
        }
        if (hermes::vm::vmisa<hermes::vm::JSArray>(*hermesValue)) {
            return ValueType::Array;
        }
        if (hermes::vm::vmisa<hermes::vm::JSArrayBuffer>(*hermesValue) ||
            hermes::vm::vmisa<hermes::vm::JSTypedArrayBase>(*hermesValue)) {
            return ValueType::TypedArray;
        }
        if (hermes::vm::vmisa<hermes::vm::JSError>(*hermesValue)) {
            return ValueType::Error;
        }

        auto jsObjectHandle = hermes::vm::Handle<hermes::vm::JSObject>::vmcast(hermesValue);

        if (hasNativeState(*_runtime, jsObjectHandle)) {
            return ValueType::ValdiObject;
        }

        if (!getLongConstructor().empty() &&
            isInstanceOf(*_runtime, jsObjectHandle, toPinnedHermesValue(getLongConstructor().get()))) {
            return Valdi::ValueType::Long;
        }

        return ValueType::Map;
    }

    return ValueType::Undefined;
}

bool HermesJavaScriptContext::isValueUndefined(const JSValue& value) {
    return toHermesValue(value).isUndefined();
}

bool HermesJavaScriptContext::isValueNull(const JSValue& value) {
    return toHermesValue(value).isNull();
}

bool HermesJavaScriptContext::isValueFunction(const JSValue& value) {
    const auto* hermesValue = toPinnedHermesValue(value);
    if (!hermesValue->isObject()) {
        return false;
    }
    return hermes::vm::vmisa<hermes::vm::Callable>(*hermesValue);
}

bool HermesJavaScriptContext::isValueNumber(const JSValue& value) {
    return toPinnedHermesValue(value)->isNumber();
}

bool HermesJavaScriptContext::isValueEqual(const JSValue& left, const JSValue& right) {
    // TODO(simon): Might be incorrect
    return toPinnedHermesValue(left)->getRaw() == toPinnedHermesValue(right)->getRaw();
}

bool HermesJavaScriptContext::isValueLong(const JSValue& value) {
    return false;
}

bool HermesJavaScriptContext::isValueObject(const JSValue& value) {
    return toPinnedHermesValue(value)->isObject();
}

void HermesJavaScriptContext::retainValue(const JSValue& value) {
    auto* managedValue = ManagedHermesValue::fromJSValue(value);
    if (managedValue != nullptr) {
        managedValue->retain();
    }
}

void HermesJavaScriptContext::releaseValue(const JSValue& value) {
    auto* managedValue = ManagedHermesValue::fromJSValue(value);
    if (managedValue != nullptr) {
        managedValue->release();
    }
}

void HermesJavaScriptContext::retainPropertyName(const JSPropertyName& value) {
    auto* managedValue = ManagedHermesValue::fromJSPropertyName(value);
    if (managedValue != nullptr) {
        managedValue->retain();
    }
}

void HermesJavaScriptContext::releasePropertyName(const JSPropertyName& value) {
    auto* managedValue = ManagedHermesValue::fromJSPropertyName(value);
    if (managedValue != nullptr) {
        managedValue->release();
    }
}

void HermesJavaScriptContext::garbageCollect() {
    _managedValues.collect();
    _runtime->collect("Explicit GC");
}

void HermesJavaScriptContext::enqueueMicrotask(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    hermes::vm::GCScope gcScope(*_runtime);
    const auto* hermesValue = toPinnedHermesValue(value);

    if (!hermes::vm::vmisa<hermes::vm::Callable>(*hermesValue)) {
        exceptionTracker.onError("Value is not a function");
        return;
    }

    auto callable = hermes::vm::Handle<hermes::vm::Callable>::vmcast(hermesValue);
    _runtime->enqueueJob(callable.get());
}

JavaScriptContextMemoryStatistics HermesJavaScriptContext::dumpMemoryStatistics() {
    JavaScriptContextMemoryStatistics out;
    hermes::vm::GCBase::HeapInfo info;

    _runtime->getHeap().getHeapInfo(info);

    out.memoryUsageBytes = static_cast<size_t>(info.allocatedBytes);

    return out;
}

BytesView HermesJavaScriptContext::dumpHeap(JSExceptionTracker& exceptionTracker) {
#ifdef HERMES_MEMORY_INSTRUMENTATION
    auto output = makeShared<ByteBuffer>();
    ByteBufferOStream stream(*output);
    _runtime->getHeap().createSnapshot(stream, true);
    // Flush stream
    return stream.toBytesView();
#else
    exceptionTracker.onError("Memory instrumentation not enabled");
    return BytesView();
#endif
}

void HermesJavaScriptContext::willEnterVM() {
    _enterVMCount++;
}

void HermesJavaScriptContext::willExitVM(JSExceptionTracker& exceptionTracker) {
    SC_ASSERT(_enterVMCount > 0);
    --_enterVMCount;
    if (_enterVMCount == 0) {
        auto result = _runtime->drainJobs();
        checkException(result, exceptionTracker);
    }
}

void HermesJavaScriptContext::startDebugger(bool isWorker) {
#ifdef HERMES_ENABLE_DEBUGGER
    auto* debuggerService = HermesDebuggerServer::getInstance(_logger);
    if (debuggerService != nullptr && _debuggerRuntimeId == kRuntimeIdUndefined) {
        auto registry = debuggerService->getRegistry();

        // TODO: This is just getting the first non-worker runtime.
        // We should have a better way to determine the parent runtime.
        auto parentRuntimeId = (isWorker) ? registry->getRuntimeIds()[0] : kRuntimeIdUndefined;
        _debuggerRuntimeId = registry->append(_jsiRuntime, makeRuntimeConfig(), getTaskScheduler(), parentRuntimeId);
    }
#endif
}

std::optional<IJavaScriptContextDebuggerInfo> HermesJavaScriptContext::getDebuggerInfo() const {
#ifdef HERMES_ENABLE_DEBUGGER
    auto* debuggerService = HermesDebuggerServer::getInstance(_logger);
    if (debuggerService == nullptr) {
        return std::nullopt;
    }
    return IJavaScriptContextDebuggerInfo{.debuggerPort = debuggerService->getPort(),
                                          .websocketTargets = debuggerService->getWebsocketTargets()};
#else
    return std::nullopt;
#endif
}

void HermesJavaScriptContext::startProfiling() {
#ifdef HERMES_ENABLE_DEBUGGER
    static const int kProfilerStartMessageId = -1;
    auto* debuggerService = HermesDebuggerServer::getInstance(_logger);
    if (debuggerService == nullptr || _debuggerRuntimeId == kRuntimeIdUndefined) {
        return;
    }
    debuggerService->getDebuggerConnection(_debuggerRuntimeId)
        ->handleCDPMessage(
            HermesDebuggerConnection::createEvent(STRING_LITERAL("Profiler.start"), kProfilerStartMessageId));
#endif
}

Result<std::vector<std::string>> HermesJavaScriptContext::stopProfiling() {
#ifdef HERMES_ENABLE_DEBUGGER
    static const int kProfilerStopMessageId = -2;
    std::vector<std::string> ret;
    auto* debuggerService = HermesDebuggerServer::getInstance(_logger);
    if (debuggerService == nullptr || _debuggerRuntimeId == kRuntimeIdUndefined) {
        return Error("Profiler uninitialized");
    }
    auto debuggerConnection = debuggerService->getDebuggerConnection(_debuggerRuntimeId);

    std::promise<std::string> profilingResultPromise;
    debuggerConnection->setCDPMessageListener([&](const Value& message) {
        if (message.getMapValue("id").toInt() == kProfilerStopMessageId) {
            profilingResultPromise.set_value(valueToJsonString(message.getMapValue("result").getMapValue("profile")));
            debuggerConnection->setCDPMessageListener(nullptr);
        }
    });
    debuggerConnection->handleCDPMessage(
        HermesDebuggerConnection::createEvent(STRING_LITERAL("Profiler.stop"), kProfilerStopMessageId));
    ret.push_back(profilingResultPromise.get_future().get());
    return ret;
#else
    return Error("Profiler disabled");
#endif
}

void HermesJavaScriptContext::setExceptionToTracker(JSExceptionTracker& exceptionTracker) {
    auto exception = _runtime->getThrownValue();
    _runtime->clearThrownValue();
    exceptionTracker.storeException(toJSValueRef(exception));
}

bool HermesJavaScriptContext::checkException(hermes::vm::ExecutionStatus status, JSExceptionTracker& exceptionTracker) {
    if (status == hermes::vm::ExecutionStatus::EXCEPTION) {
        setExceptionToTracker(exceptionTracker);
        return false;
    } else {
        return true;
    }
}

JSValueRef HermesJavaScriptContext::checkCallAndGetValue(
    const hermes::vm::CallResult<hermes::vm::HermesValue>& callResult, JSExceptionTracker& exceptionTracker) {
    if (checkException(callResult.getStatus(), exceptionTracker)) {
        return toJSValueRef(callResult.getValue());
    } else {
        return newUndefined();
    }
}

JSValueRef HermesJavaScriptContext::getConstructorFromObject(const JSValue& object,
                                                             const std::string_view& propertyName,
                                                             JSExceptionTracker& exceptionTracker) {
    auto ctor = IJavaScriptContext::getObjectProperty(object, propertyName, exceptionTracker);
    if (!exceptionTracker) {
        return ctor;
    }

    const auto* hermesValue = toPinnedHermesValue(ctor.get());
    if (hermesValue == nullptr || !hermesValue->isObject()) {
        exceptionTracker.onError("Could not resolve constructor");
        return newUndefined();
    }

    return ctor;
}

hermes::vm::Handle<::hermes::vm::JSObject> HermesJavaScriptContext::toJSObject(const JSValue& value,
                                                                               JSExceptionTracker& exceptionTracker) {
    const auto* hermesValue = toPinnedHermesValue(value);

    if (!hermesValue->isObject()) {
        exceptionTracker.onError("Value is not an object");
        return hermes::vm::Runtime::makeNullHandle<hermes::vm::JSObject>();
    }

    return hermes::vm::Handle<::hermes::vm::JSObject>::vmcast(hermesValue);
}

JSValueRef HermesJavaScriptContext::toJSValueRefUncached(const hermes::vm::HermesValue& value) {
    auto& managedValue = _managedValues.add(value);

    return JSValueRef(this, managedValue.toJSValue(), true);
}

JSValueRef HermesJavaScriptContext::toJSValueRef(const hermes::vm::HermesValue& value) {
    if (value.isUndefined() || value.isEmpty()) {
        return newUndefined();
    } else if (value.isNull()) {
        return newNull();
    } else if (value.isBool()) {
        return newBool(value.getBool());
    } else if (value.isNumber()) {
        return newNumber(value.getNumber());
    }

    return toJSValueRefUncached(value);
}

JSPropertyNameRef HermesJavaScriptContext::toPropertyNameRef(const hermes::vm::Handle<hermes::vm::SymbolID>& value) {
    auto& managedValue = _managedValues.add(value.getHermesValue());
    return JSPropertyNameRef(this, managedValue.toJSPropertyName(), true);
}

hermes::vm::Handle<hermes::vm::HermesValue> HermesJavaScriptContext::toHandle(const JSValue& value) const {
    return _runtime->makeHandle(toHermesValue(value));
}

hermes::vm::HermesValue HermesJavaScriptContext::toHermesValue(const JSValue& value) {
    auto* managedValue = ManagedHermesValue::fromJSValue(value);
    if (managedValue == nullptr) {
        return hermes::vm::HermesValue::encodeUndefinedValue();
    }

    return managedValue->get();
}

const hermes::vm::PinnedHermesValue* HermesJavaScriptContext::toPinnedHermesValue(const JSValue& value) {
    auto* managedValue = ManagedHermesValue::fromJSValue(value);
    if (managedValue == nullptr) {
        return _undefinedPinnedValue;
    }

    return &managedValue->get();
}

const hermes::vm::PinnedHermesValue* HermesJavaScriptContext::toPinnedHermesValue(const JSPropertyName& propertyName) {
    auto* managedValue = ManagedHermesValue::fromJSPropertyName(propertyName);
    return &managedValue->get();
}

hermes::vm::Handle<hermes::vm::SymbolID> HermesJavaScriptContext::toSymbolID(const JSPropertyName& propertyName) {
    return hermes::vm::Handle<hermes::vm::SymbolID>::vmcast(toPinnedHermesValue(propertyName));
}

} // namespace Valdi::Hermes

// NOLINTEND(cppcoreguidelines-pro-type-union-access)
