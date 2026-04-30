//
//  JavaScriptValueDelegate.cpp
//  valdi
//
//  Created by Simon Corsin on 2/3/23.
//

#include "valdi/runtime/JavaScript/JavaScriptValueDelegate.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JSFunctionWithValueFunction.hpp"
#include "valdi/runtime/JavaScript/JSPromise.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi/runtime/JavaScript/ValueFunctionWithJSValue.hpp"

#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Utils/InlineContainerAllocator.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueMarshaller.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#include <boost/functional/hash.hpp>
#include <fmt/format.h>

namespace std {

std::size_t hash<Valdi::JavaScriptObjectStore::ObjectStoreId>::operator()(
    const Valdi::JavaScriptObjectStore::ObjectStoreId& k) const noexcept {
    std::size_t hash = 0;
    boost::hash_combine(hash, k.contextId);
    boost::hash_combine(hash, k.objectId);
    return hash;
}

} // namespace std

namespace Valdi {

bool JavaScriptObjectStore::ObjectStoreId::operator==(const ObjectStoreId& other) const {
    return objectId == other.objectId && contextId == other.contextId;
}

bool JavaScriptObjectStore::ObjectStoreId::operator!=(const ObjectStoreId& other) const {
    return !(*this == other);
}

static JSExceptionTracker& toJSExceptionTracker(ExceptionTracker& exceptionTracker) {
    return dynamic_cast<JSExceptionTracker&>(exceptionTracker);
}

JavaScriptObjectStore::JavaScriptObjectStore(IJavaScriptContext& jsContext,
                                             bool disabled,
                                             JSExceptionTracker& exceptionTracker)
    : _jsContext(&jsContext), _attachedProxyName(jsContext.newPropertyName("$attachedProxy")), _disabled(disabled) {}

JavaScriptObjectStore::~JavaScriptObjectStore() = default;

void JavaScriptObjectStore::teardown() {
    _attachedProxyName = JSPropertyNameRef();
    _objectById.clear();
    _jsContext = nullptr;
    _disabled = true;
}

Ref<RefCountable> JavaScriptObjectStore::getValueForObjectKey(const JSValueRef& objectKey,
                                                              ExceptionTracker& exceptionTracker) {
    return getValueForObjectKey(objectKey.get(), toJSExceptionTracker(exceptionTracker));
}

Ref<RefCountable> JavaScriptObjectStore::getValueForObjectKey(const JSValue& objectKey,
                                                              JSExceptionTracker& exceptionTracker) {
    if (_disabled) {
        return nullptr;
    }

    auto value = _jsContext->getObjectProperty(objectKey, _attachedProxyName.get(), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }
    if (_jsContext->isValueUndefined(value.get())) {
        return nullptr;
    }

    return unwrapWrappedObject(*_jsContext, value.get(), exceptionTracker);
}

bool JavaScriptObjectStore::hasValueForObjectKey(const JSValue& objectKey) {
    return _jsContext->hasObjectProperty(objectKey, _attachedProxyName.get());
}

void JavaScriptObjectStore::setValueForObjectKey(const JSValueRef& objectKey,
                                                 const Ref<RefCountable>& value,
                                                 ExceptionTracker& exceptionTracker) {
    if (_disabled) {
        return;
    }

    auto wrappedObject = makeWrappedObject(*_jsContext, value, toJSExceptionTracker(exceptionTracker), true);
    if (!exceptionTracker) {
        return;
    }
    _jsContext->setObjectProperty(
        objectKey.get(), _attachedProxyName.get(), wrappedObject.get(), false, toJSExceptionTracker(exceptionTracker));
}

std::optional<JSValueRef> JavaScriptObjectStore::getObjectForId(uint32_t objectId, ExceptionTracker& exceptionTracker) {
    auto objectStoreId = makeObjectStoreId(objectId);
    const auto& it = _objectById.find(objectStoreId);
    if (it == _objectById.end()) {
        return std::nullopt;
    }

    auto deref = _jsContext->derefWeakRef(it->second.get(), toJSExceptionTracker(exceptionTracker));
    if (!exceptionTracker) {
        return std::nullopt;
    }

    if (_jsContext->isValueUndefined(deref.get())) {
        _objectById.erase(it);
        return std::nullopt;
    }

    return std::move(deref);
}

void JavaScriptObjectStore::setObjectForId(uint32_t objectId,
                                           const JSValueRef& object,
                                           ExceptionTracker& exceptionTracker) {
    if (_disabled) {
        return;
    }

    auto weakRef = _jsContext->newWeakRef(object.get(), toJSExceptionTracker(exceptionTracker));
    if (!exceptionTracker) {
        return;
    }

    auto objectStoreId = makeObjectStoreId(objectId);

    _objectById[objectStoreId] = _jsContext->ensureRetainedValue(std::move(weakRef));
}

JavaScriptObjectStore::ObjectStoreId JavaScriptObjectStore::makeObjectStoreId(uint32_t objectId) {
    auto root = Context::currentRoot();
    JavaScriptObjectStore::ObjectStoreId out;
    out.objectId = objectId;
    out.contextId = root != nullptr ? root->getContextId() : ContextIdNull;

    return out;
}

class JavaScriptMapValueVisitorBridge : public IJavaScriptPropertyNamesVisitor {
public:
    explicit JavaScriptMapValueVisitorBridge(PlatformMapValueVisitor<JSValueRef>& visitor) : _visitor(visitor) {}

