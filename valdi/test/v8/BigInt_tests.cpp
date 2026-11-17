#include "valdi/v8/V8JavaScriptContextFactory.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi/runtime/Utils/RefCountableAutoreleasePool.hpp"
#include <gtest/gtest.h>

TEST(V8BigInt, preservesNativeAndJavaScriptValues) {
    Valdi::V8::V8JavaScriptContextFactory factory;
    auto context = factory.createJsContext(nullptr, Valdi::ConsoleLogger::getLogger());
    Valdi::RefCountableAutoreleasePool pool;
    Valdi::JSExceptionTracker tracker(*context);
    context->initialize(Valdi::IJavaScriptContextConfig(), tracker);
    ASSERT_TRUE(tracker);
    for (const auto* decimal :
         {"0", "-9223372036854775808", "18446744073709551616", "-1234567890123456789012345678901234567890"}) {
        auto native = Valdi::BigInt::make(decimal, tracker);
        auto value = context->newBigInt(*native, tracker);
        ASSERT_TRUE(tracker);
        EXPECT_EQ(Valdi::ValueType::BigInt, context->getValueType(value.get()));
        auto restored = context->valueToBigInt(value.get(), tracker);
        ASSERT_TRUE(tracker);
        EXPECT_EQ(decimal, restored->toString());
        auto literal = context->evaluate(std::string(decimal) + "n", "bigint.js", tracker);
        ASSERT_TRUE(tracker);
        EXPECT_EQ(decimal, context->valueToBigInt(literal.get(), tracker)->toString());
    }
}
