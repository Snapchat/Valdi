//
//  HermesUtils.hpp
//  valdi-hermes
//
//  Created by Simon Corsin on 9/27/23.
//

#pragma once

#include "valdi/hermes/Hermes.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JSNativeClassData.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"

namespace Valdi::Hermes {

class ManagedHermesValue {
public:
    inline ManagedHermesValue() : _retainCount(0) {}

    inline void retain() {
        _retainCount++;
    }

    inline void release() {
        _retainCount--;
    }

    inline bool isFree() const {
        return _retainCount == 0;
    }

    template<typename... Args>
    void emplace(Args&&... args) {
        _retainCount = 1;
        new (&_value) hermes::vm::PinnedHermesValue(std::forward<Args>(args)...);
    }

    inline ManagedHermesValue* getNextFree() {
        return _nextFree;
    }

    inline void setNextFree(ManagedHermesValue* nextFree) {
        _nextFree = nextFree;
    }

    inline const hermes::vm::PinnedHermesValue& get() const {
        return _value;
    }

    inline hermes::vm::PinnedHermesValue& get() {
        return _value;
    }

    inline JSValue toJSValue() {
        return JSValue(this);
    }

    inline JSPropertyName toJSPropertyName() {
        return JSPropertyName(this);
    }

    inline static ManagedHermesValue* fromJSValue(const JSValue& jsValue) {
        return jsValue.bridge<ManagedHermesValue*>();
    }

    inline static ManagedHermesValue* fromJSPropertyName(const JSPropertyName& propertyName) {
        return propertyName.bridge<ManagedHermesValue*>();
    }

private:
    size_t _retainCount = 0;
    union {
        hermes::vm::PinnedHermesValue _value;
        ManagedHermesValue* _nextFree;
    };
};

class HermesJavaScriptContext;

struct JSFunctionData : public Valdi::SimpleRefCountable {
public:
    HermesJavaScriptContext& jsContext;
    Valdi::Ref<Valdi::JSFunction> function;

    JSFunctionData(HermesJavaScriptContext& jsContext, const Valdi::Ref<Valdi::JSFunction>& function);
    ~JSFunctionData() override;
};

struct HermesNativeClassData final : public SimpleRefCountable {
    HermesNativeClassData(HermesJavaScriptContext& jsContext, const Ref<JSNativeClassData>& nativeClass);
    ~HermesNativeClassData() override;

    HermesJavaScriptContext& jsContext;
    Ref<JSNativeClassData> nativeClass;
};

struct HermesNativeClassFunctionData final : public SimpleRefCountable {
    HermesNativeClassFunctionData(HermesJavaScriptContext& jsContext,
                                  const Ref<JSNativeClassData>& nativeClass,
                                  const StringBox& name);
    ~HermesNativeClassFunctionData() override;

    HermesJavaScriptContext& jsContext;
    Ref<JSNativeClassData> nativeClass;
    ReferenceInfo referenceInfo;
    JSClassCallback callback = nullptr;
};

void freeNativeState(hermes::vm::GC& gc, hermes::vm::NativeState* nativeState);
void freeContext(void* context);

RefCountable* getOwnNativeStateContext(hermes::vm::Runtime& runtime,
                                       hermes::vm::Handle<hermes::vm::JSObject> object);

hermes::vm::ExecutionStatus setNativeState(hermes::vm::Runtime& runtime,
                                           hermes::vm::Handle<hermes::vm::JSObject> object,
                                           const Ref<RefCountable>& data);

hermes::vm::CallResult<hermes::vm::HermesValue> callTrampoline(void* context,
                                                               hermes::vm::Runtime& runtime,
                                                               hermes::vm::NativeArgs args);

hermes::vm::CallResult<hermes::vm::HermesValue> callNativeClassInstanceMember(
    void* context, hermes::vm::Runtime& runtime, hermes::vm::NativeArgs args);

hermes::vm::CallResult<hermes::vm::HermesValue> callNativeClassStaticMember(
    void* context, hermes::vm::Runtime& runtime, hermes::vm::NativeArgs args);

hermes::vm::CallResult<hermes::vm::HermesValue> callNativeClassConstructor(
    void* context, hermes::vm::Runtime& runtime, hermes::vm::NativeArgs args);

hermes::vm::CallResult<hermes::vm::PseudoHandle<hermes::vm::JSObject>> createNativeClassObject(
    hermes::vm::Runtime& runtime, hermes::vm::Handle<hermes::vm::JSObject> prototype, void* context);

class ByteBufferOStream : public llvh::raw_ostream {
    ByteBuffer& _output;

    void write_impl(const char* Ptr, size_t Size) final;

    uint64_t current_pos() const final;

public:
    explicit ByteBufferOStream(ByteBuffer& output);
    ~ByteBufferOStream() override;

    BytesView toBytesView();
};

} // namespace Valdi::Hermes
