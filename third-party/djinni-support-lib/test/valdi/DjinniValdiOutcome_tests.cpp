/**
  * Copyright 2026 Snap, Inc.
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

#include "valdi/Outcome_valdi.hpp"
#include "valdi/djinni_valdi.hpp"

#include "gtest/gtest.h"

#include <optional>
#include <string>

namespace {

using OptionalString = djinni::valdi::Optional<std::optional, djinni::valdi::String>;
using OptionalI32 = djinni::valdi::Optional<std::optional, djinni::valdi::I32>;

template <class RESULT, class ERROR>
typename djinni::valdi::Outcome<RESULT, ERROR>::CppType roundTrip(
    const typename djinni::valdi::Outcome<RESULT, ERROR>::CppType& outcome) {
    using Bridge = djinni::valdi::Outcome<RESULT, ERROR>;
    return Bridge::toCpp(Bridge::fromCpp(outcome));
}

template <class RESULT, class ERROR>
Valdi::Ref<Valdi::ValdiOutcome> toValdiOutcome(const typename djinni::valdi::Outcome<RESULT, ERROR>::CppType& outcome) {
    auto value = djinni::valdi::Outcome<RESULT, ERROR>::fromCpp(outcome);
    return Valdi::castOrNull<Valdi::ValdiOutcome>(value.getValdiObject());
}

TEST(DjinniValdiOutcomeTest, successRoundTrips) {
    using Cpp = djinni::valdi::Outcome<djinni::valdi::I32, djinni::valdi::String>::CppType;
    auto result = roundTrip<djinni::valdi::I32, djinni::valdi::String>(Cpp{42});

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(42, result.value());
}

TEST(DjinniValdiOutcomeTest, errorRoundTrips) {
    using Cpp = djinni::valdi::Outcome<djinni::valdi::I32, djinni::valdi::String>::CppType;
    auto result =
        roundTrip<djinni::valdi::I32, djinni::valdi::String>(Cpp{djinni::make_unexpected(std::string("failed"))});

    ASSERT_FALSE(result.has_value());
    ASSERT_EQ("failed", result.error());
}

TEST(DjinniValdiOutcomeTest, emptyOptionalResultRoundTripsAsSuccess) {
    using Cpp = djinni::valdi::Outcome<OptionalI32, djinni::valdi::String>::CppType;
    auto result = roundTrip<OptionalI32, djinni::valdi::String>(Cpp{std::optional<int32_t>()});

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result.value().has_value());
}

TEST(DjinniValdiOutcomeTest, presentOptionalErrorRoundTrips) {
    using Cpp = djinni::valdi::Outcome<djinni::valdi::I32, OptionalString>::CppType;
    auto result = roundTrip<djinni::valdi::I32, OptionalString>(
        Cpp{djinni::make_unexpected(std::optional<std::string>("failed"))});

    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(std::optional<std::string>("failed"), result.error());
}

// A miss encoded as an error outcome with an empty optional error must stay an error across the
// bridge instead of being read back as a success with an absent result.
TEST(DjinniValdiOutcomeTest, emptyOptionalErrorRoundTripsAsError) {
    using Cpp = djinni::valdi::Outcome<djinni::valdi::I32, OptionalString>::CppType;
    auto result =
        roundTrip<djinni::valdi::I32, OptionalString>(Cpp{djinni::make_unexpected(std::optional<std::string>())});

    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(result.error().has_value());
}

TEST(DjinniValdiOutcomeTest, emptyOptionalErrorIsDiscriminatedAsError) {
    using Cpp = djinni::valdi::Outcome<djinni::valdi::Binary, OptionalString>::CppType;
    auto outcome = toValdiOutcome<djinni::valdi::Binary, OptionalString>(
        Cpp{djinni::make_unexpected(std::optional<std::string>())});

    ASSERT_NE(nullptr, outcome);
    // The platform outcome marshallers pick the result branch when the error is undefined.
    ASSERT_FALSE(outcome->error.isUndefined());
    ASSERT_TRUE(outcome->error.isNullOrUndefined());
    ASSERT_TRUE(outcome->result.isUndefined());
}

TEST(DjinniValdiOutcomeTest, emptyOptionalResultIsDiscriminatedAsSuccess) {
    using Cpp = djinni::valdi::Outcome<OptionalI32, OptionalString>::CppType;
    auto outcome = toValdiOutcome<OptionalI32, OptionalString>(Cpp{std::optional<int32_t>()});

    ASSERT_NE(nullptr, outcome);
    ASSERT_TRUE(outcome->error.isUndefined());
}

} // namespace
