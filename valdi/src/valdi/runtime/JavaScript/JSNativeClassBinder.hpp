//
//  JSNativeClassBinder.hpp
//  ValdiRuntime
//

#pragma once

#include "utils/debugging/Assert.hpp"
#include "valdi/runtime/JavaScript/JSClassDefinition.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Valdi {

namespace JSNativeClassBinderDetail {

template<typename T>
using RemoveCVRef = std::remove_cv_t<std::remove_reference_t<T>>;

template<typename>
inline constexpr bool kDependentFalse = false;

template<typename T>
struct OptionalTraits {
    static constexpr bool kIsOptional = false;
    using ValueType = void;
};

template<typename T>
struct OptionalTraits<std::optional<T>> {
    static constexpr bool kIsOptional = true;
    using ValueType = T;
};

template<typename T>
inline constexpr bool kIsOptionalPrimitive =
    std::is_same_v<T, double> || std::is_same_v<T, int32_t> || std::is_same_v<T, bool>;

template<typename T>
inline constexpr bool kIsSupportedOptional =
    OptionalTraits<T>::kIsOptional && kIsOptionalPrimitive<typename OptionalTraits<T>::ValueType>;

template<typename T>
inline constexpr bool kIsRequiredPrimitive = kIsOptionalPrimitive<T> || std::is_same_v<T, int64_t>;

template<typename T>
inline constexpr bool kIsSupportedParameter =
    std::is_same_v<T, Value> || std::is_same_v<T, StringBox> || std::is_same_v<T, Ref<StaticString>> ||
    kIsRequiredPrimitive<T> || std::is_same_v<T, JSTypedArray> || std::is_same_v<T, BytesView> ||
    std::is_same_v<T, Ref<RefCountable>> || std::is_same_v<T, JSValue> || kIsSupportedOptional<T>;

template<typename T>
inline constexpr bool kIsSupportedResult =
    std::is_void_v<T> || std::is_same_v<T, Value> || std::is_same_v<T, StringBox> ||
    std::is_same_v<T, Ref<StaticString>> || kIsRequiredPrimitive<T> || std::is_same_v<T, BytesView> ||
    std::is_same_v<T, Ref<RefCountable>> || std::is_same_v<T, JSValueRef> || kIsSupportedOptional<T>;

template<typename T>
inline constexpr bool kIsSupportedParameterForm =
    !std::is_rvalue_reference_v<T> && (!std::is_lvalue_reference_v<T> || std::is_const_v<std::remove_reference_t<T>>);

template<typename T>
struct MemberFunctionTraits {
    static constexpr bool kIsValid = false;
    using ClassType = void;
    using ReturnType = void;
    using Arguments = std::tuple<>;
};

template<typename Class, typename Return, typename... Args>
struct MemberFunctionTraits<Return (Class::*)(Args...)> {
    static constexpr bool kIsValid = true;
    using ClassType = Class;
    using ReturnType = Return;
    using Arguments = std::tuple<Args...>;
};

template<typename Class, typename Return, typename... Args>
struct MemberFunctionTraits<Return (Class::*)(Args...) const> : MemberFunctionTraits<Return (Class::*)(Args...)> {};

template<typename Class, typename Return, typename... Args>
struct MemberFunctionTraits<Return (Class::*)(Args...) noexcept> : MemberFunctionTraits<Return (Class::*)(Args...)> {};

template<typename Class, typename Return, typename... Args>
struct MemberFunctionTraits<Return (Class::*)(Args...) const noexcept>
    : MemberFunctionTraits<Return (Class::*)(Args...)> {};

template<typename T>
struct FunctionTraits {
    static constexpr bool kIsValid = false;
    using ReturnType = void;
    using Arguments = std::tuple<>;
};

template<typename Return, typename... Args>
struct FunctionTraits<Return (*)(Args...)> {
    static constexpr bool kIsValid = true;
    using ReturnType = Return;
    using Arguments = std::tuple<Args...>;
};

template<typename Return, typename... Args>
struct FunctionTraits<Return (*)(Args...) noexcept> : FunctionTraits<Return (*)(Args...)> {};

template<typename Tuple>
struct LastTupleType {
    using Type = void;
};

template<typename First, typename... Rest>
struct LastTupleType<std::tuple<First, Rest...>> {
    using Type = std::tuple_element_t<sizeof...(Rest), std::tuple<First, Rest...>>;
};

template<typename Tuple, typename Indices>
struct TupleSelect;

template<typename Tuple, size_t... indices>
struct TupleSelect<Tuple, std::index_sequence<indices...>> {
    using Type = std::tuple<std::tuple_element_t<indices, Tuple>...>;
};

template<typename Tuple, size_t size>
using TuplePrefix = typename TupleSelect<Tuple, std::make_index_sequence<size>>::Type;

template<typename Tuple>
struct ParsedTuple;

template<typename... Arguments>
struct ParsedTuple<std::tuple<Arguments...>> {
    using Type = std::tuple<RemoveCVRef<Arguments>...>;
};

template<typename Tuple, size_t... indices>
constexpr bool hasSupportedParameters(std::index_sequence<indices...> /*unused*/) {
    return (kIsSupportedParameter<RemoveCVRef<std::tuple_element_t<indices, Tuple>>> && ...) &&
           (kIsSupportedParameterForm<std::tuple_element_t<indices, Tuple>> && ...);
}

template<typename Traits>
struct BindingTraits {
    using ReturnType = typename Traits::ReturnType;
    using Arguments = typename Traits::Arguments;

