//
//  Runtime.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/24/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include <vector>

#include "valdi/runtime/Interfaces/IJavaScriptBridge.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi_core/cpp/Interfaces/ILogger.hpp"
#include "valdi_core/cpp/Interfaces/IMainThreadDispatcher.hpp"

#include "valdi/runtime/Debugger/IDebuggerServiceListener.hpp"

#include "valdi/runtime/Utils/DumpedLogs.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/runtime/Utils/SharedAtomic.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueConvertible.hpp"

#include "valdi/runtime/Resources/Resource.hpp"
#include "valdi/runtime/Resources/ResourceManager.hpp"
#include "valdi/runtime/Resources/ResourceReloaderThrottler.hpp"
#include "valdi/runtime/Resources/UserSession.hpp"

#include "valdi/runtime/JavaScript/JavaScriptRuntime.hpp"

#include "valdi/runtime/Context/ContextManager.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Context/ViewNodeTreeManager.hpp"

#include "valdi/runtime/Attributes/Animator.hpp"
#include "valdi/runtime/Attributes/AttributesManager.hpp"

#include "valdi/runtime/Views/GlobalViewFactories.hpp"

#include "valdi/runtime/IRuntimeListener.hpp"

struct YGConfig;

namespace snap::valdi {
class HTTPRequestManager;
class Keychain;
class RuntimeMessageHandler;
} // namespace snap::valdi

namespace snap::valdi_core {
class ModuleFactory;
}

namespace Valdi {

class IDiskCache;
class IResourceLoader;
class JavaScriptModuleFactory;

class Runtime;
class RenderViewNodeRequest;
using SharedRuntime = Ref<Runtime>;

class ViewManagerContext;
class ColorPalette;
class AssetLoaderManager;
class ITweakValueProvider;
class JavaScriptANRDetector;
class MetricsStopWatch;

class IDaemonClient;

extern float kUndefinedFloatValue;

class Runtime final : public IDebuggerServiceListener, IJavaScriptRuntimeListener, IContextManagerListener {
public:
    Runtime(AttributeIds& attributeIds,
            PlatformType platformType,
            const Ref<MainThreadManager>& mainThreadManager,
            IJavaScriptBridge* jsBridge,
            Ref<DispatchQueue> workerQueue,
            ThreadQoSClass jsRuntimeThreadQoS,
            const SharedAtomicObject<UserSession>& userSession,
            const Shared<snap::valdi::Keychain>& keychain,
            const Shared<IResourceLoader>& resourceLoader,
            const Ref<AssetLoaderManager>& assetLoaderManager,
            const Holder<Shared<snap::valdi_core::HTTPRequestManager>>& requestManager,
            const Ref<ColorPalette>& colorPalette,
            const Ref<IDiskCache>& diskCache,
            const std::shared_ptr<YGConfig>& yogaConfig,
            const Shared<snap::valdi::RuntimeMessageHandler>& runtimeMessageHandler,
            const Ref<JavaScriptANRDetector>& anrDetector,
            double deviceDensity,
            bool debuggerServiceEnabled,
            const Ref<ILogger>& logger);

    ~Runtime() final;

    /**
     * Destroy the Runtime, releasing all resources.
     */
    void fullTeardown();

    SharedContext createContext(const Ref<ViewManagerContext>& viewManagerContext,
                                const StringBox& path,
                                const Shared<ValueConvertible>& initialViewModel,
                                const Shared<ValueConvertible>& componentContext,
                                bool deferRender = true);

    SharedContext createContext(const Ref<ViewManagerContext>& viewManagerContext,
                                const ComponentPath& path,
                                const Shared<ValueConvertible>& initialViewModel,
                                const Shared<ValueConvertible>& componentContext,
                                bool deferRender = true);

    /**
     Create a ViewNodeTree representation for the given Context. It is an error to call createViewNodeTree()
     twice on the same context.
     */
    SharedViewNodeTree createViewNodeTree(
        const SharedContext& context,
        ViewNodeTreeThreadAffinity threadAffinity = ViewNodeTreeThreadAffinity::MAIN_THREAD);

    /**
     Get or create a ViewNodeTree representation for the given Context.
     Will return nullptr if the Context was already destroyed.
     */
    SharedViewNodeTree getOrCreateViewNodeTreeForContextId(ContextId contextId);

