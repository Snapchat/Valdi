#pragma once

#include "valdi_core/cpp/Marshalling/CppGeneratedClass.hpp"
#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include <optional>
#include <type_traits>
#include <vector>

namespace Valdi {

// Helper for dependent static_assert false (only evaluated when template is instantiated)
template<typename>
inline constexpr bool kDependentFalse = false;

template<typename T>
class Ref;

template<typename T>
struct IsOptional : std::false_type {};

template<typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

template<typename T>
inline constexpr bool kIsOptional = IsOptional<T>::value;

template<typename T>
struct IsVector : std::false_type {};

template<typename T>
struct IsVector<std::vector<T>> : std::true_type {};

template<typename T>
inline constexpr bool kIsVector = IsVector<T>::value;

template<typename T>
struct IsRef : std::false_type {};

template<typename T>
struct IsRef<Ref<T>> : std::true_type {};

template<typename T>
inline constexpr bool kIsRef = IsRef<T>::value;

template<typename T>
inline constexpr bool kIsCppGeneratedClass = std::is_base_of_v<CppGeneratedClass, T>;

template<typename T>
struct IsFunction : std::false_type {};

template<typename Signature>
struct IsFunction<Function<Signature>> : std::true_type {};

template<typename T>
inline constexpr bool kIsFunction = IsFunction<T>::value;

template<typename T>
struct FunctionSignature;

template<typename R, typename... Args>
struct FunctionSignature<Function<R(Args...)>> {
    using ReturnType = R;
    using ArgumentTypes = std::tuple<Args...>;
};

class CppGeneratedGenericClass {
public:
    template<typename... Args>
    static GetTypeArgumentsCallback makeTypeArgumentsCallback() {
        return [](auto& exceptionTracker) -> ValueSchemaVector {
            return {resolveTypeArgument<Args>(exceptionTracker)...};
        };
    }

    template<typename T>
    static ValueSchema resolveTypeArgument(ExceptionTracker& exceptionTracker) {
        if constexpr (std::is_same_v<T, int32_t>) {
            return ValueSchema::integer();
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return ValueSchema::longInteger();
        } else if constexpr (std::is_same_v<T, bool>) {
            return ValueSchema::boolean();
        } else if constexpr (std::is_same_v<T, double>) {
            return ValueSchema::doublePrecision();
        } else if constexpr (std::is_same_v<T, ValueTypedArray>) {
            return ValueSchema::valueTypedArray();
        } else if constexpr (std::is_same_v<T, BytesView>) {
            return ValueSchema::valueTypedArray();
        } else if constexpr (std::is_same_v<T, StringBox>) {
            return ValueSchema::string();
        } else if constexpr (std::is_same_v<T, ValueMap>) {
            return ValueSchema::untyped();
        } else if constexpr (std::is_same_v<T, Value>) {
            return ValueSchema::untyped();
        } else if constexpr (kIsOptional<T>) {
            using InnerType = typename T::value_type;
            return ValueSchema::optional(resolveTypeArgument<InnerType>(exceptionTracker));
        } else if constexpr (kIsVector<T>) {
            using InnerType = typename T::value_type;
            return ValueSchema::array(resolveTypeArgument<InnerType>(exceptionTracker));
        } else if constexpr (kIsCppGeneratedClass<T>) {
            return getSchemaForRegisteredClass(T::getRegisteredClass(), exceptionTracker);
        } else if constexpr (std::is_enum<T>::value) {
            return getSchemaForRegisteredClass(getRegisteredEnumClass(static_cast<const T*>(nullptr)),
                                               exceptionTracker);
        } else if constexpr (kIsRef<T>) {
            using RefInnerType = typename std::remove_cv_t<std::remove_reference_t<decltype(*std::declval<T>())>>;
            return resolveTypeArgument<RefInnerType>(exceptionTracker);
        } else if constexpr (kIsFunction<T>) {
            return resolveFunctionTypeArgument<T>(exceptionTracker);
        } else {
            static_assert(kDependentFalse<T>, "Unable to register generic type argument");
        }
    }

private:
    template<typename F>
    static ValueSchema resolveFunctionTypeArgument(ExceptionTracker& exceptionTracker) {
        using Sig = typename FunctionSignature<F>::ReturnType;
        using Args = typename FunctionSignature<F>::ArgumentTypes;
        return resolveFunctionTypeArgumentImpl<Sig, Args>(exceptionTracker);
    }

    template<typename R, typename ArgsTuple>
    static ValueSchema resolveFunctionTypeArgumentImpl(ExceptionTracker& exceptionTracker) {
        return resolveFunctionTypeArgumentImplHelper<R, ArgsTuple>(
            exceptionTracker, std::make_index_sequence<std::tuple_size_v<ArgsTuple>>{});
    }

    template<typename R, typename ArgsTuple, std::size_t... indices>
    static ValueSchema resolveFunctionTypeArgumentImplHelper(ExceptionTracker& exceptionTracker,
                                                             std::index_sequence<indices...> /*indicesParam*/) {
        ValueSchema returnType;
        if constexpr (std::is_void_v<R>) {
            returnType = ValueSchema::voidType();
        } else {
            returnType = resolveTypeArgument<R>(exceptionTracker);
        }

        // Resolve parameter types
        std::vector<ValueSchema> parameters = {
            resolveTypeArgument<std::tuple_element_t<indices, ArgsTuple>>(exceptionTracker)...};

        return ValueSchema::function(ValueFunctionSchemaAttributes(), returnType, parameters.data(), parameters.size());
    }

    static ValueSchema getSchemaForRegisteredClass(RegisteredCppGeneratedClass* registeredClass,
                                                   ExceptionTracker& exceptionTracker);
};

} // namespace Valdi