    static constexpr size_t kArgumentCount = std::tuple_size_v<Arguments>;
    static constexpr size_t kNativeArgumentCount = kArgumentCount == 0 ? 0 : kArgumentCount - 1;

    using LastArgument = typename LastTupleType<Arguments>::Type;
    using NativeArguments = TuplePrefix<Arguments, kNativeArgumentCount>;
    using ParsedArguments = typename ParsedTuple<NativeArguments>::Type;

    static constexpr bool kHasCallContext = std::is_same_v<LastArgument, JSFunctionNativeCallContext&>;
    static constexpr bool kHasSupportedParameters =
        hasSupportedParameters<NativeArguments>(std::make_index_sequence<kNativeArgumentCount>{});
    static constexpr bool kHasSupportedResult =
        !std::is_reference_v<ReturnType> && kIsSupportedResult<RemoveCVRef<ReturnType>>;
};

inline bool isParameterNullOrUndefined(JSFunctionNativeCallContext& callContext, size_t index) {
    if (index >= callContext.getParameterSize()) {
        return true;
    }

    auto value = callContext.getParameter(index);
    return callContext.getContext().isValueNull(value) || callContext.getContext().isValueUndefined(value);
}

template<typename T>
inline T parsePrimitive(JSFunctionNativeCallContext& callContext, size_t index) {
    if constexpr (std::is_same_v<T, double>) {
        return callContext.getParameterAsDouble(index);
    } else if constexpr (std::is_same_v<T, int32_t>) {
        return callContext.getParameterAsInt(index);
    } else if constexpr (std::is_same_v<T, int64_t>) {
        return callContext.getParameterAsLong(index);
    } else if constexpr (std::is_same_v<T, bool>) {
        return callContext.getParameterAsBool(index);
    } else {
        static_assert(kDependentFalse<T>, "Unsupported native class primitive parameter");
    }
}

template<typename Argument>
inline RemoveCVRef<Argument> parseArgument(JSFunctionNativeCallContext& callContext, size_t index) {
    using T = RemoveCVRef<Argument>;
    static_assert(kIsSupportedParameter<T>, "Unsupported native class parameter type");
    static_assert(kIsSupportedParameterForm<Argument>,
                  "Native class parameters must be passed by value or const reference");

    if constexpr (kIsRequiredPrimitive<T>) {
        if (isParameterNullOrUndefined(callContext, index)) {
            callContext.getExceptionTracker().onError("Native class primitive argument cannot be null or undefined");
            return T();
        }
        return parsePrimitive<T>(callContext, index);
    } else if constexpr (kIsSupportedOptional<T>) {
        if (isParameterNullOrUndefined(callContext, index)) {
            return std::nullopt;
        }
        using ValueType = typename OptionalTraits<T>::ValueType;
        return T(parsePrimitive<ValueType>(callContext, index));
    } else if constexpr (std::is_same_v<T, Value>) {
        return callContext.getParameterAsValue(index);
    } else if constexpr (std::is_same_v<T, JSValue>) {
        return callContext.getParameter(index);
    } else {
        if (isParameterNullOrUndefined(callContext, index)) {
            return T();
        }

        if constexpr (std::is_same_v<T, StringBox>) {
            return callContext.getParameterAsString(index);
        } else if constexpr (std::is_same_v<T, Ref<StaticString>>) {
            return callContext.getParameterAsStaticString(index);
        } else if constexpr (std::is_same_v<T, JSTypedArray>) {
            return callContext.getParameterAsTypedArray(index);
        } else if constexpr (std::is_same_v<T, BytesView>) {
            return callContext.getParameterAsBytesView(index);
        } else if constexpr (std::is_same_v<T, Ref<RefCountable>>) {
            return callContext.getParameterAsWrappedObject(index);
        } else {
            static_assert(kDependentFalse<T>, "Unsupported nullable native class parameter");
        }
    }
}

template<typename Arguments, size_t index = 0, typename ParsedArguments>
inline bool parseArguments(JSFunctionNativeCallContext& callContext, ParsedArguments& parsedArguments) {
    if constexpr (index == std::tuple_size_v<Arguments>) {
        return true;
    } else {
        using Argument = std::tuple_element_t<index, Arguments>;
        std::get<index>(parsedArguments) = parseArgument<Argument>(callContext, index);
        if (!callContext.getExceptionTracker()) {
            return false;
        }
        return parseArguments<Arguments, index + 1>(callContext, parsedArguments);
    }
}

template<typename Argument, typename Parsed>
inline decltype(auto) forwardArgument(Parsed& parsed) {
    if constexpr (std::is_lvalue_reference_v<Argument>) {
        return static_cast<Argument>(parsed);
    } else {
        return static_cast<RemoveCVRef<Argument>&&>(parsed);
    }
}

template<typename Result>
inline JSValueRef convertResult(Result&& result, JSFunctionNativeCallContext& callContext) {
    using T = RemoveCVRef<Result>;
    static_assert(!std::is_reference_v<Result>, "Native class results must be returned by value");
    static_assert(kIsSupportedResult<T>, "Unsupported native class result type");

    auto& context = callContext.getContext();
    auto& exceptionTracker = callContext.getExceptionTracker();

    if constexpr (std::is_same_v<T, Value>) {
        return valueToJSValue(
            context, result, ReferenceInfoBuilder(callContext.getReferenceInfo()).withReturnValue(), exceptionTracker);
    } else if constexpr (std::is_same_v<T, StringBox>) {
        if (result.isNull()) {
            return context.newUndefined();
        }
        return context.newStringUTF8(result.toStringView(), exceptionTracker);
    } else if constexpr (std::is_same_v<T, Ref<StaticString>>) {
        if (result == nullptr) {
            return context.newUndefined();
        }

        return context.newString(*result, exceptionTracker);
    } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, int32_t>) {
        return context.newNumber(result);
    } else if constexpr (std::is_same_v<T, int64_t>) {
        return context.newLong(result, exceptionTracker);
    } else if constexpr (std::is_same_v<T, bool>) {
        return context.newBool(result);
    } else if constexpr (std::is_same_v<T, BytesView>) {
        if (result.getSource() == nullptr) {
            return context.newUndefined();
        }
        return newTypedArrayFromBytesView(context, TypedArrayType::Uint8Array, result, exceptionTracker);
    } else if constexpr (std::is_same_v<T, Ref<RefCountable>>) {
        if (result == nullptr) {
            return context.newUndefined();
        }
        return context.newWrappedObject(result, exceptionTracker);
    } else if constexpr (std::is_same_v<T, JSValueRef>) {
        return std::forward<Result>(result);
    } else if constexpr (kIsSupportedOptional<T>) {
        if (!result.has_value()) {
            return context.newUndefined();
        }
        return convertResult(std::move(result.value()), callContext);
    } else {
        static_assert(kDependentFalse<T>, "Unsupported native class result");
    }
}

