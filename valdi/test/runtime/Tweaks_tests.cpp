#include <gtest/gtest.h>

#include "valdi/runtime/ValdiRuntimeTweaks.hpp"
#include "valdi/valdi.pb.h"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

using namespace Valdi;

namespace ValdiTest {

namespace {
class TestTweakValueProvider : public SharedPtrRefCountable, public ITweakValueProvider {
public:
    Value config;

    StringBox getString(const StringBox& key, const StringBox& fallback) override {
        return config.getMapValue(key).toStringBox();
    }

    bool getBool(const StringBox& key, bool fallback) override {
        return config.getMapValue(key).toBool();
    }

    float getFloat(const StringBox& key, float fallback) override {
        return config.getMapValue(key).toFloat();
    }

    int32_t getInt(const StringBox& key, int32_t fallback) override {
        return config.getMapValue(key).toInt();
    }

    Value getBinary(const StringBox& key, const Value& fallback) override {
        return config.getMapValue(key);
    }
};
} // namespace

TEST(RuntimeTweaks, tsnPerModuleSetting) {
    auto provider = makeShared<TestTweakValueProvider>();
    auto tweaks = makeShared<ValdiRuntimeTweaks>(provider.toShared());

    // not configured - always true
    ASSERT_TRUE(tweaks->enableTSNForModule(STRING_LITERAL("valdi_core")));
    ASSERT_TRUE(tweaks->enableTSNForModule(STRING_LITERAL("add_friends")));

    // create config for valdi_core
    Valdi::TsnConfig tsnConfig;
    tsnConfig.add_enabled_modules("valdi_core");

    auto len = tsnConfig.ByteSizeLong();
    std::vector<uint8_t> buffer(len);
    bool success = tsnConfig.SerializeToArray(buffer.data(), static_cast<int>(buffer.size()));
    ASSERT_TRUE(success);

    auto bytes = makeShared<Bytes>();
    bytes->assignVec(std::move(buffer));
    auto arrayValue = Value(makeShared<ValueTypedArray>(TypedArrayType::Uint8Array, Valdi::BytesView(bytes)));

    auto config = makeShared<ValueMap>();
    (*config)[STRING_LITERAL("VALDI_TSN_ENABLED_MODULES")] = arrayValue;

    // test only valdi_core is enabled
    provider->config = Value(config);
    ASSERT_TRUE(tweaks->enableTSNForModule(STRING_LITERAL("valdi_core")));
    ASSERT_FALSE(tweaks->enableTSNForModule(STRING_LITERAL("add_friends")));
}

} // namespace ValdiTest
