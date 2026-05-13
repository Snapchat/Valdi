#include "valdi_core/cpp/Marshalling/CppGeneratedEnum.hpp"
#include "valdi_core/cpp/Marshalling/RegisteredCppGeneratedClass.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"

namespace Valdi {

RegisteredCppGeneratedClass* CppGeneratedEnum::registerEnumSchema(const char* schemaString) {
    return new RegisteredCppGeneratedClass(ValueSchemaRegistry::sharedInstance().get(), schemaString, true);
}

} // namespace Valdi