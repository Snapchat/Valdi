//
//  Base64ModuleFactory.cpp
//  valdi-ios
//

#include "valdi/runtime/JavaScript/Modules/Base64ModuleFactory.hpp"

#include "utils/encoding/Base64Utils.hpp"
#include "valdi/runtime/JavaScript/JSNativeClassBinder.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTypes.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"

namespace Valdi {

Base64ModuleFactory::Base64ModuleFactory() = default;
Base64ModuleFactory::~Base64ModuleFactory() = default;

StringBox Base64ModuleFactory::getModulePath() const {
    return STRING_LITERAL("coreutils/src/Base64Native");
}

JSValueRef Base64ModuleFactory::encodeToBase64(const JSTypedArray& bytes,
                                               bool urlSafe,
                                               JSFunctionNativeCallContext& callContext) {
    auto base64 = snap::utils::encoding::binaryToBase64(reinterpret_cast<const uint8_t*>(bytes.data), bytes.length);
    if (urlSafe) {
        snap::utils::encoding::base64ToBase64UrlInPlace(base64);
    }
    return callContext.getContext().newStringUTF8(base64, callContext.getExceptionTracker());
}

JSValueRef Base64ModuleFactory::decodeFromBase64(const Ref<StaticString>& base64,
                                                 JSFunctionNativeCallContext& callContext) {
    auto storage = base64->utf8Storage();
    auto input = std::string_view(storage.data, storage.length);

    auto bytes = makeShared<Bytes>();
    if (!snap::utils::encoding::base64UrlToBinary(input, *bytes)) {
        return callContext.throwError(Error("Invalid base64 string"));
    }

    auto arrayBuffer = callContext.getContext().newArrayBuffer(BytesView(bytes), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    return callContext.getContext().newTypedArrayFromArrayBuffer(
        TypedArrayType::Uint8Array, arrayBuffer.get(), callContext.getExceptionTracker());
}

JSValueRef Base64ModuleFactory::loadModule(IJavaScriptContext& jsContext,
                                           const ReferenceInfoBuilder& /*referenceInfoBuilder*/,
                                           JSExceptionTracker& exceptionTracker) {
    auto definition = JSNativeClassBinder<Base64ModuleFactory>("Base64Native")
                          .bindClassMethod<&Base64ModuleFactory::encodeToBase64>("encodeToBase64", true, true, true)
                          .bindClassMethod<&Base64ModuleFactory::decodeFromBase64>("decodeFromBase64", true, true, true)
                          .extractClassDefinition();

    return jsContext.newNativeClass(nullptr, definition, exceptionTracker);
}

} // namespace Valdi
