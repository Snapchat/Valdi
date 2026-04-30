//
//  HTTPRequestManagerModuleFactory.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/14/20.
//

#include "HTTPRequestManagerModuleFactory.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/runtime/Utils/HTTPRequestManagerUtils.hpp"
#include "valdi_core/Cancelable.hpp"
#include "valdi_core/HTTPRequest.hpp"
#include "valdi_core/HTTPResponse.hpp"
#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

namespace Valdi {

HTTPRequestManagerModuleFactory::HTTPRequestManagerModuleFactory() = default;
HTTPRequestManagerModuleFactory::~HTTPRequestManagerModuleFactory() = default;

StringBox HTTPRequestManagerModuleFactory::getModulePath() {
    return STRING_LITERAL("valdi_http/src/NativeHTTPClient");
}

static Shared<snap::valdi_core::HTTPRequestManager> getRequestManager() {
    auto runtime = Runtime::currentRuntime();
    if (runtime == nullptr) {
        return nullptr;
    }

    return runtime->getRequestManager();
}

Value HTTPRequestManagerModuleFactory::loadModule() {
    Value out;

    auto strongSelf = strongSmallRef(this);

    out.setMapValue(
        "performRequest",
        Value(makeShared<ValueFunctionWithCallable>([strongSelf](const ValueFunctionCallContext& callContext) -> Value {
            auto requestManager = getRequestManager();
            if (requestManager == nullptr) {
                callContext.getExceptionTracker().onError(Error("No RequestManager set"));
                return Value::undefined();
            }

            const auto& requestObject = callContext.getParameter(0);
            auto url = requestObject.getMapValue("url").toStringBox();
            auto method = requestObject.getMapValue("method").toStringBox();
            auto headers = requestObject.getMapValue("headers");
            auto body = requestObject.getMapValue("body").getTypedArrayRef();
            auto priority = requestObject.getMapValue("priority").toInt();

            std::optional<BytesView> convertedBody;
            if (body != nullptr) {
                convertedBody = {body->getBuffer()};
            }

            auto completion =
                callContext.getParameter(1).checkedTo<Ref<ValueFunction>>(callContext.getExceptionTracker());
            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }

            auto requestCompletion = HTTPRequestManagerUtils::makeRequestCompletion(
                [completion = std::move(completion)](const Result<snap::valdi_core::HTTPResponse>& result) {
                    std::array<Value, 2> parameters;
                    if (result) {
                        const auto& response = result.value();

                        Value convertedResponse;
                        convertedResponse.setMapValue("headers", response.headers);
                        convertedResponse.setMapValue("statusCode", Value(response.statusCode));

                        if (response.body) {
                            convertedResponse.setMapValue(
                                "body",
                                Value(makeShared<ValueTypedArray>(TypedArrayType::Uint8Array, response.body.value())));
                        }

                        parameters[0] = std::move(convertedResponse);
                        parameters[1] = Value::undefined();
                    } else {
                        parameters[0] = Value::undefined();
                        parameters[1] = Value(result.error());
                    }

                    (*completion)(parameters.data(), parameters.size());
                });

            auto cancellable = requestManager->performRequest(
                snap::valdi_core::HTTPRequest(
                    std::move(url), std::move(method), std::move(headers), std::move(convertedBody), priority),
                requestCompletion);

            Value out;
            out.setMapValue("cancel",
                            Value(makeShared<ValueFunctionWithCallable>(
                                [cancellable](const ValueFunctionCallContext& /*callContext*/) -> Value {
                                    cancellable->cancel();
                                    return Value::undefined();
                                })));

            return out;
        })));

    return out;
}

static auto kRegisterModule = Valdi::RegisterModuleFactory::registerTyped<HTTPRequestManagerModuleFactory>();

} // namespace Valdi
