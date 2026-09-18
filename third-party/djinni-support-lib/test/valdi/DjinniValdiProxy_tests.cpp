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

#include "valdi/djinni_valdi.hpp"

#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Utils/ValueTypedObject.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"

#include "gtest/gtest.h"

namespace {

using namespace Valdi;

class ExpiredTestProxyObject : public ValueTypedProxyObject {
public:
    explicit ExpiredTestProxyObject(const Ref<ValueTypedObject>& typedObject) : ValueTypedProxyObject(typedObject) {}
    std::string_view getType() const final {
        return "Expired Test Proxy";
    }
    bool expired() const final {
        return true;
    }
};

class ScopedGlobalOneWayCalls {
public:
    explicit ScopedGlobalOneWayCalls(bool enabled) : _previous(djinni::valdi::globalOneWayCallsEnabled()) {
        djinni::valdi::setGlobalOneWayCalls(enabled);
    }
    ~ScopedGlobalOneWayCalls() {
        djinni::valdi::setGlobalOneWayCalls(_previous);
    }

private:
    const bool _previous;
};

constexpr size_t kVoidMethodIndex = 0;
constexpr size_t kNonVoidMethodIndex = 1;

djinni::valdi::ValdiProxyBase makeExpiredListenerProxy() {
    auto schema = ValueSchema::cls(STRING_LITERAL("TestListener"),
                                   true,
                                   {ClassPropertySchema(STRING_LITERAL("onEvent"),
                                                        ValueSchema::function(ValueSchema::voidType(), {})),
                                    ClassPropertySchema(STRING_LITERAL("getValue"),
                                                        ValueSchema::function(ValueSchema::integer(), {}))});
    auto typedObject = ValueTypedObject::make(schema.getClassRef());
    return djinni::valdi::ValdiProxyBase(makeShared<ExpiredTestProxyObject>(typedObject));
}

TEST(DjinniValdiProxyTest, oneWayVoidCallOnExpiredProxyIsDropped) {
    ScopedGlobalOneWayCalls oneWay(true);
    auto proxy = makeExpiredListenerProxy();

    Value result;
    EXPECT_NO_THROW(result = proxy.callJsMethod(kVoidMethodIndex, {}));
    EXPECT_TRUE(result.isUndefined());
}

TEST(DjinniValdiProxyTest, syncCallOnExpiredProxyStillThrows) {
    ScopedGlobalOneWayCalls oneWay(true);
    auto proxy = makeExpiredListenerProxy();

    EXPECT_THROW(proxy.callJsMethod(kNonVoidMethodIndex, {}), djinni::valdi::JsException);
}

TEST(DjinniValdiProxyTest, voidCallOnExpiredProxyStillThrowsWhenOneWayCallsDisabled) {
    ScopedGlobalOneWayCalls oneWay(false);
    auto proxy = makeExpiredListenerProxy();

    EXPECT_THROW(proxy.callJsMethod(kVoidMethodIndex, {}), djinni::valdi::JsException);
}

} // namespace
