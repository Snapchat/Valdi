//
//  ValdiStandalone.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/14/19.
//

#include "valdi/standalone_runtime/ValdiStandaloneRuntime.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"

#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptContextEntryPoint.hpp"
#include "valdi/runtime/Metrics/Metrics.hpp"
#include "valdi/runtime/Resources/AssetLoaderManager.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi/standalone_runtime/BridgeModules/ApplicationModule.hpp"
#include "valdi/standalone_runtime/BridgeModules/DeviceModule.hpp"
#include "valdi/standalone_runtime/InMemoryKeychain.hpp"
#include "valdi/standalone_runtime/StandaloneAssetLoader.hpp"
#include "valdi/standalone_runtime/StandaloneExitCoordinator.hpp"
#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"
#include "valdi/standalone_runtime/StandaloneResourceLoader.hpp"
#include "valdi/standalone_runtime/StandaloneView.hpp"
#include "valdi/standalone_runtime/StandaloneViewManager.hpp"
#include "valdi_core/ModuleFactory.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

#ifdef SNAP_DRAWING_ENABLED

#include "valdi/snap_drawing/Modules/SnapDrawingModuleFactoriesProvider.hpp"
#include "valdi/snap_drawing/Runtime.hpp"
#include "valdi/snap_drawing/Text/FontResolverWithRuntimeManager.hpp"

#include "snap_drawing/cpp/Drawing/IFrameScheduler.hpp"
#include "snap_drawing/cpp/Resources.hpp"
#include "snap_drawing/cpp/Text/FontManager.hpp"

#endif

namespace Valdi {

class ModuleFactory : public snap::valdi_core::ModuleFactory {
public:
    explicit ModuleFactory(Value module) : _module(std::move(module)) {}

