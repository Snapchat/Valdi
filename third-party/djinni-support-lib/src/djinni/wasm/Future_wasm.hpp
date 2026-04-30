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
#include "../cpp/Future.hpp"

namespace djinni {

template <class RESULT>
class FutureAdaptor
{
    using CppResType = typename RESULT::CppType;

public:
    using CppType = Future<CppResType>;
    using JsType = em::val;

    using Boxed = FutureAdaptor;

    using NativePromiseType = Promise<CppResType>;

    static void resolveNativePromise(int context, em::val res) {
        auto* pNativePromise = reinterpret_cast<NativePromiseType*>(context);

        if constexpr (std::is_void_v<CppResType>) {
            pNativePromise->setValue();
        } else {
            pNativePromise->setValue(RESULT::Boxed::toCpp(res));
        }

        delete pNativePromise;
    }

    static void rejectNativePromise(int context, em::val err) {
        auto* pNativePromise = reinterpret_cast<NativePromiseType*>(context);
        static auto errorGlobal = em::val::global("Error");

        // instanceof will be false if the error object is null or undefined.
        if (err.instanceof(errorGlobal)) {
            pNativePromise->setException(JsException(err));
        } else {
            // We could try to stringify the unknown type here, but rejecting a promise with a non-error
            // type should not be a common use case.
            pNativePromise->setException(std::runtime_error("JS promise rejected with non-error type"));
        }

        delete pNativePromise;
    }

    static CppType toCpp(JsType o)
    {
        auto p = new NativePromiseType();
        auto f = p->getFuture();
        auto makeNativePromiseResolver = em::val::module_property("makeNativePromiseResolver");
        auto makeNativePromiseRejecter = em::val::module_property("makeNativePromiseRejecter");
        auto next = o.call<em::val>("then", makeNativePromiseResolver(reinterpret_cast<int>(&resolveNativePromise), reinterpret_cast<int>(p)));
        next.call<void>("catch", makeNativePromiseRejecter(reinterpret_cast<int>(&rejectNativePromise), reinterpret_cast<int>(p)));
        return f;
    }

    class CppResolveHandler: public CppResolveHandlerBase {
    public:
        void init(em::val resolveFunc, em::val rejectFunc) override {
            _resolveFunc = resolveFunc;
            _rejectFunc = rejectFunc;
        }
        void resolve(Future<CppResType> future) {
            _future = std::move(future);
#ifdef __EMSCRIPTEN_PTHREADS__
            emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VI, &trampoline, this);
#else
            trampoline(this);
#endif
        }
    private:
        em::val _resolveFunc = em::val::undefined();
        em::val _rejectFunc = em::val::undefined();
        std::optional<Future<CppResType>> _future;

        // runs in main thread
        void doResolve() {
            try {
                if constexpr (std::is_void_v<typename RESULT::CppType>) {
                    _future->get();
                    _resolveFunc(em::val::undefined());
                } else {
                    _resolveFunc(RESULT::Boxed::fromCpp(_future->get()));
                }
            } catch (const std::exception& e) {
                _rejectFunc(djinni_native_exception_to_js(e));
            }
        }
        static void trampoline (void *context) {
            CppResolveHandler* pthis = reinterpret_cast<CppResolveHandler*>(context);
            pthis->doResolve();
            delete pthis;
        };
    };
    
    static JsType fromCpp(CppType c)
    {
        static auto jsPromiseBuilderClass = em::val::module_property("DjinniJsPromiseBuilder");
        auto* cppResolveHandler = new CppResolveHandler();
        // Promise constructor calls cppResolveHandler.init(), and stores the JS
        // resolve handler routine in cppResolveHandler.
        em::val jsPromiseBuilder = jsPromiseBuilderClass.new_(reinterpret_cast<int>(cppResolveHandler));
        c.then([cppResolveHandler] (Future<CppResType> res) {
            cppResolveHandler->resolve(std::move(res));
        });
        return jsPromiseBuilder["promise"];
    }
};

template<typename U>
struct ExceptionHandlingTraits<FutureAdaptor<U>> {
    static em::val handleNativeException(const std::exception& e) {
        auto r = FutureAdaptor<U>::NativePromiseType::reject(std::current_exception());
        return FutureAdaptor<U>::fromCpp(std::move(r));
    }
};

} // namespace djinni
