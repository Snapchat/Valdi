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

#include "Provider_c.h"
#include <functional>
#include <memory>

namespace djinni::c_api {

template <typename Tr>
struct ProviderTranslator {
    using CppType = std::function<typename Tr::CppType()>;
    using CType = djinni_provider_ref;

    static CppType toCpp(djinni_provider_ref provider) {
        djinni_ref_retain(provider);
        // Use shared_ptr so the ref is released when the last copy of the std::function
        // is destroyed, not when the lambda is invoked. This allows the provider to be
        // called multiple times and avoids leaking if the lambda is never invoked.
        auto provider_guard = std::shared_ptr<djinni_provider_ref>(new djinni_provider_ref(provider),
            [](djinni_provider_ref* p) {
                djinni_ref_release(*p);
                delete p;
            });
        return [provider_guard]() -> typename Tr::CppType {
            auto result_c = djinni_provider_call(*provider_guard);
            auto result_cpp = Tr::toCpp(result_c);
            djinni_ref_release(result_c);
            return result_cpp;
        };
    }

    static CType fromCpp(const CppType& provider) {
        auto* func_ptr = new CppType(provider);
        return djinni_provider_make(
            [](void* context) -> djinni_ref {
                auto provider_func = static_cast<CppType*>(context);
                auto result = (*provider_func)();
                return Tr::fromCpp(std::move(result));
            },
            func_ptr,
            [](void* context) {
                delete static_cast<CppType*>(context);
            }
        );
    }

    static CType fromCpp(CppType&& provider) {
        auto* func_ptr = new CppType(std::move(provider));
        return djinni_provider_make(
            [](void* context) -> djinni_ref {
                auto provider_func = static_cast<CppType*>(context);
                auto result = (*provider_func)();
                return Tr::fromCpp(std::move(result));
            },
            func_ptr,
            [](void* context) {
                delete static_cast<CppType*>(context);
            }
        );
    }
};

} // namespace djinni::c_api
