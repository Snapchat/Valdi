#include "valdi/runtime/Context/ViewNodesAssetTrackerCallbackBridge.hpp"

#include "valdi_core/cpp/Utils/Value.hpp"

namespace Valdi {

ViewNodesAssetTrackerCallbackBridge::ViewNodesAssetTrackerCallbackBridge(Ref<ValueFunction> callback)
    : _callback(std::move(callback)) {}

ViewNodesAssetTrackerCallbackBridge::~ViewNodesAssetTrackerCallbackBridge() = default;

void ViewNodesAssetTrackerCallbackBridge::onBeganRequestingLoadedAsset(RawViewNodeId viewNodeId,
                                                                       const Ref<Asset>& /* asset */) {
    _callback->call(ValueFunctionFlagsNone, {Value(kEventTypeBeganRequesting), Value(viewNodeId)});
}

void ViewNodesAssetTrackerCallbackBridge::onEndRequestingLoadedAsset(RawViewNodeId viewNodeId,
                                                                     const Ref<Asset>& /* asset */) {
    _callback->call(ValueFunctionFlagsNone, {Value(kEventTypeEndRequesting), Value(viewNodeId)});
}

void ViewNodesAssetTrackerCallbackBridge::onLoadedAssetChanged(RawViewNodeId viewNodeId,
                                                               const Ref<Asset>& /* asset */,
                                                               const std::optional<StringBox>& error) {
    _callback->call(ValueFunctionFlagsNone,
                    {Value(kEventTypeLoadedAssetChanged),
                     Value(viewNodeId),
                     error ? Value(error.value()) : Value::undefined()});
}

} // namespace Valdi