    bool visitPropertyName(IJavaScriptContext& context,
                           JSValue object,
                           const JSPropertyName& propertyName,
                           JSExceptionTracker& exceptionTracker) final {
        auto objectProperty = context.getObjectProperty(object, propertyName, exceptionTracker);
        if (!exceptionTracker) {
            return false;
        }

        if (!context.isValueUndefined(objectProperty.get())) {
            return _visitor.visit(context.propertyNameToString(propertyName), objectProperty, exceptionTracker);
        }

        return true;
    }

private:
    PlatformMapValueVisitor<JSValueRef>& _visitor;
};

class JavaScriptProxyObject : public ValueTypedProxyObject {
public:
    JavaScriptProxyObject(const Ref<ValueTypedObject>& typedObject,
                          IJavaScriptContext& jsContext,
                          const JSValue& jsValue,
                          const ReferenceInfo& referenceInfo,
                          JSExceptionTracker& exceptionTracker,
                          bool captureStackTrace)
        : ValueTypedProxyObject(typedObject),
          _value(jsContext, jsValue, referenceInfo, exceptionTracker, captureStackTrace) {}
    ~JavaScriptProxyObject() override = default;

    std::string_view getType() const final {
        return "JS Proxy";
    }

    bool expired() const override {
        return _value.expired();
    }

private:
    JSValueRefHolder _value;
};

class JavaScriptClassDelegate : public PlatformObjectClassDelegate<JSValueRef> {
public:
    ~JavaScriptClassDelegate() override {
        for (size_t i = 0; i < _propertiesSize; i++) {
            getContext().releasePropertyName(getPropertyName(i));
        }
    }

    const JSPropertyName& getPropertyName(size_t index) const {
        InlineContainerAllocator<JavaScriptClassDelegate, JSPropertyName> allocator;
        return allocator.getContainerStartPtr(this)[index];
    }

    void setPropertyName(size_t index, JSPropertyNameRef&& propertyNameRef) {
        InlineContainerAllocator<JavaScriptClassDelegate, JSPropertyName> allocator;
        allocator.getContainerStartPtr(this)[index] = propertyNameRef.unsafeReleaseValue();
    }

    JSValueRef newObject(const JSValueRef* propertyValues, ExceptionTracker& exceptionTracker) final {
        auto& jsContext = getContext();
        auto& jsExceptionTracker = toJSExceptionTracker(exceptionTracker);

        auto object = jsContext.newObject(jsExceptionTracker);
        if (!jsExceptionTracker) {
            return jsContext.newUndefined();
        }

        for (size_t i = 0; i < _propertiesSize; i++) {
            jsContext.setObjectProperty(object.get(), getPropertyName(i), propertyValues[i].get(), jsExceptionTracker);
            if (!jsExceptionTracker) {
                return jsContext.newUndefined();
            }
        }

        return object;
    }

    JSValueRef getProperty(const JSValueRef& object, size_t propertyIndex, ExceptionTracker& exceptionTracker) final {
        return getContext().getObjectProperty(
            object.get(), getPropertyName(propertyIndex), toJSExceptionTracker(exceptionTracker));
    }

    Ref<ValueTypedProxyObject> newProxy(const JSValueRef& object,
                                        const Ref<ValueTypedObject>& typedObject,
                                        ExceptionTracker& exceptionTracker) final {
        auto proxy = makeShared<JavaScriptProxyObject>(
            typedObject, getContext(), object.get(), ReferenceInfo(), toJSExceptionTracker(exceptionTracker), false);
        if (!exceptionTracker) {
            return nullptr;
        }

        return proxy;
    }

    static Ref<JavaScriptClassDelegate> make(IJavaScriptContext& jsContext, size_t propertiesSize) {
        InlineContainerAllocator<JavaScriptClassDelegate, JSPropertyName> allocator;

        return allocator.allocate(propertiesSize, jsContext, propertiesSize);
    }

private:
    IJavaScriptContext& _jsContext;
    size_t _propertiesSize;

    friend InlineContainerAllocator<JavaScriptClassDelegate, JSPropertyName>;

    JavaScriptClassDelegate(IJavaScriptContext& jsContext, size_t propertiesSize)
        : _jsContext(jsContext), _propertiesSize(propertiesSize) {}

    IJavaScriptContext& getContext() const {
        return _jsContext;
    }
};

class JavaScriptEnumClassDelegate : public PlatformEnumClassDelegate<JSValueRef> {
public:
    ~JavaScriptEnumClassDelegate() override {
        InlineContainerAllocator<JavaScriptEnumClassDelegate, JSValueRef> allocator;
        allocator.deallocate(this, _enumSchema->getCasesSize());
    }

    const JSValueRef& getEnumCaseForIndex(size_t index) const {
        InlineContainerAllocator<JavaScriptEnumClassDelegate, JSValueRef> allocator;
        return allocator.getContainerStartPtr(this)[index];
    }

    void setEnumCaseForIndex(size_t index, JSValueRef&& enumCase) {
        InlineContainerAllocator<JavaScriptEnumClassDelegate, JSValueRef> allocator;
        allocator.getContainerStartPtr(this)[index] = _jsContext.ensureRetainedValue(std::move(enumCase));
    }

    JSValueRef newEnum(size_t enumCaseIndex, bool /*asBoxed*/, ExceptionTracker& exceptionTracker) final {
        return getEnumCaseForIndex(enumCaseIndex).asUnretained();
    }

    Value enumCaseToValue(const JSValueRef& enumeration, bool /*isBoxed*/, ExceptionTracker& exceptionTracker) final {
        if (_enumSchema->getCaseSchema().isString()) {
            return Value(_jsContext.valueToString(enumeration.get(), toJSExceptionTracker(exceptionTracker)));
        } else if (_enumSchema->getCaseSchema().isInteger()) {
            return Value(_jsContext.valueToInt(enumeration.get(), toJSExceptionTracker(exceptionTracker)));
        } else {
            return jsValueToValue(
                _jsContext, enumeration.get(), ReferenceInfoBuilder(), toJSExceptionTracker(exceptionTracker));
        }
    }

