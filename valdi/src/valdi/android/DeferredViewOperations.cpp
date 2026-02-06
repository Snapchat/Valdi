//
//  DeferredViewOperations.cpp
//  valdi-android
//
//  Created by Simon Corsin on 7/21/21.
//

#include "valdi/android/DeferredViewOperations.hpp"

#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/jni/GlobalRefJavaObject.hpp"

#include "valdi_core/NativeAnimator.hpp"

#include "valdi/runtime/Attributes/Animator.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"

#include "valdi/runtime/Views/View.hpp"

namespace ValdiAndroid {

DeferredViewOperationsPool::DeferredViewOperationsPool() = default;
DeferredViewOperationsPool::~DeferredViewOperationsPool() = default;

Valdi::Ref<DeferredViewOperations> DeferredViewOperationsPool::make() {
    std::lock_guard<Valdi::Mutex> guard(_mutex);
    if (_pool.empty()) {
        return Valdi::makeShared<DeferredViewOperations>();
    }

    auto viewOperations = std::move(_pool.front());
    _pool.pop_front();

    return viewOperations;
}

void DeferredViewOperationsPool::release(Valdi::Ref<DeferredViewOperations> viewOperations) {
    viewOperations->clear();

    std::lock_guard<Valdi::Mutex> guard(_mutex);
    _pool.emplace_back(std::move(viewOperations));
}

DeferredViewOperations::DeferredViewOperations() = default;

DeferredViewOperations::~DeferredViewOperations() = default;

Valdi::ByteBuffer& DeferredViewOperations::enqueueApplyAttribute(ViewOperation operation,
                                                                 const Valdi::Ref<Valdi::View>& view,
                                                                 const Valdi::Value& handler,
                                                                 const Valdi::Ref<Valdi::Animator>& animator) {
    updateActiveAnimator(animator);

    auto& buffer = writeHeader(operation, true);
    write(buffer, handler);
    write(buffer, Valdi::Value(view));
    return buffer;
}

void DeferredViewOperations::enqueueResetAttribute(const Valdi::Ref<Valdi::View>& view,
                                                   const Valdi::Value& handler,
                                                   const Valdi::Ref<Valdi::Animator>& animator) {
    if (view == nullptr) {
        // Skip enqueueing the reset operation since the View no longer exists
        return;
    }

    updateActiveAnimator(animator);

    auto& buffer = writeHeader(ViewOperationResetAttribute, false);
    write(buffer, handler);
    write(buffer, Valdi::Value(view));
}

void DeferredViewOperations::updateActiveAnimator(const Valdi::Ref<Valdi::Animator>& animator) {
    if (_lastAnimator != animator) {
        _lastAnimator = animator;

        if (animator == nullptr) {
            writeHeader(ViewOperationSetActiveAnimator, false);
        } else {
            auto ref = djinni_generated_client::valdi_core::NativeAnimator::fromCpp(JavaEnv::getUnsafeEnv(),
                                                                                    animator->getNativeAnimator());
            auto globalRef = Valdi::makeShared<GlobalRefJavaObject>(JavaEnv(), ref, "Animator", false);

            auto& buffer = writeHeader(ViewOperationSetActiveAnimator, true);
            write(buffer, Valdi::Value(globalRef));
        }
    }
}

void DeferredViewOperations::enqueueMoveToParent(const Valdi::Ref<Valdi::View>& view,
                                                 const Valdi::Ref<Valdi::View>& parentView,
                                                 int32_t index) {
    auto& buffer = writeHeader(ViewOperationMoveToParent, true);
    write(buffer, Valdi::Value(view));
    write(buffer, Valdi::Value(parentView));
    write(buffer, index);
}

void DeferredViewOperations::enqueueRemoveFromParent(const Valdi::Ref<Valdi::View>& view, bool shouldClearViewNode) {
    auto& buffer = writeHeader(ViewOperationMoveToParent, false);
    write(buffer, Valdi::Value(view));
    write(buffer, static_cast<int32_t>(shouldClearViewNode));
}

void DeferredViewOperations::enqueueSetFrame(const Valdi::Ref<Valdi::View>& view,
                                             int32_t x,
                                             int32_t y,
                                             int32_t width,
                                             int32_t height,
                                             bool isRightToLeft,
                                             const Valdi::Ref<Valdi::Animator>& animator) {
    updateActiveAnimator(animator);

    auto& buffer = writeHeader(ViewOperationSetFrame, isRightToLeft);
    write(buffer, Valdi::Value(view));
    write(buffer, x);
    write(buffer, y);
    write(buffer, width);
    write(buffer, height);
}

void DeferredViewOperations::enqueueSetScrollableSpecs(const Valdi::Ref<Valdi::View>& view,
                                                       int32_t contentOffsetX,
                                                       int32_t contentOffsetY,
                                                       int32_t contentWidth,
                                                       int32_t contentHeight,
                                                       bool animated) {
    auto& buffer = writeHeader(ViewOperationSetScrollableSpecs, true);
    write(buffer, Valdi::Value(view));
    write(buffer, contentOffsetX);
    write(buffer, contentOffsetY);
    write(buffer, contentWidth);
    write(buffer, contentHeight);
    write(buffer, static_cast<int32_t>(animated));
}

void DeferredViewOperations::enqueueSetLoadedAsset(const Valdi::Ref<Valdi::View>& view,
                                                   const Valdi::Ref<Valdi::LoadedAsset>& loadedAsset,
                                                   bool shouldDrawFlipped) {
    auto& buffer = writeHeader(ViewOperationSetLoadedAsset, loadedAsset != nullptr);
    write(buffer, Valdi::Value(view));
    write(buffer, static_cast<int32_t>(shouldDrawFlipped));
    if (loadedAsset != nullptr) {
        write(buffer, Valdi::Value(loadedAsset));
    }
}

void DeferredViewOperations::enqueueMovedToTree(const Valdi::Ref<Valdi::View>& view,
                                                const Valdi::Value& userData,
                                                int32_t viewNodeId) {
    auto& buffer = writeHeader(ViewOperationMovedToTree, true);
    write(buffer, Valdi::Value(view));
    write(buffer, userData);
    write(buffer, viewNodeId);
}

void DeferredViewOperations::enqueueAddedToPool(const Valdi::Ref<Valdi::View>& view) {
    auto& buffer = writeHeader(ViewOperationAddedToPool, false);
    write(buffer, Valdi::Value(view));
}

void DeferredViewOperations::enqueueBeginRenderingView(const Valdi::Ref<Valdi::View>& view) {
    auto& buffer = writeHeader(ViewOperationBeginRenderingView, false);
    write(buffer, Valdi::Value(view));
}

void DeferredViewOperations::enqueueEndRenderingView(const Valdi::Ref<Valdi::View>& view, bool layoutDidBecomeDirty) {
    auto& buffer = writeHeader(ViewOperationEndRenderingView, layoutDidBecomeDirty);
    write(buffer, Valdi::Value(view));
}

std::optional<SerializedViewOperations> DeferredViewOperations::dequeueOperations() {
    if (_buffer.empty()) {
        return std::nullopt;
    }

    SerializedViewOperations operations;
    operations.buffer = &_buffer;
    operations.attachedValues = _attachedValues.build();

    return {std::move(operations)};
}

void DeferredViewOperations::clear() {
    _lastAnimator = nullptr;
    _buffer.clear();
    _attachedValues.clear();
    _indexByAttachedValue.clear();
}

template<typename T>
void DeferredViewOperations::doWrite(Valdi::ByteBuffer& buffer, T value) {
    auto* ptr = buffer.appendWritable(sizeof(T));
    std::memcpy(ptr, &value, sizeof(T));
}

void DeferredViewOperations::write(Valdi::ByteBuffer& buffer, int32_t value) {
    doWrite(buffer, value);
}

void DeferredViewOperations::write(Valdi::ByteBuffer& buffer, int64_t value) {
    doWrite(buffer, value);
}

void DeferredViewOperations::write(Valdi::ByteBuffer& buffer, float value) {
    doWrite(buffer, value);
}

void DeferredViewOperations::write(Valdi::ByteBuffer& buffer, const Valdi::Value& value) {
    const auto& it = _indexByAttachedValue.find(value);
    if (it != _indexByAttachedValue.end()) {
        write(buffer, static_cast<int32_t>(it->second));
        return;
    }

    auto index = _attachedValues.size();
    _attachedValues.append(value);
    _indexByAttachedValue[value] = index;
    write(buffer, static_cast<int32_t>(index));
}

Valdi::ByteBuffer& DeferredViewOperations::writeHeader(ViewOperation operation, bool hasValue) {
    auto& buffer = _buffer;

    int32_t header = operation;
    if (hasValue) {
        header |= 1 << 8;
    }

    write(buffer, header);
    return buffer;
}

} // namespace ValdiAndroid
