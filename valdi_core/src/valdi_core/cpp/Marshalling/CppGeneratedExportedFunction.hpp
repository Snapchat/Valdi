#pragma once

#include "valdi_core/cpp/Utils/Function.hpp"

namespace Valdi {

template<typename R, typename... A>
class CppGeneratedExportedFunction {
public:
    using Callable = Function<R(A...)>;

    explicit CppGeneratedExportedFunction(Callable&& callable) : _callable(std::move(callable)) {}

    ~CppGeneratedExportedFunction() = default;

    const Callable& callable() const {
        return _callable;
    }

    Callable& callable() {
        return _callable;
    }

    R operator()(A... arguments) const {
        return _callable(std::move(arguments)...);
    }

private:
    Callable _callable;
};

} // namespace Valdi