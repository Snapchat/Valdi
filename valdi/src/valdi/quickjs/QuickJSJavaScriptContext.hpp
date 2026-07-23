//
//  QuickJSJavaScriptContext.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/17/19.
//

#pragma once

#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi_core/cpp/Threading/ThreadAccessChecker.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <deque>
#include <mutex>
#include <quickjs/quickjs.h>

namespace Valdi {
class JavaScriptHeapDumpBuilder;
}

namespace ValdiQuickJS {

class QuickJSValue;
class QuickJSValueGlobalRefManager;
class WeakReference;

class QuickJSJavaScriptContextEntry;
struct JSClassDefWithId;
struct NativeClassFunctionData;

struct QuickJSRejectedPromise {
    JSValue promise;
    JSValue reason;
};

class QuickJSJavaScriptContext : public Valdi::IJavaScriptContext {
public:
    explicit QuickJSJavaScriptContext(Valdi::JavaScriptTaskScheduler* taskScheduler);
    ~QuickJSJavaScriptContext() override;

    Valdi::JSValueRef getGlobalObject(Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::BytesView preCompile(const std::string_view& script,
                                const std::string_view& sourceFilename,
                                Valdi::JSExceptionTracker& exceptionTracker) override;

    bool supportsPreCompilation() const override;

    Valdi::JSValueRef evaluate(const std::string& script,
                               const std::string_view& sourceFilename,
                               Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef evaluatePreCompiled(const Valdi::BytesView& script,
                                          const std::string_view& sourceFilename,
                                          Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef evaluateNative(const std::string_view& sourceFilename,
                                     Valdi::JSExceptionTracker& exceptionTracker) final;

    std::optional<Valdi::IJavaScriptNativeModuleInfo> getNativeModuleInfo(const std::string_view& sourceFilename) final;

    Valdi::JSPropertyNameRef newPropertyName(const std::string_view& str) override;

    Valdi::StringBox propertyNameToString(const Valdi::JSPropertyName& propertyName) override;

    Valdi::JSValueRef newObject(Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newFunction(const Valdi::Ref<Valdi::JSFunction>& callable,
                                  Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newStringUTF8(const std::string_view& str, Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newStringUTF16(const std::u16string_view& str,
                                     Valdi::JSExceptionTracker& exceptionTracker) override;

    Valdi::JSValueRef newArray(size_t initialSize, Valdi::JSExceptionTracker& exceptionTracker) override;

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

    void dumpHeap(Valdi::JavaScriptHeapDumpBuilder& heapDumpBuilder);

    void enqueueMicrotask(const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) override;

    JSContext* getContext() const;

    void appendRejectedPromise(JSValueConst promise, JSValueConst reason);
    void removeRejectedPromise(JSValueConst promise);

    void removeWeakReference(size_t weakReferenceId);

    void willEnterVM() override;
    void willExitVM(Valdi::JSExceptionTracker& exceptionTracker) override;
    void requestExecutionTermination() override;

protected:
    Valdi::JSValueRef onNewBool(bool boolean) override;

    Valdi::JSValueRef onNewNumber(int32_t number) override;

    Valdi::JSValueRef onNewNumber(double number) override;

    Valdi::JSValueRef onNewNull() override;

    Valdi::JSValueRef onNewUndefined() override;

    void onInitialize(Valdi::JSExceptionTracker& exceptionTracker) override;

private:
    friend QuickJSJavaScriptContextEntry;

    Valdi::ThreadAccessChecker _threadAccessChecker;

    JSRuntime* _runtime = nullptr;
    JSContext* _context = nullptr;

    JSValue _functionPrototype = JS_UNDEFINED;
    JSValue _objectPrototype = JS_UNDEFINED;
    JSValue _emptyArrayBuffer = JS_UNDEFINED;
    JSValue _int8ArrayCtor = JS_UNDEFINED;
    JSValue _int16ArrayCtor = JS_UNDEFINED;
    JSValue _int32ArrayCtor = JS_UNDEFINED;
    JSValue _uint8ArrayCtor = JS_UNDEFINED;
    JSValue _uint8ClampedArrayCtor = JS_UNDEFINED;
    JSValue _uint16ArrayCtor = JS_UNDEFINED;
    JSValue _uint32ArrayCtor = JS_UNDEFINED;
    JSValue _float32ArrayCtor = JS_UNDEFINED;
    JSValue _float64ArrayCtor = JS_UNDEFINED;
    JSAtom _objectFinalizerAtomKey = JS_ATOM_NULL;

    JSClassID _functionClassID = 0;
    JSClassID _wrappedObjectClassID = 0;
    JSClassID _weakReferenceFinalizerClassID = 0;
    JSClassID _nativeClassConstructorClassID = 0;
    size_t _enterVmCount = 0;
    size_t _weakReferenceSequence = 0;
    bool _needsGarbageCollect = false;
    std::thread::id _lastThreadId;

    std::deque<QuickJSRejectedPromise> _rejectedPromises;
    Valdi::FlatMap<size_t, JSValue> _weakReferences;

    size_t associateWeakReference(const JSValue& value, Valdi::JSExceptionTracker& exceptionTracker);

    void setObjectPropertyNonEumerable(const Valdi::JSValue& object,
                                       const Valdi::JSPropertyName& propertyName,
                                       const JSValue& propertyValue,
                                       Valdi::JSExceptionTracker& exceptionTracker);

    Valdi::JSValueRef checkCallAndGetValue(Valdi::JSExceptionTracker& exceptionTracker, const JSValue& value);
    bool checkCall(Valdi::JSExceptionTracker& exceptionTracker, int retValue);
    void setExceptionToTracker(Valdi::JSExceptionTracker& exceptionTracker);

    void notifyRejectedPromises();

    inline Valdi::JSValueRef toRetainedJSValueRef(const JSValue& value);
    inline Valdi::JSValueRef toUnretainedJSValueRef(const JSValue& value);

    JSClassID initializeClass(const JSClassDefWithId* classDef, Valdi::JSExceptionTracker& exceptionTracker);

    void updateCurrentStackLimit();

    JSValue getConstructorFromObject(const Valdi::JSValue& object,
                                     const std::string_view& propertyName,
                                     Valdi::JSExceptionTracker& exceptionTracker);

    Valdi::JSValueRef newTypedArrayFromConstructor(const JSValue& ctor,
                                                   const Valdi::JSValue& arrayBuffer,
                                                   Valdi::JSExceptionTracker& exceptionTracker);

    Valdi::JSValueRef newNativeClassFunction(const Valdi::Ref<NativeClassFunctionData>& functionData,
                                             const JSClassDefWithId* classDef,
                                             std::string_view name,
                                             Valdi::JSExceptionTracker& exceptionTracker);
};

} // namespace ValdiQuickJS
