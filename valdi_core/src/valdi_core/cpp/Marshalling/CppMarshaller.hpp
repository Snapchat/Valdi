//
//  CppMarshaller.hpp
//  valdi_core-pc
//
//  Created by Simon Corsin on 4/11/23.
//

#pragma once

#include "valdi_core/cpp/Marshalling/CppGeneratedClass.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Future.hpp"
#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueTypedObject.hpp"

#include <tuple>

namespace Valdi {

class BytesView;

template<typename R, typename... A>
class CppValueFunction;

template<typename R, typename... A>
class CppMethodValueFunction;

template<typename R, typename... A>
class CppMethodRef;

class CppObjectStore {
public:
    std::unique_lock<std::recursive_mutex> lock() const;

    void setObjectProxyForId(uint32_t id, const Ref<CppGeneratedInterface>& object);
    Ref<CppGeneratedInterface> getObjectProxyForId(uint32_t id);

    static CppObjectStore* sharedInstance();

private:
    mutable std::recursive_mutex _mutex;
    FlatMap<uint32_t, Weak<CppGeneratedInterface>> _objectProxyById;
};

class CppProxyMarshallerBase {
public:
    CppProxyMarshallerBase(CppObjectStore* objectStore, std::unique_lock<std::recursive_mutex>&& lock);
    ~CppProxyMarshallerBase();

    CppObjectStore* getObjectStore() const;

private:
    CppObjectStore* _objectStore;
    std::unique_lock<std::recursive_mutex> _lock;
};

class CppProxyMarshaller : public CppProxyMarshallerBase {
public:
    CppProxyMarshaller(CppObjectStore* objectStore,
                       std::unique_lock<std::recursive_mutex>&& lock,
                       bool finishedMarshalling);
    ~CppProxyMarshaller();

    bool finishedMarshalling() const;

private:
    bool _finishedMarshalling;
};

class CppProxyUnmarshaller : public CppProxyMarshallerBase {
public:
    CppProxyUnmarshaller(CppObjectStore* objectStore,
                         std::unique_lock<std::recursive_mutex>&& lock,
                         Ref<ValueTypedProxyObject>&& proxyObject,
                         Ref<CppGeneratedInterface>&& object);

    ~CppProxyUnmarshaller();

    const Ref<ValueTypedProxyObject>& getProxyObject() const;
    const Ref<CppGeneratedInterface>& getObject() const;
    const Ref<ValueTypedObject>& getTypedObject() const;

private:
    Ref<ValueTypedProxyObject> _proxyObject;
    Ref<CppGeneratedInterface> _object;
};

class CppMarshaller {
public:
    inline static void marshall(ExceptionTracker& exceptionTracker, int32_t value, Value& out) {
        out = Value(value);
    }

    inline static void marshall(ExceptionTracker& exceptionTracker, int64_t value, Value& out) {
        out = Value(value);
    }

    inline static void marshall(ExceptionTracker& exceptionTracker, bool value, Value& out) {
        out = Value(value);
    }

    inline static void marshall(ExceptionTracker& exceptionTracker, double value, Value& out) {
        out = Value(value);
    }

    inline static void marshall(ExceptionTracker& exceptionTracker, const StringBox& value, Value& out) {
        out = Value(value);
    }

    inline static void marshall(ExceptionTracker& exceptionTracker, const Ref<ValueMap>& value, Value& out) {
        out = Value(value);
    }

    inline static void marshall(ExceptionTracker& exceptionTracker, const Value& value, Value& out) {
        out = value;
    }

    template<typename T, std::enable_if_t<std::is_base_of<CppGeneratedModel, T>::value, int> = 0>
    static void marshall(ExceptionTracker& exceptionTracker, const T& value, Value& out) {
        T::marshall(exceptionTracker, value, out);
    }