    /**
     * Convenience method to create both a Context and an associated ViewNodeTree.
     **/
    SharedViewNodeTree createViewNodeTreeAndContext(const Ref<ViewManagerContext>& viewManagerContext,
                                                    const StringBox& path);

    /**
     * Convenience method to create both a Context and an associated ViewNodeTree.
     * The context will be set with the given initial view model
     **/
    SharedViewNodeTree createViewNodeTreeAndContext(const Ref<ViewManagerContext>& viewManagerContext,
                                                    const StringBox& path,
                                                    const Shared<ValueConvertible>& initialViewModel);

    /**
     * Convenience method to create both a Context and an associated ViewNodeTree.
     * The context will be set with the given initial view model and component context
     **/
    SharedViewNodeTree createViewNodeTreeAndContext(const Ref<ViewManagerContext>& viewManagerContext,
                                                    const StringBox& path,
                                                    const Shared<ValueConvertible>& initialViewModel,
                                                    const Shared<ValueConvertible>& componentContext);

    /**
     * Returns whether there is a view node tree associated with the given context.
     */
    bool hasViewNodeTreeForContext(const SharedContext& context) const;

    /**
     Destroy the given Context and the JS component. Will also destroy the ViewNodeTree if there is one.
     */
    void destroyContext(const SharedContext& context);

    /**
     Destroy the given ViewNodeTree and its children, unattach all the views.
     */
    void destroyViewNodeTree(ViewNodeTree& viewNodeTree);

    /**
     Update the state of the given attribute name and value on the view node.
     This updates all the internal states so that the attribute has the given value.
     */
    void updateAttributeState(ViewNode& viewNode, const StringBox& attributeName, const Value& attributeValue);

    /**
     Update the resource, which can be a document or a js file, and reload all contexts that uses it.
     */
    void updateResource(const BytesView& resource, const StringBox& bundleName, const StringBox& filePathWithinBundle);

    /**
     Update resources in a batch.
     */
    void updateResources(const std::vector<Shared<Resource>>& resources);

    void onViewNodeTreeLayoutBecameDirty(ViewNodeTree& viewNodeTree);

    void receivedUpdatedResources(const std::vector<Shared<Resource>>& resources) override;

    void daemonClientConnected(const Shared<IDaemonClient>& daemonClient) override;
    void daemonClientDisconnected(const Shared<IDaemonClient>& daemonClient) override;
    void daemonClientDidReceiveClientPayload(const Shared<IDaemonClient>& daemonClient,
                                             int senderClientId,
                                             const BytesView& payload) override;

    void removeUnusedResources();

    ResourceManager& getResourceManager();

    JavaScriptRuntime* getJavaScriptRuntime();
    const ContextManager& getContextManager() const;
    ContextManager& getContextManager();
    const ViewNodeTreeManager& getViewNodeTreeManager() const;

    void setListener(const std::shared_ptr<IRuntimeListener>& listener);
    const std::shared_ptr<IRuntimeListener>& getListener() const;

    MainThreadManager& getMainThreadManager();

    /**
     Returns how many times resources were reloaded through the hot reloader
     */
    int getHotReloadSequence() const;

    void setTraceViewLifecycle(bool traceViewLifecycle);

    void setDebugRequests(bool debugRequests);

    void postInit();

    DumpedLogs dumpLogs(bool includeMetadata, bool includeVerbose, bool isCrashing) const;

    std::string dumpVerboseLogs() const;

    void setLimitToViewportDisabled(bool limitToViewportDisabled);
    bool limitToViewportDisabled() const;

    const Ref<DispatchQueue>& getWorkerQueue() const;

    ILogger& getLogger() const;

    // Made public just for tests
    void processRenderRequest(const Ref<RenderRequest>& renderRequest);

    bool debuggerServiceEnabled() const;

    AttributeIds& getAttributeIds() const;

    void setRuntimeMessageHandler(const Shared<snap::valdi::RuntimeMessageHandler>& runtimeMessageHandler);

    void registerNativeModuleFactory(const Shared<snap::valdi_core::ModuleFactory>& moduleFactory);
    void registerTypeConverter(const StringBox& className, const StringBox& functionPath);
    void registerJavaScriptModuleFactory(const Ref<JavaScriptModuleFactory>& moduleFactory);

