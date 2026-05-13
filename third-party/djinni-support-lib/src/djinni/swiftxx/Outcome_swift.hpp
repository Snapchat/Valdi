#pragma once

#include "djinni_support.hpp"
#include "../cpp/expected.hpp"

namespace djinni::swift {

template<typename RESULT, typename ERROR>
class OutcomeAdaptor {
    using ResultCppType = typename RESULT::CppType;
    using ErrorCppType = typename ERROR::CppType;
public:
    using CppType = expected<ResultCppType, ErrorCppType>;
    static CppType toCpp(const AnyValue& s) {
        auto comp = std::get<CompositeValuePtr>(s);
        auto val = comp->getValue(0);
        if (!std::holds_alternative<VoidValue>(val)) {
            return { RESULT::toCpp(val) }; 
        } else {
            auto err = comp->getValue(1);
            return make_unexpected(ERROR::toCpp(err));
        }
    }
    static AnyValue fromCpp(const CppType& c) {
        auto ret = std::make_shared<CompositeValue>();
        if (c.has_value()) {
            ret->addValue(RESULT::fromCpp(c.value()));
        } else {
            ret->addValue(makeVoidValue());
            ret->addValue(ERROR::fromCpp(c.error()));
        }
        return {ret};
    }
};

}