    template<typename T, std::enable_if_t<std::is_base_of<CppGeneratedModel, T>::value, int> = 0>
    static void marshall(ExceptionTracker& exceptionTracker, const Ref<T>& value, Value& out) {
        if (value == nullptr) {
            out = Value::undefined();
        } else {
            T::marshall(exceptionTracker, *value, out);
        }
    }

    template<typename T, std::enable_if_t<std::is_base_of<CppGeneratedInterface, T>::value, int> = 0>
    static void marshall(ExceptionTracker& exceptionTracker, const Ref<T>& value, Value& out) {
        if (value == nullptr) {
            out = Value::undefined();
        } else {
            T::marshall(exceptionTracker, value, out);
        }
    }

    template<typename T>
    static void marshall(ExceptionTracker& exceptionTracker, const std::optional<T>& value, Value& out) {
        if (value) {
            marshall(exceptionTracker, value.value(), out);
        } else {
            out = Value::undefined();
        }
    }

    template<typename T>
    static void marshall(ExceptionTracker& exceptionTracker, const std::vector<T>& value, Value& out) {
        auto array = ValueArray::make(value.size());

        auto* ptr = array->begin();
        for (const auto& item : value) {
            marshall(exceptionTracker, item, *ptr);
            if (!exceptionTracker) {
                return;
            }
            ptr++;
        }

        out = Value(array);
    }

    template<typename R, typename... A>
    static void marshall(ExceptionTracker& exceptionTracker, const Function<R(A...)>& value, Value& out) {
        out = Value(makeShared<CppValueFunction<R, A...>>(value));
    }

    template<typename R, typename... A>
    static void marshall(ExceptionTracker& exceptionTracker, CppMethodRef<R, A...> value, Value& out) {
        out = Value(makeShared<CppMethodValueFunction<R, A...>>(std::move(value)));
    }

    template<typename T, std::enable_if_t<std::is_enum<T>::value, int> = 0>
    static void marshall(ExceptionTracker& exceptionTracker, const T& value, Value& out) {
        marshallEnum(exceptionTracker, value, out);
    }

    template<typename T>
    static void marshall(ExceptionTracker& exceptionTracker, const Future<T>& value, Value& out) {
        auto resolvablePromise = makeShared<ResolvablePromise>();
        out = Value(resolvablePromise);

        value.then([resolvablePromise = std::move(resolvablePromise)](auto result) {
            if (!result) {
                resolvablePromise->fulfill(result.moveError());
                return;
            }

            if constexpr (std::is_same_v<T, void>) {
                resolvablePromise->fulfill(Value::undefined());
            } else {
                SimpleExceptionTracker exceptionTracker;
                Value outValue;
                CppMarshaller::marshall(exceptionTracker, result.moveValue(), outValue);

                if (!exceptionTracker) {
                    resolvablePromise->fulfill(exceptionTracker.extractError());
                    return;
                }

                resolvablePromise->fulfill(Result<Value>(std::move(outValue)));
            }
        });
    }

    static void marshall(ExceptionTracker& exceptionTracker, const BytesView& value, Value& out);

    inline static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, int32_t& out) {
        out = value.checkedTo<int32_t>(exceptionTracker);
    }

