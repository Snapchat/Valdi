#include "RuntimeTestsUtils.hpp"
#include "TestAsyncUtils.hpp"

using namespace Valdi;

namespace ValdiTest {

RuntimeWrapper::RuntimeWrapper() = default;
RuntimeWrapper::RuntimeWrapper(Valdi::IJavaScriptBridge* jsBridge, TSNMode tsnMode)
    : RuntimeWrapper(jsBridge, tsnMode, false) {}

RuntimeWrapper::RuntimeWrapper(Valdi::IJavaScriptBridge* jsBridge,
                               TSNMode tsnMode,
                               const Ref<IDiskCache>& runtimeDiskCache)
    : RuntimeWrapper(jsBridge, tsnMode, false, nullptr, runtimeDiskCache) {}

RuntimeWrapper::RuntimeWrapper(Valdi::IJavaScriptBridge* jsBridge, TSNMode tsnMode, bool enableViewPreloader)
    : RuntimeWrapper(jsBridge, tsnMode, enableViewPreloader, nullptr) {}

RuntimeWrapper::RuntimeWrapper(Valdi::IJavaScriptBridge* jsBridge,
                               TSNMode tsnMode,
                               bool enableViewPreloader,
                               const Shared<Valdi::ITweakValueProvider>& tweakValueProvider)
    : RuntimeWrapper(jsBridge, tsnMode, enableViewPreloader, tweakValueProvider, nullptr) {}

RuntimeWrapper::RuntimeWrapper(Valdi::IJavaScriptBridge* jsBridge,
                               TSNMode tsnMode,
                               bool enableViewPreloader,
                               const Shared<Valdi::ITweakValueProvider>& tweakValueProvider,
                               const Ref<IDiskCache>& runtimeDiskCache)
    : logger(&Valdi::ConsoleLogger::getLogger()),
      mainQueue(Valdi::makeShared<MainQueue>()),
      runtimeListener(Valdi::makeShared<RuntimeListener>()),
      diskCache(Valdi::makeShared<InMemoryDiskCache>()),
      runtimeManager(nullptr),
      runtime(nullptr),
      resourceLoader(Valdi::makeShared<Valdi::StandaloneResourceLoader>()) {
    standaloneRuntime = Valdi::ValdiStandaloneRuntime::create(false,
                                                              false,
                                                              enableViewPreloader,
                                                              true,
                                                              true,
                                                              jsBridge,
                                                              mainQueue,
                                                              runtimeDiskCache != nullptr ? runtimeDiskCache : diskCache,
                                                              runtimeListener,
                                                              resourceLoader,
                                                              tweakValueProvider);
    runtimeManager = &standaloneRuntime->getRuntimeManager();
    runtime = &standaloneRuntime->getRuntime();
    runtime->getResourceManager().setLazyModulePreloadingEnabled(false);
    runtime->getResourceManager().setEnableTSN(tsnMode == TSNMode::Enabled);
    runtime->setLimitToViewportDisabled(true);
    runtime->registerNativeModuleFactory(Valdi::makeShared<StandaloneGlobalScrollPerfLoggerBridgeModuleFactory>());

    // The MainThreadManager does a dispatch to figure out the main thread id,
    // so we flush it out here.
    mainQueue->runOnce();
}

RuntimeWrapper::~RuntimeWrapper() = default;

void RuntimeWrapper::teardown() {
    if (mainQueue != nullptr) {
        mainQueue->stop();
    }

    auto weakRuntime = Valdi::weakRef(runtime);
    auto weakRuntimeManager = Valdi::weakRef(runtimeManager);
    auto weakJavaScriptRuntime = Valdi::weakRef(runtime != nullptr ? runtime->getJavaScriptRuntime() : nullptr);

    standaloneRuntime = nullptr;
    runtime = nullptr;
    runtimeManager = nullptr;

    SC_ASSERT(weakRuntimeManager.expired());
    SC_ASSERT(weakRuntime.expired());
    SC_ASSERT(weakJavaScriptRuntime.expired());
}

Valdi::SharedViewNodeTree RuntimeWrapper::createViewNodeTreeAndContext(const char* bundleName,
                                                                       const char* documentName) {
    return createViewNodeTreeAndContext(bundleName, documentName, Valdi::Value::undefined());
}

Valdi::SharedViewNodeTree RuntimeWrapper::createViewNodeTreeAndContext(const char* bundleName,
                                                                       const char* documentName,
                                                                       const Valdi::Value& initialViewModel) {
    return createViewNodeTreeAndContext(bundleName, documentName, initialViewModel, Valdi::Value::undefined());
}

Valdi::SharedViewNodeTree RuntimeWrapper::createViewNodeTreeAndContext(const char* bundleName,
                                                                       const char* documentName,
                                                                       const Valdi::Value& initialViewModel,
                                                                       const Valdi::Value& context) {
    auto documentNameString = STRING_LITERAL(documentName);

    StringBox componentPath;
    if (documentNameString.indexOf('/')) {
        componentPath = STRING_FORMAT("{}/{}", bundleName, documentNameString);
    } else {
        componentPath = STRING_FORMAT("{}/src/{}.valdi", bundleName, documentNameString);
    }

    return createViewNodeTreeAndContext(componentPath, initialViewModel, context);
}

Valdi::SharedViewNodeTree RuntimeWrapper::createViewNodeTreeAndContext(const Valdi::StringBox& componentPath,
                                                                       const Valdi::Value& initialViewModel,
                                                                       const Valdi::Value& context) {
    Valdi::Shared<Valdi::LazyValueConvertible> viewModelConvertible;
    if (!initialViewModel.isNullOrUndefined()) {
        viewModelConvertible = Valdi::makeShared<Valdi::LazyValueConvertible>([=]() { return initialViewModel; });
    }

    auto contextConvertible = Valdi::makeShared<Valdi::LazyValueConvertible>([=]() { return context; });

    auto tree = standaloneRuntime->getRuntime().createViewNodeTreeAndContext(
        standaloneRuntime->getViewManagerContext(), componentPath, viewModelConvertible, contextConvertible);

    tree->setRootViewWithDefaultViewClass();

    return tree;
}

void RuntimeWrapper::setViewModel(const Valdi::SharedContext& context, const Valdi::Value& viewModel) {
    auto viewModelConvertible = Valdi::makeShared<Valdi::LazyValueConvertible>([=]() { return viewModel; });
    context->setViewModel(viewModelConvertible);
}

void RuntimeWrapper::flushJsQueue() {
    if (standaloneRuntime->getRuntime().getJavaScriptRuntime() != nullptr) {
        standaloneRuntime->getRuntime().getJavaScriptRuntime()->dispatchSynchronouslyOnJsThread(
            [](auto& jsEntry) { return; });
    }
}

void RuntimeWrapper::flushQueues() {
    flushJsQueue();
    mainQueue->flush();
}

void RuntimeWrapper::waitUntilAllUpdatesCompleted() {
    auto group = Valdi::makeShared<Valdi::AsyncGroup>();

    auto contexts = runtime->getContextManager().getAllContexts();
    for (const auto& context : contexts) {
        group->enter();
        context->waitUntilAllUpdatesCompleted(
            Valdi::makeShared<Valdi::ValueFunctionWithCallable>([=](const auto& /*callContext*/) -> Valdi::Value {
                group->leave();
                return Valdi::Value();
            }));
    }

    snap::utils::time::StopWatch sw;
    sw.start();
    auto expired = false;

    auto delay = std::chrono::milliseconds(1);

    while (!group->isCompleted() && !expired) {
        if (!mainQueue->runNextTask(std::chrono::steady_clock::now() + delay)) {
            if (sw.elapsed().seconds() > 10) {
                expired = true;
            }
        }
    }

    if (expired) {
        throw Valdi::Exception("Timer expired");
    }
}

void RuntimeWrapper::waitForNextRenderRequest() {
    auto totalRenderCount = runtimeListener->numberOfRenders + 1;
    auto str = fmt::format("Waiting for next render, total render count {}", totalRenderCount);
    logger->log(Valdi::LogTypeInfo, str);
    try {
        mainQueue->runUntilTrue(str.c_str(),
                                [&]() -> bool { return runtimeListener->numberOfRenders == totalRenderCount; });
    } catch (const Valdi::Exception& exc) {
        VALDI_ERROR(
            *logger, "Failed to wait for {} renders, reached {}", totalRenderCount, runtimeListener->numberOfRenders);
        throw exc;
    }
}

// Replace a resource from another existing resource to test hot reloading.
Result<Void> RuntimeWrapper::hotReload(const char* sourceResourcePath, const char* destResourcePath) {
    auto resourceId = Valdi::JavaScriptPathResolver::resolveResourceId(STRING_LITERAL(sourceResourcePath));
    if (!resourceId) {
        return resourceId.moveError();
    }

    auto bundle = runtime->getResourceManager().getBundle(resourceId.value().bundleName);

    auto jsResult = bundle->getJs(resourceId.value().resourcePath);
    if (!jsResult) {
        return jsResult.moveError();
    }

    auto destResourceId = Valdi::JavaScriptPathResolver::resolveResourceId(STRING_LITERAL(destResourcePath));
    if (!destResourceId) {
        return destResourceId.moveError();
    }

    auto jsResource = ResourceId(destResourceId.value().bundleName, destResourceId.value().resourcePath.append(".js"));

    hotReload(jsResource, jsResult.value().content);

    return Void();
}

void RuntimeWrapper::hotReload(const Valdi::StringBox& bundleName,
                               const Valdi::StringBox& path,
                               const std::string& fileToInject) {
    auto bytes = Valdi::makeShared<Valdi::ByteBuffer>(fileToInject);
    hotReload(Valdi::ResourceId(bundleName, path), bytes->toBytesView());
}

void RuntimeWrapper::hotReload(const Valdi::ResourceId& resourceId, const Valdi::BytesView& bytes) {
    std::vector<Valdi::Shared<Valdi::Resource>> resources;
    resources.emplace_back(Valdi::makeShared<Valdi::Resource>(resourceId, bytes));

    runtime->getJavaScriptRuntime()->dispatchOnJsThreadSync(
        nullptr, [runtime = runtime, resources = std::move(resources)](auto& /*jsEntry*/) {
            runtime->updateResources(resources);
        });
}

Valdi::Result<Valdi::Void> RuntimeWrapper::loadModule(const Valdi::StringBox& bundleName,
                                                      Valdi::ResourceManagerLoadModuleType loadType) {
    auto resultHolder = ResultHolder<Valdi::Void>::make();

    runtime->getResourceManager().loadModuleAsync(bundleName, loadType, resultHolder->makeCompletion());

    return resultHolder->waitForResult();
}

} // namespace ValdiTest
