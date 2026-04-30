/**
 * Copyright 2024 Snap, Inc.
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

#include "djinni_valdi.hpp"
#include <functional>

namespace djinni::valdi {

template <class RESULT>
class ProviderAdaptor {
    using CppResType = typename RESULT::CppType;

public:
    using CppType = std::function<CppResType()>;
    using ValdiType = Valdi::Value;
    using Boxed = ProviderAdaptor;

    static CppType toCpp(ValdiType v) {
        // Valdi provider functions are represented as ValueFunction
        // Get the ValueFunction from the Valdi value
        auto valdiFunc = v.getFunctionRef();

        return [valdiFunc]() -> CppResType {
            // Call the Valdi function with no parameters
            auto result = (*valdiFunc)();
            if (!result.success()) {
                throw std::runtime_error("Provider function call failed: " + result.error().toString());
            }
            if constexpr (std::is_void_v<CppResType>) {
                return;
            } else {
                return RESULT::Boxed::toCpp(result.value());
            }
        };
    }

    static ValdiType fromCpp(const CppType& c) {
        // Create a Valdi callable that wraps the C++ function
        auto lambda_ptr = std::make_shared<CppType>(c);

        auto valdiFunc = Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
            [lambda_ptr](const Valdi::ValueFunctionCallContext& /*callContext*/) -> Valdi::Value {
                if constexpr (std::is_void_v<CppResType>) {
                    (*lambda_ptr)();
                    return Valdi::Value::undefined();
                } else {
                    return RESULT::Boxed::fromCpp((*lambda_ptr)());
                }
            }
        );

        return Valdi::Value(valdiFunc);
    }

    static const Valdi::ValueSchema& schema() {
        static auto schema = Valdi::ValueSchema::function(schemaOrRef<RESULT>(), {});
        return schema;
    }
};

} // namespace djinni::valdi