    inline static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, int64_t& out) {
        out = value.checkedTo<int64_t>(exceptionTracker);
    }

    inline static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, bool& out) {
        out = value.checkedTo<bool>(exceptionTracker);
    }

    inline static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, double& out) {
        out = value.checkedTo<double>(exceptionTracker);
    }

    inline static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, StringBox& out) {
        out = value.checkedTo<StringBox>(exceptionTracker);
    }

    inline static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, Ref<ValueMap>& out) {
        out = value.checkedTo<Ref<ValueMap>>(exceptionTracker);
    }

    template<typename T>
    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, Future<T>& out) {
        auto promise = value.checkedToValdiObject<Promise>(exceptionTracker);
        if (!exceptionTracker) {
            return;
        }

        TypedPromise<T> typedPromise;
        out = typedPromise.getFuture();

        promise->onComplete([typedPromise = std::move(typedPromise)](const Result<Value>& result) {
            if (!result) {
                typedPromise.setError(result.error());
                return;
            }

            if constexpr (std::is_same_v<T, void>) {
                typedPromise.setValue();
            } else {
                SimpleExceptionTracker exceptionTracker;
                T outValue;
                CppMarshaller::unmarshall(exceptionTracker, result.value(), outValue);
                if (!exceptionTracker) {
                    typedPromise.setError(exceptionTracker.extractError());
                    return;
                }

                typedPromise.setValue(std::move(outValue));
            }
        });
    }

    template<typename T, std::enable_if_t<std::is_base_of<CppGeneratedModel, T>::value, int> = 0>
    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, T& out) {
        T::unmarshall(exceptionTracker, value, out);
    }

    template<typename T, std::enable_if_t<std::is_base_of<CppGeneratedModel, T>::value, int> = 0>
    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, Ref<T>& out) {
        if (value.isNullOrUndefined()) {
            out = nullptr;
        } else {
            out = makeShared<T>();
            T::unmarshall(exceptionTracker, value, *out);
        }
    }

    template<typename T, typename = std::enable_if_t<std::is_convertible<T*, CppGeneratedInterface*>::value>>
    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, Ref<T>& out) {
        if (value.isNullOrUndefined()) {
            out = nullptr;
        } else {
            T::unmarshall(exceptionTracker, value, out);
        }
    }

    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, Value& out);

    template<typename T>
    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, std::optional<T>& out) {
        if (value.isNullOrUndefined()) {
            out = std::nullopt;
        } else {
            T outValue;
            unmarshall(exceptionTracker, value, outValue);
            if (!exceptionTracker) {
                out = std::nullopt;
            } else {
                out = std::make_optional<T>(std::move(outValue));
            }
        }
    }

    template<typename T, std::enable_if_t<std::is_enum<T>::value, int> = 0>
    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, T& out) {
        unmarshallEnum(exceptionTracker, value, out);
    }

    template<typename T>
    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, std::vector<T>& out) {
        auto array = value.checkedTo<Ref<ValueArray>>(exceptionTracker);
        if (!exceptionTracker) {
            return;
        }

        out.clear();
        out.reserve(array->size());

        for (const auto& item : *array) {
            auto& insertedItem = out.emplace_back();
            unmarshall(exceptionTracker, item, insertedItem);
            if (!exceptionTracker) {
                return;
            }
        }
    }

    // Specialized overload for std::vector<bool> due to its specialized implementation
    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, std::vector<bool>& out) {
        auto array = value.checkedTo<Ref<ValueArray>>(exceptionTracker);
        if (!exceptionTracker) {
            return;
        }

        out.clear();
        out.reserve(array->size());

        for (const auto& item : *array) {
            bool itemValue;
            unmarshall(exceptionTracker, item, itemValue);
            if (!exceptionTracker) {
                return;
            }
            out.push_back(itemValue);
        }
    }

    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, BytesView& out);

    template<typename R, typename... A>
    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, Function<R(A...)>& out) {
        auto fn = value.checkedTo<Ref<ValueFunction>>(exceptionTracker);
        if (!exceptionTracker) {
            return;
        }

        out = [fn](A... arguments) -> R {
            SimpleExceptionTracker exceptionTracker;
            constexpr size_t kSize = sizeof...(arguments);
            Value convertedArguments[kSize];
            auto* outArguments = &convertedArguments[0];

            (
                [&] {
                    if (!exceptionTracker) {
                        return;
                    }

                    marshall(exceptionTracker, arguments, *outArguments);
                    outArguments++;
                }(),
                ...);

            if (!exceptionTracker) {
                throw Exception(exceptionTracker.extractError());
            }

            ValueFunctionFlags flags;
            if constexpr (std::is_same<void, R>::value) {
                flags = ValueFunctionFlagsNone;
            } else {
                flags = static_cast<ValueFunctionFlags>(ValueFunctionFlagsCallSync | ValueFunctionFlagsPropagatesError);
            }

            auto retValue = (*fn)(ValueFunctionCallContext(flags, convertedArguments, kSize, exceptionTracker));

            if (!exceptionTracker) {
                throw Exception(exceptionTracker.extractError());
            }

            if constexpr (!std::is_same<void, R>::value) {
                R convertedRetValue;
                unmarshall(exceptionTracker, retValue, convertedRetValue);

                if (!exceptionTracker) {
                    throw Exception(exceptionTracker.extractError());
                }

                return std::move(convertedRetValue);
            }
        };
    }

    static Value* marshallTypedObjectPrologue(ExceptionTracker& exceptionTracker,
                                              RegisteredCppGeneratedClass& registeredClass,
                                              Value& out,
                                              size_t inputPropertiesSize);
    static void onMarshallTypedObjectFailure(ExceptionTracker& exceptionTracker,
                                             RegisteredCppGeneratedClass& registeredClass,
                                             size_t propertyIndex);

    template<typename T>
    inline static bool marshallTypedObjectProperty(ExceptionTracker& exceptionTracker,
                                                   RegisteredCppGeneratedClass& registeredClass,
                                                   Value* outputProperties,
                                                   size_t& propertyIndex,
                                                   const T& property) {
        marshall(exceptionTracker, property, outputProperties[propertyIndex]);
        if (!exceptionTracker) {
            onMarshallTypedObjectFailure(exceptionTracker, registeredClass, propertyIndex);
            return false;
        }
        propertyIndex++;
        return true;
    }

    template<typename... T>
    static void marshallTypedObject(ExceptionTracker& exceptionTracker,
                                    RegisteredCppGeneratedClass& registeredClass,
                                    Value& out,
                                    const T&... properties) {
        auto* outputProperties =
            marshallTypedObjectPrologue(exceptionTracker, registeredClass, out, sizeof...(properties));
        if (!exceptionTracker) {
            return;
        }

        size_t propertyIndex = 0;
        [[maybe_unused]] auto result =
            (marshallTypedObjectProperty(
                 exceptionTracker, registeredClass, outputProperties, propertyIndex, properties) &&
             ...);
    }

    static Ref<ValueTypedObject> unmarshallTypedObjectPrologue(ExceptionTracker& exceptionTracker,
                                                               RegisteredCppGeneratedClass& registeredClass,
                                                               const Value& value,
                                                               size_t outputPropertiesSize);

    static void onUnmarshallTypedObjectFailure(ExceptionTracker& exceptionTracker,
                                               RegisteredCppGeneratedClass& registeredClass,
                                               size_t propertyIndex);

    template<typename T>
    inline static bool unmarshallTypedObjectProperty(ExceptionTracker& exceptionTracker,
                                                     RegisteredCppGeneratedClass& registeredClass,
                                                     const Value* inputProperties,
                                                     size_t& propertyIndex,
                                                     T& property) {
        unmarshall(exceptionTracker, inputProperties[propertyIndex], property);
        if (!exceptionTracker) {
            onUnmarshallTypedObjectFailure(exceptionTracker, registeredClass, propertyIndex);
            return false;
        }
        propertyIndex++;
        return true;
    }

    template<typename... T>
    static void unmarshallTypedObject(ExceptionTracker& exceptionTracker,
                                      RegisteredCppGeneratedClass& registeredClass,
                                      const Value& value,
                                      T&... properties) {
        auto typedObject =
            unmarshallTypedObjectPrologue(exceptionTracker, registeredClass, value, sizeof...(properties));
        if (!exceptionTracker) {
            return;
        }

        const Value* inputProperties = typedObject->getProperties();
        size_t propertyIndex = 0;
        [[maybe_unused]] auto result =
            (unmarshallTypedObjectProperty(
                 exceptionTracker, registeredClass, inputProperties, propertyIndex, properties) &&
             ...);
    }

    static CppProxyMarshaller marshallProxyObjectPrologue(CppObjectStore* objectStore,
                                                          CppGeneratedInterface& value,
                                                          Value& out);

    static void marshallProxyObjectEpilogue(ExceptionTracker& exceptionTracker,
                                            CppProxyMarshaller& marshaller,
                                            CppGeneratedInterface& value,
                                            Value& out);

    template<typename F>
    static void marshallProxyObject(ExceptionTracker& exceptionTracker,
                                    CppObjectStore* objectStore,
                                    RegisteredCppGeneratedClass& registeredClass,
                                    CppGeneratedInterface& value,
                                    Value& out,
                                    F&& marshallTypedObject) {
        auto proxyMarshaller = marshallProxyObjectPrologue(objectStore, value, out);
        if (proxyMarshaller.finishedMarshalling()) {
            return;
        }

        marshallTypedObject();

        marshallProxyObjectEpilogue(exceptionTracker, proxyMarshaller, value, out);
    }

    template<typename F>
    static void marshallProxyObject(ExceptionTracker& exceptionTracker,
                                    RegisteredCppGeneratedClass& registeredClass,
                                    CppGeneratedInterface& value,
                                    Value& out,
                                    F&& marshallTypedObject) {
        marshallProxyObject(exceptionTracker,
                            CppObjectStore::sharedInstance(),
                            registeredClass,
                            value,
                            out,
                            std::forward<F>(marshallTypedObject));
    }

    static CppProxyUnmarshaller unmarshallProxyObjectPrologue(ExceptionTracker& exceptionTracker,
                                                              CppObjectStore* objectStore,
                                                              const Value& value);

    static void unmarshallProxyObjectEpilogue(CppProxyUnmarshaller& unmarshaller,
                                              CppGeneratedInterface& generatedInterface);

    template<typename ProxyT,
             typename T,
             typename = std::enable_if_t<std::is_convertible<ProxyT*, CppGeneratedInterface*>::value>>
    static void unmarshallProxyObject(ExceptionTracker& exceptionTracker,
                                      CppObjectStore* objectStore,
                                      RegisteredCppGeneratedClass& registeredClass,
                                      const Value& value,
                                      Ref<T>& out) {
        auto unmarshaller = unmarshallProxyObjectPrologue(exceptionTracker, objectStore, value);
        if (!exceptionTracker) {
            return;
        }

        if (unmarshaller.getObject() != nullptr) {
            out = castOrNull<T>(unmarshaller.getObject());
            return;
        }

        Ref<ProxyT> proxyOut;
        ProxyT::unmarshall(exceptionTracker, Value(unmarshaller.getTypedObject()), proxyOut);
        if (!exceptionTracker) {
            return;
        }

        out = proxyOut;
        unmarshallProxyObjectEpilogue(unmarshaller, *proxyOut);
    }

    template<typename ProxyT, typename T>
    static void unmarshallProxyObject(ExceptionTracker& exceptionTracker,
                                      RegisteredCppGeneratedClass& registeredClass,
                                      const Value& value,
                                      Ref<T>& out) {
        unmarshallProxyObject<ProxyT, T>(
            exceptionTracker, CppObjectStore::sharedInstance(), registeredClass, value, out);
    }

    template<typename T, auto method>
    static auto methodToFunction(const Ref<T>& object) {
        using MethodPtr = decltype(method);
        return methodToFunctionImpl<T, method>(object, static_cast<MethodPtr*>(nullptr));
    }

    [[noreturn]] static void throwUnimplementedMethod();