template<auto method, typename Receiver, typename Arguments, typename ParsedArguments, size_t... indices>
inline decltype(auto) invokeMember(Receiver& receiver,
                                   ParsedArguments& parsedArguments,
                                   JSFunctionNativeCallContext& callContext,
                                   std::index_sequence<indices...> /*unused*/) {
    return (receiver.*method)(
        forwardArgument<std::tuple_element_t<indices, Arguments>>(std::get<indices>(parsedArguments))..., callContext);
}

template<auto function, typename Arguments, typename ParsedArguments, size_t... indices>
inline decltype(auto) invokeFunction(ParsedArguments& parsedArguments,
                                     JSFunctionNativeCallContext& callContext,
                                     std::index_sequence<indices...> /*unused*/) {
    return function(forwardArgument<std::tuple_element_t<indices, Arguments>>(std::get<indices>(parsedArguments))...,
                    callContext);
}

template<typename T, typename Arguments, typename ParsedArguments, size_t... indices>
inline Ref<RefCountable> construct(ParsedArguments& parsedArguments,
                                   JSFunctionNativeCallContext& callContext,
                                   std::index_sequence<indices...> /*unused*/) {
    return makeShared<T>(
        forwardArgument<std::tuple_element_t<indices, Arguments>>(std::get<indices>(parsedArguments))..., callContext);
}

