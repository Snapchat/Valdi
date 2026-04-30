//
//  ValdiStandaloneRuntime.hpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/14/19.
//

#pragma once

#include "valdi/runtime/IRuntimeListener.hpp"
#include "valdi/runtime/Interfaces/IDiskCache.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

namespace Valdi {

class StandaloneMainQueue;
class StandaloneResourceLoader;
class StandaloneViewManager;
class Runtime;
class RuntimeManager;
class StandaloneExitCoordinator;
class ViewManagerContext;
class IJavaScriptBridge;
class ITweakValueProvider;

class ValdiStandaloneRuntime : public SharedPtrRefCountable {
public:
    ValdiStandaloneRuntime(Ref<RuntimeManager> runtimeManager,
                           Ref<Runtime> runtime,
                           Shared<StandaloneResourceLoader> resourceLoader,
                           Ref<StandaloneMainQueue> mainQueue,
                           std::unique_ptr<StandaloneViewManager> viewManager,
                           Ref<ViewManagerContext> viewManagerContext,
                           bool shouldWaitForHotReload);
    ~ValdiStandaloneRuntime() override;

    StandaloneResourceLoader& getResourceLoader() const;

    Runtime& getRuntime() const;
    RuntimeManager& getRuntimeManager() const;
    StandaloneViewManager& getViewManager() const;
    const Ref<ViewManagerContext>& getViewManagerContext() const;
    const Ref<StandaloneMainQueue>& getMainQueue() const;

    void setupJsRuntime(const std::vector<StringBox>& jsArguments);
    void evalScript(const StringBox& scriptPath, const std::vector<StringBox>& jsArguments);

    static Result<Void> preCompile(IJavaScriptBridge* jsBridge,
                                   const StringBox& inputPath,
                                   const StringBox& outputPath,
                                   const StringBox& filename);
    static Result<BytesView> preCompile(IJavaScriptBridge* jsBridge,
                                        const BytesView& inputJs,
                                        const StringBox& filename);

    static Ref<ValdiStandaloneRuntime> create(bool enableDebuggerService,
                                              bool disableHotReloader,
                                              bool enableViewPreloader,
                                              bool registerCustomAttributes,
                                              bool keepAttributesHistory,
                                              IJavaScriptBridge* jsBridge,
                                              const Ref<StandaloneMainQueue>& mainQueue,
                                              const Ref<IDiskCache>& diskCache,
                                              const Shared<IRuntimeListener>& runtimeListener,
                                              const Shared<StandaloneResourceLoader>& resourceLoader,
                                              const Shared<Valdi::ITweakValueProvider>& tweakValueProvider = nullptr);

private:
    Ref<RuntimeManager> _runtimeManager;
    Ref<Runtime> _runtime;
    Shared<StandaloneResourceLoader> _resourceLoader;
    Ref<StandaloneMainQueue> _mainQueue;
    std::unique_ptr<StandaloneViewManager> _viewManager;
    Ref<StandaloneExitCoordinator> _exitCoordinator;
    Ref<ViewManagerContext> _viewManagerContext;
    bool _shouldWaitForHotReload;

    Ref<ValueFunction> makeMainThreadFunction(const ValueFunctionCallable& callable);

    void doEvalScript(const StringBox& scriptPath, const std::vector<StringBox>& jsArguments);

    void waitForHotReloadAndEvalScript(const StringBox& scriptPath, const std::vector<StringBox>& jsArguments);
};

} // namespace Valdi