private:
    template<typename T, auto method, typename R, typename... A>
    static CppMethodRef<R, A...> methodToFunctionImpl(const Ref<T>& object, R (T::** methodPtr)(A...)) {
        (void)methodPtr;
        return CppMethodRef<R, A...>::template make<T, method>(object);
    }

    template<typename T, auto method, typename R, typename... A>
    static CppMethodRef<R, A...> methodToFunctionImpl(const Ref<T>& object, R (T::** methodPtr)(A...) const) {
        (void)methodPtr;
        return CppMethodRef<R, A...>::template make<T, method>(object);
    }
};

template<typename R, typename... A>
class CppMethodRef {
public:
    template<typename T, auto method>
    static CppMethodRef make(const Ref<T>& object) {
        CppMethodRef ref;
        ref._object = object.toWeak();
        ref._callable = [](void* self, A... arguments) -> R {
            return (reinterpret_cast<T*>(self)->*method)(std::move(arguments)...);
        };

        return ref;
    }

    CppMethodRef() = default;
    ~CppMethodRef() = default;

    R operator()(A... arguments) const {
        auto self = _object.lock();
        if (!self) {
            throw Exception("Cannot call method: object was deallocated");
        }

        return _callable(self.get(), std::move(arguments)...);
    }

private:
    Weak<void> _object;
    R (*_callable)(void*, A...) = nullptr;
};

