//
//  IJavaScriptContext.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/27/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "utils/base/NonCopyable.hpp"
#include "utils/platform/BuildOptions.hpp"
#include "valdi/runtime/JavaScript/JSFunctionExportMode.hpp"
#include "valdi/runtime/JavaScript/JSClassDefinition.hpp"
#include "valdi/runtime/JavaScript/JavaScriptLong.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTypes.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/SequenceIDGenerator.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace Valdi {

class StringCache;
class StaticString;
class JavaScriptLong;

class JSFunctionCallContext;
class MainThreadManager;
class IJavaScriptContext;
class JavaScriptTaskScheduler;
class JavaScriptValueMarshaller;
class IDiskCache;

constexpr bool shouldEnableJsHeapDump() {
    return snap::kIsDevBuild || snap::kIsGoldBuild;
}

class IJavaScriptContextListener {
public:
    IJavaScriptContextListener() = default;
    virtual ~IJavaScriptContextListener() = default;

    virtual JSValueRef symbolicateError(const JSValueRef& jsError) = 0;

    virtual Ref<JSStackTraceProvider> captureCurrentStackTrace() = 0;

    // This method can be called in an arbitrary and must be thread safe.
    virtual MainThreadManager* getMainThreadManager() const = 0;

    virtual void onUnhandledRejectedPromise(IJavaScriptContext& jsContext, const JSValue& promiseResult) = 0;

    virtual void onInterrupt(IJavaScriptContext& jsContext) = 0;
};

class IJavaScriptPropertyNamesVisitor {
public:
    virtual ~IJavaScriptPropertyNamesVisitor() = default;

    virtual bool visitPropertyName(IJavaScriptContext& context,
                                   JSValue object,
                                   const JSPropertyName& propertyName,
                                   JSExceptionTracker& exceptionTracker) = 0;
};

struct JavaScriptContextMemoryStatistics {
    size_t memoryUsageBytes = 0;
    size_t objectsCount = 0;
};

struct IJavaScriptContextConfig {
    bool disableUTF16 = false;
    bool disableValueMarshaller = false;
    bool disableProxyObjectStore = false;
};

struct IJavaScriptContextDebuggerInfo {
    uint16_t debuggerPort = 0;
    std::vector<StringBox> websocketTargets = {};
};

struct IJavaScriptNativeModuleInfo {
    StringBox name;
    StringBox sha256;
};

using JSValueID = SequenceID;

/// @brief Base class for all Valdi's JavaScript contexts.
/// @details A context provides a common interface across different JavaScript engines.
class IJavaScriptContext : public SharedPtrRefCountable, public snap::NonCopyable {
public:
    explicit IJavaScriptContext(JavaScriptTaskScheduler* taskScheduler);
    ~IJavaScriptContext() override;

    void initialize(const IJavaScriptContextConfig& config, JSExceptionTracker& exceptionTracker);

    JavaScriptTaskScheduler* getTaskScheduler() const;

    virtual JSValueRef getGlobalObject(JSExceptionTracker& exceptionTracker) = 0;

    virtual JSValueRef evaluate(const std::string& script,
                                const std::string_view& sourceFilename,
                                JSExceptionTracker& exceptionTracker) = 0;

    virtual JSValueRef evaluatePreCompiled(const BytesView& /*bytes*/,
                                           const std::string_view& sourceFilename,
                                           JSExceptionTracker& exceptionTracker) = 0;

    /**
     * Evaluate the given module natively. The module should have been compiled ahead of time
     * into the binary for this function to work.
     */
    virtual JSValueRef evaluateNative(const std::string_view& sourceFilename, JSExceptionTracker& exceptionTracker);

    /**
     * Return the information about the given native module, or return nullopt if the module does not
     * exist.
     */
    virtual std::optional<IJavaScriptNativeModuleInfo> getNativeModuleInfo(const std::string_view& sourceFilename);

    virtual bool supportsPreCompilation() const {
        return false;
    }

    virtual BytesView preCompile(const std::string_view& /*script*/,
                                 const std::string_view& /*sourceFilename*/,
                                 JSExceptionTracker& exceptionTracker) = 0;

    virtual JSPropertyNameRef newPropertyName(const std::string_view& str) = 0;

    virtual StringBox propertyNameToString(const JSPropertyName& propertyName) = 0;

