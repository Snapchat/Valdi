#pragma once

#include "Future.hpp"
#include "djinni_c_ref.hpp"
#include "djinni_c_types.hpp"
#include "Future_c.h"

namespace djinni::c_api {

class FutureException : public std::exception {
public:
  FutureException(djinni_string_ref error);
  ~FutureException() override;

  const char *what() const noexcept override;

private:
  Ref<djinni_string_ref> _error;
};

template <typename Tr> struct FutureTranslator {
  using CppType = ::djinni::Future<typename Tr::CppType>;
  using CType = djinni_future_ref;

  static void deallocatePromise(void *opaque) {
    delete reinterpret_cast<::djinni::Promise<typename Tr::CppType> *>(opaque);
  }

  static void promiseCallback(void *opaque, djinni_ref value,
                              djinni_string_ref error) {
    auto *promise =
        reinterpret_cast<::djinni::Promise<typename Tr::CppType> *>(opaque);

    if (error) {
      promise->setException(FutureException(error));
    } else {
      if constexpr (std::is_void_v<typename Tr::CppType>) {
        promise->setValue();
      } else {
        promise->setValue(Tr::toCpp(value));
      }
    }
  }

  static CppType toCpp(CType future) {
    auto promise = new ::djinni::Promise<typename Tr::CppType>();

    djinni_future_on_complete(future, promise, &deallocatePromise,
                              &promiseCallback);

    return promise->getFuture();
  }

  static CType fromCpp(CppType &&future) {
    auto promise = Ref(djinni_promise_new());

    auto outputFuture = djinni_promise_get_future(promise.get());
    future.then([promise = std::move(promise)](auto future) {
      try {
        if constexpr (std::is_void_v<typename Tr::CppType>) {
          future.get();
          djinni_promise_resolve(promise.get(), nullptr);
        } else {
          auto convertedResult = Ref(Tr::fromCpp(future.get()));
          djinni_promise_resolve(promise.get(), convertedResult.get());
        }

      } catch (const std::exception &exc) {
        const auto *what = exc.what();
        auto errorMessage = Ref(djinni_string_new(what, strlen(what)));
        djinni_promise_reject(promise.get(), errorMessage.get());
      }
    });

    return outputFuture;
  }
};

} // namespace djinni::c_api