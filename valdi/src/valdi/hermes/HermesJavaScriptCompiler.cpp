//
//  HermesJavaScriptCompiler.cpp
//  valdi-hermes
//
//  Created by Simon Corsin on 12/1/23.
//

#include "valdi/hermes/HermesJavaScriptCompiler.hpp"
#include "utils/platform/TargetPlatform.hpp"
#include "valdi/hermes/HermesUtils.hpp"

namespace Valdi::Hermes {

class BytesViewBuffer : public hermes::Buffer {
public:
    explicit BytesViewBuffer(const BytesView& bytesView)
        : hermes::Buffer(bytesView.data(), bytesView.size()), _dataSource(bytesView.getSource()) {}
    ~BytesViewBuffer() override = default;

private:
    Ref<RefCountable> _dataSource;
};

constexpr size_t kCacheCapacity = 40;

HermesJavaScriptCompiler::HermesJavaScriptCompiler() : _cache(kCacheCapacity) {}
HermesJavaScriptCompiler::~HermesJavaScriptCompiler() = default;

#ifndef HERMESVM_LEAN
Shared<hermes::hbc::BCProvider> HermesJavaScriptCompiler::compile(const std::string& script,
                                                                  const std::string_view& sourceFilename,
                                                                  JSExceptionTracker& exceptionTracker) {
    std::lock_guard<Mutex> lock(_mutex);
    auto entry = _cache.find(script);
    if (entry != _cache.end()) {
        return entry->value();
    }

    auto compiledBytecode = compileToBytecode(script,
                                              sourceFilename,
                                              hermes::OutputFormatKind::Execute,
                                              {},
                                              hermes::BytecodeGenerationOptions::defaults(),
                                              exceptionTracker);

    if (!exceptionTracker) {
        return nullptr;
    }

    _cache.insert(script, compiledBytecode);

    return compiledBytecode;
}

BytesView HermesJavaScriptCompiler::compileToSerializedBytecode(const std::string& script,
                                                                const std::string_view& sourceFilename,
                                                                JSExceptionTracker& exceptionTracker) {
    std::function<void(hermes::Module&)> runOptimizationPasses;
    if constexpr (snap::isDesktop()) {
        // Enable optimizations on desktop for release type builds.
        // This will slow down JS compilation but emit JS code that is faster
        runOptimizationPasses = hermes::runFullOptimizationPasses;
    }

    auto options = hermes::BytecodeGenerationOptions::defaults();
    options.format = hermes::OutputFormatKind::EmitBundle;
    options.optimizationEnabled = true;
    options.stripFunctionNames = true;
    options.stripDebugInfoSection = true;
    options.stripSourceMappingURL = true;

    auto compiledBytecode = compileToBytecode(
        script, sourceFilename, hermes::OutputFormatKind::EmitBundle, runOptimizationPasses, options, exceptionTracker);

    if (!exceptionTracker) {
        return BytesView();
    }

    auto output = makeShared<ByteBuffer>();

    ByteBufferOStream stream(*output);

    hermes::hbc::BytecodeSerializer serializer{stream, options};
    serializer.serialize(
        *compiledBytecode->getBytecodeModule(),
        llvh::SHA1::hash(llvh::makeArrayRef(reinterpret_cast<const uint8_t*>(script.data()), script.size())));

    return stream.toBytesView();
}

static hermes::hbc::CompileFlags getCompileFlags(hermes::OutputFormatKind outputFormat, bool debug) {
    hermes::hbc::CompileFlags compileFlags;
    compileFlags.format = outputFormat;
    compileFlags.strict = true;
    compileFlags.includeLibHermes = false;
    compileFlags.enableBlockScoping = true;
    compileFlags.enableES6Classes = true;
    compileFlags.debug = debug;
    compileFlags.emitAsyncBreakCheck = true;

    return compileFlags;
}

Shared<hermes::hbc::BCProviderFromSrc> HermesJavaScriptCompiler::compileToBytecode(
    const std::string& script,
    const std::string_view& sourceFilename,
    hermes::OutputFormatKind outputFormat,
    std::function<void(hermes::Module&)> runOptimizations,
    const hermes::BytecodeGenerationOptions& bytecodeGenerationsOptions,
    JSExceptionTracker& exceptionTracker) {
    auto compileFlags = getCompileFlags(outputFormat, !runOptimizations);

    auto buffer =
        std::make_unique<hermes::OwnedMemoryBuffer>(llvh::MemoryBuffer::getMemBuffer(llvh::StringRef(script)));

    auto res = hermes::hbc::BCProviderFromSrc::createBCProviderFromSrc(
        std::move(buffer),
        llvh::StringRef(sourceFilename.data(), sourceFilename.size()),
        nullptr,
        compileFlags,
        {},
        {},
        nullptr,
        runOptimizations,
        bytecodeGenerationsOptions);

    if (!res.first) {
        exceptionTracker.onError(res.second);
        return nullptr;
    }

    return Shared<hermes::hbc::BCProviderFromSrc>(std::move(res.first));
}

HermesJavaScriptCompiler* HermesJavaScriptCompiler::get() {
    static auto* kInstance = new HermesJavaScriptCompiler();
    return kInstance;
}

#endif

static std::unique_ptr<hermes::Buffer> toBytecodeBuffer(const BytesView& bytecode) {
    if (llvh::alignAddr(bytecode.data(), hermes::hbc::BYTECODE_ALIGNMENT) != (uintptr_t)bytecode.data()) {
        // Buffer not aligned, should copy into an aligned buffer
        return std::make_unique<hermes::OwnedMemoryBuffer>(llvh::MemoryBuffer::getMemBufferCopy(
            llvh::StringRef(reinterpret_cast<const char*>(bytecode.data()), bytecode.size())));
    } else {
        return std::make_unique<BytesViewBuffer>(bytecode);
    }
}

Shared<hermes::hbc::BCProvider> HermesJavaScriptCompiler::deserializeBytecode(const BytesView& serializedBytecode,
                                                                              JSExceptionTracker& exceptionTracker) {
    auto res = hermes::hbc::BCProviderFromBuffer::createBCProviderFromBuffer(toBytecodeBuffer(serializedBytecode));
    if (!res.first) {
        exceptionTracker.onError(res.second);
        return nullptr;
    }

    return Shared<hermes::hbc::BCProvider>(std::move(res.first));
}

} // namespace Valdi::Hermes