    virtual JSValueRef newObject(JSExceptionTracker& exceptionTracker) = 0;

    virtual JSValueRef newFunction(const Ref<JSFunction>& callable, JSExceptionTracker& exceptionTracker) = 0;

    JSValueRef newBool(bool boolean);

    JSValueRef newNumber(int32_t number);

    JSValueRef newNumber(double number);

    JSValueRef newNull();

    JSValueRef newUndefined();

    virtual JSValueRef newStringUTF8(const std::string_view& str, JSExceptionTracker& exceptionTracker) = 0;
    virtual JSValueRef newStringUTF16(const std::u16string_view& str, JSExceptionTracker& exceptionTracker) = 0;

    JSValueRef newString(const StaticString& str, JSExceptionTracker& exceptionTracker);

    virtual JSValueRef newArray(size_t initialSize, JSExceptionTracker& exceptionTracker) = 0;
    virtual JSValueRef newArrayWithValues(const JSValue* values, size_t size, JSExceptionTracker& exceptionTracker);

    virtual JSValueRef newArrayBuffer(const BytesView& buffer, JSExceptionTracker& exceptionTracker) = 0;

    virtual JSValueRef newArrayBufferCopy(const Byte* data, size_t size, JSExceptionTracker& exceptionTracker);

    virtual JSValueRef newTypedArrayFromArrayBuffer(const TypedArrayType& type,
                                                    const JSValue& arrayBuffer,
                                                    JSExceptionTracker& exceptionTracker) = 0;

    virtual JSValueRef newWrappedObject(const Ref<RefCountable>& wrappedObject,
                                        JSExceptionTracker& exceptionTracker) = 0;

    virtual JSValueRef newNativeClass(const Ref<RefCountable>& classOpaque,
                                      const JSClassDefinition& classDefinition,
                                      JSExceptionTracker& exceptionTracker) = 0;

    virtual JSValueRef newObjectFromNativeClass(const Ref<RefCountable>& opaque,
                                                const JSValue& cls,
                                                JSExceptionTracker& exceptionTracker) = 0;

    virtual JSValueRef newWeakRef(const JSValue& object, JSExceptionTracker& exceptionTracker) = 0;

    JSValueRef newLong(int64_t value, JSExceptionTracker& exceptionTracker);

    JSValueRef newUnsignedLong(uint64_t value, JSExceptionTracker& exceptionTracker);

    virtual JSValueRef getObjectProperty(const JSValue& object,
                                         const std::string_view& propertyName,
                                         JSExceptionTracker& exceptionTracker);

    virtual JSValueRef getObjectProperty(const JSValue& object,
                                         const JSPropertyName& propertyName,
                                         JSExceptionTracker& exceptionTracker) = 0;

    virtual JSValueRef getObjectPropertyForIndex(const JSValue& object,
                                                 size_t index,
                                                 JSExceptionTracker& exceptionTracker) = 0;

    virtual bool hasObjectProperty(const JSValue& object, const JSPropertyName& propertyName) = 0;

    void setObjectProperty(const JSValue& object,
                           const std::string_view& propertyName,
                           const JSValue& propertyValue,
                           JSExceptionTracker& exceptionTracker);

    virtual void setObjectProperty(const JSValue& object,
                                   const std::string_view& propertyName,
                                   const JSValue& propertyValue,
                                   bool enumerable,
                                   JSExceptionTracker& exceptionTracker);

    void setObjectProperty(const JSValue& object,
                           const JSPropertyName& propertyName,
                           const JSValue& propertyValue,
                           JSExceptionTracker& exceptionTracker);

    virtual void setObjectProperty(const JSValue& object,
                                   const JSPropertyName& propertyName,
                                   const JSValue& propertyValue,
                                   bool enumerable,
                                   JSExceptionTracker& exceptionTracker) = 0;

    void setObjectProperty(const JSValue& object,
                           const JSValue& propertyName,
                           const JSValue& propertyValue,
                           JSExceptionTracker& exceptionTracker);

    virtual void setObjectProperty(const JSValue& object,
                                   const JSValue& propertyName,
                                   const JSValue& propertyValue,
                                   bool enumerable,
                                   JSExceptionTracker& exceptionTracker) = 0;

