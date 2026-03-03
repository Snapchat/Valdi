//
//  AssetLoaderRequestHandler.cpp
//  valdi
//
//  Created by Simon Corsin on 8/19/21.
//

#include "valdi/runtime/Resources/AssetLoaderRequestHandler.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Context/ContextEntry.hpp"
#include "valdi/runtime/Resources/AssetLoader.hpp"
#include "valdi/runtime/Resources/AssetRequestPayloadCache.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"

#include "valdi_core/Cancelable.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

namespace Valdi {

AssetLoaderRequestHandler::AssetLoaderRequestHandler(Weak<AssetLoaderRequestHandlerListener> listener,
                                                     const Ref<Context>& context,
                                                     const AssetKey& assetKey,
                                                     const Ref<AssetRequestPayloadCache>& payloadCache,
                                                     const StringBox& url,
                                                     int32_t requestedWidth,
                                                     int32_t requestedHeight,
                                                     const Value& associatedData)
    : _listener(std::move(listener)),
      _context(context),
      _assetKey(assetKey),
      _payloadCache(payloadCache),
      _url(url),
      _requestedWidth(requestedWidth),
      _requestedHeight(requestedHeight),
      _associatedData(associatedData) {}

AssetLoaderRequestHandler::~AssetLoaderRequestHandler() = default;

void AssetLoaderRequestHandler::onLoadComplete(const Result<Ref<LoadedAsset>>& result) {
    auto listener = _listener.lock();
    if (listener != nullptr) {
        listener->onLoad(strongSmallRef(this), result);
    }
}

const Ref<Context>& AssetLoaderRequestHandler::getContext() const {
    return _context;
}

void AssetLoaderRequestHandler::startLoadIfNeeded() {
    if (_loadStarted) {
        return;
    }
    _loadStarted = true;

    ContextEntry entry(_context);

    auto requestPayload = _payloadCache->getRequestPayload();

    if (requestPayload.empty()) {
        VALDI_TRACE("Valdi.requestPayloadFromURL")
        requestPayload = _payloadCache->getAssetLoader()->requestPayloadFromURL(_url);
        _payloadCache->setRequestPayload(requestPayload);
    }

    if (!requestPayload) {
        onLoadComplete(requestPayload.error());
        return;
    }

    VALDI_TRACE("Valdi.loadAsset")
    _cancelable = _payloadCache->getAssetLoader()->loadAsset(
        requestPayload.value(), _requestedWidth, _requestedHeight, _associatedData, Valdi::strongSmallRef(this));
}

void AssetLoaderRequestHandler::cancel() {
    auto cancelable = std::move(_cancelable);
    if (cancelable != nullptr) {
        VALDI_TRACE("Valdi.cancelLoadAsset")
        cancelable->cancel();
    }
}

bool AssetLoaderRequestHandler::scheduledForCancelation() const {
    return _scheduledForCancelation;
}

void AssetLoaderRequestHandler::setScheduledForCancelation() {
    _scheduledForCancelation = true;
}

bool AssetLoaderRequestHandler::scheduledForLoad() const {
    return _scheduledForLoad;
}

void AssetLoaderRequestHandler::setScheduledForLoad() {
    _scheduledForLoad = true;
}

size_t AssetLoaderRequestHandler::decrementConsumersCount() {
    SC_ASSERT(_consumersCount > 0);
    return --_consumersCount;
}

void AssetLoaderRequestHandler::incrementConsumersCount() {
    _consumersCount++;
}

int32_t AssetLoaderRequestHandler::getRequestedWidth() const {
    return _requestedWidth;
}

int32_t AssetLoaderRequestHandler::getRequestedHeight() const {
    return _requestedHeight;
}

const Value& AssetLoaderRequestHandler::getAttachedData() const {
    return _associatedData;
}

const AssetKey& AssetLoaderRequestHandler::getAssetKey() const {
    return _assetKey;
}

const StringBox& AssetLoaderRequestHandler::getUrl() const {
    return _url;
}

const Result<Ref<LoadedAsset>>& AssetLoaderRequestHandler::getLastLoadResult() const {
    return _lastLoadResult;
}

void AssetLoaderRequestHandler::setLastLoadResult(const Result<Ref<LoadedAsset>>& lastLoadResult) {
    _lastLoadResult = lastLoadResult;
}

} // namespace Valdi