    static Ref<JavaScriptEnumClassDelegate> make(IJavaScriptContext& jsContext, const Ref<EnumSchema>& enumSchema) {
        InlineContainerAllocator<JavaScriptEnumClassDelegate, JSValueRef> allocator;

        return allocator.allocate(enumSchema->getCasesSize(), jsContext, enumSchema);
    }

private:
    friend InlineContainerAllocator<JavaScriptEnumClassDelegate, JSValueRef>;

    IJavaScriptContext& _jsContext;
    Ref<EnumSchema> _enumSchema;

    JavaScriptEnumClassDelegate(IJavaScriptContext& jsContext, const Ref<EnumSchema>& enumSchema)
        : _jsContext(jsContext), _enumSchema(enumSchema) {
        InlineContainerAllocator<JavaScriptEnumClassDelegate, JSValueRef> allocator;
        allocator.constructContainerEntries(this, enumSchema->getCasesSize());
    }
};

class JSFunctionWithTypedValueFunction : public JSFunctionWithValueFunction {
public:
    JSFunctionWithTypedValueFunction(const Ref<ValueFunction>& function,
                                     bool isSingleCall,
                                     const ReferenceInfo& referenceInfo,
                                     const Ref<PlatformFunctionTrampoline<JSValueRef>>& trampoline)
        : JSFunctionWithValueFunction(Ref<ValueFunction>(function), isSingleCall, referenceInfo),
          _trampoline(trampoline) {}
    ~JSFunctionWithTypedValueFunction() override = default;

    const Ref<PlatformFunctionTrampoline<JSValueRef>>& getTrampoline() const {
        return _trampoline;
    }

protected:
    JSValueRef doCall(const Ref<ValueFunction>& function,
                      const StringBox* precomputedFunctionIdentifier,
                      JSFunctionNativeCallContext& callContext) const final {
        auto result = _trampoline->forwardCall(function.get(),
                                               callContext.getParameters(),
                                               callContext.getParameterSize(),
                                               ReferenceInfoBuilder(getReferenceInfo()),
                                               precomputedFunctionIdentifier,
                                               callContext.getExceptionTracker());
        if (!result) {
            return callContext.getContext().newUndefined();
        }

        return std::move(result.value());
    }

private:
    Ref<PlatformFunctionTrampoline<JSValueRef>> _trampoline;
};

class TypedValueFunctionWithJSValue : public ValueFunctionWithJSValue {
public:
    TypedValueFunctionWithJSValue(IJavaScriptContext& context,
                                  const JSValue& value,
                                  bool isSingleCall,
                                  const ReferenceInfo& referenceInfo,
                                  JSExceptionTracker& exceptionTracker,
                                  const Ref<PlatformFunctionTrampoline<JSValueRef>>& trampoline)
        : ValueFunctionWithJSValue(context, value, isSingleCall, referenceInfo, exceptionTracker),
          _trampoline(trampoline) {}

    ~TypedValueFunctionWithJSValue() override = default;

protected:
    Value callJsFunction(JavaScriptEntryParameters& jsEntry,
                         const JSValue& function,
                         const Value* parameters,
                         size_t parametersSize,
                         bool /*ignoreRetValue*/) const final {
        auto referenceInfoBuilder = ReferenceInfoBuilder(getReferenceInfo());
        JSValueRef jsParameters[parametersSize];

        if (!_trampoline->unmarshallParameters(parameters,
                                               parametersSize,
                                               &jsParameters[0],
                                               parametersSize,
                                               referenceInfoBuilder,
                                               jsEntry.exceptionTracker)) {
            return Value::undefined();
        }

        JSFunctionCallContext callContext(
            jsEntry.jsContext, &jsParameters[0], parametersSize, jsEntry.exceptionTracker);
        auto result = jsEntry.jsContext.callObjectAsFunction(function, callContext);

        if (!jsEntry.exceptionTracker) {
            return Value::undefined();
        }

        return _trampoline->marshallReturnValue(result, referenceInfoBuilder, jsEntry.exceptionTracker);
    }

private:
    Ref<PlatformFunctionTrampoline<JSValueRef>> _trampoline;
};

class TypedValueFunctionWithJSValueAndReceiver : public ValueFunctionWithJSValue {
public:
    TypedValueFunctionWithJSValueAndReceiver(IJavaScriptContext& context,
                                             const JSValue& receiver,
                                             const JSValue& value,
                                             bool isSingleCall,
                                             const ReferenceInfo& referenceInfo,
                                             JSExceptionTracker& exceptionTracker,
                                             const Ref<PlatformFunctionTrampoline<JSValueRef>>& trampoline)
        : ValueFunctionWithJSValue(context, value, isSingleCall, referenceInfo, exceptionTracker),
          _receiver(context, receiver, referenceInfo, exceptionTracker, false),
          _trampoline(trampoline) {}

