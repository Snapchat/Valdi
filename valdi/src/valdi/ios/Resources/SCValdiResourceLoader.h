//
//  SCValdiResourceLoader.h
//  valdi-ios
//
//  Created by Simon Corsin on 10/1/19.
//

#ifdef __cplusplus

#include "valdi/runtime/Interfaces/IResourceLoader.hpp"
@protocol SCValdiCustomModuleProvider;

namespace ValdiIOS {

class ResourceLoader : public Valdi::IResourceLoader {
public:
    ResourceLoader(id<SCValdiCustomModuleProvider> moduleProvider);
    ~ResourceLoader() override;

    Valdi::Result<Valdi::BytesView> loadModuleContent(const Valdi::StringBox& module) override;

    std::optional<Valdi::ModuleContentIdentity> getModuleIdentity(const Valdi::StringBox& module) override;

    Valdi::StringBox resolveLocalAssetURL(const Valdi::StringBox& moduleName,
                                          const Valdi::StringBox& resourcePath) override;

private:
    id<SCValdiCustomModuleProvider> _moduleProvider;
};

} // namespace ValdiIOS

#endif
