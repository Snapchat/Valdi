//
//  JavaScriptTypes.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 7/11/19.
//

#pragma once

#include "valdi/runtime/JavaScript/JavaScriptCircularRefChecker.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

namespace Valdi {

class JSFunctionNativeCallContext;
class IJavaScriptContext;
class ReferenceInfo;

template<size_t size>
struct BridgedValue {
    uint8_t storage[size];

    inline BridgedValue() {
        for (size_t i = 0; i < size; i++) {
            storage[i] = 0;
        }
    }

    template<typename T>
    explicit inline BridgedValue(const T& source) {
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        static_assert(sizeof(storage) >= sizeof(T), "BridgedValue is not large enough to store the given type");
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        std::memcpy(storage, &source, sizeof(T));

        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        if constexpr (sizeof(storage) > sizeof(T)) {
            // NOLINTNEXTLINE(bugprone-sizeof-expression)
            for (size_t i = sizeof(T); i < sizeof(storage); i++) {
                storage[i] = 0;
            }
        }
    }

    template<typename T>
    inline T bridge() const {
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        static_assert(sizeof(storage) >= sizeof(T), "BridgedValue is not large enough to store the given type");

        T out;
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        std::memcpy(&out, storage, sizeof(T));
        return out;
    }

    inline bool operator==(const BridgedValue<size>& other) const {
        for (size_t i = 0; i < sizeof(storage); i++) {
            if (storage[i] != other.storage[i]) {
                return false;
            }
        }
        return true;
    }

    inline bool operator!=(const BridgedValue<size>& other) const {
        return !(*this == other);
    }
};

constexpr size_t kJSPropertyNameStorageSize = 8;
constexpr size_t kJSValueStorageSize = 16;

struct JSPropertyName : public BridgedValue<kJSPropertyNameStorageSize> {
    using BridgedValue<kJSPropertyNameStorageSize>::BridgedValue;
};

struct JSValue : public BridgedValue<kJSValueStorageSize> {
    using BridgedValue<kJSValueStorageSize>::BridgedValue;
};

void releaseRef(IJavaScriptContext& context, const JSValue& value);
void retainRef(IJavaScriptContext& context, const JSValue& value);
void releaseRef(IJavaScriptContext& context, const JSPropertyName& propertyName);
void retainRef(IJavaScriptContext& context, const JSPropertyName& propertyName);

template<typename T>
class JSRef {
public:
    JSRef() = default;

    JSRef(IJavaScriptContext* context, const T& value, bool isRetained)
        : _context(context), _value(value), _isRetained(isRetained) {}

    JSRef(const JSRef<T>& other) : _context(other._context), _value(other._value) {
        if (other._isRetained) {
            retain();
        }
    }

    JSRef(JSRef<T>&& other) noexcept : _context(other._context), _value(other._value), _isRetained(other._isRetained) {
        other._context = nullptr;
        other._value = T();
        other._isRetained = false;
    }

    ~JSRef() {
        release();
        _value = T();
    }

    constexpr IJavaScriptContext* getContext() const {
        return _context;
    }

    constexpr const T& get() const {
        return _value;
    }

    JSRef<T>& operator=(JSRef<T>&& other) noexcept {
        if (this != &other) {
            release();

            _context = other._context;
            _value = other._value;
            _isRetained = other._isRetained;

            other._context = nullptr;
            other._value = T();
            other._isRetained = false;
        }

        return *this;
    }

    JSRef<T>& operator=(const JSRef<T>& other) {
        if (this != &other) {
            release();

            _context = other._context;
            _value = other._value;

            if (other._isRetained) {
                retain();
            }
        }

        return *this;
    }

    bool empty() const {
        return _context == nullptr;
    }

    T unsafeReleaseValue() {
        auto value = _value;

        _isRetained = false;
        _context = nullptr;
        _value = T();

        return value;
    }

    JSRef<T> asUnretained() const {
        return JSRef<T>(_context, _value, false);
    }

    static JSRef<T> makeRetained(IJavaScriptContext& context, const T& value) {
        auto ref = JSRef<T>(&context, value, false);
        ref.retain();
        return ref;
    }

    static JSRef<T> makeUnretained(IJavaScriptContext& context, const T& value) {
        return JSRef<T>(&context, value, false);
    }

    constexpr bool isRetained() const {
        return _isRetained;
    }

private:
    IJavaScriptContext* _context = nullptr;
    T _value;
    bool _isRetained = false;

    void release() {
        if (_isRetained) {
            releaseRef(*_context, _value);
            _isRetained = false;
        }
    }

    void retain() {
        if (!_isRetained && _context != nullptr) {
            _isRetained = true;
            retainRef(*_context, _value);
        }
    }
};

using JSPropertyNameRef = JSRef<JSPropertyName>;
using JSValueRef = JSRef<JSValue>;

class JSStackTraceProvider : public SimpleRefCountable {
public:
    virtual StringBox getStackTrace() = 0;
    /** Bundle of the innermost feature frame in getStackTrace(), when the provider can tell; empty otherwise. */
    virtual StringBox getStackBundleName() {
        return StringBox();
    }
};

class JSExceptionTracker final : public ExceptionTracker {
public:
    explicit JSExceptionTracker(IJavaScriptContext& jsContext);
    ~JSExceptionTracker() final;

    JSValueRef getExceptionAndClear();

    JavaScriptCircularRefChecker<JSValue>& getCircularRefChecker();

    void storeException(const JSValue& exception);
    void storeException(JSValueRef&& exception);

    const Ref<JSStackTraceProvider>& getStackTraceProvider() const;
    void setStackTraceProvider(Ref<JSStackTraceProvider>&& stackTraceProvider);

    Result<JSValueRef> toRetainedResult(JSValueRef value);

protected:
    Error onGetError() const final;
    void onClearError() final;
    void onSetError(Error&& error) final;

private:
    IJavaScriptContext& _jsContext;
    JSValueRef _exception;
    std::optional<Error> _error;
    Ref<JSStackTraceProvider> _stackTraceProvider;
    JavaScriptCircularRefChecker<JSValue> _circularRefChecker;
};

class JSFunction : public SimpleRefCountable {
public:
    virtual const ReferenceInfo& getReferenceInfo() const;
    virtual JSValueRef operator()(JSFunctionNativeCallContext& callContext) noexcept = 0;
};

struct JSTypedArray {
    TypedArrayType type = TypedArrayType::ArrayBuffer;
    void* data = nullptr;
    size_t length = 0;
    JSValueRef arrayBuffer = JSValueRef();

    inline JSTypedArray() = default;
    inline JSTypedArray(TypedArrayType type, void* data, size_t length, JSValueRef&& arrayBuffer)
        : type(type), data(data), length(length), arrayBuffer(std::move(arrayBuffer)) {}
};

} // namespace Valdi
