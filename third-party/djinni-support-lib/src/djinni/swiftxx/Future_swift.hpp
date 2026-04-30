#pragma once

#include "djinni_support.hpp"
#include "../cpp/Future.hpp"

namespace djinni::swift {

// Swift function prototypes called by C++
typedef void (*FutureCb)(void* ctx, AnyValue* result);
typedef void (*CleanupCb)(void* subscription);

// C++ function declarations called by Swift
void setFutureCb(const AnyValue* futureValue, FutureCb cb, void* ctx);
AnyValue makeFutureValue(CleanupCb cleanup);
void setFutureResult(const AnyValue* futureValue, const AnyValue* futureResult);
void storeSubscription(const AnyValue* futureValue, void* subscription);

// Common interface for a Swift accessible C++ future
struct AbstractCppFutureHolder: OpaqueValue {
    virtual ~AbstractCppFutureHolder() = default;
    virtual void setFutureCb(FutureCb cb, void* ctx) = 0;
};

// Holds a C++ future that returns a RESULT type
template<typename RESULT>
struct CppFutureHolder: AbstractCppFutureHolder {
    using CppResType = typename RESULT::CppType;
    djinni::Future<CppResType> future;
    explicit CppFutureHolder(djinni::Future<CppResType> f) : future(std::move(f)) {}

    template<typename T>
    static AnyValue getFutureResult(Future<T>& f) {return RESULT::fromCpp(f.get());}
    template<>
    static AnyValue getFutureResult(Future<void>& f) {return makeVoidValue();}

    void setFutureCb(FutureCb cb, void* ctx) override {
        future.then([cb, ctx] (Future<CppResType> f) {
            try {
                auto v = getFutureResult(f);
                cb(ctx, &v);
            } catch (ErrorValue& e) {
                AnyValue errorValue = e;
                cb(ctx, &errorValue);
            }
        });
    }
};

// Maintains the link from a Swift future to a C++ accessible future
struct SwiftFutureHolder: OpaqueValue {
    djinni::Promise<AnyValue> promise;
    std::shared_ptr<djinni::Future<AnyValue>> future;
    void* subscription = nullptr;
    CleanupCb cleanup;
    SwiftFutureHolder(CleanupCb cleanup) {
        this->cleanup = cleanup;
        this->future = std::make_shared<djinni::Future<AnyValue>>(promise.getFuture());
    }
    ~SwiftFutureHolder() override {
        cleanup(subscription);
    }
    void setValue(const AnyValue* futureValue) {
        promise.setValue(*futureValue);
    }
};

template <class RESULT>
class FutureAdaptor
{
    using CppResType = typename RESULT::CppType;

    template<typename T>
    static void setValue(Promise<T>& p, const AnyValue& res) {p.setValue(RESULT::toCpp(res));}
    template<>
    static void setValue(Promise<void>& p, const AnyValue& res) {p.setValue();}
public:
    using CppType = Future<CppResType>;

    static CppType toCpp(const AnyValue& o) {
        // If already a C++ future (can this happen?), just return the wrapped future
        auto holder = std::dynamic_pointer_cast<CppFutureHolder<RESULT>>(std::get<OpaqueValuePtr>(o));
        if (holder) {
            return std::move(holder->future);
        }
        // Acquire the holder for Swift future
        auto swiftHolder = std::dynamic_pointer_cast<SwiftFutureHolder>(std::get<OpaqueValuePtr>(o));
        Promise<CppResType> p;
        auto f = p.getFuture();
        // And return a C++ future connected to it
        swiftHolder->future->then([p = std::move(p)] (Future<AnyValue> f) mutable {
            auto res = f.get();
            if (std::holds_alternative<ErrorValue>(res)) {
                const auto& e = std::get<ErrorValue>(res);
                p.setException(e);
            } else {
                setValue(p, res);
            }
        });
        return f;
    }
    static AnyValue fromCpp(CppType c) {
        OpaqueValuePtr holder = std::make_shared<CppFutureHolder<RESULT>>(std::move(c));
        return {holder};
    }
};

}
