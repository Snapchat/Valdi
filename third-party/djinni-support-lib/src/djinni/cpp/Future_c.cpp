#include "Future_c.h"
#include "Future.hpp"
#include "Future_c_translator.hpp"
#include "djinni_c_ref.hpp"
#include "djinni_c_types.hpp"

namespace djinni::c_api {

class PromiseHolder : public Object {
public:
  PromiseHolder() = default;
  ~PromiseHolder() override = default;

  Promise<Ref<djinni_ref>> &getPromise() { return _promise; }

private:
  Promise<Ref<djinni_ref>> _promise;
};

class FutureHolder : public Object {
public:
  FutureHolder(Future<Ref<djinni_ref>> future) : _future(std::move(future)) {}
  ~FutureHolder() override = default;

  Future<Ref<djinni_ref>> &getFuture() { return _future; }

private:
  Future<Ref<djinni_ref>> _future;
};

class FutureOpaqueHolder {
public:
  FutureOpaqueHolder(void *opaque, djinni_opaque_deallocator opaqueDeallocator)
      : _opaque(opaque), _opaqueDeallocator(opaqueDeallocator) {}

  FutureOpaqueHolder(const FutureOpaqueHolder &other) = delete;

  FutureOpaqueHolder(FutureOpaqueHolder &&other)
      : _opaque(other._opaque), _opaqueDeallocator(other._opaqueDeallocator) {
    other._opaque = nullptr;
    other._opaqueDeallocator = nullptr;
  }

  ~FutureOpaqueHolder() {
    if (_opaqueDeallocator) {
      _opaqueDeallocator(_opaque);
    }
  }

  FutureOpaqueHolder &operator=(const FutureOpaqueHolder &other) = delete;
  FutureOpaqueHolder &operator=(FutureOpaqueHolder &&other) {
    if (_opaqueDeallocator) {
      _opaqueDeallocator(_opaque);
    }

    _opaque = other._opaque;
    _opaqueDeallocator = other._opaqueDeallocator;

    other._opaque = nullptr;
    other._opaqueDeallocator = nullptr;

    return *this;
  }

  void *getOpaque() const { return _opaque; }

private:
  void *_opaque;
  djinni_opaque_deallocator _opaqueDeallocator;
};

} // namespace djinni::c_api

using namespace djinni;
using namespace djinni::c_api;

djinni_promise_ref djinni_promise_new() { return toC(new PromiseHolder()); }

void djinni_promise_resolve(djinni_promise_ref promise, djinni_ref value) {
  fromC<PromiseHolder>(promise)->getPromise().setValue(Ref(value));
}

void djinni_promise_reject(djinni_promise_ref promise, djinni_ref error) {
  fromC<PromiseHolder>(promise)->getPromise().setException(
      FutureException(error));
}

djinni_future_ref djinni_promise_get_future(djinni_promise_ref promise) {
  return toC(new FutureHolder(
      fromC<PromiseHolder>(promise)->getPromise().getFuture()));
}

void djinni_future_on_complete(djinni_future_ref future, void *opaque,
                               djinni_opaque_deallocator opaque_deallocator,
                               djinni_future_callback callback) {
  fromC<FutureHolder>(future)->getFuture().then(
      [opaqueHolder = FutureOpaqueHolder(opaque, opaque_deallocator),
       callback](auto completedFuture) {
        try {
          auto ref = completedFuture.get();
          callback(opaqueHolder.getOpaque(), ref.get(), nullptr);
        } catch (const std::exception &exc) {
          const auto *what = exc.what();
          auto errorMessage = Ref(djinni_string_new(what, strlen(what)));
          callback(opaqueHolder.getOpaque(), nullptr, errorMessage.get());
        }
      });
}
