#include "valdi_core/cpp/Marshalling/RustValdi.hpp"

#include "valdi_core/cpp/Marshalling/CppGeneratedExportedFunctionUtils.hpp"
#include "valdi_core/cpp/Marshalling/CppMarshaller.hpp"
#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#include "valdi_core/cpp/Utils/Error.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/Promise.hpp"
#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedObject.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#include "valdi_core/ModuleFactory.hpp"

#include <string>
#include <memory>

namespace {
#if defined(VALDI_RUST_TRACK_HANDLES)
std::atomic<size_t> gLiveRustHandleCount{0};
#endif

class RustBufferOwner final : public Valdi::SimpleRefCountable {
public:
    explicit RustBufferOwner(ValdiRustBuffer buffer) noexcept : _buffer(buffer) {}

    ~RustBufferOwner() override {
        valdi_rust_buffer_drop(_buffer);
    }

private:
    ValdiRustBuffer _buffer;
};

class RustValueFunction final : public Valdi::ValueFunction {
public:
    RustValueFunction(void* instance, ValdiRustValueInvokeFunction invoke, ValdiRustValueDropFunction drop)
        : _instance(instance), _invoke(invoke), _drop(drop) {}

    ~RustValueFunction() override {
        _drop(_instance);
    }

    Valdi::Value operator()(const Valdi::ValueFunctionCallContext& callContext) noexcept final {
        Valdi::Marshaller marshaller(callContext.getExceptionTracker());
        const auto argumentCount = callContext.getParametersSize();
        for (size_t index = 0; index < callContext.getParametersSize(); index++) {
            marshaller.push(callContext.getParameter(index));
        }
        const auto status = _invoke(_instance, {&marshaller, argumentCount});
        if (status.code != 0) {
            std::string message(reinterpret_cast<const char*>(status.message.data), status.message.len);
            valdi_rust_buffer_drop(status.message);
            callContext.getExceptionTracker().onError(message);
            return Valdi::Value::undefined();
        }
        if (static_cast<size_t>(marshaller.size()) != argumentCount + 1) {
            callContext.getExceptionTracker().onError("Rust callback did not produce exactly one Valdi value");
            return Valdi::Value::undefined();
        }
        return marshaller.get(static_cast<int>(argumentCount));
    }

    std::string_view getFunctionType() const final {
        return "rust callback";
    }

private:
    void* _instance;
    ValdiRustValueInvokeFunction _invoke;
    ValdiRustValueDropFunction _drop;
};

class RustModuleState final : public Valdi::SharedPtrRefCountable {
public:
    RustModuleState(void* instance, Valdi::RustModuleDropFunction drop) : _instance(instance), _drop(drop) {}

    ~RustModuleState() override {
        _drop(_instance);
    }

    void* instance() const {
        return _instance;
    }

private:
    void* _instance;
    Valdi::RustModuleDropFunction _drop;
};

class RustModuleMethod final : public Valdi::ValueFunction {
public:
    RustModuleMethod(const Valdi::Ref<RustModuleState>& state,
                     size_t methodIndex,
                     Valdi::RustModuleInvokeFunction invoke)
        : _state(state), _methodIndex(methodIndex), _invoke(invoke) {}

    Valdi::Value operator()(const Valdi::ValueFunctionCallContext& callContext) noexcept final {
        Valdi::Marshaller marshaller(callContext.getExceptionTracker());
        const auto argumentCount = callContext.getParametersSize();
        for (size_t index = 0; index < callContext.getParametersSize(); index++) {
            marshaller.push(callContext.getParameter(index));
        }
        const auto status = _invoke(_state->instance(), _methodIndex, {&marshaller, argumentCount});
        if (status.code != 0) {
            std::string message(reinterpret_cast<const char*>(status.message.data), status.message.len);
            valdi_rust_buffer_drop(status.message);
            callContext.getExceptionTracker().onError(message);
            return Valdi::Value::undefined();
        }
        if (static_cast<size_t>(marshaller.size()) != argumentCount + 1) {
            callContext.getExceptionTracker().onError("Rust module did not produce exactly one Valdi value");
            return Valdi::Value::undefined();
        }
        return marshaller.get(static_cast<int>(argumentCount));
    }