template<typename T, auto method>
struct InstanceCallback {
    using CallableTraits = MemberFunctionTraits<decltype(method)>;
    using Binding = BindingTraits<CallableTraits>;
    using ReturnType = typename Binding::ReturnType;

    static constexpr size_t kNativeArgumentCount = Binding::kNativeArgumentCount;
    static constexpr bool kIsValid = CallableTraits::kIsValid && Binding::kHasCallContext &&
                                     Binding::kHasSupportedParameters && Binding::kHasSupportedResult &&
                                     std::is_base_of_v<typename CallableTraits::ClassType, T>;

    static JSValueRef call(RefCountable* opaque, JSFunctionNativeCallContext& callContext) noexcept {
        static_assert(kIsValid, "Invalid native class instance method signature");

        typename Binding::ParsedArguments parsedArguments;
        if (!parseArguments<typename Binding::NativeArguments>(callContext, parsedArguments)) {
            return callContext.getContext().newUndefined();
        }

        auto& receiver = *static_cast<T*>(opaque);
        auto indices = std::make_index_sequence<Binding::kNativeArgumentCount>{};
        if constexpr (std::is_void_v<ReturnType>) {
            invokeMember<method, T, typename Binding::NativeArguments>(receiver, parsedArguments, callContext, indices);
            return callContext.getContext().newUndefined();
        } else {
            auto result = invokeMember<method, T, typename Binding::NativeArguments>(
                receiver, parsedArguments, callContext, indices);
            return convertResult(std::move(result), callContext);
        }
    }
};

template<auto function>
struct ClassCallback {
    using CallableTraits = FunctionTraits<decltype(function)>;
    using Binding = BindingTraits<CallableTraits>;
    using ReturnType = typename Binding::ReturnType;

    static constexpr size_t kNativeArgumentCount = Binding::kNativeArgumentCount;
    static constexpr bool kIsValid = CallableTraits::kIsValid && Binding::kHasCallContext &&
                                     Binding::kHasSupportedParameters && Binding::kHasSupportedResult;

    static JSValueRef call(RefCountable* /*classOpaque*/, JSFunctionNativeCallContext& callContext) noexcept {
        static_assert(kIsValid, "Invalid native class method signature");

        typename Binding::ParsedArguments parsedArguments;
        if (!parseArguments<typename Binding::NativeArguments>(callContext, parsedArguments)) {
            return callContext.getContext().newUndefined();
        }

        auto indices = std::make_index_sequence<Binding::kNativeArgumentCount>{};
        if constexpr (std::is_void_v<ReturnType>) {
            invokeFunction<function, typename Binding::NativeArguments>(parsedArguments, callContext, indices);
            return callContext.getContext().newUndefined();
        } else {
            auto result =
                invokeFunction<function, typename Binding::NativeArguments>(parsedArguments, callContext, indices);
            return convertResult(std::move(result), callContext);
        }
    }
};

template<typename T, typename... Arguments>
struct ConstructorCallback {
    using NativeArguments = std::tuple<Arguments...>;
    using ParsedArguments = typename ParsedTuple<NativeArguments>::Type;

    static constexpr bool kHasSupportedParameters =
        hasSupportedParameters<NativeArguments>(std::index_sequence_for<Arguments...>{});

    static Ref<RefCountable> call(RefCountable* /*classOpaque*/, JSFunctionNativeCallContext& callContext) noexcept {
        static_assert(std::is_base_of_v<RefCountable, T>, "Native class instance types must inherit from RefCountable");
        static_assert(kHasSupportedParameters, "Invalid native class constructor parameter type");
        static_assert(std::is_constructible_v<T, Arguments..., JSFunctionNativeCallContext&>,
                      "Native class constructor must accept the bound arguments followed by "
                      "JSFunctionNativeCallContext&");

        ParsedArguments parsedArguments;
        if (!parseArguments<NativeArguments>(callContext, parsedArguments)) {
            return nullptr;
        }

        return construct<T, NativeArguments>(parsedArguments, callContext, std::index_sequence_for<Arguments...>{});
    }
};

} // namespace JSNativeClassBinderDetail

