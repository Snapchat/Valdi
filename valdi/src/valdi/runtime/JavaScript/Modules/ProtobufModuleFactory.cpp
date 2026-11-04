//
//  ProtobufModuleFactory.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 11/21/19.
//

#include "valdi/runtime/JavaScript/Modules/ProtobufModuleFactory.hpp"

#include "valdi/runtime/JavaScript/Modules/ProtobufMessageFactory.hpp"
#include "valdi/runtime/JavaScript/Modules/ProtobufModule.hpp"

#include "valdi/runtime/JavaScript/JSFunctionWithMethod.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTypes.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi/runtime/Resources/ResourceManager.hpp"
#include "valdi/runtime/ValdiRuntimeTweaks.hpp"

#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

namespace Valdi {

class ProtobufModuleCallable : public JSFunctionWithMethod<ProtobufModule> {
public:
    using JSFunctionWithMethod<ProtobufModule>::JSFunctionWithMethod;

    ~ProtobufModuleCallable() override = default;

    void setModuleRef(const Ref<ProtobufModule>& moduleRef) {
        _moduleRef = moduleRef;
    }

private:
    Ref<ProtobufModule> _moduleRef;
};

ProtobufModuleFactory::ProtobufModuleFactory(ResourceManager& resourcesManager,
                                             const Ref<DispatchQueue>& workerQueue,
                                             ILogger& logger)
    : _resourcesManager(resourcesManager), _workerQueue(workerQueue), _logger(logger) {}

ProtobufModuleFactory::~ProtobufModuleFactory() = default;

static void registerModuleFunction(IJavaScriptContext& jsContext,
                                   const ReferenceInfoBuilder& referenceInfoBuilder,
                                   JSExceptionTracker& exceptionTracker,
                                   const JSValue& jsModule,
                                   const Ref<ProtobufModule>& pbModule,
                                   const std::string& functionName,
                                   JSValueRef (ProtobufModule::*function)(JSFunctionNativeCallContext&)) {
    auto internedFunctionName = StringCache::getGlobal().makeString(functionName);

    auto callable = makeShared<ProtobufModuleCallable>(
        *pbModule, function, referenceInfoBuilder.withProperty(internedFunctionName).build());
    callable->setModuleRef(pbModule);

    auto jsFunction = jsContext.newFunction(callable, exceptionTracker);

    if (!exceptionTracker) {
        return;
    }

    jsContext.setObjectProperty(jsModule, internedFunctionName.toStringView(), jsFunction.get(), exceptionTracker);
}

void ProtobufModuleFactory::preloadProtoModule(const Ref<ResourceManager>& resourcesManager,
                                               const StringBox& modulePath,
                                               ILogger& logger) {
    VALDI_TRACE("Protobuf.preloadMessages");
    SimpleExceptionTracker exceptionTracker;
    ProtobufModule::getMessageFactoryAtPath(*resourcesManager, modulePath, exceptionTracker);
    if (!exceptionTracker) {
        auto error = exceptionTracker.extractError();
        VALDI_ERROR(logger, "Failed to preload proto module at '{}': {}", modulePath, error);
    }
}

StringBox ProtobufModuleFactory::getModulePath() const {
    return STRING_LITERAL("ValdiProtobuf");
}

JSValueRef ProtobufModuleFactory::loadModule(IJavaScriptContext& jsContext,
                                             const ReferenceInfoBuilder& referenceInfoBuilder,
                                             JSExceptionTracker& exceptionTracker) {
    auto module = jsContext.newObject(exceptionTracker);
    if (!exceptionTracker) {
        return JSValueRef();
    }

    auto functions =
        std::vector({std::make_pair("loadMessages", &ProtobufModule::loadMessages),
                     std::make_pair("getFieldsForMessageDescriptor", &ProtobufModule::getFieldsForMessageDescriptor),
                     std::make_pair("getNamespaceEntries", &ProtobufModule::getNamespaceEntries),
                     std::make_pair("createMessage", &ProtobufModule::arenaCreateMessage),
                     std::make_pair("decodeMessage", &ProtobufModule::arenaDecodeMessage),
                     std::make_pair("decodeMessageAsync", &ProtobufModule::arenaDecodeMessageAsync),
                     std::make_pair("encodeMessage", &ProtobufModule::arenaEncodeMessage),
                     std::make_pair("encodeMessageAsync", &ProtobufModule::arenaEncodeMessageAsync),
                     std::make_pair("batchEncodeMessageAsync", &ProtobufModule::arenaBatchEncodeMessageAsync),
                     std::make_pair("encodeMessageToJSON", &ProtobufModule::arenaEncodeMessageToJSON),
                     std::make_pair("setMessageField", &ProtobufModule::arenaSetMessageField),
                     std::make_pair("getMessageFields", &ProtobufModule::arenaGetMessageFields),
                     std::make_pair("getMessageField", &ProtobufModule::arenaGetMessageField),
                     std::make_pair("copyMessage", &ProtobufModule::arenaCopyMessage),
                     std::make_pair("createArena", &ProtobufModule::createArena)});

    if constexpr (ProtobufModule::areProtoDebugFeaturesEnabled()) {
        functions.emplace_back("decodeMessageDebugJSONAsync", &ProtobufModule::arenaDecodeMessageDebugJSONAsync);
        functions.emplace_back("parseAndLoadMessages", &ProtobufModule::loadMessagesFromProtoFileContent);
    }

    auto m = makeShared<ProtobufModule>(_resourcesManager, _workerQueue, _logger);

    for (const auto& function : functions) {
        registerModuleFunction(
            jsContext, referenceInfoBuilder, exceptionTracker, module.get(), m, function.first, function.second);
        if (!exceptionTracker) {
            return JSValueRef();
        }
    }

    // The TS Arena reads this once per arena; the getter stays registered either way so tests can
    // force the per-field path without a tweak provider.
    const auto& runtimeTweaks = _resourcesManager.getRuntimeTweaks();
    auto fieldReadsEnabled = runtimeTweaks != nullptr && runtimeTweaks->enableProtobufFieldReads();
    jsContext.setObjectProperty(
        module.get(), "fieldReadsEnabled", jsContext.newBool(fieldReadsEnabled).get(), exceptionTracker);
    if (!exceptionTracker) {
        return JSValueRef();
    }

    return module;
}

} // namespace Valdi