    ~TypedValueFunctionWithJSValueAndReceiver() override = default;

protected:
    Value callJsFunction(JavaScriptEntryParameters& jsEntry,
                         const JSValue& function,
                         const Value* parameters,
                         size_t parametersSize,
                         bool /*ignoreRetValue*/) const final {
        auto referenceInfoBuilder = ReferenceInfoBuilder(getReferenceInfo());
        auto thisValue = _receiver.getJsValue(jsEntry.jsContext, jsEntry.exceptionTracker);
        if (!jsEntry.exceptionTracker) {
            return Value::undefined();
        }

        JSValueRef jsParameters[parametersSize];

        if (!_trampoline->unmarshallParameters(parameters,
                                               parametersSize,
                                               &jsParameters[0],
                                               parametersSize,
                                               referenceInfoBuilder,
                                               jsEntry.exceptionTracker)) {
            return Value::undefined();
        }

        JSFunctionCallContext callContext(
            jsEntry.jsContext, &jsParameters[0], parametersSize, jsEntry.exceptionTracker);
        callContext.setThisValue(thisValue);
        auto result = jsEntry.jsContext.callObjectAsFunction(function, callContext);

        if (!jsEntry.exceptionTracker) {
            return Value::undefined();
        }

        return _trampoline->marshallReturnValue(result, referenceInfoBuilder, jsEntry.exceptionTracker);
    }

private:
    JSValueRefHolder _receiver;
    Ref<PlatformFunctionTrampoline<JSValueRef>> _trampoline;
};

class JavaScriptFunctionClassDelegate : public PlatformFunctionClassDelegate<JSValueRef> {
public:
    JavaScriptFunctionClassDelegate(IJavaScriptContext& jsContext,
                                    const Ref<PlatformFunctionTrampoline<JSValueRef>>& trampoline,
                                    bool isSingleCall,
                                    bool allowSyncCall)
        : _jsContext(jsContext), _trampoline(trampoline), _isSingleCall(isSingleCall), _allowSyncCall(allowSyncCall) {}

    ~JavaScriptFunctionClassDelegate() override = default;

    JSValueRef newFunction(const Ref<ValueFunction>& valueFunction,
                           const ReferenceInfoBuilder& referenceInfoBuilder,
                           ExceptionTracker& exceptionTracker) final {
        auto jsCallable = castOrNull<JSFunctionWithTypedValueFunction>(valueFunction);
        if (jsCallable != nullptr && jsCallable->getTrampoline() == _trampoline) {
            return _jsContext.newFunction(jsCallable, toJSExceptionTracker(exceptionTracker));
        } else {
            jsCallable = makeShared<JSFunctionWithTypedValueFunction>(
                valueFunction, _isSingleCall, referenceInfoBuilder.build(), _trampoline);
            return _jsContext.newFunction(jsCallable, toJSExceptionTracker(exceptionTracker));
        }
    }

