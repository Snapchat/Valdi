//
//  ViewNodesVisibilityObserver.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 11/6/19.
//

#pragma once

#include "valdi/runtime/Context/RawViewNodeId.hpp"
#include "valdi_core/cpp/Views/Frame.hpp"

#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/TimePoint.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

namespace Valdi {

class MainThreadManager;
class Context;

class ViewNodesVisibilityObserver : public SharedPtrRefCountable {
public:
    explicit ViewNodesVisibilityObserver(MainThreadManager* mainThreadManager);
    ~ViewNodesVisibilityObserver() override;

    void flush(const Ref<Context>& context);

    void onViewNodeVisibilityChanged(RawViewNodeId id, bool visible);

    void onViewNodeViewportChanged(RawViewNodeId id, const Frame& viewport);

    void setCallback(const Ref<ValueFunction>& callback);

private:
    MainThreadManager* _mainThreadManager;
    Ref<ValueFunction> _callback;
    FlatMap<RawViewNodeId, bool> _visibilityUpdates;
    FlatMap<RawViewNodeId, Frame> _viewportUpdates;
    TimePoint _lastUpdateTime;
    bool _isFlushing = false;

    void onFlushAcknowledged(const Ref<Context>& context);

    void updateLastUpdateTimeIfNeeded();
};

} // namespace Valdi
