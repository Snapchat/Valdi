//
//  JavaScriptCoreContext.hpp
//  ValdiIOS
//
//  Created by Simon Corsin on 5/30/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include <JavaScriptCore/JavaScriptCore.h>
#include <deque>

namespace Valdi {
class ILogger;
}

namespace ValdiJSCore {

struct NativeClassFunctionData;

class JavaScriptCoreContext : public Valdi::IJavaScriptContext {
public:
    explicit JavaScriptCoreContext(Valdi::JavaScriptTaskScheduler* taskScheduler, Valdi::ILogger& logger);
    ~JavaScriptCoreContext() override;

    Valdi::JSValueRef getGlobalObject(Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::BytesView preCompile(const std::string_view& script,
                                const std::string_view& sourceFilename,
                                Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef evaluate(const std::string& script,
                               const std::string_view& sourceFilename,
                               Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef evaluatePreCompiled(const Valdi::BytesView& script,
                                          const std::string_view& sourceFilename,
                                          Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSPropertyNameRef newPropertyName(const std::string_view& str) override;

    Valdi::StringBox propertyNameToString(const Valdi::JSPropertyName& propertyName) override;

    Valdi::JSValueRef newObject(Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newFunction(const Valdi::Ref<Valdi::JSFunction>& callable,
                                  Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newStringUTF8(const std::string_view& str, Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newStringUTF16(const std::u16string_view& str,
                                     Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newArray(size_t initialSize, Valdi::JSExceptionTracker& exceptionTracker) override;
    Valdi::JSValueRef newArrayWithValues(const Valdi::JSValue* values,
                                         size_t size,
                                         Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newArrayBuffer(const Valdi::BytesView& buffer,
                                     Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newTypedArrayFromArrayBuffer(const Valdi::TypedArrayType& type,
                                                   const Valdi::JSValue& arrayBuffer,
                                                   Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newWrappedObject(const Valdi::Ref<Valdi::RefCountable>& wrappedObject,
                                       Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newNativeClass(const Valdi::Ref<Valdi::RefCountable>& classOpaque,
                                     const Valdi::JSClassDefinition& classDefinition,
                                     Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newObjectFromNativeClass(const Valdi::Ref<Valdi::RefCountable>& opaque,
                                               const Valdi::JSValue& cls,
                                               Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newWeakRef(const Valdi::JSValue& object, Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef getObjectProperty(const Valdi::JSValue& object,
                                        const std::string_view& propertyName,
                                        Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef getObjectProperty(const Valdi::JSValue& object,
                                        const Valdi::JSPropertyName& propertyName,
                                        Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef getObjectPropertyForIndex(const Valdi::JSValue& object,
                                                size_t index,
                                                Valdi::JSExceptionTracker& exceptionTracker) override;

    bool hasObjectProperty(const Valdi::JSValue& object, const Valdi::JSPropertyName& propertyName) final;

    void setObjectProperty(const Valdi::JSValue& object,
                           const std::string_view& propertyName,
                           const Valdi::JSValue& propertyValue,
                           bool enumerable,
                           Valdi::JSExceptionTracker& exceptionTracker) override;

    void setObjectProperty(const Valdi::JSValue& object,
                           const Valdi::JSPropertyName& propertyName,
                           const Valdi::JSValue& propertyValue,
                           bool enumerable,
                           Valdi::JSExceptionTracker& exceptionTracker) override;

    void setObjectProperty(const Valdi::JSValue& object,
                           const Valdi::JSValue& propertyName,
                           const Valdi::JSValue& propertyValue,
                           bool enumerable,
                           Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef callObjectAsFunction(const Valdi::JSValue& object,
                                           Valdi::JSFunctionCallContext& callContext) override;

    Valdi::JSValueRef callObjectAsConstructor(const Valdi::JSValue& object,
                                              Valdi::JSFunctionCallContext& callContext) override;

    void setObjectPropertyIndex(const Valdi::JSValue& object,
                                size_t index,
                                const Valdi::JSValue& value,
                                Valdi::JSExceptionTracker& exceptionTracker) override;

    void visitObjectPropertyNames(const Valdi::JSValue& object,
                                  Valdi::JSExceptionTracker& exceptionTracker,
                                  Valdi::IJavaScriptPropertyNamesVisitor& visitor) override;

    Valdi::JSValueRef derefWeakRef(const Valdi::JSValue& weakRef, Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::StringBox valueToString(const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::Ref<Valdi::StaticString> valueToStaticString(const Valdi::JSValue& value,
                                                        Valdi::JSExceptionTracker& exceptionTracker) override;

    bool valueToBool(const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) override;

    double valueToDouble(const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) override;

    int32_t valueToInt(const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::Ref<Valdi::RefCountable> valueToWrappedObject(const Valdi::JSValue& value,
                                                         Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::Ref<Valdi::JSFunction> valueToFunction(const Valdi::JSValue& value,
                                                  Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSTypedArray valueToTypedArray(const Valdi::JSValue& value,
                                          Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::ValueType getValueType(const Valdi::JSValue& value) override;

    void enqueueMicrotask(const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) override;

    bool isValueUndefined(const Valdi::JSValue& value) override;
    bool isValueNull(const Valdi::JSValue& value) override;
    bool isValueFunction(const Valdi::JSValue& value) override;
    bool isValueNumber(const Valdi::JSValue& value) override;
    bool isValueLong(const Valdi::JSValue& value) override;
    bool isValueObject(const Valdi::JSValue& value) override;

    bool isValueEqual(const Valdi::JSValue& left, const Valdi::JSValue& right) override;

    void retainValue(const Valdi::JSValue& value) override;
    void releaseValue(const Valdi::JSValue& value) override;

    void retainPropertyName(const Valdi::JSPropertyName& value) override;
    void releasePropertyName(const Valdi::JSPropertyName& value) override;

    void garbageCollect() override;

    Valdi::JavaScriptContextMemoryStatistics dumpMemoryStatistics() override;
    static Valdi::JavaScriptContextMemoryStatistics (*s_dumpMemoryStatisticsOverride)(JSGlobalContextRef);

    void startDebugger(bool isWorker) override;

    void willEnterVM() override;
    void willExitVM(Valdi::JSExceptionTracker& exceptionTracker) override;
    void requestExecutionTermination() override;

    const Valdi::JSPropertyName& getPrototypePropertyName() const;

    const Valdi::JSPropertyName& getPrototypePropertyName() const;

protected:
    Valdi::JSValueRef onNewBool(bool boolean) override;

    Valdi::JSValueRef onNewNumber(int32_t number) override;

    Valdi::JSValueRef onNewNumber(double number) override;

    Valdi::JSValueRef onNewNull() override;

    Valdi::JSValueRef onNewUndefined() override;

    void onInitialize(Valdi::JSExceptionTracker& exceptionTracker) override;

private:
    [[maybe_unused]] Valdi::ILogger& _logger;
    JSContextGroupRef _contextGroup;
    JSGlobalContextRef _globalContext;
    Valdi::JSPropertyNameRef _prototypePropertyName;
    JSValueRef _functionPrototype = nullptr;
    JSObjectRef _errorConstructor = nullptr;
    JSObjectRef _emptyArrayBuffer = nullptr;
    JSObjectRef _weakRefConstructor = nullptr;
    JSObjectRef _weakRefDerefFunction = nullptr;
    Valdi::JSValueRef _nativeClassDefineAccessor;
    bool _needGarbageCollect = false;
    bool _isGarbageCollecting = false;
    [[maybe_unused]] uint16_t _debuggerPort = 0;
    size_t _enterVmCount = 0;
    std::deque<JSObjectRef> _microtasks;

    Valdi::JSValueRef checkCallAndGetValue(Valdi::JSExceptionTracker& exceptionTracker,
                                           const JSValueRef& value,
                                           const JSValueRef& exception);

    inline JSContextRef getJSGlobalContext() const;

    inline Valdi::JSValueRef toUnretainedJSValueRef(const JSValueRef& value, JSType type);
    inline Valdi::JSValueRef toRetainedJSValueRef(const JSValueRef& value, JSType type);
    Valdi::JSValueRef returnJSValueRef(const JSValueRef& value, JSType type);

    void storeException(Valdi::JSExceptionTracker& exceptionTracker, const JSValueRef& exception);

    Valdi::JSValueRef newNativeClassFunction(const Valdi::Ref<NativeClassFunctionData>& functionData,
                                             JSClassRef classRef,
                                             std::string_view name,
                                             Valdi::JSExceptionTracker& exceptionTracker);

    static JSClassRef getNativeClassConstructorClassRef();
};

} // namespace ValdiJSCore