    Ref<ValueFunction> toValueFunction(const JSValueRef* receiver,
                                       const JSValueRef& function,
                                       const ReferenceInfoBuilder& referenceInfoBuilder,
                                       ExceptionTracker& exceptionTracker) final {
        auto result = jsFunctionToFunction(
            _jsContext,
            function.get(),
            referenceInfoBuilder,
            toJSExceptionTracker(exceptionTracker),
            [receiver, trampoline = &_trampoline, isSingleCall = _isSingleCall](
                IJavaScriptContext& jsContext,
                const JSValue& jsValue,
                const ReferenceInfo& referenceInfo,
                JSExceptionTracker& exceptionTracker) -> Ref<ValueFunctionWithJSValue> {
                if (receiver != nullptr) {
                    return makeShared<TypedValueFunctionWithJSValueAndReceiver>(jsContext,
                                                                                receiver->get(),
                                                                                jsValue,
                                                                                isSingleCall,
                                                                                referenceInfo,
                                                                                exceptionTracker,
                                                                                *trampoline);
                } else {
                    return makeShared<TypedValueFunctionWithJSValue>(
                        jsContext, jsValue, isSingleCall, referenceInfo, exceptionTracker, *trampoline);
                }
            });
        if (result != nullptr) {
            auto vf = castOrNull<ValueFunctionWithJSValue>(result);
            if (vf != nullptr) {
                vf->setAllowSyncCall(_allowSyncCall);
            }
        }
        return result;
    }

private:
    IJavaScriptContext& _jsContext;
    Ref<PlatformFunctionTrampoline<JSValueRef>> _trampoline;
    bool _isSingleCall;
    bool _allowSyncCall;
};

JavaScriptValueDelegate::JavaScriptValueDelegate(IJavaScriptContext& jsContext,
                                                 bool disableProxyObjectStore,
                                                 JSExceptionTracker& exceptionTracker)
    : _jsContext(&jsContext), _objectStore(jsContext, disableProxyObjectStore, exceptionTracker) {}

JavaScriptValueDelegate::~JavaScriptValueDelegate() = default;

void JavaScriptValueDelegate::teardown() {
    _objectStore.teardown();
    _promiseExecutor = nullptr;
    _promiseExecutorJS = JSValueRef();
    _jsContext = nullptr;
}

JSValueRef JavaScriptValueDelegate::newVoid() {
    return _jsContext->newUndefined();
}

JSValueRef JavaScriptValueDelegate::newNull() {
    return _jsContext->newUndefined();
}

JSValueRef JavaScriptValueDelegate::newInt(int32_t value, ExceptionTracker& exceptionTracker) {
    return _jsContext->newNumber(value);
}

JSValueRef JavaScriptValueDelegate::newIntObject(int32_t value, ExceptionTracker& exceptionTracker) {
    return _jsContext->newNumber(value);
}

JSValueRef JavaScriptValueDelegate::newLong(int64_t value, ExceptionTracker& exceptionTracker) {
    return _jsContext->newLong(value, toJSExceptionTracker(exceptionTracker));
}

JSValueRef JavaScriptValueDelegate::newLongObject(int64_t value, ExceptionTracker& exceptionTracker) {
    return _jsContext->newLong(value, toJSExceptionTracker(exceptionTracker));
}

JSValueRef JavaScriptValueDelegate::newDouble(double value, ExceptionTracker& exceptionTracker) {
    return _jsContext->newNumber(value);
}

JSValueRef JavaScriptValueDelegate::newDoubleObject(double value, ExceptionTracker& exceptionTracker) {
    return _jsContext->newNumber(value);
}

JSValueRef JavaScriptValueDelegate::newBool(bool value, ExceptionTracker& exceptionTracker) {
    return _jsContext->newBool(value);
}

JSValueRef JavaScriptValueDelegate::newBoolObject(bool value, ExceptionTracker& exceptionTracker) {
    return _jsContext->newBool(value);
}

JSValueRef JavaScriptValueDelegate::newStringUTF8(std::string_view str, ExceptionTracker& exceptionTracker) {
    return _jsContext->newStringUTF8(str, toJSExceptionTracker(exceptionTracker));
}

JSValueRef JavaScriptValueDelegate::newStringUTF16(std::u16string_view str, ExceptionTracker& exceptionTracker) {
    return _jsContext->newStringUTF16(str, toJSExceptionTracker(exceptionTracker));
}

JSValueRef JavaScriptValueDelegate::newByteArray(const BytesView& bytes, ExceptionTracker& exceptionTracker) {
    return newTypedArrayFromBytesView(
        *_jsContext, TypedArrayType::Uint8Array, bytes, toJSExceptionTracker(exceptionTracker));
}

JSValueRef JavaScriptValueDelegate::newTypedArray(TypedArrayType arrayType,
                                                  const BytesView& bytes,
                                                  ExceptionTracker& exceptionTracker) {
    return newTypedArrayFromBytesView(*_jsContext, arrayType, bytes, toJSExceptionTracker(exceptionTracker));
}

JSValueRef JavaScriptValueDelegate::newUntyped(const Value& value,
                                               const ReferenceInfoBuilder& referenceInfoBuilder,
                                               ExceptionTracker& exceptionTracker) {
    return valueToJSValue(*_jsContext, value, referenceInfoBuilder, toJSExceptionTracker(exceptionTracker));
}

JSValueRef JavaScriptValueDelegate::newMap(size_t /*capacity*/, ExceptionTracker& exceptionTracker) {
    return _jsContext->newObject(toJSExceptionTracker(exceptionTracker));
}

void JavaScriptValueDelegate::setMapEntry(const JSValueRef& map,
                                          const JSValueRef& key,
                                          const JSValueRef& value,
                                          ExceptionTracker& exceptionTracker) {
    _jsContext->setObjectProperty(map.get(), key.get(), value.get(), toJSExceptionTracker(exceptionTracker));
}

size_t JavaScriptValueDelegate::getMapEstimatedLength(const JSValueRef& map, ExceptionTracker& exceptionTracker) {
    // don't have an efficient way to check this
    return 0;
}

void JavaScriptValueDelegate::visitMapKeyValues(const JSValueRef& map,
                                                PlatformMapValueVisitor<JSValueRef>& visitor,
                                                ExceptionTracker& exceptionTracker) {
    JavaScriptMapValueVisitorBridge visitorBridge(visitor);
    _jsContext->visitObjectPropertyNames(map.get(), toJSExceptionTracker(exceptionTracker), visitorBridge);
}

JSValueRef JavaScriptValueDelegate::newES6Collection(CollectionType type, ExceptionTracker& exceptionTracker) {
    static StringBox kES6CollectionClassName[] = {STRING_LITERAL("Map"), STRING_LITERAL("Set")};
    auto ctor = _jsContext->getPropertyFromGlobalObjectCached(kES6CollectionClassName[static_cast<int>(type)],
                                                              toJSExceptionTracker(exceptionTracker));
    if (!exceptionTracker) {
        return _jsContext->newUndefined();
    }
    JSFunctionCallContext ctx(*_jsContext, nullptr, 0, toJSExceptionTracker(exceptionTracker));
    return _jsContext->callObjectAsConstructor(ctor.get(), ctx);
}

void JavaScriptValueDelegate::setES6CollectionEntry(const JSValueRef& collection,
                                                    CollectionType type,
                                                    std::initializer_list<JSValueRef> items,
                                                    ExceptionTracker& exceptionTracker) {
    static StringBox kES6CollectionSetMethod[] = {STRING_LITERAL("set"), STRING_LITERAL("add")};
    auto method = _jsContext->getPropertyNameCached(kES6CollectionSetMethod[static_cast<int>(type)]);
    JSFunctionCallContext ctx(*_jsContext, std::data(items), items.size(), toJSExceptionTracker(exceptionTracker));
    _jsContext->callObjectProperty(collection.get(), method, ctx);
}

void JavaScriptValueDelegate::visitES6Collection(const JSValueRef& collection,
                                                 PlatformMapValueVisitor<JSValueRef>& visitor,
                                                 ExceptionTracker& exceptionTracker) {
    JSFunctionCallContext ctx(*_jsContext, nullptr, 0, toJSExceptionTracker(exceptionTracker));
    auto iter = _jsContext->callObjectProperty(collection.get(), "entries", ctx);
    while (true) {
        auto result = _jsContext->callObjectProperty(iter.get(), "next", ctx);
        auto done = _jsContext->getObjectProperty(result.get(), "done", toJSExceptionTracker(exceptionTracker));
        if (valueToBool(done, toJSExceptionTracker(exceptionTracker))) {
            break;
        }
        auto value = _jsContext->getObjectProperty(result.get(), "value", toJSExceptionTracker(exceptionTracker));
        visitor.visit(_jsContext->getObjectPropertyForIndex(value.get(), 0, toJSExceptionTracker(exceptionTracker)),
                      _jsContext->getObjectPropertyForIndex(value.get(), 1, toJSExceptionTracker(exceptionTracker)),
                      exceptionTracker);
    }
}

JSValueRef JavaScriptValueDelegate::newDate(double millisecondsSinceEpoch, ExceptionTracker& exceptionTracker) {
    static auto kDateClassName = STRING_LITERAL("Date");
    auto ctor = _jsContext->getPropertyFromGlobalObjectCached(kDateClassName, toJSExceptionTracker(exceptionTracker));
    if (!exceptionTracker) {
        return _jsContext->newUndefined();
    }
    JSValueRef millisecondsJs = _jsContext->newNumber(millisecondsSinceEpoch);
    JSFunctionCallContext ctx(*_jsContext, &millisecondsJs, 1, toJSExceptionTracker(exceptionTracker));
    return _jsContext->callObjectAsConstructor(ctor.get(), ctx);
}

PlatformArrayBuilder<JSValueRef> JavaScriptValueDelegate::newArrayBuilder(size_t capacity,
                                                                          ExceptionTracker& exceptionTracker) {
    return PlatformArrayBuilder<JSValueRef>(_jsContext->newArray(capacity, toJSExceptionTracker(exceptionTracker)));
}

void JavaScriptValueDelegate::setArrayEntry(const PlatformArrayBuilder<JSValueRef>& arrayBuilder,
                                            size_t index,
                                            const JSValueRef& value,
                                            ExceptionTracker& exceptionTracker) {
    _jsContext->setObjectPropertyIndex(
        arrayBuilder.getBuilder().get(), index, value.get(), toJSExceptionTracker(exceptionTracker));
}

JSValueRef JavaScriptValueDelegate::finalizeArray(PlatformArrayBuilder<JSValueRef>& arrayBuilder,
                                                  ExceptionTracker& exceptionTracker) {
    return std::move(arrayBuilder.getBuilder());
}

PlatformArrayIterator<JSValueRef> JavaScriptValueDelegate::newArrayIterator(const JSValueRef& array,
                                                                            ExceptionTracker& exceptionTracker) {
    return PlatformArrayIterator<JSValueRef>(
        array, jsArrayGetLength(*_jsContext, array.get(), toJSExceptionTracker(exceptionTracker)));
}

JSValueRef JavaScriptValueDelegate::getArrayItem(const PlatformArrayIterator<JSValueRef>& arrayIterator,
                                                 size_t index,
                                                 ExceptionTracker& exceptionTracker) {
    return _jsContext->getObjectPropertyForIndex(
        arrayIterator.getIterator().get(), index, toJSExceptionTracker(exceptionTracker));
}

class JSBridgedPromiseCallback final : public PromiseCallback {
public:
    JSBridgedPromiseCallback(IJavaScriptContext& jsContext,
                             JSValueRef&& resolve,
                             JSValueRef&& reject,
                             const Ref<ValueMarshaller<JSValueRef>>& valueMarshaller)
        : _context(Context::currentRoot()),
          _taskScheduler(weakRef(jsContext.getTaskScheduler())),
          _resolve(jsContext.stashJSValue(std::move(resolve))),
          _reject(jsContext.stashJSValue(std::move(reject))),
          _valueMarshaller(valueMarshaller) {}