    void setRuntimeTweaks(const Ref<ValdiRuntimeTweaks>& runtimeTweaks);

    void setMetrics(const Ref<Metrics>& metrics);
    const Ref<Metrics>& getMetrics() const;

    /**
     Set whether render requests should automatically be flushed when they are emitted.
     When automatic rendering is disabled, render requests won't be processed until setAutoRenderDisabled(false)
     is called.
     */
    void setAutoRenderDisabled(bool autoRenderDisabled);

    /**
     Set whether all updates propagated to the JS representation of a Component tree should be processed synchronously
     in the calling thread where the event occurs.
     */
    void setShouldProcessUpdatesSynchronously(bool shouldProcessUpdatesSynchronously);

    void emitInitMetrics();

    void updateColorPalette(const Value& colorPaletteMap) override;
    Value getColorPalette();

    const Ref<IDiskCache>& getDiskCache() const;

    Shared<snap::valdi_core::HTTPRequestManager> getRequestManager() const;

    /**
     * Return the runtime that is active in the current thread
     */
    static Ref<Runtime> currentRuntime();

    /**
     * Returns whether the hit test sync deadline should be disabled,
     * as controlled by the VALDI_DISABLE_HIT_TEST_SYNC_DEADLINE runtime tweak.
     */
    bool disableHitTestSyncDeadline() const;

protected:
    void receivedRenderRequest(const Ref<RenderRequest>& renderRequest) override;

    void receivedCallActionMessage(const ContextId& contextId,
                                   const StringBox& actionName,
                                   const Ref<ValueArray>& parameters) override;

    void resolveViewNodeTree(const SharedContext& context,
                             bool inMainThread,
                             bool createIfNeeded,
                             Function<void(const SharedViewNodeTree&)>&& function) override;

    void onContextCreated(const SharedContext& context) override;
    void onContextDestroyed(Context& context) override;

    void onUncaughtJsError(const StringBox& moduleName, const Error& error) override;

    void onDebugMessage(int32_t level, const StringBox& message) override;

    Ref<ValdiRuntimeTweaks> getRuntimeTweaks() override;

private:
    std::unique_ptr<MetricsStopWatch> _initStopWatch;
    AttributeIds& _attributeIds;
    PlatformType _platformType;
    Ref<ResourceManager> _resourceManager;
    ResourceReloaderThrottler _reloaderThrottler;
    ContextManager _contextManager;
    ViewNodeTreeManager _viewNodeManager;
    Ref<MainThreadManager> _mainThreadManager;
    Ref<ColorPalette> _colorPalette;
    Ref<IDiskCache> _diskCache;
    SharedAtomicObject<UserSession> _userSession;
    const Holder<Shared<snap::valdi_core::HTTPRequestManager>> _requestManager;
    Shared<snap::valdi::Keychain> _keychain;

    Ref<JavaScriptRuntime> _javaScriptRuntime;

    Shared<YGConfig> _yogaConfig;
    Ref<DispatchQueue> _workerQueue;
    Shared<snap::valdi::RuntimeMessageHandler> _runtimeMessageHandler;

    Ref<ILogger> _logger;
    bool _traceViewLifecycle = false;
    bool _debugRequests = false;
    bool _didEvaluateKotlinJs = false;
    bool _limitToViewportDisabled = false;
    bool _debuggerServiceEnabled;
    bool _didInit = false;
    bool _shouldProcessUpdatesSynchronously = false;
    std::atomic_bool _autoRenderDisabled = false;
    std::atomic_int _hotReloadSequence = 0;

    std::shared_ptr<IRuntimeListener> _listener;

    DumpedLogs dumpContextsLogs() const;

    Result<SharedContext> createContext(const StringBox& bundleName,
                                        const StringBox& documentName,
                                        ContextId emittingContextId,
                                        const StringBox& elementIdWithinParent,
                                        const Value& initialViewModel,
                                        const Value& componentContext);

    void destroyViewNodeTreeWithId(ContextId contextId);

    void doDestroyContext(const SharedContext& context);

    void runWithExclusiveJsThreadLock(DispatchFunction&& cb);
    bool disablePersistentStoreEncryption();
};

} // namespace Valdi