    Value loadModule() override {
        return _module;
    }

private:
    Value _module;
};

#ifdef SNAP_DRAWING_ENABLED

void registerSnapDrawingModuleFactoriesProvider(RuntimeManager& runtimeManager) {
    auto lazy = makeShared<Lazy<Ref<snap::drawing::Runtime>>>([weakRuntimeManager = weakRef(&runtimeManager)]() {
        auto runtime = makeShared<snap::drawing::Runtime>(nullptr,
                                                          snap::drawing::GesturesConfiguration::getDefault(),
                                                          nullptr,
                                                          nullptr,
                                                          ConsoleLogger::getLogger(),
                                                          nullptr,
                                                          0);
        runtime->initializeViewManager(Valdi::PlatformTypeIOS);

        auto runtimeManager = weakRuntimeManager.lock();
        if (runtimeManager != nullptr) {
            runtime->registerAssetLoaders(*runtimeManager->getAssetLoaderManager());
            runtime->getFontManager()->setListener(
                makeShared<snap::drawing::FontResolverWithRuntimeManager>(runtimeManager));
        }

        return runtime;
    });
    auto moduleFactoriesProvider =
        makeShared<snap::drawing::SnapDrawingModuleFactoriesProvider>([lazy]() -> Ref<snap::drawing::Runtime> {
            return lazy->getOrCreate();
        }).toShared();

    runtimeManager.registerModuleFactoriesProvider(moduleFactoriesProvider);
}

#endif

ValdiStandaloneRuntime::ValdiStandaloneRuntime(Ref<RuntimeManager> runtimeManager,
                                               Ref<Runtime> runtime,
                                               Shared<StandaloneResourceLoader> resourceLoader,
                                               Ref<StandaloneMainQueue> mainQueue,
                                               std::unique_ptr<StandaloneViewManager> viewManager,
                                               Ref<ViewManagerContext> viewManagerContext,
                                               bool shouldWaitForHotReload)
    : _runtimeManager(std::move(runtimeManager)),
      _runtime(std::move(runtime)),
      _resourceLoader(std::move(resourceLoader)),
      _mainQueue(std::move(mainQueue)),
      _viewManager(std::move(viewManager)),
      _viewManagerContext(std::move(viewManagerContext)),
      _shouldWaitForHotReload(shouldWaitForHotReload) {
    if (!_shouldWaitForHotReload) {
        _exitCoordinator = Valdi::makeShared<StandaloneExitCoordinator>(
            _runtime->getJavaScriptRuntime()->getJsDispatchQueue(), _mainQueue);
        _exitCoordinator->postInit();
    }
}

ValdiStandaloneRuntime::~ValdiStandaloneRuntime() {
    if (_exitCoordinator != nullptr) {
        _exitCoordinator->flushUpdatesSync();
    }
    _exitCoordinator = nullptr;
    // Stop debugger services and other shared resources BEFORE tearing down the runtime.
    // This prevents the debugger from accepting new connections during runtime teardown.
    // Note: _runtime was created via _runtimeManager->createRuntime(), so it's in _runtimes
    // and will be torn down by _runtimeManager->fullTeardown(). Don't call it again.
    _runtimeManager->fullTeardown();
    _runtime = nullptr;
    _runtimeManager = nullptr;
}

Ref<ValueFunction> ValdiStandaloneRuntime::makeMainThreadFunction(const ValueFunctionCallable& callable) {
    auto mainQueue = _mainQueue;
    return makeShared<ValueFunctionWithCallable>([=](const ValueFunctionCallContext& callContext) -> Value {
        if (callContext.getParametersSize() < 1) {
            callContext.getExceptionTracker().onError(Error("Need at least one parameter"));
            return Value();
        }

        auto callback = callContext.getParameterAsFunction(callContext.getParametersSize() - 1);
        if (!callContext.getExceptionTracker()) {
            return Value();
        }

        Valdi::SmallVector<Value, 4> parameters;
        for (size_t i = 0; i < callContext.getParametersSize() - 1; i++) {
            parameters.emplace_back(callContext.getParameter(i));
        }

        mainQueue->enqueue([=]() {
            SimpleExceptionTracker exceptionTracker;
            auto result = callable(ValueFunctionCallContext(
                ValueFunctionFlagsNone, parameters.data(), parameters.size(), exceptionTracker));

            if (exceptionTracker) {
                (*callback)(&result, 1);
            } else {
                std::initializer_list<Value> outParameters = {Value::undefined(),
                                                              Value(exceptionTracker.extractError().toString())};
                (*callback)(outParameters.begin(), outParameters.size());
            }
        });

        return Value::undefined();
    });
}

void ValdiStandaloneRuntime::setupJsRuntime(const std::vector<StringBox>& jsArguments) {
    auto exitCoordinator = _exitCoordinator;

    auto standaloneRuntime = makeShared<ValueMap>();
    (*standaloneRuntime)[STRING_LITERAL("exit")] = Value(makeShared<ValueFunctionWithCallable>(
        [this, exitCoordinator](const ValueFunctionCallContext& callContext) -> Value {
            if (exitCoordinator != nullptr) {
                exitCoordinator->setEnabled(false);
            }

            if (callContext.getParametersSize() > 0) {
                _mainQueue->exit(static_cast<int>(callContext.getParameter(0).toInt()));
            } else {
                _mainQueue->exit(0);
            }
            return Value::undefined();
        }));
    (*standaloneRuntime)[STRING_LITERAL("debuggerEnabled")] = Value(_runtimeManager->debuggerServiceEnabled());

    (*standaloneRuntime)[STRING_LITERAL("destroyAllComponents")] =
        Value(makeShared<ValueFunctionWithCallable>([this](const ValueFunctionCallContext& /*callContext*/) -> Value {
            _mainQueue->enqueue([this]() {
                auto contexts = _runtime->getContextManager().getAllContexts();
                for (const auto& context : contexts) {
                    _runtime->destroyContext(context);
                }
            });

            return Value();
        }));

    (*standaloneRuntime)[STRING_LITERAL("getRenderedElement")] =
        Value(makeMainThreadFunction([this](const ValueFunctionCallContext& callContext) -> Value {
            auto componentId = callContext.getParameter(0);

            auto viewNodeTree = _runtime->getViewNodeTreeManager().getViewNodeTreeForContextId(
                static_cast<ContextId>(componentId.toInt()));
            if (viewNodeTree == nullptr) {
                callContext.getExceptionTracker().onError(
                    Error(STRING_FORMAT("Could not find rendered node with component id {}", componentId.toString())));
                return Value::undefined();
            }

            auto rootViewNode = viewNodeTree->getRootViewNode();
            if (rootViewNode == nullptr) {
                callContext.getExceptionTracker().onError(
                    Error(STRING_FORMAT("No rendered root node for component id {}", componentId.toString())));
                return Value::undefined();
            }

            return Value(rootViewNode->getDebugDescriptionMap());
        }));

    auto stdoutObject = makeShared<ValueMap>();
    (*stdoutObject)[STRING_LITERAL("write")] =
        Value(makeShared<ValueFunctionWithCallable>([=](const ValueFunctionCallContext& callContext) -> Value {
            auto str = callContext.getParameter(0).toStringBox();
            ConsoleLogger::getLogger().writeDirect(str.toStringView());

            return Value(true);
        }));

    ValueArrayBuilder outJsArguments;
    for (const auto& jsArgument : jsArguments) {
        outJsArguments.emplace(jsArgument);
    }

    auto processArguments = outJsArguments;
    processArguments.prepend(Value(STRING_LITERAL("valdi_standalone")));

    auto process = makeShared<ValueMap>();
    (*process)[STRING_LITERAL("argv")] = Value(processArguments.build());
    (*process)[STRING_LITERAL("stdout")] = Value(stdoutObject);
    _runtime->getJavaScriptRuntime()->setValueToGlobalObject(STRING_LITERAL("process"), Value(process));

    (*standaloneRuntime)[STRING_LITERAL("arguments")] = Value(outJsArguments.build());

    // TODO(4112): only get valdiStandalone once we're building tests from source
    _runtime->getJavaScriptRuntime()->setValueToGlobalObject(STRING_LITERAL("valdiStandalone"),
                                                             Value(standaloneRuntime));
    _runtime->getJavaScriptRuntime()->setValueToGlobalObject(STRING_LITERAL("valdiStandalone"),
                                                             Value(standaloneRuntime));
}

void ValdiStandaloneRuntime::doEvalScript(const StringBox& scriptPath, const std::vector<StringBox>& jsArguments) {
    setupJsRuntime(jsArguments);

    auto result = _runtime->getJavaScriptRuntime()->evalModuleSync(scriptPath, _shouldWaitForHotReload);
    if (!result) {
        VALDI_ERROR(ConsoleLogger::getLogger(), "{}", result.error());
        if (!_shouldWaitForHotReload) {
            _mainQueue->exit(1);
        }
    }
}

void ValdiStandaloneRuntime::waitForHotReloadAndEvalScript(const StringBox& scriptPath,
                                                           const std::vector<StringBox>& jsArguments) {
    if (_runtime->getHotReloadSequence() > 0) {
        VALDI_INFO(_runtime->getLogger(), "Received hot reloader resources! Starting runtime...");

        _runtime->postInit();

        doEvalScript(scriptPath, jsArguments);
        return;
    }

    auto weakSelf = weakRef(this);
    _mainQueue->enqueue(
        [weakSelf, scriptPath, jsArguments]() {
            auto strongSelf = weakSelf.lock();
            if (strongSelf != nullptr) {
                strongSelf->waitForHotReloadAndEvalScript(scriptPath, jsArguments);
            }
        },
        std::chrono::milliseconds(100));
}

void ValdiStandaloneRuntime::evalScript(const StringBox& scriptPath, const std::vector<StringBox>& jsArguments) {
    auto exitCoordinator = _exitCoordinator;
    if (exitCoordinator != nullptr) {
        exitCoordinator->setEnabled(true);
    }

    if (_shouldWaitForHotReload) {
        VALDI_INFO(_runtime->getLogger(), "Waiting for hot reloader resources...");
        waitForHotReloadAndEvalScript(scriptPath, jsArguments);
    } else {
        doEvalScript(scriptPath, jsArguments);
    }
}

StandaloneResourceLoader& ValdiStandaloneRuntime::getResourceLoader() const {
    return *_resourceLoader;
}

Runtime& ValdiStandaloneRuntime::getRuntime() const {
    return *_runtime;
}

RuntimeManager& ValdiStandaloneRuntime::getRuntimeManager() const {
    return *_runtimeManager;
}

const Ref<StandaloneMainQueue>& ValdiStandaloneRuntime::getMainQueue() const {
    return _mainQueue;
}

template<typename T>
static Result<T> withJsContext(IJavaScriptBridge* jsBridge, Valdi::Function<Result<T>(IJavaScriptContext&)> cb) {
    auto& logger = ConsoleLogger::getLogger();

    auto valdiContext = Valdi::makeShared<Valdi::Context>(1, Valdi::strongSmallRef(&logger));
    valdiContext->retainDisposables();

    JavaScriptContextEntry entry(valdiContext);

    auto context = jsBridge->createJsContext(nullptr, logger);
    auto result = cb(*context);

    valdiContext->releaseDisposables();

    return result;
}

Result<Void> ValdiStandaloneRuntime::preCompile(IJavaScriptBridge* jsBridge,
                                                const StringBox& inputPath,
                                                const StringBox& outputPath,
                                                const StringBox& filename) {
    auto inputFile = DiskUtils::load(Path(inputPath.toStringView()));
    if (!inputFile) {
        return inputFile.moveError();
    }

    auto preCompileResult = preCompile(jsBridge, inputFile.value(), filename);

    if (!preCompileResult) {
        return preCompileResult.moveError();
    }

    Path output(outputPath.toStringView());

    DiskUtils::remove(output);

    return DiskUtils::store(output, preCompileResult.value());
}

Result<BytesView> ValdiStandaloneRuntime::preCompile(IJavaScriptBridge* jsBridge,
                                                     const BytesView& inputJs,
                                                     const StringBox& filename) {
    auto dispatchQueue = DispatchQueue::createThreaded(STRING_LITERAL("Compile Thread"), ThreadQoSClassMax);

    Result<BytesView> preCompileResult;

    dispatchQueue->sync([&]() {
        preCompileResult = withJsContext<BytesView>(jsBridge, [&](IJavaScriptContext& jsContext) -> Result<BytesView> {
            JSExceptionTracker exceptionTracker(jsContext);
            jsContext.initialize(Valdi::IJavaScriptContextConfig(), exceptionTracker);
            if (!exceptionTracker) {
                return exceptionTracker.extractError();
            }

            return exceptionTracker.toResult(
                jsContext.preCompile(inputJs.asStringView(), filename.toStringView(), exceptionTracker));
        });
    });

    return preCompileResult;
}

StandaloneViewManager& ValdiStandaloneRuntime::getViewManager() const {
    return *_viewManager;
}

const Ref<ViewManagerContext>& ValdiStandaloneRuntime::getViewManagerContext() const {
    return _viewManagerContext;
}

Ref<ValdiStandaloneRuntime> ValdiStandaloneRuntime::create(
    bool enableDebuggerService,
    bool disableHotReloader,
    bool enableViewPreloader,
    bool registerCustomAttributes,
    bool keepAttributesHistory,
    IJavaScriptBridge* jsBridge,
    const Ref<StandaloneMainQueue>& mainQueue,
    const Ref<IDiskCache>& diskCache,
    const Shared<IRuntimeListener>& runtimeListener,
    const Shared<StandaloneResourceLoader>& resourceLoader,
    const Shared<Valdi::ITweakValueProvider>& tweakValueProvider) {
    auto viewManager = std::make_unique<StandaloneViewManager>();
    viewManager->setRegisterCustomAttributes(registerCustomAttributes);
    viewManager->setKeepAttributesHistory(keepAttributesHistory);

    auto runtimeManager = makeShared<Valdi::RuntimeManager>(mainQueue->createMainThreadDispatcher(),
                                                            jsBridge,
                                                            diskCache,
                                                            Valdi::makeShared<InMemoryKeychain>(),
                                                            nullptr,
                                                            viewManager->getPlatformType(),
                                                            Valdi::ThreadQoSClassMax,
                                                            Valdi::strongSmallRef(&ConsoleLogger::getLogger()),
                                                            enableDebuggerService,
                                                            disableHotReloader,
                                                            /* isStandalone */ true);
    runtimeManager->postInit();
    runtimeManager->registerBytesAssetLoader(diskCache);
    auto shouldWaitForHotReload = enableDebuggerService && !disableHotReloader;
    if (shouldWaitForHotReload) {
        runtimeManager->setDisableRuntimeAutoInit(true);
    }

#ifdef SNAP_DRAWING_ENABLED
    registerSnapDrawingModuleFactoriesProvider(*runtimeManager);
#endif

    if (diskCache != nullptr) {
        runtimeManager->getAssetLoaderManager()->registerAssetLoader(makeShared<StandaloneAssetLoader>(*diskCache));
    }
    if (tweakValueProvider != nullptr) {
        runtimeManager->setTweakValueProvider(tweakValueProvider);
    }

    auto viewManagerContext = runtimeManager->createViewManagerContext(*viewManager, enableViewPreloader);

    auto runtime = runtimeManager->createRuntime(resourceLoader, static_cast<double>(viewManager->getPointScale()));
    runtime->setListener(runtimeListener);

    runtime->registerNativeModuleFactory(makeShared<DeviceModule>().toShared());
    runtime->registerNativeModuleFactory(makeShared<ApplicationModule>().toShared());

    if (runtime->getJavaScriptRuntime() != nullptr) {
        runtime->getJavaScriptRuntime()->setDefaultViewManagerContext(viewManagerContext);
    }

    runtimeManager->attachHotReloader(runtime);

    runtimeManager->applicationDidResume();

    return Valdi::makeShared<ValdiStandaloneRuntime>(std::move(runtimeManager),
                                                     std::move(runtime),
                                                     resourceLoader,
                                                     mainQueue,
                                                     std::move(viewManager),
                                                     std::move(viewManagerContext),
                                                     shouldWaitForHotReload);
}

} // namespace Valdi