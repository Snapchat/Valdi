//
//  IModuleArchiveCompiler.hpp
//  ValdiRuntime
//

#pragma once

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <string_view>

namespace Valdi {

/**
 * Compiles the JavaScript files of a module archive to the bytecode of the JS engine of this process, so that
 * ModuleArchiveCache can store bytecode in place of source. Only called from the cache's background queue,
 * never concurrently.
 */
class IModuleArchiveCompiler : public SimpleRefCountable {
public:
    /**
     * Returns whether the engine of this process can load compiled modules. Called before the first compile;
     * a false result turns compilation off for the rest of the process.
     */
    virtual bool isSupported() = 0;

    /**
     * Returns whether `jsFile` is already compiled for this engine.
     */
    virtual bool isCompiled(const BytesView& jsFile) = 0;

    /**
     * Compiles one JavaScript file. `filename` is the import path of the file, without the `.js` suffix, as the
     * build passes it.
     */
    virtual Result<BytesView> compile(std::string_view source, std::string_view filename) = 0;

    /**
     * Releases the engine resources held between compiles. Called after each publish batch.
     */
    virtual void releaseResources() = 0;
};

} // namespace Valdi
