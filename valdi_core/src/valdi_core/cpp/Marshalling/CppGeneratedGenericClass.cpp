#include "valdi_core/cpp/Marshalling/CppGeneratedGenericClass.hpp"
#include "valdi_core/cpp/Marshalling/RegisteredCppGeneratedClass.hpp"

namespace Valdi {

ValueSchema CppGeneratedGenericClass::getSchemaForRegisteredClass(RegisteredCppGeneratedClass* registeredClass,
                                                                  ExceptionTracker& exceptionTracker) {
    return registeredClass->getResolvedSchema(exceptionTracker);
}

} // namespace Valdi