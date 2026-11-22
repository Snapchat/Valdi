//
//  ModuleArchiveBytecodeCompiler.cpp
//  ValdiRuntime
//

#include "valdi/runtime/JavaScript/ModuleArchiveBytecodeCompiler.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptBridge.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptContextEntryPoint.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTypes.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi_core/cpp/Interfaces/ILogger.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"

namespace Valdi {

ModuleArchiveBytecodeCompiler::ModuleArchiveBytecodeCompiler(IJavaScriptBridge& jsBridge, Ref<ILogger> logger)
    : _jsBridge(jsBridge), _logger(std::move(logger)) {}

ModuleArchiveBytecodeCompiler::~ModuleArchiveBytecodeCompiler() {
    releaseResources();
}

bool ModuleArchiveBytecodeCompiler::isSupported() {
    if (!_supported) {
        Result<Void> result = Void();
        ensureQueue().sync([&]() { result = ensureContextOnQueue(); });
        _supported = result && _jsContext != nullptr && _jsContext->supportsPreCompilation();
        if (!*_supported) {
            releaseResources();
        }
    }
    return *_supported;
}

bool ModuleArchiveBytecodeCompiler::isCompiled(const BytesView& jsFile) {
    return getPreCompiledJsModuleData(jsFile).has_value();
}

DispatchQueue& ModuleArchiveBytecodeCompiler::ensureQueue() {
    // QuickJS must always be called from the thread that created its context.
    if (_queue == nullptr) {
        _queue = DispatchQueue::createThreaded(STRING_LITERAL("Valdi Bytecode Compiler"), ThreadQoSClassLowest);
    }
    return *_queue;
}

Result<Void> ModuleArchiveBytecodeCompiler::ensureContextOnQueue() {
    if (_jsContext != nullptr) {
        return Void();
    }
    auto valdiContext = makeShared<Context>(1, _logger);
    valdiContext->retainDisposables();
    JavaScriptContextEntry entry(valdiContext);
    auto jsContext = _jsBridge.createJsContext(nullptr, *_logger);
    JSExceptionTracker exceptionTracker(*jsContext);
    jsContext->initialize(IJavaScriptContextConfig(), exceptionTracker);
    if (!exceptionTracker) {
        auto error = exceptionTracker.extractError();
        jsContext = nullptr;
        valdiContext->releaseDisposables();
        return error;
    }
    _valdiContext = std::move(valdiContext);
    _jsContext = std::move(jsContext);
    return Void();
}

Result<BytesView> ModuleArchiveBytecodeCompiler::compile(std::string_view source, std::string_view filename) {
    Result<BytesView> result;
    ensureQueue().sync([&]() {
        auto ready = ensureContextOnQueue();
        if (!ready) {
            result = ready.moveError();
            return;
        }
        JavaScriptContextEntry entry(_valdiContext);
        JSExceptionTracker exceptionTracker(*_jsContext);
        auto bytecode = _jsContext->preCompile(source, filename, exceptionTracker);
        result = exceptionTracker.toResult(std::move(bytecode));
    });
    return result;
}

void ModuleArchiveBytecodeCompiler::releaseResources() {
    if (_queue == nullptr) {
        return;
    }
    _queue->sync([&]() {
        auto valdiContext = std::move(_valdiContext);
        if (valdiContext == nullptr) {
            return;
        }
        JavaScriptContextEntry entry(valdiContext);
        _jsContext = nullptr;
        valdiContext->releaseDisposables();
    });
    _queue->fullTeardown();
    _queue = nullptr;
}

} // namespace Valdi