    std::string_view getFunctionType() const final {
        return "rust method";
    }

private:
    Valdi::Ref<RustModuleState> _state;
    size_t _methodIndex;
    Valdi::RustModuleInvokeFunction _invoke;
};

class RustModuleProxy final : public Valdi::ValueTypedProxyObject {
public:
    RustModuleProxy(const Valdi::Ref<Valdi::ValueTypedObject>& object,
                    const Valdi::Ref<RustModuleState>& state,
                    std::string_view type)
        : ValueTypedProxyObject(object), _state(state), _type(type) {}

    std::string_view getType() const final {
        return _type;
    }

private:
    Valdi::Ref<RustModuleState> _state;
    std::string_view _type;
};

class RustModuleFactory final : public snap::valdi_core::ModuleFactory {
public:
    RustModuleFactory(const char* modulePath,
                      Valdi::RegisteredCppGeneratedClass* schema,
                      size_t methodCount,
                      Valdi::RustModuleCreateFunction create,
                      Valdi::RustModuleDropFunction drop,
                      Valdi::RustModuleInvokeFunction invoke)
        : _modulePath(Valdi::StringBox::fromCString(modulePath)),
          _schema(schema),
          _methodCount(methodCount),
          _create(create),
          _drop(drop),
          _invoke(invoke) {}

    Valdi::StringBox getModulePath() final {
        return _modulePath;
    }

    Valdi::Value loadModule() final {
        Valdi::SimpleExceptionTracker exceptionTracker;
        auto schema = _schema->getResolvedClassSchema(exceptionTracker);
        if (!exceptionTracker) {
            return Valdi::Value(exceptionTracker.extractError());
        }
        if (schema->getPropertiesSize() != _methodCount) {
            return Valdi::Value(Valdi::Error("Rust module method count does not match its schema"));
        }

        auto* instance = _create();
        if (instance == nullptr) {
            return Valdi::Value(Valdi::Error("Rust module factory panicked while creating its instance"));
        }
        std::unique_ptr<void, Valdi::RustModuleDropFunction> instanceOwner(instance, _drop);
        auto state = Valdi::makeShared<RustModuleState>(instance, _drop);
        instanceOwner.release();
        auto object = Valdi::ValueTypedObject::make(schema);
        for (size_t index = 0; index < _methodCount; index++) {
            object->setProperty(index, Valdi::Value(Valdi::makeShared<RustModuleMethod>(state, index, _invoke)));
        }
        return Valdi::Value(Valdi::makeShared<RustModuleProxy>(object, state, "Rust Module"));
    }

private:
    Valdi::StringBox _modulePath;
    Valdi::RegisteredCppGeneratedClass* _schema;
    size_t _methodCount;
    Valdi::RustModuleCreateFunction _create;
    Valdi::RustModuleDropFunction _drop;
    Valdi::RustModuleInvokeFunction _invoke;
};

constexpr ValdiRustStatus rustSuccessStatus() noexcept {
    return {0, {nullptr, 0, 0}};
}

ValdiRustStatus rustErrorStatus(const Valdi::Error& error) noexcept {
    return Valdi::makeRustErrorStatus(error.getMessage().toStringView());
}

ValdiRustStatus rustErrorStatus(Valdi::ExceptionTracker& exceptionTracker) noexcept {
    return rustErrorStatus(exceptionTracker.extractError());
}

const Valdi::Value& rustValueRef(const void* value) {
    return *static_cast<const Valdi::Value*>(value);
}

Valdi::Marshaller& rustMarshaller(void* marshaller) {
    return *static_cast<Valdi::Marshaller*>(marshaller);
}

const Valdi::Marshaller& rustMarshaller(const void* marshaller) {
    return *static_cast<const Valdi::Marshaller*>(marshaller);
}

ValdiRustValueKind rustValueKind(const Valdi::Value& value) {
    if (value.isNull()) {
        return ValdiRustValueKindNull;
    }
    if (value.isUndefined()) {
        return ValdiRustValueKindUndefined;
    }
    if (value.isBool()) {
        return ValdiRustValueKindBool;
    }
    if (value.isBigInt()) {
        return ValdiRustValueKindBigInt;
    }
    if (value.isLong()) {
        return ValdiRustValueKindLong;
    }
    if (value.isNumber()) {
        return ValdiRustValueKindNumber;
    }
    if (value.isString()) {
        return ValdiRustValueKindString;
    }
    if (value.isTypedArray()) {
        return ValdiRustValueKindBytes;
    }
    return ValdiRustValueKindObject;
}
} // namespace

