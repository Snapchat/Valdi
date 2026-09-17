#pragma once

#include "valdi_test_utils.hpp"

namespace Valdi {
class ITweakValueProvider;
}

namespace ValdiTest {

enum class TSNMode {
    Enabled,
    Disabled,
};

struct RuntimeWrapper {
    Valdi::ConsoleLogger* logger = nullptr;
    Valdi::Ref<MainQueue> mainQueue;
    std::shared_ptr<RuntimeListener> runtimeListener;

    Valdi::Ref<Valdi::InMemoryDiskCache> diskCache;
    Valdi::Ref<Valdi::ValdiStandaloneRuntime> standaloneRuntime;
    Valdi::RuntimeManager* runtimeManager = nullptr;
    Valdi::Runtime* runtime = nullptr;
    Valdi::Shared<Valdi::StandaloneResourceLoader> resourceLoader;

    RuntimeWrapper();
    RuntimeWrapper(Valdi::IJavaScriptBridge* jsBridge, TSNMode tsnMode);

    RuntimeWrapper(Valdi::IJavaScriptBridge* jsBridge,
                   TSNMode tsnMode,
                   const Valdi::Ref<Valdi::IDiskCache>& runtimeDiskCache);

    RuntimeWrapper(Valdi::IJavaScriptBridge* jsBridge, TSNMode tsnMode, bool enableViewPreloader);

    RuntimeWrapper(Valdi::IJavaScriptBridge* jsBridge,
                   TSNMode tsnMode,
                   bool enableViewPreloader,
                   const Valdi::Shared<Valdi::ITweakValueProvider>& tweakValueProvider);

    RuntimeWrapper(Valdi::IJavaScriptBridge* jsBridge,
                   TSNMode tsnMode,
                   bool enableViewPreloader,
                   const Valdi::Shared<Valdi::ITweakValueProvider>& tweakValueProvider,
                   const Valdi::Ref<Valdi::IDiskCache>& runtimeDiskCache);

    ~RuntimeWrapper();

    void teardown();

    Valdi::SharedViewNodeTree createViewNodeTreeAndContext(const char* bundleName, const char* documentName);

    Valdi::SharedViewNodeTree createViewNodeTreeAndContext(const char* bundleName,
                                                           const char* documentName,
                                                           const Valdi::Value& initialViewModel);

    Valdi::SharedViewNodeTree createViewNodeTreeAndContext(const char* bundleName,
                                                           const char* documentName,
                                                           const Valdi::Value& initialViewModel,
                                                           const Valdi::Value& context);

    Valdi::SharedViewNodeTree createViewNodeTreeAndContext(const Valdi::StringBox& componentPath,
                                                           const Valdi::Value& initialViewModel,
                                                           const Valdi::Value& context);

    void setViewModel(const Valdi::SharedContext& context, const Valdi::Value& viewModel);

    void flushJsQueue();

    void flushQueues();

    void waitUntilAllUpdatesCompleted();

    void waitForNextRenderRequest();

    // Replace a resource from another existing resource to test hot reloading.
    Valdi::Result<Valdi::Void> hotReload(const char* sourceResourcePath, const char* destResourcePath);

    void hotReload(const Valdi::StringBox& bundleName, const Valdi::StringBox& path, const std::string& fileToInject);

    void hotReload(const Valdi::ResourceId& resourceId, const Valdi::BytesView& bytes);

    Valdi::Result<Valdi::Void> loadModule(const Valdi::StringBox& bundleName,
                                          Valdi::ResourceManagerLoadModuleType loadType);
};

} // namespace ValdiTest
