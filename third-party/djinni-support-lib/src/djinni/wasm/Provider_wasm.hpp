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

#include "djinni_wasm.hpp"
#include <functional>
#include <memory>
#include <emscripten.h>

namespace djinni {

// Provider is a function that returns a value: ()=>T
template <class RESULT>
class ProviderAdaptor {
    using CppResType = typename RESULT::CppType;

public:
    using CppType = std::function<CppResType()>;
    using JsType = em::val;
    using Boxed = ProviderAdaptor;

    static CppType toCpp(const JsType& j) {
        // Capture the JavaScript function in a shared_ptr for proper lifetime management
        auto jsFunc = std::make_shared<em::val>(j);

        return [jsFunc]() -> CppResType {
            // Call the JavaScript function and convert result to C++
            em::val result = (*jsFunc)();
            if constexpr (std::is_void_v<CppResType>) {
                return;
            } else {
                return RESULT::Boxed::toCpp(result);
            }
        };
    }

    static JsType fromCpp(const CppType& c) {
        // Allocate on heap; makeNativeProviderCallback registers the JS function with
        // FinalizationRegistry so the C++ handler is freed when the JS function is GC'd
        auto* callback = new std::function<em::val()>([c]() -> em::val {
            if constexpr (std::is_void_v<CppResType>) {
                c();
                return em::val::undefined();
            } else {
                return RESULT::Boxed::fromCpp(c());
            }
        });

        // Use the Module.makeNativeProviderCallback helper to create a JavaScript function
        auto makeNativeProvider = em::val::module_property("makeNativeProviderCallback");
        return makeNativeProvider(reinterpret_cast<int>(callback));
    }
};

} // namespace djinni
