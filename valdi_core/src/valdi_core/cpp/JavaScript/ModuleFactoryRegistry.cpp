#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"

namespace Valdi {

static const RegisterModuleFactory* kModuleFactoryPtr = nullptr;

ModuleFactoryRegistry::ModuleFactoryRegistry() = default;

ModuleFactoryRegistry::~ModuleFactoryRegistry() = default;

void ModuleFactoryRegistry::collectStaticEntries() {
    const auto* current = kModuleFactoryPtr;

    while (current != nullptr) {
        _entries.emplace_back(current->_factory);
        current = current->_prev;
    }
}

void ModuleFactoryRegistry::registerModuleFactory(
    const std::shared_ptr<snap::valdi_core::ModuleFactory>& moduleFactory) {
    std::lock_guard<Mutex> guard(_mutex);
    _entries.emplace_back(moduleFactory);
}

void ModuleFactoryRegistry::registerModuleFactoriesProvider(
    const std::shared_ptr<snap::valdi_core::ModuleFactoriesProvider>& moduleFactoriesProvider) {
    std::lock_guard<Mutex> guard(_mutex);
    _entries.emplace_back(moduleFactoriesProvider);
}

void ModuleFactoryRegistry::registerModuleFactoryFn(ModuleFactoryFn moduleFactoryFn) {
    std::lock_guard<Mutex> guard(_mutex);
    _entries.emplace_back(std::move(moduleFactoryFn));
}

const std::shared_ptr<ModuleFactoryRegistry>& ModuleFactoryRegistry::sharedInstance() {
    static auto kInstance = std::make_shared<ModuleFactoryRegistry>();
    return kInstance;
}

std::vector</*not-null*/ std::shared_ptr<snap::valdi_core::ModuleFactory>> ModuleFactoryRegistry::createModuleFactories(
    const Valdi::Value& runtimeManager) {
    std::vector<std::shared_ptr<snap::valdi_core::ModuleFactory>> output;
    std::lock_guard<Mutex> guard(_mutex);

    if (!_collectedStaticEntries) {
        _collectedStaticEntries = true;
        collectStaticEntries();
    }

    for (const auto& entry : _entries) {
        if (const auto* moduleFactoryPtr = std::get_if<std::shared_ptr<snap::valdi_core::ModuleFactory>>(&entry)) {
            output.emplace_back(*moduleFactoryPtr);
        } else if (const auto* moduleFactoriesProviderPtr =
                       std::get_if<std::shared_ptr<snap::valdi_core::ModuleFactoriesProvider>>(&entry)) {
            auto moduleFactories = (*moduleFactoriesProviderPtr)->createModuleFactories(runtimeManager);
            output.insert(output.end(), moduleFactories.begin(), moduleFactories.end());
        } else if (const auto* moduleFactoryFnPtr = std::get_if<ModuleFactoryFn>(&entry)) {
            output.emplace_back((*moduleFactoryFnPtr)());
        }
    }

    return output;
}

RegisterModuleFactory::RegisterModuleFactory(ModuleFactoryFn factory)
    : _factory(std::move(factory)), _prev(kModuleFactoryPtr) {
    kModuleFactoryPtr = this;
}

} // namespace Valdi