template<typename T, std::size_t index>
inline T unmarshallCppFunctionArgument(ExceptionTracker& exceptionTracker,
                                       const ValueFunctionCallContext& callContext) {
    auto out = T();
    if (exceptionTracker) {
        CppMarshaller::unmarshall(callContext.getExceptionTracker(), callContext.getParameter(index), out);
    }
    return out;
}

template<typename R, typename F, typename... A, std::size_t... indices>
inline Value handleCppFunctionCallImpl(const F& callable,
                                       const ValueFunctionCallContext& callContext,
                                       std::index_sequence<indices...> /*unused*/) {
    auto& exceptionTracker = callContext.getExceptionTracker();
    auto parameters = std::tuple<A...>{unmarshallCppFunctionArgument<A, indices>(exceptionTracker, callContext)...};
    if (!exceptionTracker) {
        return Value::undefined();
    }

    if constexpr (std::is_same<void, R>::value) {
        try {
            std::apply(callable, parameters);
        } catch (const Exception& e) {
            exceptionTracker.onError(e.getError());
        }
        return Value::undefined();
    } else {
        R result;
        try {
            result = std::apply(callable, parameters);
        } catch (const Exception& e) {
            exceptionTracker.onError(e.getError());
            return Value::undefined();
        }

        Value out;

        CppMarshaller::marshall<R>(exceptionTracker, result, out);

        return out;
    }
}

