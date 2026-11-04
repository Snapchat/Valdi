//
//  ProtobufModuleFactory.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 11/21/19.
//

#pragma once

#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/Modules/JavaScriptModuleFactory.hpp"
#include "valdi_core/cpp/Interfaces/ILogger.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include "utils/platform/BuildOptions.hpp"
#include "utils/platform/TargetPlatform.hpp"

namespace Valdi {

class ResourceManager;
class IJavaScriptContext;
class DispatchQueue;
class ProtobufMessageFactory;

class ProtobufModule : public SimpleRefCountable {
public:
    ProtobufModule(ResourceManager& resourceManager, const Ref<DispatchQueue>& workerQueue, ILogger& logger);
    ~ProtobufModule() override;

    constexpr static bool areProtoDebugFeaturesEnabled() {
        return snap::isDesktop() || !snap::kIsAppstoreBuild;
    }

    static Ref<ProtobufMessageFactory> getMessageFactoryAtPath(ResourceManager& resourceManager,
                                                               const StringBox& importPath,
                                                               ExceptionTracker& exceptionTracker);

    JSValueRef loadMessages(JSFunctionNativeCallContext& callContext);
    JSValueRef loadMessagesFromProtoFileContent(JSFunctionNativeCallContext& callContext);

    JSValueRef getFieldsForMessageDescriptor(JSFunctionNativeCallContext& callContext);
    JSValueRef getNamespaceEntries(JSFunctionNativeCallContext& callContext);

    JSValueRef createArena(JSFunctionNativeCallContext& callContext);
    JSValueRef arenaCreateMessage(JSFunctionNativeCallContext& callContext);
    JSValueRef arenaDecodeMessage(JSFunctionNativeCallContext& callContext);
    JSValueRef arenaDecodeMessageAsync(JSFunctionNativeCallContext& callContext);
    JSValueRef arenaEncodeMessage(JSFunctionNativeCallContext& callContext);
    JSValueRef arenaEncodeMessageAsync(JSFunctionNativeCallContext& callContext);
    JSValueRef arenaBatchEncodeMessageAsync(JSFunctionNativeCallContext& callContext);
    JSValueRef arenaEncodeMessageToJSON(JSFunctionNativeCallContext& callContext);
    JSValueRef arenaDecodeMessageDebugJSONAsync(JSFunctionNativeCallContext& callContext);
    JSValueRef arenaSetMessageField(JSFunctionNativeCallContext& callContext);
    JSValueRef arenaGetMessageFields(JSFunctionNativeCallContext& callContext);
    JSValueRef arenaGetMessageField(JSFunctionNativeCallContext& callContext);
    JSValueRef arenaCopyMessage(JSFunctionNativeCallContext& callContext);

protected:
    ResourceManager& _resourcesManager;
    Ref<DispatchQueue> _workerQueue;
    [[maybe_unused]] ILogger& _logger;

    JSValueRef doLoadMessagesFromFactory(const Ref<ProtobufMessageFactory>& messageFactory,
                                         JSFunctionNativeCallContext& callContext);
};

} // namespace Valdi
