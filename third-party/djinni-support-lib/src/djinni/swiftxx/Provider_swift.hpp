/**
 * Copyright 2021 Snap, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "djinni_support.hpp"
#include <functional>

namespace djinni::swift {

// Holder for a C++ provider function
template<typename RESULT>
struct CppProviderHolder: CallableProvider {
    using CppResType = typename RESULT::CppType;
    std::function<CppResType()> func;

    explicit CppProviderHolder(std::function<CppResType()> f) : func(std::move(f)) {}

    AnyValue call() override {
        if constexpr (std::is_void_v<CppResType>) {
            func();
            return makeVoidValue();
        } else {
            return RESULT::fromCpp(func());
        }
    }
};

template <class RESULT>
class ProviderAdaptor {
    using CppResType = typename RESULT::CppType;

public:
    using CppType = std::function<CppResType()>;

    static CppType toCpp(const AnyValue& o) {
        auto ptr = std::get<OpaqueValuePtr>(o);
        // Already a C++ provider function (e.g. from fromCpp earlier)
        auto cppHolder = std::dynamic_pointer_cast<CppProviderHolder<RESULT>>(ptr);
        if (cppHolder) {
            // Return a copy; the shared holder may be used again (e.g. toCpp called twice
            // with the same AnyValue, or call() invoked later). Moving would leave the
            // holder's func empty and cause std::bad_function_call.
            return cppHolder->func;
        }
        // Swift-created provider: AnyValue holds CallbackProviderHolder from makeProviderFunction.
        // Use it directly so call() invokes the Swift callback.
        auto callable = std::dynamic_pointer_cast<CallableProvider>(ptr);
        if (callable) {
            return [callable]() -> CppResType {
                auto result = callable->call();
                if constexpr (std::is_void_v<CppResType>) {
                    return;
                } else {
                    return RESULT::toCpp(result);
                }
            };
        }
        throw ErrorValue("ProviderAdaptor::toCpp: value is not a provider");
    }

    static AnyValue fromCpp(const CppType& c) {
        auto holder = std::make_shared<CppProviderHolder<RESULT>>(c);
        OpaqueValuePtr ptr = holder;
        return {ptr};
    }

    static AnyValue fromCpp(CppType&& c) {
        auto holder = std::make_shared<CppProviderHolder<RESULT>>(std::move(c));
        OpaqueValuePtr ptr = holder;
        return {ptr};
    }
};

} // namespace djinni::swift