template<typename R, typename... A, typename F>
Value handleCppFunctionCall(const F& callable, const ValueFunctionCallContext& callContext) {
    return handleCppFunctionCallImpl<R, F, A...>(callable, callContext, std::index_sequence_for<A...>{});
}

template<typename R, typename... A>
class CppValueFunction : public ValueFunction {
public:
    explicit CppValueFunction(const Function<R(A...)>& callable) : _callable(callable) {}
    ~CppValueFunction() override = default;

    Value operator()(const ValueFunctionCallContext& callContext) noexcept override {
        return handleCppFunctionCall<R, A...>(_callable, callContext);
    }

    std::string_view getFunctionType() const override {
        return "cpp function";
    }

private:
    Function<R(A...)> _callable;

    template<typename T>
    T unmarshallArgument(ExceptionTracker& exceptionTracker,
                         const ValueFunctionCallContext& callContext,
                         size_t& parameterIndex) {
        auto out = T();
        if (exceptionTracker) {
            CppMarshaller::unmarshall(callContext.getExceptionTracker(), callContext.getParameter(parameterIndex), out);
        }
        parameterIndex++;

        return out;
    }
};

template<typename R, typename... A>
class CppMethodValueFunction : public ValueFunction {
public:
    explicit CppMethodValueFunction(CppMethodRef<R, A...> methodRef) : _methodRef(std::move(methodRef)) {}
    ~CppMethodValueFunction() override = default;

    Value operator()(const ValueFunctionCallContext& callContext) noexcept override {
        return handleCppFunctionCall<R, A...>(_methodRef, callContext);
    }

    std::string_view getFunctionType() const override {
        return "cpp method";
    }

private:
    CppMethodRef<R, A...> _methodRef;
};

} // namespace Valdi
