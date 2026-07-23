//
//  IJavaScriptContext.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 11/22/19.
//

#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi/runtime/JavaScript/JavaScriptValueMarshaller.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"

namespace Valdi {

IJavaScriptContext::IJavaScriptContext(JavaScriptTaskScheduler* taskScheduler) : _taskScheduler(taskScheduler) {}

IJavaScriptContext::~IJavaScriptContext() = default;

JavaScriptTaskScheduler* IJavaScriptContext::getTaskScheduler() const {
    return _taskScheduler;
}

void IJavaScriptContext::setLongConstructor(const JSValueRef& longConstructor) {
    _longConstructor = ensureRetainedValue(longConstructor);
}

const JSValueRef& IJavaScriptContext::getLongConstructor() const {
    return _longConstructor;
}

void IJavaScriptContext::initialize(const IJavaScriptContextConfig& config, JSExceptionTracker& exceptionTracker) {
    onInitialize(exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    _utf16Disabled = config.disableUTF16;

    _undefinedValue = ensureRetainedValue(onNewUndefined());
    _nullValue = ensureRetainedValue(onNewNull());
    _trueValue = ensureRetainedValue(onNewBool(true));
    _falseValue = ensureRetainedValue(onNewBool(false));
    _zeroValue = ensureRetainedValue(onNewNumber(0));
    _signedLongCache = ensureRetainedValue(newArray(0, exceptionTracker));
    if (!exceptionTracker) {
        return;
    }
    _unsignedLongCache = ensureRetainedValue(newArray(0, exceptionTracker));
    if (!exceptionTracker) {
        return;
    }

    _exportModePropertyName = newPropertyName("__exportMode__");

    if (!config.disableValueMarshaller) {
        _valueMarshaller =
            std::make_unique<JavaScriptValueMarshaller>(*this, config.disableProxyObjectStore, exceptionTracker);
    }
}

void IJavaScriptContext::prepareForTeardown() {
    _tearingDown = true;
    _valueMarshaller = nullptr;
    clearGlobalObjectPropertyCache();
    clearPropertyNameCache();

    for (auto& stashedJSValue : _stashedJSValues) {
        if (!stashedJSValue.empty()) {
            auto jsValue = stashedJSValue.value;
            stashedJSValue = StashedJSValue();
            releaseValue(jsValue);
        }
    }

    _signedLongCache = JSValueRef();
    _unsignedLongCache = JSValueRef();
    _undefinedValue = JSValueRef();
    _nullValue = JSValueRef();
    _trueValue = JSValueRef();
    _falseValue = JSValueRef();
    _zeroValue = JSValueRef();
    _longConstructor = JSValueRef();
    _promiseConstructor = JSValueRef();

    _exportModePropertyName = JSPropertyNameRef();
}

JSValueRef IJavaScriptContext::newBool(bool boolean) {
    return boolean ? _trueValue.asUnretained() : _falseValue.asUnretained();
}

JSValueRef IJavaScriptContext::newNumber(int32_t number) {
    return number == 0 ? _zeroValue.asUnretained() : onNewNumber(number);
}

JSValueRef IJavaScriptContext::newNumber(double number) {
    return number == 0.0 ? _zeroValue.asUnretained() : onNewNumber(number);
}

JSValueRef IJavaScriptContext::newNull() {
    return _nullValue.asUnretained();
}

JSValueRef IJavaScriptContext::newUndefined() {
    return _undefinedValue.asUnretained();
}

JSValueRef IJavaScriptContext::newString(const StaticString& str, JSExceptionTracker& exceptionTracker) {
    switch (str.encoding()) {
        case StaticString::Encoding::UTF8:
            return newStringUTF8(str.utf8StringView(), exceptionTracker);
        case StaticString::Encoding::UTF16:
            return newStringUTF16(str.utf16StringView(), exceptionTracker);
        case StaticString::Encoding::UTF32: {
            auto utf8 = str.utf8Storage();
            return newStringUTF8(utf8.toStringView(), exceptionTracker);
        }
    }
}

JSValueRef IJavaScriptContext::newArrayWithValues(const JSValue* values,
                                                  size_t size,
                                                  JSExceptionTracker& exceptionTracker) {
    return newArrayWithValues(
        size, exceptionTracker, [&](size_t i) -> JSValueRef { return JSValueRef::makeUnretained(*this, values[i]); });
}

JSValueRef IJavaScriptContext::callObjectProperty(const JSValue& object,
                                                  const JSPropertyName& propertyName,
                                                  JSFunctionCallContext& callContext) {
    auto func = getObjectProperty(object, propertyName, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    callContext.setThisValue(object);
    return callObjectAsFunction(func.get(), callContext);
}

JSValueRef IJavaScriptContext::callObjectProperty(const JSValue& object,
                                                  const std::string_view& propertyName,
                                                  JSFunctionCallContext& callContext) {
    auto jsPropertyName = newPropertyName(propertyName);
    return callObjectProperty(object, jsPropertyName.get(), callContext);
}

JSValueRef IJavaScriptContext::getObjectProperty(const JSValue& object,
                                                 const std::string_view& propertyName,
                                                 JSExceptionTracker& exceptionTracker) {
    auto jsPropertyName = newPropertyName(propertyName);
    return getObjectProperty(object, jsPropertyName.get(), exceptionTracker);
}

void IJavaScriptContext::setObjectProperty(const JSValue& object,
                                           const std::string_view& propertyName,
                                           const JSValue& propertyValue,
                                           JSExceptionTracker& exceptionTracker) {
    setObjectProperty(object, propertyName, propertyValue, true, exceptionTracker);
}

void IJavaScriptContext::setObjectProperty(const JSValue& object,
                                           const JSPropertyName& propertyName,
                                           const JSValue& propertyValue,
                                           JSExceptionTracker& exceptionTracker) {
    setObjectProperty(object, propertyName, propertyValue, true, exceptionTracker);
}

void IJavaScriptContext::setObjectProperty(const JSValue& object,
                                           const std::string_view& propertyName,
                                           const JSValue& propertyValue,
                                           bool enumerable,
                                           JSExceptionTracker& exceptionTracker) {
    auto jsPropertyName = newPropertyName(propertyName);
    setObjectProperty(object, jsPropertyName.get(), propertyValue, enumerable, exceptionTracker);
}

void IJavaScriptContext::setObjectProperty(const JSValue& object,
                                           const JSValue& propertyName,
                                           const JSValue& propertyValue,
                                           JSExceptionTracker& exceptionTracker) {
    setObjectProperty(object, propertyName, propertyValue, true, exceptionTracker);
}

JSValueRef IJavaScriptContext::newError(std::string_view message,
                                        std::optional<std::string_view> stack,
                                        JSExceptionTracker& exceptionTracker) {
    static auto kErrorConstructor = STRING_LITERAL("Error");

    auto jsMessage = newStringUTF8(message, exceptionTracker);
    if (!exceptionTracker) {
        return newUndefined();
    }

    auto errorConstructor = getPropertyFromGlobalObjectCached(kErrorConstructor, exceptionTracker);
    if (!exceptionTracker) {
        return newUndefined();
    }

    JSFunctionCallContext params(*this, &jsMessage, 1, exceptionTracker);

    auto jsError = callObjectAsConstructor(errorConstructor.get(), params);

    if (exceptionTracker && stack) {
        auto jsStack = newStringUTF8(stack.value(), exceptionTracker);
        if (!exceptionTracker) {
            return newUndefined();
        }

        setObjectProperty(jsError.get(), "stack", jsStack.get(), true, exceptionTracker);
    }

    return jsError;
}

JSValueRef IJavaScriptContext::newLongFromCache(const JavaScriptLong& value,
                                                const JSValueRef& cache,
                                                size_t cacheIndex,
                                                JSExceptionTracker& exceptionTracker) {
    auto cachedValue = getObjectPropertyForIndex(cache.get(), cacheIndex, exceptionTracker);
    if (!exceptionTracker) {
        return cachedValue;
    }

    if (isValueUndefined(cachedValue.get())) {
        cachedValue = newLongUncached(JavaScriptLong(value), exceptionTracker);
        if (!exceptionTracker) {
            return cachedValue;
        }

        setObjectPropertyIndex(cache.get(), cacheIndex, cachedValue.get(), exceptionTracker);
    }

    return cachedValue;
}

JSValueRef IJavaScriptContext::newLong(int64_t value, JSExceptionTracker& exceptionTracker) {
    if (value >= -128 && value < 128) {
        return newLongFromCache(
            JavaScriptLong(value), _signedLongCache, static_cast<size_t>(value + 128), exceptionTracker);
    } else {
        return newLongUncached(JavaScriptLong(value), exceptionTracker);
    }
}

JSValueRef IJavaScriptContext::newUnsignedLong(uint64_t value, JSExceptionTracker& exceptionTracker) {
    if (value < 256) {
        return newLongFromCache(
            JavaScriptLong(value), _unsignedLongCache, static_cast<size_t>(value), exceptionTracker);
    } else {
        return newLongUncached(JavaScriptLong(value), exceptionTracker);
    }
}

JSValueRef IJavaScriptContext::newLongUncached(const JavaScriptLong& value, JSExceptionTracker& exceptionTracker) {
    if (_longConstructor.empty()) {
        exceptionTracker.onError(Error("Long Constructor is not configured"));
        return JSValueRef();
    }

    std::initializer_list<JSValueRef> parameters = {
        newNumber(value.lowBits()), newNumber(value.highBits()), newBool(value.isUnsigned())};

    JSFunctionCallContext params(*this, parameters.begin(), parameters.size(), exceptionTracker);

    return callObjectAsConstructor(_longConstructor.get(), params);
}

JSValueRef IJavaScriptContext::newArrayBufferCopy(const Byte* data, size_t size, JSExceptionTracker& exceptionTracker) {
    return newArrayBuffer(makeShared<ByteBuffer>(data, data + size)->toBytesView(), exceptionTracker);
}

JavaScriptLong IJavaScriptContext::valueToLong(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    static auto kLow = STRING_LITERAL("low");
    static auto kHigh = STRING_LITERAL("high");
    static auto kUnsigned = STRING_LITERAL("unsigned");

    auto low = getObjectProperty(value, getPropertyNameCached(kLow), exceptionTracker);
    if (!exceptionTracker) {
        return JavaScriptLong();
    }

    auto high = getObjectProperty(value, getPropertyNameCached(kHigh), exceptionTracker);
    if (!exceptionTracker) {
        return JavaScriptLong();
    }

    auto isUnsigned = getObjectProperty(value, getPropertyNameCached(kUnsigned), exceptionTracker);
    if (!exceptionTracker) {
        return JavaScriptLong();
    }

    auto lowValue = valueToInt(low.get(), exceptionTracker);
    if (!exceptionTracker) {
        return JavaScriptLong();
    }

    auto highValue = valueToInt(high.get(), exceptionTracker);
    if (!exceptionTracker) {
        return JavaScriptLong();
    }

    auto isUnsignedValue = valueToBool(isUnsigned.get(), exceptionTracker);
    if (!exceptionTracker) {
        return JavaScriptLong();
    }

    return JavaScriptLong(static_cast<int32_t>(lowValue), static_cast<int32_t>(highValue), isUnsignedValue);
}

JSValueRef IJavaScriptContext::evaluateNative(const std::string_view& /*sourceFilename*/,
                                              JSExceptionTracker& exceptionTracker) {
    exceptionTracker.onError("Native module evaluation is not supported in this JS context");
    return JSValueRef();
}

std::optional<IJavaScriptNativeModuleInfo> IJavaScriptContext::getNativeModuleInfo(
    const std::string_view& /*sourceFilename*/) {
    return std::nullopt;
}

JSValueRef IJavaScriptContext::newPromise(const JSValueRef& executor, JSExceptionTracker& exceptionTracker) {
    if (_promiseConstructor.empty()) {
        auto globalObject = getGlobalObject(exceptionTracker);
        if (!exceptionTracker) {
            return globalObject;
        }
        auto promise = getObjectProperty(globalObject.get(), "Promise", exceptionTracker);
        if (!exceptionTracker) {
            return promise;
        }

        _promiseConstructor = JSValueRef::makeRetained(*this, promise.get());
    }

    JSFunctionCallContext callContext(*this, &executor, 1, exceptionTracker);
    return callObjectAsConstructor(_promiseConstructor.get(), callContext);
}

JSFunctionExportMode IJavaScriptContext::getValueFunctionExportMode(const JSValue& function,
                                                                    JSExceptionTracker& exceptionTracker) {
    if (isValueUndefined(function)) {
        return kJSFunctionExportModeEmpty;
    }

    auto jsExportMode = getObjectProperty(function, _exportModePropertyName.get(), exceptionTracker);
    if (!exceptionTracker || isValueUndefined(jsExportMode.get())) {
        return kJSFunctionExportModeEmpty;
    }

    return static_cast<JSFunctionExportMode>(valueToInt(jsExportMode.get(), exceptionTracker));
}

void IJavaScriptContext::setValueFunctionExportMode(const JSValue& function,
                                                    JSFunctionExportMode exportMode,
                                                    JSExceptionTracker& exceptionTracker) {
    auto jsExportMode = newNumber(static_cast<int32_t>(exportMode));
    setObjectProperty(function, _exportModePropertyName.get(), jsExportMode.get(), exceptionTracker);
}

void IJavaScriptContext::clearPropertyNameCache() {
    _propertyNameCache.clear();
}

JSPropertyName IJavaScriptContext::getPropertyNameCached(const StringBox& str) {
    const auto& it = _propertyNameCache.find(str);
    if (it != _propertyNameCache.end()) {
        return it->second.get();
    }

    auto propertyNameRef = newPropertyName(str.toStringView());
    auto propertyName = propertyNameRef.get();
    _propertyNameCache[str] = std::move(propertyNameRef);
    return propertyName;
}

JSValueRef IJavaScriptContext::getPropertyFromGlobalObjectCached(const StringBox& str,
                                                                 JSExceptionTracker& exceptionTracker) {
    const auto& it = _cachedGlobalObjectsProperties.find(str);
    if (it != _cachedGlobalObjectsProperties.end()) {
        return it->second;
    }

    auto globalObject = getGlobalObject(exceptionTracker);
    if (!exceptionTracker) {
        return newUndefined();
    }

    auto propertyValue = getObjectProperty(globalObject.get(), getPropertyNameCached(str), exceptionTracker);
    if (!exceptionTracker) {
        return newUndefined();
    }

    _cachedGlobalObjectsProperties[str] = propertyValue;

    return propertyValue;
}

void IJavaScriptContext::clearGlobalObjectPropertyCache() {
    _cachedGlobalObjectsProperties.clear();
}

void IJavaScriptContext::setListener(IJavaScriptContextListener* listener) {
    _listener = listener;
}

IJavaScriptContextListener* IJavaScriptContext::getListener() const {
    return _listener;
}

JSValueRef IJavaScriptContext::ensureRetainedValue(JSValueRef value) {
    if (value.isRetained()) {
        return value;
    } else {
        return JSValueRef::makeRetained(*this, value.get());
    }
}

JavaScriptValueMarshaller* IJavaScriptContext::getValueMarshaller() const {
    return _valueMarshaller.get();
}

bool IJavaScriptContext::utf16Disabled() const {
    return _utf16Disabled;
}

// We don't care about pure thread safety correctness of the
// interrupt boolean, it needs to be as low overhead as possible
__attribute__((no_sanitize("thread"))) void IJavaScriptContext::requestInterrupt() {
    _interruptRequested = true;
}

__attribute__((no_sanitize("thread"))) void IJavaScriptContext::onInterrupt() {
    if (_listener != nullptr) {
        _listener->onInterrupt(*this);
    }
    _interruptRequested = false;
}

__attribute__((no_sanitize("thread"))) bool IJavaScriptContext::interruptRequested() const {
    return _interruptRequested;
}

JSValueID IJavaScriptContext::stashJSValue(JSValueRef&& valueRef) {
    auto wasRetained = valueRef.isRetained();
    return stashJSValue(valueRef.unsafeReleaseValue(), !wasRetained);
}

JSValueID IJavaScriptContext::stashJSValue(const JSValue& value) {
    return stashJSValue(value, true);
}

JSValueID IJavaScriptContext::stashJSValue(const JSValue& value, bool shouldRetain) {
    if (VALDI_UNLIKELY(_tearingDown)) {
        return JSValueID();
    }

    auto id = _sequenceIdGenerator.newId();
    auto index = id.getIndexAsSize();
    while (index >= _stashedJSValues.size()) {
        _stashedJSValues.emplace_back();
    }

    auto& it = _stashedJSValues[index];
    it.salt = id.getSalt();
    it.value = value;

    if (shouldRetain) {
        retainValue(value);
    }

    return id;
}

bool IJavaScriptContext::removedStashedJSValue(const JSValueID& id) {
    auto* stashedJSValue = doGetStashedJSValue(id);
    if (stashedJSValue == nullptr) {
        return false;
    }

    _sequenceIdGenerator.releaseId(id);

    auto jsValue = stashedJSValue->value;

    *stashedJSValue = StashedJSValue();

    releaseValue(jsValue);

    return true;
}

std::optional<JSValue> IJavaScriptContext::getStashedJSValue(const JSValueID& id) {
    auto* stashedJSValue = doGetStashedJSValue(id);
    if (stashedJSValue == nullptr) {
        return std::nullopt;
    }

    return {stashedJSValue->value};
}

std::vector<JSValue> IJavaScriptContext::getAllStashedJSValues() {
    std::vector<JSValue> out;
    out.reserve(_stashedJSValues.size());

    for (auto& stashedJSValue : _stashedJSValues) {
        if (!stashedJSValue.empty()) {
            out.emplace_back(stashedJSValue.value);
        }
    }

    return out;
}

IJavaScriptContext::StashedJSValue* IJavaScriptContext::doGetStashedJSValue(const JSValueID& id) {
    auto index = id.getIndexAsSize();
    if (index >= _stashedJSValues.size()) {
        return nullptr;
    }

    auto& it = _stashedJSValues[index];
    if (it.salt != id.getSalt()) {
        return nullptr;
    }

    return &it;
}

void IJavaScriptContext::willEnterVM() {}

void IJavaScriptContext::willExitVM(JSExceptionTracker& exceptionTracker) {}

} // namespace Valdi
