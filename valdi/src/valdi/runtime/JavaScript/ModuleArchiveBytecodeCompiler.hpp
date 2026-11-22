//
//  ModuleArchiveBytecodeCompiler.hpp
//  ValdiRuntime
//

#pragma once

#include "valdi/runtime/Resources/IModuleArchiveCompiler.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <optional>

namespace Valdi {

class Context;
class DispatchQueue;
class IJavaScriptBridge;
class IJavaScriptContext;
class ILogger;

/**
 * Compiles JS files with a bare context of the process's JS engine, the same call the build's precompile step
 * makes. The context and its thread are created on the first compile and released by releaseResources().
 */
class ModuleArchiveBytecodeCompiler : public IModuleArchiveCompiler {
public:
    // `jsBridge` must outlive this object; the bridges are process-wide.
    ModuleArchiveBytecodeCompiler(IJavaScriptBridge& jsBridge, Ref<ILogger> logger);
    ~ModuleArchiveBytecodeCompiler() override;

    bool isSupported() override;
    bool isCompiled(const BytesView& jsFile) override;
    Result<BytesView> compile(std::string_view source, std::string_view filename) override;
    void releaseResources() override;

private:
    IJavaScriptBridge& _jsBridge;
    Ref<ILogger> _logger;
    Ref<DispatchQueue> _queue;
    Ref<Context> _valdiContext;
    Ref<IJavaScriptContext> _jsContext;
    std::optional<bool> _supported;

    DispatchQueue& ensureQueue();
    Result<Void> ensureContextOnQueue();
};

} // namespace Valdi
