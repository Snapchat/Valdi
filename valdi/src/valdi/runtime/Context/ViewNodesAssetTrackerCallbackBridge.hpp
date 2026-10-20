#pragma once

#include "valdi/runtime/Context/IViewNodesAssetTracker.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

namespace Valdi {

class ViewNodesAssetTrackerCallbackBridge final : public IViewNodesAssetTracker {
public:
    static constexpr int32_t kEventTypeBeganRequesting = 1;
    static constexpr int32_t kEventTypeEndRequesting = 2;
    static constexpr int32_t kEventTypeLoadedAssetChanged = 3;

    explicit ViewNodesAssetTrackerCallbackBridge(Ref<ValueFunction> callback);
    ~ViewNodesAssetTrackerCallbackBridge() override;

    void onBeganRequestingLoadedAsset(RawViewNodeId viewNodeId, const Ref<Asset>& asset) override;
    void onEndRequestingLoadedAsset(RawViewNodeId viewNodeId, const Ref<Asset>& asset) override;
    void onLoadedAssetChanged(RawViewNodeId viewNodeId,
                              const Ref<Asset>& asset,
                              const std::optional<StringBox>& error) override;

private:
    Ref<ValueFunction> _callback;
};

} // namespace Valdi
