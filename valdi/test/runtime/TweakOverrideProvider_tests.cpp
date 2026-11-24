#include <gtest/gtest.h>

#include "valdi/runtime/Interfaces/TweakOverrideProvider.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

using namespace Valdi;

namespace ValdiTest {

namespace {

class RecordingProvider : public SharedPtrRefCountable, public ITweakValueProvider {
public:
    StringBox getString(const StringBox& key, const StringBox& fallback) override {
        return STRING_LITERAL("base");
    }
    bool getBool(const StringBox& key, bool fallback) override {
        return true;
    }
    float getFloat(const StringBox& key, float fallback) override {
        return 7.5f;
    }
    int32_t getInt(const StringBox& key, int32_t fallback) override {
        return 7;
    }
    Value getBinary(const StringBox& key, const Value& fallback) override {
        return Value(STRING_LITERAL("binary"));
    }
};

Shared<TweakOverrideProvider> make(std::string_view spec, Shared<ITweakValueProvider> base = nullptr) {
    return makeShared<TweakOverrideProvider>(TweakOverrideProvider::parse(spec), std::move(base)).toShared();
}

} // namespace

TEST(TweakOverrideProvider, parsesEntries) {
    auto overrides = TweakOverrideProvider::parse(" A = 1 ,B=two,,=x,novalue,C=,A=3");

    ASSERT_EQ(static_cast<size_t>(3), overrides.size());
    ASSERT_EQ("3", overrides["A"]);
    ASSERT_EQ("two", overrides["B"]);
    ASSERT_EQ("", overrides["C"]);
}

TEST(TweakOverrideProvider, parsesEmptySpec) {
    ASSERT_TRUE(TweakOverrideProvider::parse("").empty());
}

TEST(TweakOverrideProvider, readsBools) {
    auto provider = make("T1=1,T2=TRUE,T3=yes,T4=on,F1=0,F2=False,F3=no,F4=off,BAD=maybe");

    for (const char* key : {"T1", "T2", "T3", "T4"}) {
        ASSERT_TRUE(provider->getBool(StringBox::fromCString(key), false)) << key;
    }
    for (const char* key : {"F1", "F2", "F3", "F4"}) {
        ASSERT_FALSE(provider->getBool(StringBox::fromCString(key), true)) << key;
    }
    ASSERT_TRUE(provider->getBool(STRING_LITERAL("BAD"), true));
    ASSERT_FALSE(provider->getBool(STRING_LITERAL("BAD"), false));
}

TEST(TweakOverrideProvider, readsNumbers) {
    auto provider = make("I=42,N=-3,F=1.5,BAD=4x");

    ASSERT_EQ(42, provider->getInt(STRING_LITERAL("I"), 0));
    ASSERT_EQ(-3, provider->getInt(STRING_LITERAL("N"), 0));
    ASSERT_EQ(1.5f, provider->getFloat(STRING_LITERAL("F"), 0));
    ASSERT_EQ(9, provider->getInt(STRING_LITERAL("BAD"), 9));
    ASSERT_EQ(9.0f, provider->getFloat(STRING_LITERAL("BAD"), 9));
}

TEST(TweakOverrideProvider, readsStrings) {
    auto provider = make("S=hello");

    ASSERT_EQ(STRING_LITERAL("hello"), provider->getString(STRING_LITERAL("S"), STRING_LITERAL("fallback")));
    ASSERT_EQ(STRING_LITERAL("fallback"), provider->getString(STRING_LITERAL("MISSING"), STRING_LITERAL("fallback")));
}

TEST(TweakOverrideProvider, unsetKeysUseTheFallbackWithoutABaseProvider) {
    auto provider = make("A=1");

    ASSERT_TRUE(provider->getBool(STRING_LITERAL("MISSING"), true));
    ASSERT_FALSE(provider->getBool(STRING_LITERAL("MISSING"), false));
    ASSERT_EQ(5, provider->getInt(STRING_LITERAL("MISSING"), 5));
    ASSERT_EQ(2.5f, provider->getFloat(STRING_LITERAL("MISSING"), 2.5f));
    ASSERT_EQ(Value(STRING_LITERAL("x")), provider->getBinary(STRING_LITERAL("MISSING"), Value(STRING_LITERAL("x"))));
}

TEST(TweakOverrideProvider, unsetKeysAndBadValuesDeferToTheBaseProvider) {
    auto provider = make("OVERRIDDEN=0,BAD=zzz", makeShared<RecordingProvider>().toShared());

    ASSERT_FALSE(provider->getBool(STRING_LITERAL("OVERRIDDEN"), true));
    ASSERT_TRUE(provider->getBool(STRING_LITERAL("OTHER"), false));
    ASSERT_EQ(7, provider->getInt(STRING_LITERAL("BAD"), 0));
    ASSERT_EQ(7.5f, provider->getFloat(STRING_LITERAL("BAD"), 0));
    ASSERT_EQ(STRING_LITERAL("base"), provider->getString(STRING_LITERAL("OTHER"), StringBox()));
    ASSERT_EQ(Value(STRING_LITERAL("binary")), provider->getBinary(STRING_LITERAL("OTHER"), Value()));
}

} // namespace ValdiTest