/**
 Binds native C++ constructors and functions to a JSClassDefinition without
 storing callable state. JSValue parameters are borrowed for the duration of
 the native call.
 */
template<typename T>
class JSNativeClassBinder {
public:
    explicit JSNativeClassBinder(const char* name) : _classDefinition(StringBox::fromCString(name), nullptr) {
        static_assert(std::is_base_of_v<RefCountable, T>, "Native class instance types must inherit from RefCountable");
    }

    ~JSNativeClassBinder() = default;

    template<typename... Arguments>
    JSNativeClassBinder& bindConstructor() {
        SC_ASSERT(_classDefinition.getConstructor() == nullptr);
        _classDefinition.setConstructor(&JSNativeClassBinderDetail::ConstructorCallback<T, Arguments...>::call);
        return *this;
    }

    template<auto method>
    JSNativeClassBinder& bindMethod(const char* name) {
        return bindMethod<method>(name, true, false, true);
    }

    template<auto method>
    JSNativeClassBinder& bindMethod(const char* name, bool writable, bool enumerable, bool configurable) {
        using Callback = JSNativeClassBinderDetail::InstanceCallback<T, method>;
        static_assert(Callback::kIsValid, "Invalid native class instance method signature");
        return appendInstanceEntry(
            JSClassEntry::method(makeName(name), &Callback::call, writable, enumerable, configurable));
    }

    template<auto getter, auto setter>
    JSNativeClassBinder& bindAccessor(const char* name) {
        return bindAccessor<getter, setter>(name, false, true);
    }

    template<auto getter, auto setter>
    JSNativeClassBinder& bindAccessor(const char* name, bool enumerable, bool configurable) {
        validateInstanceGetter<getter>();
        validateInstanceSetter<setter>();
        return appendInstanceEntry(JSClassEntry::accessor(makeName(name),
                                                          &JSNativeClassBinderDetail::InstanceCallback<T, getter>::call,
                                                          &JSNativeClassBinderDetail::InstanceCallback<T, setter>::call,
                                                          enumerable,
                                                          configurable));
    }

    template<auto getter>
    JSNativeClassBinder& bindGetter(const char* name) {
        return bindGetter<getter>(name, false, true);
    }

    template<auto getter>
    JSNativeClassBinder& bindGetter(const char* name, bool enumerable, bool configurable) {
        validateInstanceGetter<getter>();
        return appendInstanceEntry(JSClassEntry::accessor(makeName(name),
                                                          &JSNativeClassBinderDetail::InstanceCallback<T, getter>::call,
                                                          nullptr,
                                                          enumerable,
                                                          configurable));
    }

    template<auto setter>
    JSNativeClassBinder& bindSetter(const char* name) {
        return bindSetter<setter>(name, false, true);
    }

    template<auto setter>
    JSNativeClassBinder& bindSetter(const char* name, bool enumerable, bool configurable) {
        validateInstanceSetter<setter>();
        return appendInstanceEntry(JSClassEntry::accessor(makeName(name),
                                                          nullptr,
                                                          &JSNativeClassBinderDetail::InstanceCallback<T, setter>::call,
                                                          enumerable,
                                                          configurable));
    }

    JSNativeClassBinder& bindConstant(const char* name, JSValueRef value) {
        return bindConstant(name, std::move(value), false, false, false);
    }

    JSNativeClassBinder& bindConstant(
        const char* name, JSValueRef value, bool writable, bool enumerable, bool configurable) {
        return appendInstanceEntry(
            JSClassEntry::constant(makeName(name), std::move(value), writable, enumerable, configurable));
    }

    template<auto function>
    JSNativeClassBinder& bindClassMethod(const char* name) {
        return bindClassMethod<function>(name, true, false, true);
    }

    template<auto function>
    JSNativeClassBinder& bindClassMethod(const char* name, bool writable, bool enumerable, bool configurable) {
        using Callback = JSNativeClassBinderDetail::ClassCallback<function>;
        static_assert(Callback::kIsValid, "Invalid native class method signature");
        return appendClassEntry(
            JSClassEntry::method(makeName(name), &Callback::call, writable, enumerable, configurable));
    }

    template<auto getter, auto setter>
    JSNativeClassBinder& bindClassAccessor(const char* name) {
        return bindClassAccessor<getter, setter>(name, false, true);
    }