void valdi_rust_handle_retain(ValdiRustHandle* handle) {
    if (handle != nullptr) {
        handle->refCount.fetch_add(1, std::memory_order_relaxed);
    }
}

void valdi_rust_handle_release(ValdiRustHandle* handle) {
    if (handle != nullptr && handle->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        handle->destroy(handle);
    }
}

size_t valdi_rust_handle_live_count() {
#if defined(VALDI_RUST_TRACK_HANDLES)
    return gLiveRustHandleCount.load(std::memory_order_acquire);
#else
    return 0;
#endif
}

ValdiRustStatus valdi_rust_register_schema(ValdiRustSlice schema,
                                           ValdiRustSchemaKind kind,
                                           ValdiRustArray dependencies,
                                           void** out) {
    if (kind != ValdiRustSchemaKindClass && kind != ValdiRustSchemaKindGeneric && kind != ValdiRustSchemaKindEnum) {
        return Valdi::makeRustErrorStatus("Invalid Rust schema kind");
    }

    const auto* dependencyData = static_cast<const ValdiRustSchemaFunction*>(dependencies.data);
    std::vector<ValdiRustSchemaFunction> dependencyFunctions;
    if (dependencies.len != 0) {
        dependencyFunctions.assign(dependencyData, dependencyData + dependencies.len);
    }

    Valdi::GetTypeReferencesFunction getTypeReferences;
    if (!dependencyFunctions.empty()) {
        getTypeReferences = [dependencyFunctions = std::move(dependencyFunctions)] {
            std::vector<Valdi::RegisteredCppGeneratedClass*> result;
            result.reserve(dependencyFunctions.size());
            for (const auto dependency : dependencyFunctions) {
                result.emplace_back(static_cast<Valdi::RegisteredCppGeneratedClass*>(dependency()));
            }
            return result;
        };
    }

    auto schemaString = std::string(reinterpret_cast<const char*>(schema.data), schema.len);
    auto* registry = Valdi::ValueSchemaRegistry::sharedInstance().get();
    switch (kind) {
        case ValdiRustSchemaKindClass:
            *out = new Valdi::RegisteredCppGeneratedClass(
                registry, std::move(schemaString), std::move(getTypeReferences), {});
            break;
        case ValdiRustSchemaKindGeneric:
            *out = new Valdi::RegisteredCppGeneratedClass(
                registry,
                std::move(schemaString),
                std::move(getTypeReferences),
                [](Valdi::ExceptionTracker&) -> std::vector<Valdi::ValueSchema> { return {}; });
            break;
        case ValdiRustSchemaKindEnum:
            *out = new Valdi::RegisteredCppGeneratedClass(registry, std::move(schemaString), true);
            break;
    }
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_register_module(ValdiRustSlice modulePath,
                                           void* schema,
                                           size_t methodCount,
                                           ValdiRustModuleCreateFunction create,
                                           ValdiRustModuleDropFunction drop,
                                           ValdiRustModuleInvokeFunction invoke) {
    if (schema == nullptr) {
        return Valdi::makeRustErrorStatus("Cannot register a Rust module without its schema");
    }
    auto path = std::string(reinterpret_cast<const char*>(modulePath.data), modulePath.len);
    auto* registeredSchema = static_cast<Valdi::RegisteredCppGeneratedClass*>(schema);
    Valdi::ModuleFactoryRegistry::sharedInstance()->registerModuleFactoryFn(
        [path = std::move(path), registeredSchema, methodCount, create, drop, invoke] {
            return std::make_shared<RustModuleFactory>(
                path.c_str(), registeredSchema, methodCount, create, drop, invoke);
        });
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_new_proxy(void* schema,
                                           size_t methodCount,
                                           void* instance,
                                           ValdiRustProxyInvokeFunction invoke,
                                           ValdiRustValueDropFunction drop,
                                           ValdiRustHandle** out) {
    return Valdi::makeRustProxyValue(
        static_cast<Valdi::RegisteredCppGeneratedClass*>(schema), methodCount, instance, invoke, drop, out);
}

ValdiRustStatus valdi_rust_resolve_exported_function(
    void* runtime, void* nativeObjectsManager, ValdiRustSlice modulePath, void* schema, ValdiRustHandle** out) {
    if (schema == nullptr) {
        return Valdi::makeRustErrorStatus("Cannot resolve a Rust exported function without its schema");
    }
    auto path = std::string(reinterpret_cast<const char*>(modulePath.data), modulePath.len);
    auto result = Valdi::CppGeneratedExportedFunctionUtils::resolveAsTypedObject(
        *static_cast<snap::valdi_core::JSRuntime*>(runtime),
        *static_cast<const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>*>(nativeObjectsManager),
        path.c_str(),
        *static_cast<Valdi::RegisteredCppGeneratedClass*>(schema));
    if (!result) {
        return rustErrorStatus(result.error());
    }
    *out = Valdi::makeRustHandle(result.value()->getProperty(0));
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_from_marshaller(void* context,
                                                 ValdiRustMarshallerFunction pushValue,
                                                 ValdiRustHandle** out) {
    Valdi::SimpleExceptionTracker exceptionTracker;
    Valdi::Marshaller value(exceptionTracker);
    auto status = pushValue(context, &value);
    if (status.code != 0) {
        return status;
    }
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }
    if (value.size() != 1) {
        return Valdi::makeRustErrorStatus("Rust value materialization did not produce exactly one Valdi value");
    }
    *out = Valdi::makeRustHandle(value.get(0));
    return rustSuccessStatus();
}

const void* valdi_rust_value_borrow(const ValdiRustHandle* value) {
    return &Valdi::getRustHandleValue<Valdi::Value>(value);
}

ValdiRustStatus valdi_rust_value_clone_ref(const void* value, ValdiRustHandle** out) {
    *out = Valdi::makeRustHandle(rustValueRef(value));
    return rustSuccessStatus();
}

const void* valdi_rust_marshaller_get(const void* marshaller, size_t index) {
    const auto& valueMarshaller = rustMarshaller(marshaller);
    if (index >= static_cast<size_t>(const_cast<Valdi::Marshaller&>(valueMarshaller).size())) {
        return &Valdi::Value::undefinedRef();
    }
    return valueMarshaller.getValues() + index;
}

void valdi_rust_marshaller_push_null(void* marshaller) {
    rustMarshaller(marshaller).pushNull();
}

void valdi_rust_marshaller_push_undefined(void* marshaller) {
    rustMarshaller(marshaller).pushUndefined();
}

void valdi_rust_marshaller_push_bool(void* marshaller, uint8_t value) {
    rustMarshaller(marshaller).pushBool(value != 0);
}

void valdi_rust_marshaller_push_number(void* marshaller, double value) {
    rustMarshaller(marshaller).pushDouble(value);
}

void valdi_rust_marshaller_push_long(void* marshaller, int64_t value) {
    rustMarshaller(marshaller).pushLong(value);
}

void valdi_rust_marshaller_push_string(void* marshaller, ValdiRustSlice value) {
    rustMarshaller(marshaller)
        .pushString(
            Valdi::StringBox::fromString(std::string_view(reinterpret_cast<const char*>(value.data), value.len)));
}

ValdiRustStatus valdi_rust_marshaller_push_bytes(void* marshaller, ValdiRustSlice value) {
    auto& valueMarshaller = rustMarshaller(marshaller);
    Valdi::Value result;
    Valdi::CppMarshaller::marshall(valueMarshaller.getExceptionTracker(), Valdi::makeRustBytesView(value), result);
    if (!valueMarshaller.getExceptionTracker()) {
        return rustErrorStatus(valueMarshaller.getExceptionTracker());
    }
    valueMarshaller.push(std::move(result));
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_marshaller_push_owned_bytes(void* marshaller, ValdiRustBuffer value) {
    auto& valueMarshaller = rustMarshaller(marshaller);
    Valdi::Value result;
    Valdi::CppMarshaller::marshall(valueMarshaller.getExceptionTracker(), Valdi::takeRustBytes(value), result);
    if (!valueMarshaller.getExceptionTracker()) {
        return rustErrorStatus(valueMarshaller.getExceptionTracker());
    }
    valueMarshaller.push(std::move(result));
    return rustSuccessStatus();
}

void valdi_rust_marshaller_push_bytes_view(void* marshaller, void* owner) {
    auto* typedArray = static_cast<Valdi::ValueTypedArray*>(owner);
    rustMarshaller(marshaller).push(Valdi::Value(Valdi::Ref<Valdi::ValueTypedArray>(typedArray)));
}

void valdi_rust_marshaller_push_value(void* marshaller, const void* value) {
    rustMarshaller(marshaller).push(rustValueRef(value));
}

size_t valdi_rust_marshaller_push_array(void* marshaller, size_t length) {
    return static_cast<size_t>(rustMarshaller(marshaller).pushArray(static_cast<int>(length)));
}

ValdiRustStatus valdi_rust_marshaller_set_array_item(void* marshaller, size_t array, size_t item) {
    auto& valueMarshaller = rustMarshaller(marshaller);
    valueMarshaller.setArrayItem(static_cast<int>(array), static_cast<int>(item));
    if (!valueMarshaller.getExceptionTracker()) {
        return rustErrorStatus(valueMarshaller.getExceptionTracker());
    }
    return rustSuccessStatus();
}

size_t valdi_rust_marshaller_push_map(void* marshaller, size_t capacity) {
    return static_cast<size_t>(rustMarshaller(marshaller).pushMap(static_cast<int>(capacity)));
}

ValdiRustStatus valdi_rust_marshaller_put_map_property(void* marshaller, size_t map, ValdiRustSlice key) {
    auto& valueMarshaller = rustMarshaller(marshaller);
    valueMarshaller.putMapProperty(
        Valdi::StringBox::fromString(std::string_view(reinterpret_cast<const char*>(key.data), key.len)),
        static_cast<int>(map));
    if (!valueMarshaller.getExceptionTracker()) {
        return rustErrorStatus(valueMarshaller.getExceptionTracker());
    }
    return rustSuccessStatus();
}

ValdiRustValueKind valdi_rust_value_ref_kind(const void* value) {
    return rustValueKind(rustValueRef(value));
}

ValdiRustStatus valdi_rust_value_ref_get_bool(const void* value, uint8_t* out) {
    Valdi::SimpleExceptionTracker exceptionTracker;
    *out = static_cast<uint8_t>(rustValueRef(value).checkedTo<bool>(exceptionTracker));
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_ref_get_number(const void* value, double* out) {
    Valdi::SimpleExceptionTracker exceptionTracker;
    *out = rustValueRef(value).checkedTo<double>(exceptionTracker);
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_ref_get_long(const void* value, int64_t* out) {
    Valdi::SimpleExceptionTracker exceptionTracker;
    *out = rustValueRef(value).checkedTo<int64_t>(exceptionTracker);
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_ref_get_string(const void* value, ValdiRustString* out) {
    const auto& stringValue = rustValueRef(value);
    if (stringValue.isInternedString()) {
        const auto string = stringValue.getTypedRef<Valdi::InternedStringImpl>();
        out->slice = Valdi::makeRustSlice(string->toStringView());
        out->owned = {nullptr, 0, 0};
        return rustSuccessStatus();
    }
    if (stringValue.isStaticString()) {
        const auto* string = stringValue.getStaticString();
        if (string->encoding() == Valdi::StaticString::Encoding::UTF8) {
            out->slice = Valdi::makeRustSlice(string->utf8StringView());
            out->owned = {nullptr, 0, 0};
            return rustSuccessStatus();
        }
        const auto storage = string->utf8Storage();
        out->owned = valdi_rust_buffer_clone({reinterpret_cast<const uint8_t*>(storage.data), storage.length});
        out->slice = {out->owned.data, out->owned.len};
        return rustSuccessStatus();
    }
    return Valdi::makeRustErrorStatus("Valdi value is not a string");
}

ValdiRustStatus valdi_rust_value_ref_get_bytes(const void* value, ValdiRustSlice* out) {
    Valdi::SimpleExceptionTracker exceptionTracker;
    const auto array = rustValueRef(value).checkedTo<Valdi::Ref<Valdi::ValueTypedArray>>(exceptionTracker);
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }
    const auto& bytes = array->getBuffer();
    *out = {bytes.data(), bytes.size()};
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_ref_get_bytes_view(const void* value, ValdiRustBytesView* out) {
    auto* array = const_cast<Valdi::ValueTypedArray*>(rustValueRef(value).getTypedArray());
    if (array == nullptr) {
        return Valdi::makeRustErrorStatus("Valdi value is not a typed array");
    }
    const auto& bytes = array->getBuffer();
    *out = {bytes.data(), bytes.size(), array};
    return rustSuccessStatus();
}

void* valdi_rust_bigint_from_i64(int64_t value) {
    return Valdi::unsafeBridgeRetain(Valdi::BigInt::make(value).get());
}

ValdiRustStatus valdi_rust_bigint_parse(ValdiRustSlice decimal, void** out) {
    Valdi::SimpleExceptionTracker tracker;
    auto value =
        Valdi::BigInt::make(std::string_view(reinterpret_cast<const char*>(decimal.data), decimal.len), tracker);
    if (!tracker) {
        return rustErrorStatus(tracker);
    }
    *out = Valdi::unsafeBridgeRetain(value.get());
    return rustSuccessStatus();
}

void valdi_rust_bigint_retain(void* value) {
    Valdi::unsafeRetain(Valdi::unsafeBridgeUnretained<Valdi::BigInt>(value));
}

void valdi_rust_bigint_release(void* value) {
    Valdi::unsafeBridgeRelease(value);
}

uint8_t valdi_rust_bigint_to_i64(const void* value, int64_t* out) {
    auto integer = Valdi::unsafeBridgeUnretained<Valdi::BigInt>(const_cast<void*>(value))->toInt64();
    if (!integer) {
        return 0;
    }
    *out = *integer;
    return 1;
}

ValdiRustBuffer valdi_rust_bigint_to_string(const void* value) {
    auto text = Valdi::unsafeBridgeUnretained<Valdi::BigInt>(const_cast<void*>(value))->toString();
    return valdi_rust_buffer_clone(Valdi::makeRustSlice(text));
}

ValdiRustStatus valdi_rust_value_ref_get_bigint(const void* value, void** out) {
    auto* integer = rustValueRef(value).getBigInt();
    if (integer == nullptr) {
        return Valdi::makeRustErrorStatus("Valdi value is not a bigint");
    }
    *out = Valdi::unsafeBridgeCast(const_cast<Valdi::BigInt*>(integer));
    return rustSuccessStatus();
}

ValdiRustHandle* valdi_rust_bigint_to_value(void* value) {
    return Valdi::makeRustHandle(Valdi::Value(Valdi::unsafeBridge<Valdi::BigInt>(value)));
}

void valdi_rust_marshaller_push_bigint(void* marshaller, void* value) {
    rustMarshaller(marshaller).push(Valdi::Value(Valdi::unsafeBridge<Valdi::BigInt>(value)));
}

void valdi_rust_bytes_view_retain(void* owner) {
    static_cast<Valdi::ValueTypedArray*>(owner)->unsafeRetainInner();
}

void valdi_rust_bytes_view_release(void* owner) {
    static_cast<Valdi::ValueTypedArray*>(owner)->unsafeReleaseInner();
}

ValdiRustStatus valdi_rust_value_ref_get_property(const void* value, ValdiRustSlice key, const void** out) {
    const auto& object = rustValueRef(value);
    auto name = Valdi::StringBox::fromString(std::string_view(reinterpret_cast<const char*>(key.data), key.len));
    if (const auto* typedObject = object.getTypedObject()) {
        *out = &typedObject->getPropertyForName(name);
        return rustSuccessStatus();
    }
    if (const auto* proxy = object.getProxyObject()) {
        *out = &proxy->getTypedObject()->getPropertyForName(name);
        return rustSuccessStatus();
    }
    if (const auto* map = object.getMap()) {
        const auto iterator = map->find(name);
        *out = iterator == map->end() ? &Valdi::Value::undefinedRef() : &iterator->second;
        return rustSuccessStatus();
    }
    return Valdi::makeRustErrorStatus("Valdi value is not an object");
}

ValdiRustStatus valdi_rust_value_ref_map_len(const void* value, size_t* out) {
    Valdi::SimpleExceptionTracker exceptionTracker;
    const auto map = rustValueRef(value).checkedTo<Valdi::Ref<Valdi::ValueMap>>(exceptionTracker);
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }
    *out = map->size();
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_map_set(ValdiRustHandle* value, ValdiRustSlice key, const void* entry) {
    auto& object = Valdi::getRustHandleValue<Valdi::Value>(value);
    auto map = object.getMapRef();
    if (map == nullptr) {
        return Valdi::makeRustErrorStatus("Valdi value is not a map");
    }
    auto name = Valdi::StringBox::fromString(std::string_view(reinterpret_cast<const char*>(key.data), key.len));
    map->insert_or_assign(std::move(name), rustValueRef(entry));
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_ref_array_len(const void* value, size_t* out) {
    Valdi::SimpleExceptionTracker exceptionTracker;
    const auto array = rustValueRef(value).checkedTo<Valdi::Ref<Valdi::ValueArray>>(exceptionTracker);
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }
    *out = array->size();
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_ref_array_get(const void* value, size_t index, const void** out) {
    Valdi::SimpleExceptionTracker exceptionTracker;
    const auto array = rustValueRef(value).checkedTo<Valdi::Ref<Valdi::ValueArray>>(exceptionTracker);
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }
    if (index >= array->size()) {
        return Valdi::makeRustErrorStatus("Valdi array index out of bounds");
    }
    *out = &(*array)[index];
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_new_array(ValdiRustArray values, ValdiRustHandle** out) {
    const auto* refs = static_cast<const void* const*>(values.data);
    auto array = Valdi::ValueArray::make(values.len);
    for (size_t index = 0; index < values.len; index++) {
        (*array)[index] = rustValueRef(refs[index]);
    }
    *out = Valdi::makeRustHandle(Valdi::Value(std::move(array)));
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_new_object(ValdiRustArray entries, ValdiRustHandle** out) {
    const auto* values = static_cast<const ValdiRustObjectEntry*>(entries.data);
    auto map = Valdi::makeShared<Valdi::ValueMap>();
    map->reserve(entries.len);
    for (size_t index = 0; index < entries.len; index++) {
        const auto& entry = values[index];
        auto key = Valdi::StringBox::fromString(
            std::string_view(reinterpret_cast<const char*>(entry.key.data), entry.key.len));
        map->insert_or_assign(std::move(key), rustValueRef(entry.value));
    }
    *out = Valdi::makeRustHandle(Valdi::Value(std::move(map)));
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_new_function(void* instance,
                                              ValdiRustValueInvokeFunction invoke,
                                              ValdiRustValueDropFunction drop,
                                              ValdiRustHandle** out) {
    std::unique_ptr<void, ValdiRustValueDropFunction> instanceOwner(instance, drop);
    auto function = Valdi::makeShared<RustValueFunction>(instance, invoke, drop);
    instanceOwner.release();
    *out = Valdi::makeRustHandle(Valdi::Value(std::move(function)));
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_call_function(const void* function,
                                               void* context,
                                               ValdiRustMarshallerFunction pushArguments,
                                               ValdiRustMarshallerReadFunction readResult) {
    Valdi::SimpleExceptionTracker exceptionTracker;
    const auto callable = rustValueRef(function).checkedTo<Valdi::Ref<Valdi::ValueFunction>>(exceptionTracker);
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }

    Valdi::Marshaller marshaller(exceptionTracker);
    auto status = pushArguments(context, &marshaller);
    if (status.code != 0) {
        return status;
    }
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }

    const auto argumentCount = static_cast<size_t>(marshaller.size());
    auto result = callable->call(static_cast<Valdi::ValueFunctionFlags>(Valdi::ValueFunctionFlagsCallSync |
                                                                        Valdi::ValueFunctionFlagsPropagatesError),
                                 marshaller.getValues(),
                                 argumentCount);
    if (!result) {
        return rustErrorStatus(result.error());
    }

    marshaller.push(result.moveValue());
    return readResult(context, &marshaller, argumentCount);
}

ValdiRustStatus valdi_rust_value_new_promise(ValdiRustHandle** promise, ValdiRustHandle** completer) {
    auto resolvable = Valdi::makeShared<Valdi::ResolvablePromise>();
    *completer = Valdi::makeRustHandle(resolvable);
    *promise = Valdi::makeRustHandle(Valdi::Value(Valdi::Ref<Valdi::ValdiObject>(resolvable.get())));
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_observe_promise(const void* promise,
                                                 void* context,
                                                 ValdiRustPromiseSuccessFunction success,
                                                 ValdiRustPromiseErrorFunction error,
                                                 ValdiRustValueDropFunction dropContext) {
    auto owner = std::shared_ptr<void>(context, [dropContext](void* value) { dropContext(value); });
    Valdi::SimpleExceptionTracker exceptionTracker;
    auto value = rustValueRef(promise).checkedToValdiObject<Valdi::Promise>(exceptionTracker);
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }
    value->onComplete([owner, success, error](const Valdi::Result<Valdi::Value>& result) {
        if (result) {
            success(owner.get(), &result.value());
        } else {
            const auto message = result.error().getMessage().toStringView();
            error(owner.get(), Valdi::makeRustSlice(message));
        }
    });
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_resolve_promise(ValdiRustHandle* completer,
                                                 void* context,
                                                 ValdiRustMarshallerFunction pushValue) {
    Valdi::SimpleExceptionTracker exceptionTracker;
    Valdi::Marshaller value(exceptionTracker);
    auto status = pushValue(context, &value);
    if (status.code != 0) {
        return status;
    }
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }
    if (value.size() != 1) {
        return Valdi::makeRustErrorStatus("Rust future completer did not produce exactly one Valdi value");
    }
    Valdi::getRustHandleValue<Valdi::Ref<Valdi::ResolvablePromise>>(completer)->fulfill(
        Valdi::Result<Valdi::Value>(value.get(0)));
    return rustSuccessStatus();
}

ValdiRustStatus valdi_rust_value_reject_promise(ValdiRustHandle* completer, ValdiRustSlice message) {
    Valdi::getRustHandleValue<Valdi::Ref<Valdi::ResolvablePromise>>(completer)->fulfill(Valdi::Result<Valdi::Value>(
        Valdi::Error(std::string_view(reinterpret_cast<const char*>(message.data), message.len))));
    return rustSuccessStatus();
}

namespace Valdi {

#if defined(VALDI_RUST_TRACK_HANDLES)
void rustHandleCreated() noexcept {
    gLiveRustHandleCount.fetch_add(1, std::memory_order_relaxed);
}

void rustHandleDestroyed() noexcept {
    gLiveRustHandleCount.fetch_sub(1, std::memory_order_release);
}
#endif

BytesView makeRustBytesView(ValdiRustSlice value) {
    auto bytes = makeShared<Bytes>();
    bytes->assignData(value.data, value.len);
    return BytesView(bytes);
}

BytesView takeRustBytes(ValdiRustBuffer value) {
    auto owner = makeShared<RustBufferOwner>(value);
    return BytesView(Ref<RefCountable>(owner.get()), value.data, value.len);
}

ValdiRustStatus makeRustProxyValue(RegisteredCppGeneratedClass* registeredSchema,
                                   size_t methodCount,
                                   void* instance,
                                   ValdiRustProxyInvokeFunction invoke,
                                   ValdiRustValueDropFunction drop,
                                   ValdiRustHandle** out) noexcept {
    std::unique_ptr<void, ValdiRustValueDropFunction> instanceOwner(instance, drop);
    if (registeredSchema == nullptr) {
        return makeRustErrorStatus("Cannot create a Rust proxy without its schema");
    }
    SimpleExceptionTracker exceptionTracker;
    auto schema = registeredSchema->getResolvedClassSchema(exceptionTracker);
    if (!exceptionTracker) {
        return rustErrorStatus(exceptionTracker);
    }
    if (schema->getPropertiesSize() != methodCount) {
        return makeRustErrorStatus("Rust proxy method count does not match its schema");
    }

    auto state = makeShared<RustModuleState>(instance, drop);
    instanceOwner.release();
    auto object = ValueTypedObject::make(schema);
    for (size_t index = 0; index < methodCount; index++) {
        object->setProperty(index, Value(makeShared<RustModuleMethod>(state, index, invoke)));
    }
    *out = makeRustHandle(Value(makeShared<RustModuleProxy>(object, state, "Rust Proxy")));
    return rustSuccessStatus();
}

ValdiRustStatus makeRustErrorStatus(std::string_view message) noexcept {
    return valdi_rust_status_error(makeRustSlice(message));
}

} // namespace Valdi
