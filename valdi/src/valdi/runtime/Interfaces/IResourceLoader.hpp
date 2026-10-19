//
//  IResourceLoader.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/30/19.
//

#pragma once

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace Valdi {

/**
 * Identifies the content of a compressed module. For a given module name, two equal identities always
 * denote the same bytes.
 */
struct ModuleContentIdentity {
    enum class Kind : uint8_t {
        // SHA-256 of the decompressed archive, from the compiler-emitted content-hash trailer.
        ContentSha256 = 1,
        // XXH3-128 of the compressed bytes.
        CompressedHash = 2,
        // Identity of the dynamic-delivery container that serves the module.
        DeliveryContainer = 3,
        // Identity of a module file in the app bundle (device, inode, size and modification time).
        BundledFile = 4,
        // Identity of the immutable package that contains the module (e.g. the installed APK).
        BundledContainer = 5,
    };

    Kind kind = Kind::ContentSha256;
    // Opaque bytes; only compared for equality.
    std::string value;
};

class IResourceLoader {
public:
    virtual ~IResourceLoader() = default;

    /**
     Load the given Valdi module name and return it as bytes.
     */
    virtual Result<BytesView> loadModuleContent(const StringBox& module) = 0;

    /**
     Returns the identity of the bytes loadModuleContent(module) would return, without reading them, or
     nullopt if the loader can't tell cheaply. Implementations must return a different identity whenever
     the bytes could differ.
     */
    virtual std::optional<ModuleContentIdentity> getModuleIdentity(const StringBox& /*module*/) {
        return std::nullopt;
    }

    /**
     Resolves the local asset URL for the given asset.
     */
    virtual StringBox resolveLocalAssetURL(const StringBox& moduleName, const StringBox& resourcePath) = 0;
};

} // namespace Valdi