    template<auto getter, auto setter>
    JSNativeClassBinder& bindClassAccessor(const char* name, bool enumerable, bool configurable) {
        validateClassGetter<getter>();
        validateClassSetter<setter>();
        return appendClassEntry(JSClassEntry::accessor(makeName(name),
                                                       &JSNativeClassBinderDetail::ClassCallback<getter>::call,
                                                       &JSNativeClassBinderDetail::ClassCallback<setter>::call,
                                                       enumerable,
                                                       configurable));
    }

    template<auto getter>
    JSNativeClassBinder& bindClassGetter(const char* name) {
        return bindClassGetter<getter>(name, false, true);
    }

    template<auto getter>
    JSNativeClassBinder& bindClassGetter(const char* name, bool enumerable, bool configurable) {
        validateClassGetter<getter>();
        return appendClassEntry(JSClassEntry::accessor(makeName(name),
                                                       &JSNativeClassBinderDetail::ClassCallback<getter>::call,
                                                       nullptr,
                                                       enumerable,
                                                       configurable));
    }

    template<auto setter>
    JSNativeClassBinder& bindClassSetter(const char* name) {
        return bindClassSetter<setter>(name, false, true);
    }

    template<auto setter>
    JSNativeClassBinder& bindClassSetter(const char* name, bool enumerable, bool configurable) {
        validateClassSetter<setter>();
        return appendClassEntry(JSClassEntry::accessor(makeName(name),
                                                       nullptr,
                                                       &JSNativeClassBinderDetail::ClassCallback<setter>::call,
                                                       enumerable,
                                                       configurable));
    }

    JSNativeClassBinder& bindClassConstant(const char* name, JSValueRef value) {
        return bindClassConstant(name, std::move(value), false, false, false);
    }

    JSNativeClassBinder& bindClassConstant(
        const char* name, JSValueRef value, bool writable, bool enumerable, bool configurable) {
        return appendClassEntry(
            JSClassEntry::constant(makeName(name), std::move(value), writable, enumerable, configurable));
    }

    const JSClassDefinition& getClassDefinition() & {
        return _classDefinition;
    }

    JSClassDefinition extractClassDefinition() {
        return std::move(_classDefinition);
    }

private:
    JSClassDefinition _classDefinition;

    static StringBox makeName(const char* name) {
        return StringBox::fromCString(name);
    }

    JSNativeClassBinder& appendInstanceEntry(JSClassEntry entry) {
        _classDefinition.appendInstanceEntry(std::move(entry));
        return *this;
    }

    JSNativeClassBinder& appendClassEntry(JSClassEntry entry) {
        _classDefinition.appendClassEntry(std::move(entry));
        return *this;
    }

    template<auto getter>
    static void validateInstanceGetter() {
        using Callback = JSNativeClassBinderDetail::InstanceCallback<T, getter>;
        static_assert(Callback::kIsValid, "Invalid native class instance getter signature");
        static_assert(Callback::kNativeArgumentCount == 0, "Native class getters cannot have JavaScript parameters");
        static_assert(!std::is_void_v<typename Callback::ReturnType>, "Native class getters must return a value");
    }

    template<auto setter>
    static void validateInstanceSetter() {
        using Callback = JSNativeClassBinderDetail::InstanceCallback<T, setter>;
        static_assert(Callback::kIsValid, "Invalid native class instance setter signature");
        static_assert(Callback::kNativeArgumentCount == 1,
                      "Native class setters must have exactly one JavaScript parameter");
        static_assert(std::is_void_v<typename Callback::ReturnType>, "Native class setters must return void");
    }

    template<auto getter>
    static void validateClassGetter() {
        using Callback = JSNativeClassBinderDetail::ClassCallback<getter>;
        static_assert(Callback::kIsValid, "Invalid native class getter signature");
        static_assert(Callback::kNativeArgumentCount == 0, "Native class getters cannot have JavaScript parameters");
        static_assert(!std::is_void_v<typename Callback::ReturnType>, "Native class getters must return a value");
    }

    template<auto setter>
    static void validateClassSetter() {
        using Callback = JSNativeClassBinderDetail::ClassCallback<setter>;
        static_assert(Callback::kIsValid, "Invalid native class setter signature");
        static_assert(Callback::kNativeArgumentCount == 1,
                      "Native class setters must have exactly one JavaScript parameter");
        static_assert(std::is_void_v<typename Callback::ReturnType>, "Native class setters must return void");
    }
};

} // namespace Valdi