    virtual void setObjectPropertyIndex(const JSValue& object,
                                        size_t index,
                                        const JSValue& value,
                                        JSExceptionTracker& exceptionTracker) = 0;

    virtual JSValueRef callObjectAsFunction(const JSValue& object, JSFunctionCallContext& callContext) = 0;

    virtual JSValueRef callObjectAsConstructor(const JSValue& object, JSFunctionCallContext& callContext) = 0;

    virtual void visitObjectPropertyNames(const JSValue& object,
                                          JSExceptionTracker& exceptionTracker,
                                          IJavaScriptPropertyNamesVisitor& visitor) = 0;

    virtual JSValueRef derefWeakRef(const JSValue& weakRef, JSExceptionTracker& exceptionTracker) = 0;

    virtual StringBox valueToString(const JSValue& value, JSExceptionTracker& exceptionTracker) = 0;
    virtual Ref<StaticString> valueToStaticString(const JSValue& value, JSExceptionTracker& exceptionTracker) = 0;
    virtual bool valueToBool(const JSValue& value, JSExceptionTracker& exceptionTracker) = 0;
    virtual double valueToDouble(const JSValue& value, JSExceptionTracker& exceptionTracker) = 0;
    virtual int32_t valueToInt(const JSValue& value, JSExceptionTracker& exceptionTracker) = 0;
    virtual Ref<RefCountable> valueToWrappedObject(const JSValue& value, JSExceptionTracker& exceptionTracker) = 0;
    virtual JSTypedArray valueToTypedArray(const JSValue& value, JSExceptionTracker& exceptionTracker) = 0;
    virtual Ref<JSFunction> valueToFunction(const JSValue& value, JSExceptionTracker& exceptionTracker) = 0;
    JavaScriptLong valueToLong(const JSValue& value, JSExceptionTracker& exceptionTracker);

    JSFunctionExportMode getValueFunctionExportMode(const JSValue& function, JSExceptionTracker& exceptionTracker);
    void setValueFunctionExportMode(const JSValue& function,
                                    JSFunctionExportMode exportMode,
                                    JSExceptionTracker& exceptionTracker);

    virtual ValueType getValueType(const JSValue& value) = 0;

    virtual void enqueueMicrotask(const JSValue& value, JSExceptionTracker& exceptionTracker) = 0;

    virtual bool isValueUndefined(const JSValue& value) = 0;
    virtual bool isValueNull(const JSValue& value) = 0;
    virtual bool isValueFunction(const JSValue& value) = 0;
    virtual bool isValueNumber(const JSValue& value) = 0;
    virtual bool isValueLong(const JSValue& value) = 0;
    virtual bool isValueObject(const JSValue& value) = 0;

    virtual bool isValueEqual(const JSValue& left, const JSValue& right) = 0;

    virtual void retainValue(const JSValue& value) = 0;
    virtual void releaseValue(const JSValue& value) = 0;

    virtual void retainPropertyName(const JSPropertyName& value) = 0;
    virtual void releasePropertyName(const JSPropertyName& value) = 0;

    virtual void garbageCollect() = 0;

    virtual JavaScriptContextMemoryStatistics dumpMemoryStatistics() = 0;

    virtual void startDebugger(bool isWorker) {}

    virtual std::optional<IJavaScriptContextDebuggerInfo> getDebuggerInfo() const {
        return std::nullopt;
    }

    virtual void startProfiling() {}

    virtual Result<std::vector<std::string>> stopProfiling() {
        return Error("Profiler Unimplemented");
    }

    virtual void setBytecodeDiskCache(const Ref<IDiskCache>& diskCache) {}

    IJavaScriptContextListener* getListener() const;

    void setListener(IJavaScriptContextListener* listener);

    template<typename F>
    JSValueRef newArrayWithValues(size_t size, JSExceptionTracker& exceptionTracker, F&& cb) {
        auto array = newArray(size, exceptionTracker);
        if (!exceptionTracker) {
            return newUndefined();
        }

        for (size_t i = 0; i < size; i++) {
            auto value = cb(i);
            if (!exceptionTracker) {
                return newUndefined();
            }

            setObjectPropertyIndex(array.get(), i, value.get(), exceptionTracker);
            if (!exceptionTracker) {
                return newUndefined();
            }
        }

        return array;
    }

    JSValueRef newPromise(const JSValueRef& executor, JSExceptionTracker& exceptionTracker);

