#include "valdi_core/ModuleFactory.hpp"
#include "valdi_core/cpp/Marshalling/CppGeneratedClass.hpp"
#include "valdi_core/cpp/Marshalling/CppMarshaller.hpp"

namespace Valdi {

template<typename T>
class CppGeneratedModuleFactory : public SharedPtrRefCountable, public snap::valdi_core::ModuleFactory {
public:
    explicit CppGeneratedModuleFactory() = default;

    Value loadModule() override {
        auto module = onLoadModule();
        SimpleExceptionTracker exceptionTracker;
        Value out;
        T::marshall(exceptionTracker, module, out);
        if (!exceptionTracker) {
            return Value(exceptionTracker.extractError());
        }
        return out;
    }

protected:
    virtual Ref<T> onLoadModule() = 0;
};

} // namespace Valdi