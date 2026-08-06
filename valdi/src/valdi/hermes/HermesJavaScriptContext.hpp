//
//  HermesJavaScriptContext.hpp
//  valdi-hermes
//
//  Created by Simon Corsin on 9/27/23.
//

#pragma once

#include "valdi/hermes/Hermes.hpp"
#include "valdi/hermes/HermesDebuggerRegistry.hpp"
#include "valdi/hermes/HermesUtils.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"

namespace facebook::hermes {
class HermesRuntime;
}

namespace Valdi {
class ILogger;
}

namespace Valdi::Hermes {

class HermesJavaScriptContext : public IJavaScriptContext {
public:
    explicit HermesJavaScriptContext(JavaScriptTaskScheduler* taskScheduler, ILogger& logger);
    ~HermesJavaScriptContext() override;

    JSValueRef getGlobalObject(JSExceptionTracker& exceptionTracker) final;

    BytesView preCompile(const std::string_view& script,
                         const std::string_view& sourceFilename,
                         JSExceptionTracker& exceptionTracker) final;

    bool supportsPreCompilation() const final;

    JSValueRef evaluate(const std::string& script,
                        const std::string_view& sourceFilename,
                        JSExceptionTracker& exceptionTracker) final;

    JSValueRef evaluatePreCompiled(const BytesView& script,
                                   const std::string_view& sourceFilename,
                                   JSExceptionTracker& exceptionTracker) final;

    JSPropertyNameRef newPropertyName(const std::string_view& str) final;

    StringBox propertyNameToString(const JSPropertyName& propertyName) final;

    JSValueRef newObject(JSExceptionTracker& exceptionTracker) final;

    JSValueRef newFunction(const Ref<JSFunction>& callable, JSExceptionTracker& exceptionTracker) final;

    JSValueRef newStringUTF8(const std::string_view& str, JSExceptionTracker& exceptionTracker) final;

    JSValueRef newStringUTF16(const std::u16string_view& str, JSExceptionTracker& exceptionTracker) final;

    JSValueRef newArray(size_t initialSize, JSExceptionTracker& exceptionTracker) final;

    JSValueRef newArrayBuffer(const BytesView& buffer, JSExceptionTracker& exceptionTracker) final;

    JSValueRef newTypedArrayFromArrayBuffer(const TypedArrayType& type,
                                            const JSValue& arrayBuffer,
                                            JSExceptionTracker& exceptionTracker) final;

    JSValueRef newWrappedObject(const Ref<RefCountable>& wrappedObject, JSExceptionTracker& exceptionTracker) final;

    JSValueRef newNativeClass(const Ref<RefCountable>& classOpaque,
                              const JSClassDefinition& classDefinition,
                              JSExceptionTracker& exceptionTracker) final;

    JSValueRef newObjectFromNativeClass(const Ref<RefCountable>& opaque,
                                        const JSValue& cls,
                                        JSExceptionTracker& exceptionTracker) final;

    JSValueRef newWeakRef(const JSValue& object, JSExceptionTracker& exceptionTracker) final;

    JSValueRef getObjectProperty(const JSValue& object,
                                 const JSPropertyName& propertyName,
                                 JSExceptionTracker& exceptionTracker) final;

    JSValueRef getObjectPropertyForIndex(const JSValue& object,
                                         size_t index,
                                         JSExceptionTracker& exceptionTracker) final;

    bool hasObjectProperty(const JSValue& object, const JSPropertyName& propertyName) final;

    void setObjectProperty(const JSValue& object,
                           const JSPropertyName& propertyName,
                           const JSValue& propertyValue,
                           bool enumerable,
                           JSExceptionTracker& exceptionTracker) final;

    void setObjectProperty(const JSValue& object,
                           const JSValue& propertyName,
                           const JSValue& propertyValue,
                           bool enumerable,
                           JSExceptionTracker& exceptionTracker) final;

    JSValueRef callObjectAsFunction(const JSValue& object, JSFunctionCallContext& callContext) final;

    JSValueRef callObjectAsConstructor(const JSValue& object, JSFunctionCallContext& callContext) final;

    void setObjectPropertyIndex(const JSValue& object,
                                size_t index,
                                const JSValue& value,
                                JSExceptionTracker& exceptionTracker) final;

    void visitObjectPropertyNames(const JSValue& object,
                                  JSExceptionTracker& exceptionTracker,
                                  IJavaScriptPropertyNamesVisitor& visitor) final;

    JSValueRef derefWeakRef(const JSValue& weakRef, JSExceptionTracker& exceptionTracker) final;

    StringBox valueToString(const JSValue& value, JSExceptionTracker& exceptionTracker) final;

    Ref<StaticString> valueToStaticString(const JSValue& value, JSExceptionTracker& exceptionTracker) final;

    bool valueToBool(const JSValue& value, JSExceptionTracker& exceptionTracker) final;

    double valueToDouble(const JSValue& value, JSExceptionTracker& exceptionTracker) final;

    int32_t valueToInt(const JSValue& value, JSExceptionTracker& exceptionTracker) final;

    Ref<RefCountable> valueToWrappedObject(const JSValue& value, JSExceptionTracker& exceptionTracker) final;

