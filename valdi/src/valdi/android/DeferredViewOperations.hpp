//
//  DeferredViewOperations.hpp
//  valdi-android
//
//  Created by Simon Corsin on 7/29/21.
//

#pragma once

#include "valdi/runtime/Attributes/AttributesBindingContext.hpp"

#include "valdi/runtime/Attributes/BorderRadius.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"

#include "valdi_core/cpp/Interfaces/ILogger.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include "valdi_core/jni/JavaValue.hpp"

#include <deque>
#include <optional>
#include <vector>

namespace Valdi {
class LoadedAsset;
}

namespace ValdiAndroid {

class GlobalRefJavaObject;

enum ViewOperation : uint8_t {
    ViewOperationBeginRenderingView = 1,
    ViewOperationEndRenderingView = 2,
    ViewOperationMovedToTree = 3,
    ViewOperationMoveToParent = 4,
    ViewOperationAddedToPool = 5,
    ViewOperationSetFrame = 6,
    ViewOperationSetScrollableSpecs = 7,
    ViewOperationSetActiveAnimator = 8,
    ViewOperationSetLoadedAsset = 9,
    ViewOperationResetAttribute = 10,
    ViewOperationApplyAttributeBool = 11,
    ViewOperationApplyAttributeInt = 12,
    ViewOperationApplyAttributeLong = 13,
    ViewOperationApplyAttributeFloat = 14,
    ViewOperationApplyAttributeObject = 15,
    ViewOperationApplyAttributePercent = 16,
    ViewOperationApplyAttributeCorners = 17,
    ViewOperationOnNextDraw = 18,
};

struct SerializedViewOperations {
    Valdi::ByteBuffer* buffer = nullptr;
    Valdi::Ref<Valdi::ValueArray> attachedValues;
};

class DeferredViewOperations;

class DeferredViewOperationsPool : public Valdi::SimpleRefCountable {
public:
    DeferredViewOperationsPool();
    ~DeferredViewOperationsPool() override;

    Valdi::Ref<DeferredViewOperations> make();
    void release(Valdi::Ref<DeferredViewOperations> viewOperations);

private:
    Valdi::Mutex _mutex;
    std::deque<Valdi::Ref<DeferredViewOperations>> _pool;
};

class DeferredViewOperations : public Valdi::SimpleRefCountable {
public:
    DeferredViewOperations();
    ~DeferredViewOperations() override;

    std::optional<SerializedViewOperations> dequeueOperations();
    void clear();

    Valdi::ByteBuffer& enqueueApplyAttribute(ViewOperation operation,
                                             const Valdi::Ref<Valdi::View>& view,
                                             const Valdi::Value& handler,
                                             const Valdi::Ref<Valdi::Animator>& animator);

    void enqueueResetAttribute(const Valdi::Ref<Valdi::View>& view,
                               const Valdi::Value& handler,
                               const Valdi::Ref<Valdi::Animator>& animator);

    void enqueueMoveToParent(const Valdi::Ref<Valdi::View>& view,
                             const Valdi::Ref<Valdi::View>& parentView,
                             int32_t index);
    void enqueueRemoveFromParent(const Valdi::Ref<Valdi::View>& view, bool shouldClearViewNode);

    void enqueueSetFrame(const Valdi::Ref<Valdi::View>& view,
                         int32_t x,
                         int32_t y,
                         int32_t width,
                         int32_t height,
                         bool isRightToLeft,
                         const Valdi::Ref<Valdi::Animator>& animator);

    void enqueueSetScrollableSpecs(const Valdi::Ref<Valdi::View>& view,
                                   int32_t contentOffsetX,
                                   int32_t contentOffsetY,
                                   int32_t contentWidth,
                                   int32_t contentHeight,
                                   bool animated);

    void enqueueSetLoadedAsset(const Valdi::Ref<Valdi::View>& view,
                               const Valdi::Ref<Valdi::LoadedAsset>& loadedAsset,
                               bool shouldDrawFlipped);

    void enqueueMovedToTree(const Valdi::Ref<Valdi::View>& view, const Valdi::Value& userData, int32_t viewNodeId);

    void enqueueAddedToPool(const Valdi::Ref<Valdi::View>& view);

    void enqueueBeginRenderingView(const Valdi::Ref<Valdi::View>& view);
    void enqueueEndRenderingView(const Valdi::Ref<Valdi::View>& view, bool layoutDidBecomeDirty);
    void enqueueOnNextDraw(const Valdi::Ref<Valdi::View>& view, int64_t callbackHandle);

    void write(Valdi::ByteBuffer& buffer, const Valdi::Value& value);
    void write(Valdi::ByteBuffer& buffer, int32_t value);
    void write(Valdi::ByteBuffer& buffer, int64_t value);
    void write(Valdi::ByteBuffer& buffer, float value);

private:
    Valdi::ByteBuffer _buffer;
    Valdi::ValueArrayBuilder _attachedValues;
    Valdi::FlatMap<Valdi::Value, size_t> _indexByAttachedValue;
    Valdi::Ref<Valdi::Animator> _lastAnimator;

    template<typename T>
    void doWrite(Valdi::ByteBuffer& buffer, T value);

    void updateActiveAnimator(const Valdi::Ref<Valdi::Animator>& animator);

    Valdi::ByteBuffer& writeHeader(ViewOperation operation, bool hasValue);

    Valdi::Ref<Valdi::ByteBuffer> makeBuffer();
};

} // namespace ValdiAndroid
