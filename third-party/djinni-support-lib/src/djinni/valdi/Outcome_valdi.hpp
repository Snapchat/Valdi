/**
  * Copyright 2024 Snap, Inc.
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *    http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */

#pragma once

#include "djinni_valdi.hpp"
#include "../cpp/expected.hpp"

namespace djinni::valdi {

template <class RESULT, class ERROR>
class Outcome
{
    using ResultCppType = typename RESULT::CppType;
    using ErrorCppType = typename ERROR::CppType;
    using ResultValdiType = typename RESULT::Boxed::ValdiType;
    using ErrorValdiType = typename ERROR::Boxed::ValdiType;
public:
    using CppType = expected<ResultCppType, ErrorCppType>;
    using ValdiType = Valdi::Value;
    using Boxed = Outcome;

    static CppType toCpp(ValdiType v)
    {
        auto outcomeCpp = castOrNull<Valdi::ValdiOutcome>(v.getValdiObject());
        if (outcomeCpp->error.isUndefined()) {
            return {RESULT::toCpp(outcomeCpp->result)};
        } else {
            return djinni::make_unexpected(ERROR::toCpp(outcomeCpp->error));
        }
    }

    static ValdiType fromCpp(const CppType& c)
    {
        auto outcomeCpp = Valdi::makeShared<Valdi::ValdiOutcome>();
        if (c.has_value()) {
            outcomeCpp->result = RESULT::fromCpp(c.value());
        } else {
            outcomeCpp->error = ERROR::fromCpp(c.error());
        }
        return Valdi::Value(outcomeCpp);
    }

    static const Valdi::ValueSchema& schema() {
        static auto schema = Valdi::ValueSchema::outcome(schemaOrRef<RESULT>(), schemaOrRef<ERROR>());
        return schema;
    }
};

} // namespace djinni
