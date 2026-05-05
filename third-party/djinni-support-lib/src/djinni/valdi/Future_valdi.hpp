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
#include "../cpp/Future.hpp"
#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"

namespace djinni::valdi {

template <class RESULT>
class FutureAdaptor
{
    using CppResType = typename RESULT::CppType;

public:
    using CppType = Future<CppResType>;
    using ValdiType = Valdi::Value;

    using Boxed = FutureAdaptor;

    using NativePromiseType = Promise<CppResType>;

    static CppType toCpp(ValdiType o)
    {
        auto valdiPromise = castOrNull<Valdi::Promise>(o.getValdiObject());
        auto cppPromise = Valdi::makeShared<Promise<CppResType>>();
        auto cppFuture = cppPromise->getFuture();
        valdiPromise->onComplete([cppPromise] (const Valdi::Result<Valdi::Value>& result) {
            if (result.success()) {
                if constexpr(std::is_same_v<Void, RESULT>) {
                    cppPromise->setValue();
                } else {
                    cppPromise->setValue(RESULT::toCpp(result.value()));
                }
            } else {
                cppPromise->setException(std::runtime_error(result.error().toString().c_str()));
            }
        });
        return cppFuture;
    }

    static ValdiType fromCpp(CppType c)
    {
        auto valdiPromise = Valdi::makeShared<Valdi::ResolvablePromise>();
        c.then([valdiPromise] (Future<CppResType> f) {
            try {
                if constexpr(std::is_same_v<Void, RESULT>) {
                    valdiPromise->fulfill(Valdi::Result<Valdi::Value>(Valdi::Value::undefined()));
                } else {
                    valdiPromise->fulfill(Valdi::Result<Valdi::Value>(RESULT::fromCpp(f.get())));
                }
            } catch (const std::exception& e) {
                valdiPromise->fulfill({Valdi::Result<Valdi::Value>{Valdi::Error(e.what())}});
            }
        });
        return Valdi::Value(valdiPromise);
    }
    static const Valdi::ValueSchema& schema() {
        static auto schema = Valdi::ValueSchema::promise(schemaOrRef<RESULT>());
        return schema;
    }
};

template<typename U>
struct ExceptionHandlingTraits<FutureAdaptor<U>> {
    static Valdi::Value handleNativeException(const std::exception& e, const Valdi::ValueFunctionCallContext& callContext) noexcept {
        // store C++ exception in JS Error and raise in JS runtime
        auto msg = STRING_FORMAT("C++: {}", e.what());
        auto valdiPromise = Valdi::makeShared<Valdi::ResolvablePromise>();
        valdiPromise->fulfill(Valdi::Result<Valdi::Value>(Valdi::Error(std::move(msg))));
        return Valdi::Value(valdiPromise);
    }
    static Valdi::Value handleNativeException(const JsException& e, const Valdi::ValueFunctionCallContext& callContext) noexcept {
        // JS error passthrough
        auto valdiPromise = Valdi::makeShared<Valdi::ResolvablePromise>();
        valdiPromise->fulfill(Valdi::Result<Valdi::Value>(e.cause()));
        return Valdi::Value(valdiPromise);
    }
};

} // namespace djinni