    JSValueRef callObjectProperty(const JSValue& object,
                                  const JSPropertyName& propertyName,
                                  JSFunctionCallContext& callContext);

    JSValueRef callObjectProperty(const JSValue& object,
                                  const std::string_view& propertyName,
                                  JSFunctionCallContext& callContext);

    JSValueRef newError(std::string_view message,
                        std::optional<std::string_view> stack,
                        JSExceptionTracker& exceptionTracker);

    JSPropertyName getPropertyNameCached(const StringBox& str);
    void clearPropertyNameCache();

    JSValueRef getPropertyFromGlobalObjectCached(const StringBox& str, JSExceptionTracker& exceptionTracker);
    void clearGlobalObjectPropertyCache();

    void setLongConstructor(const JSValueRef& longConstructor);
    const JSValueRef& getLongConstructor() const;

    JSValueRef ensureRetainedValue(JSValueRef value);

    JavaScriptValueMarshaller* getValueMarshaller() const;

    bool utf16Disabled() const;

    void requestInterrupt();
    virtual void requestExecutionTermination();
    bool onInterrupt();

    bool interruptRequested() const;
    bool executionTerminationRequested() const;

    virtual void willEnterVM();

    virtual void willExitVM(JSExceptionTracker& exceptionTracker);

    /**
     Stash a JSValue for later retrieval, which will keep it
     alive until removedStashedJSValue is called.
     */
    JSValueID stashJSValue(const JSValue& value);

    /**
     Stash a JSValue for later retrieval, which will keep it
     alive until removedStashedJSValue is called.
     */
    JSValueID stashJSValue(JSValueRef&& valueRef);

    /**
     Remove a previously stashed JSValue.
     Returns whether the value was succesfully removed, or
     false if the value was not found.
     */
    bool removedStashedJSValue(const JSValueID& id);

    /**
     Retrieve a previously stashed JSValue.
     Returns null optional if the value was not found.
     */
    std::optional<JSValue> getStashedJSValue(const JSValueID& id);

    std::vector<JSValue> getAllStashedJSValues();

protected:
    // The public variants of those methods implement some caching to avoid repeatedly creating
    // values from the engine for common values.

    virtual JSValueRef onNewBool(bool boolean) = 0;

    virtual JSValueRef onNewNumber(int32_t number) = 0;

    virtual JSValueRef onNewNumber(double number) = 0;

    virtual JSValueRef onNewNull() = 0;

    virtual JSValueRef onNewUndefined() = 0;

    virtual void onInitialize(JSExceptionTracker& exceptionTracker) = 0;

    void prepareForTeardown();

private:
    struct StashedJSValue {
        uint32_t salt = 0;
        JSValue value;

        bool empty() const {
            return salt == 0;
        }
    };

    JavaScriptTaskScheduler* _taskScheduler;
    IJavaScriptContextListener* _listener = nullptr;
    FlatMap<StringBox, JSPropertyNameRef> _propertyNameCache;
    FlatMap<StringBox, JSValueRef> _cachedGlobalObjectsProperties;
    std::vector<StashedJSValue> _stashedJSValues;
    SequenceIDGenerator _sequenceIdGenerator;

    JSValueRef _trueValue;
    JSValueRef _falseValue;
    JSValueRef _zeroValue;
    JSValueRef _nullValue;
    JSValueRef _undefinedValue;
    JSValueRef _longConstructor;
    JSValueRef _promiseConstructor;
    JSValueRef _unsignedLongCache;
    JSValueRef _signedLongCache;
    JSPropertyNameRef _exportModePropertyName;
    bool _utf16Disabled = false;
    bool _interruptRequested = false;
    bool _executionTerminationRequested = false;
    bool _tearingDown = false;

    std::unique_ptr<JavaScriptValueMarshaller> _valueMarshaller;

    StashedJSValue* doGetStashedJSValue(const JSValueID& id);

    JSValueRef newLongUncached(const JavaScriptLong& value, JSExceptionTracker& exceptionTracker);
    JSValueRef newLongFromCache(const JavaScriptLong& value,
                                const JSValueRef& cache,
                                size_t cacheIndex,
                                JSExceptionTracker& exceptionTracker);

    JSValueID stashJSValue(const JSValue& value, bool shouldRetain);
};

} // namespace Valdi
