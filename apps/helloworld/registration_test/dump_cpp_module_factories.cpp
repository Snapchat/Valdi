// Prints every C++ ModuleFactory statically registered by the linked libraries, one per line:
// <module path>\t<comma-separated export names>. Read by hello_world_package_test.py.

#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

int main() {
    auto factories = Valdi::ModuleFactoryRegistry::sharedInstance()->createModuleFactories(Valdi::Value());
    for (const auto& factory : factories) {
        std::vector<std::string> exports;
        auto module = factory->loadModule();
        if (const auto* map = module.getMap()) {
            for (const auto& entry : *map) {
                exports.emplace_back(entry.first.slowToString());
            }
        }
        std::sort(exports.begin(), exports.end());
        std::cout << factory->getModulePath().slowToString() << "\t";
        for (size_t i = 0; i < exports.size(); i++) {
            std::cout << (i ? "," : "") << exports[i];
        }
        std::cout << "\n";
    }
    return 0;
}
