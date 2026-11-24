#include "JSIntegrationTestsUtils.hpp"
#include "valdi_core/cpp/Utils/BigInt.hpp"

using namespace Valdi;

namespace ValdiTest {

Ref<ValueFunction> toFunction(const Value& value) {
    if (value.getFunctionRef() == nullptr) {
        throw Exception(STRING_FORMAT("Value {} is not a function", value.toString()));
    }

    return value.getFunctionRef();
}

Valdi::Result<Valdi::Value> callFunction(const Ref<ValueFunction>& function, const std::vector<Value>& params) {
    return function->call(ValueFunctionFlagsCallSync, params.data(), params.size());
}

Valdi::Result<Valdi::Value> callFunction(const Ref<ValueFunction>& function) {
    return callFunction(function, {});
}

bool jsBridgeSupportsNativeBigInt(IJavaScriptBridge* jsBridge) {
    JSContextWrapper wrapper(jsBridge, nullptr);
    auto supported = false;
    wrapper.withContext([&](IJavaScriptContext& context, JSExceptionTracker& tracker) {
        context.newBigInt(*BigInt::make(int64_t(0)), tracker);
        supported = static_cast<bool>(tracker);
        tracker.clearError();
    });
    return supported;
}

} // namespace ValdiTest
