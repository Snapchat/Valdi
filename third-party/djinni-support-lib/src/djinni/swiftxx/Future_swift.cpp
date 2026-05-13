#include "Future_swift.hpp"

namespace djinni::swift {

void setFutureCb(const AnyValue* futureValue, FutureCb cb, void* ctx) {
    auto holder = std::dynamic_pointer_cast<AbstractCppFutureHolder>(std::get<OpaqueValuePtr>(*futureValue));
    holder->setFutureCb(cb, ctx);
}

AnyValue makeFutureValue(CleanupCb cleanup) {
    OpaqueValuePtr holder = std::make_shared<SwiftFutureHolder>(cleanup);
    return {holder};
}

void setFutureResult(const AnyValue* futureValue, const AnyValue* futureResult) {
    auto holder = std::dynamic_pointer_cast<SwiftFutureHolder>(std::get<OpaqueValuePtr>(*futureValue));
    holder->setValue(futureResult);
}

void storeSubscription(const AnyValue* futureValue, void* subscription) {
    auto holder = std::dynamic_pointer_cast<SwiftFutureHolder>(std::get<OpaqueValuePtr>(*futureValue));
    holder->subscription = subscription;
}

}