    ~JSBridgedPromiseCallback() override {
        auto taskScheduler = _taskScheduler.lock();
        if (taskScheduler != nullptr) {
            taskScheduler->dispatchOnJsThreadAsync(
                _context, [resolve = _resolve, reject = _reject](const JavaScriptEntryParameters& jsEntry) {
                    jsEntry.jsContext.removedStashedJSValue(resolve);
                    jsEntry.jsContext.removedStashedJSValue(reject);
                });
        }
    }

    void onSuccess(const Value& value) final {
        fulfill(value);
    }

    void onFailure(const Error& error) final {
        fulfill(error);
    }

private:
    Ref<Context> _context;
    Weak<JavaScriptTaskScheduler> _taskScheduler;
    JSValueID _resolve;
    JSValueID _reject;
    Ref<ValueMarshaller<JSValueRef>> _valueMarshaller;

    void fulfill(const Result<Value>& result) {
        auto taskScheduler = _taskScheduler.lock();
        if (taskScheduler != nullptr) {
            taskScheduler->dispatchOnJsThreadAsyncAfter(
                _context, 0, [self = Valdi::strongSmallRef(this), result](const JavaScriptEntryParameters& jsEntry) {
                    self->doFullfill(result, jsEntry);
                });
        }
    }

    void doFullfill(const Result<Value>& result, const JavaScriptEntryParameters& jsEntry) {
        auto resolve = jsEntry.jsContext.getStashedJSValue(_resolve);
        auto reject = jsEntry.jsContext.getStashedJSValue(_reject);
        if (!resolve || !reject) {
            return;
        }

        if (result) {
            auto jsResult =
                _valueMarshaller->unmarshall(result.value(), ReferenceInfoBuilder(), jsEntry.exceptionTracker);
            if (!jsEntry.exceptionTracker) {
                // Propagate exception to the reject
                auto exception = jsEntry.exceptionTracker.getExceptionAndClear();
                JSFunctionCallContext callContext(jsEntry.jsContext, &exception, 1, jsEntry.exceptionTracker);
                jsEntry.jsContext.callObjectAsFunction(reject.value(), callContext);
            } else {
                // Call resolve()
                JSFunctionCallContext callContext(jsEntry.jsContext, &jsResult, 1, jsEntry.exceptionTracker);
                jsEntry.jsContext.callObjectAsFunction(resolve.value(), callContext);
            }
        } else {
            auto jsError = convertValdiErrorToJSError(jsEntry.jsContext, result.error(), jsEntry.exceptionTracker);
            if (!jsEntry.exceptionTracker) {
                // Can't create error, stop now
                return;
            }

            JSFunctionCallContext callContext(jsEntry.jsContext, &jsError, 1, jsEntry.exceptionTracker);
            jsEntry.jsContext.callObjectAsFunction(reject.value(), callContext);
        }
    }
};

JSValueRef JavaScriptValueDelegate::newBridgedPromise(const Ref<Promise>& promise,
                                                      const Ref<ValueMarshaller<JSValueRef>>& valueMarshaller,
                                                      ExceptionTracker& exceptionTracker) {
    auto& jsExceptionTracker = toJSExceptionTracker(exceptionTracker);

    if (_promiseExecutor == nullptr) {
        auto promiseExecutor = makeShared<PromiseExecutor>();
        auto promiseExecutorJS = _jsContext->newFunction(promiseExecutor, jsExceptionTracker);
        if (!jsExceptionTracker) {
            return JSValueRef();
        }

        _promiseExecutor = std::move(promiseExecutor);
        _promiseExecutorJS = JSValueRef::makeRetained(*_jsContext, promiseExecutorJS.get());
    }

    auto promiseJS = _jsContext->newPromise(_promiseExecutorJS, jsExceptionTracker);
    if (!jsExceptionTracker) {
        return JSValueRef();
    }

    JSPromiseCancel::attachToJSPromise(*_jsContext, promiseJS.get(), promise, jsExceptionTracker);
    if (!jsExceptionTracker) {
        return JSValueRef();
    }

    promise->onComplete(makeShared<JSBridgedPromiseCallback>(
        *_jsContext, _promiseExecutor->extractResolve(), _promiseExecutor->extractReject(), valueMarshaller));

    return promiseJS;
}

Ref<PlatformObjectClassDelegate<JSValueRef>> JavaScriptValueDelegate::newObjectClass(
    const Ref<ClassSchema>& schema, ExceptionTracker& exceptionTracker) {
    auto classDelegate = JavaScriptClassDelegate::make(*_jsContext, schema->getPropertiesSize());

    size_t propertyIndex = 0;
    for (const auto& property : *schema) {
        classDelegate->setPropertyName(propertyIndex, _jsContext->newPropertyName(property.name.toStringView()));
        propertyIndex++;
    }

    return classDelegate;
}

Ref<PlatformEnumClassDelegate<JSValueRef>> JavaScriptValueDelegate::newEnumClass(const Ref<EnumSchema>& schema,
                                                                                 ExceptionTracker& exceptionTracker) {
    auto enumClassDelegate = JavaScriptEnumClassDelegate::make(*_jsContext, schema);
    size_t caseIndex = 0;
    for (const auto& enumCase : *schema) {
        auto enumValue =
            valueToJSValue(*_jsContext, enumCase.value, ReferenceInfoBuilder(), toJSExceptionTracker(exceptionTracker));
        if (!exceptionTracker) {
            return nullptr;
        }
        enumClassDelegate->setEnumCaseForIndex(caseIndex, std::move(enumValue));
        caseIndex++;
    }

    return enumClassDelegate;
}

Ref<PlatformFunctionClassDelegate<JSValueRef>> JavaScriptValueDelegate::newFunctionClass(
    const Ref<PlatformFunctionTrampoline<JSValueRef>>& trampoline,
    const Ref<ValueFunctionSchema>& schema,
    ExceptionTracker& exceptionTracker) {
    return makeShared<JavaScriptFunctionClassDelegate>(
        *_jsContext, trampoline, schema->getAttributes().isSingleCall(), schema->getAttributes().allowSyncCall());
}

bool JavaScriptValueDelegate::valueIsNull(const JSValueRef& value) const {
    return _jsContext->isValueNull(value.get()) || _jsContext->isValueUndefined(value.get());
}

int32_t JavaScriptValueDelegate::valueToInt(const JSValueRef& value, ExceptionTracker& exceptionTracker) const {
    return _jsContext->valueToInt(value.get(), toJSExceptionTracker(exceptionTracker));
}

int64_t JavaScriptValueDelegate::valueToLong(const JSValueRef& value, ExceptionTracker& exceptionTracker) const {
    return _jsContext->valueToLong(value.get(), toJSExceptionTracker(exceptionTracker)).toInt64();
}

double JavaScriptValueDelegate::valueToDouble(const JSValueRef& value, ExceptionTracker& exceptionTracker) const {
    return _jsContext->valueToDouble(value.get(), toJSExceptionTracker(exceptionTracker));
}

bool JavaScriptValueDelegate::valueToBool(const JSValueRef& value, ExceptionTracker& exceptionTracker) const {
    return _jsContext->valueToBool(value.get(), toJSExceptionTracker(exceptionTracker));
}

int32_t JavaScriptValueDelegate::valueObjectToInt(const JSValueRef& value, ExceptionTracker& exceptionTracker) const {
    return _jsContext->valueToInt(value.get(), toJSExceptionTracker(exceptionTracker));
}

int64_t JavaScriptValueDelegate::valueObjectToLong(const JSValueRef& value, ExceptionTracker& exceptionTracker) const {
    return _jsContext->valueToLong(value.get(), toJSExceptionTracker(exceptionTracker)).toInt64();
}

double JavaScriptValueDelegate::valueObjectToDouble(const JSValueRef& value, ExceptionTracker& exceptionTracker) const {
    return _jsContext->valueToDouble(value.get(), toJSExceptionTracker(exceptionTracker));
}

bool JavaScriptValueDelegate::valueObjectToBool(const JSValueRef& value, ExceptionTracker& exceptionTracker) const {
    return _jsContext->valueToBool(value.get(), toJSExceptionTracker(exceptionTracker));
}

Ref<Promise> JavaScriptValueDelegate::valueToPromise(const JSValueRef& value,
                                                     const Ref<ValueMarshaller<JSValueRef>>& valueMarshaller,
                                                     const ReferenceInfoBuilder& referenceInfoBuilder,
                                                     ExceptionTracker& exceptionTracker) const {
    return makeShared<JSPromise>(*_jsContext,
                                 value.get(),
                                 referenceInfoBuilder.build(),
                                 valueMarshaller,
                                 toJSExceptionTracker(exceptionTracker));
}

Ref<StaticString> JavaScriptValueDelegate::valueToString(const JSValueRef& value,
                                                         ExceptionTracker& exceptionTracker) const {
    return _jsContext->valueToStaticString(value.get(), toJSExceptionTracker(exceptionTracker));
}

BytesView JavaScriptValueDelegate::valueToByteArray(const JSValueRef& value,
                                                    const ReferenceInfoBuilder& referenceInfoBuilder,
                                                    ExceptionTracker& exceptionTracker) const {
    auto typedArray = jsTypedArrayToValueTypedArray(
        *_jsContext, value.get(), referenceInfoBuilder, toJSExceptionTracker(exceptionTracker));
    if (typedArray == nullptr) {
        return BytesView();
    }
    return typedArray->getBuffer();
}

Ref<ValueTypedArray> JavaScriptValueDelegate::valueToTypedArray(const JSValueRef& value,
                                                                const ReferenceInfoBuilder& referenceInfoBuilder,
                                                                ExceptionTracker& exceptionTracker) const {
    return jsTypedArrayToValueTypedArray(
        *_jsContext, value.get(), referenceInfoBuilder, toJSExceptionTracker(exceptionTracker));
}

double JavaScriptValueDelegate::dateToDouble(const JSValueRef& value, ExceptionTracker& exceptionTracker) const {
    JSFunctionCallContext ctx(*_jsContext, nullptr, 0, toJSExceptionTracker(exceptionTracker));
    auto millisecondsJs = _jsContext->callObjectProperty(value.get(), "getTime", ctx);
    return _jsContext->valueToDouble(millisecondsJs.get(), toJSExceptionTracker(exceptionTracker));
}

BytesView JavaScriptValueDelegate::encodeProto(const JSValueRef& proto,
                                               const ReferenceInfoBuilder& referenceInfoBuilder,
                                               ExceptionTracker& exceptionTracker) const {
    JSFunctionCallContext ctx(*_jsContext, nullptr, 0, toJSExceptionTracker(exceptionTracker));
    auto bytes = _jsContext->callObjectProperty(proto.get(), "encode", ctx);
    return valueToByteArray(bytes, referenceInfoBuilder, exceptionTracker);
}

/* In order for this to work, the TS code in this context must have a global namespace `protoSupport` and under it:
 *
 * protoSupport
 *   Arena -> { Arena } from 'valdi_protobuf/src/Arena'
 *   protoLibs -> TS protobuf namespace map
 *     "djinni.test": proto.djinni.test
 *
 * This is global namespace layout is created by calling `registerProtobufLib` in ProtoSupport.ts
 */
JSValueRef JavaScriptValueDelegate::decodeProto(const BytesView& bytes,
                                                const std::vector<std::string_view>& classNameParts,
                                                const ReferenceInfoBuilder& referenceInfoBuilder,
                                                ExceptionTracker& exceptionTracker) const {
    static auto kProtoSupportName = STRING_LITERAL("protoSupport");
    auto protoSupport =
        _jsContext->getPropertyFromGlobalObjectCached(kProtoSupportName, toJSExceptionTracker(exceptionTracker));
    if (!exceptionTracker) {
        return _jsContext->newUndefined();
    }
    auto arenaCtor = _jsContext->getObjectProperty(protoSupport.get(), "Arena", toJSExceptionTracker(exceptionTracker));
    if (!exceptionTracker) {
        return _jsContext->newUndefined();
    }
    auto protoClass =
        _jsContext->getObjectProperty(protoSupport.get(), "protoLibs", toJSExceptionTracker(exceptionTracker));
    if (!exceptionTracker) {
        return _jsContext->newUndefined();
    }
    for (auto classNamePart : classNameParts) {
        protoClass =
            _jsContext->getObjectProperty(protoClass.get(), classNamePart, toJSExceptionTracker(exceptionTracker));
        if (!exceptionTracker) {
            return _jsContext->newUndefined();
        }
    }
    auto decode = _jsContext->getObjectProperty(protoClass.get(), "decode", toJSExceptionTracker(exceptionTracker));
    if (!exceptionTracker) {
        return _jsContext->newUndefined();
    }

    auto array = newTypedArrayFromBytesView(
        *_jsContext, TypedArrayType::Uint8Array, bytes, toJSExceptionTracker(exceptionTracker));
    if (!exceptionTracker) {
        return _jsContext->newUndefined();
    }
    JSFunctionCallContext areanaCtx(*_jsContext, nullptr, 0, toJSExceptionTracker(exceptionTracker));
    auto arena = _jsContext->callObjectAsConstructor(arenaCtor.get(), areanaCtx);
    if (!exceptionTracker) {
        return _jsContext->newUndefined();
    }

    std::initializer_list<JSValueRef> args = {arena, array};
    JSFunctionCallContext decodeCtx(*_jsContext, std::data(args), args.size(), toJSExceptionTracker(exceptionTracker));
    auto proto = _jsContext->callObjectAsFunction(decode.get(), decodeCtx);
    if (!exceptionTracker) {
        return _jsContext->newUndefined();
    }
    return proto;
}

Value JavaScriptValueDelegate::valueToUntyped(const JSValueRef& value,
                                              const ReferenceInfoBuilder& referenceInfoBuilder,
                                              ExceptionTracker& exceptionTracker) const {
    return jsValueToValue(*_jsContext, value.get(), referenceInfoBuilder, toJSExceptionTracker(exceptionTracker));
}

PlatformObjectStore<JSValueRef>& JavaScriptValueDelegate::getObjectStore() {
    return getJsObjectStore();
}

JavaScriptObjectStore& JavaScriptValueDelegate::getJsObjectStore() {
    return _objectStore;
}

} // namespace Valdi