    Ref<JSFunction> valueToFunction(const JSValue& value, JSExceptionTracker& exceptionTracker) final;

    JSTypedArray valueToTypedArray(const JSValue& value, JSExceptionTracker& exceptionTracker) final;

    ValueType getValueType(const JSValue& value) final;

    bool isValueUndefined(const JSValue& value) final;
    bool isValueNull(const JSValue& value) final;
    bool isValueFunction(const JSValue& value) final;
    bool isValueNumber(const JSValue& value) final;
    bool isValueLong(const JSValue& value) final;
    bool isValueObject(const JSValue& value) final;

    bool isValueEqual(const JSValue& left, const JSValue& right) final;

    void retainValue(const JSValue& value) final;
    void releaseValue(const JSValue& value) final;

    void retainPropertyName(const JSPropertyName& value) final;
    void releasePropertyName(const JSPropertyName& value) final;

    void garbageCollect() final;
    JavaScriptContextMemoryStatistics dumpMemoryStatistics() final;
    BytesView dumpHeap(JSExceptionTracker& exceptionTracker);

    void enqueueMicrotask(const JSValue& value, JSExceptionTracker& exceptionTracker) final;

    void willEnterVM() final;
    void willExitVM(JSExceptionTracker& exceptionTracker) final;

    void startDebugger(bool isWorker) final;
    std::optional<IJavaScriptContextDebuggerInfo> getDebuggerInfo() const final;

    void startProfiling() final;
    Result<std::vector<std::string>> stopProfiling() final;

    JSValueRef toJSValueRef(const hermes::vm::HermesValue& value);
    static hermes::vm::HermesValue toHermesValue(const JSValue& value);

protected:
    JSValueRef onNewBool(bool boolean) final;

    JSValueRef onNewNumber(int32_t number) final;

    JSValueRef onNewNumber(double number) final;

    JSValueRef onNewNull() final;

    JSValueRef onNewUndefined() final;

    void onInitialize(JSExceptionTracker& exceptionTracker) final;

private:
    [[maybe_unused]] ILogger& _logger;
    [[maybe_unused]] std::shared_ptr<facebook::hermes::HermesRuntime> _jsiRuntime;
    std::shared_ptr<hermes::vm::Runtime> _sharedRuntime;
    hermes::vm::Runtime* _runtime = nullptr;
    hermes::ManagedChunkedList<ManagedHermesValue> _managedValues;
    JSValueRef _emptyArrayBuffer;
    JSValueRef _int8ArrayCtor;
    JSValueRef _int16ArrayCtor;
    JSValueRef _int32ArrayCtor;
    JSValueRef _uint8ArrayCtor;
    JSValueRef _uint8ClampedArrayCtor;
    JSValueRef _uint16ArrayCtor;
    JSValueRef _uint32ArrayCtor;
    JSValueRef _float32ArrayCtor;
    JSValueRef _float64ArrayCtor;
    size_t _enterVMCount = 0;
    [[maybe_unused]] HermesRuntimeId _debuggerRuntimeId = kRuntimeIdUndefined;

    const hermes::vm::PinnedHermesValue* _undefinedPinnedValue = nullptr;

    bool checkException(hermes::vm::ExecutionStatus status, JSExceptionTracker& exceptionTracker);
    JSValueRef toJSValueRefUncached(const hermes::vm::HermesValue& value);
    JSPropertyNameRef toPropertyNameRef(const hermes::vm::Handle<hermes::vm::SymbolID>& value);

    JSValueRef checkCallAndGetValue(const hermes::vm::CallResult<hermes::vm::HermesValue>& callResult,
                                    JSExceptionTracker& exceptionTracker);

    hermes::vm::Handle<hermes::vm::HermesValue> toHandle(const JSValue& value) const;

    hermes::vm::Handle<::hermes::vm::JSObject> toJSObject(const JSValue& value, JSExceptionTracker& exceptionTracker);

    static hermes::vm::Handle<hermes::vm::SymbolID> toSymbolID(const JSPropertyName& propertyName);
    hermes::vm::Handle<hermes::vm::SymbolID> newSymbolIDFromString(const std::string_view& str);

    const hermes::vm::PinnedHermesValue* toPinnedHermesValue(const JSValue& value);
    static const hermes::vm::PinnedHermesValue* toPinnedHermesValue(const JSPropertyName& propertyName);

    hermes::vm::CallResult<hermes::vm::PseudoHandle<hermes::vm::HermesValue>> doCall(
        hermes::vm::ScopedNativeCallFrame& callFrame,
        const hermes::vm::Handle<hermes::vm::Callable>& handle,
        JSFunctionCallContext& callContext);

    void setExceptionToTracker(JSExceptionTracker& exceptionTracker);

    JSValueRef newTypedArrayFromConstructor(const JSValueRef& ctor,
                                            const JSValue& arrayBuffer,
                                            JSExceptionTracker& exceptionTracker);

    JSValueRef getConstructorFromObject(const JSValue& object,
                                        const std::string_view& propertyName,
                                        JSExceptionTracker& exceptionTracker);

    void initializePromiseRejectionTracker(JSExceptionTracker& exceptionTracker);
};

} // namespace Valdi::Hermes
