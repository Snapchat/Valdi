//
//  ViewNodesFrameObserver.hpp
//  valdi-pc
//
//  Created by Simon Corsin on 4/22/20.
//

#pragma once

#include "valdi/runtime/Context/RawViewNodeId.hpp"
#include "valdi_core/cpp/Views/Frame.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

namespace Valdi {

class ByteBuffer;

class ViewNodesFrameObserver : public SharedPtrRefCountable {
public:
    ViewNodesFrameObserver();
    ~ViewNodesFrameObserver() override;

    void flush();

    void onFrameChanged(RawViewNodeId id, const Frame& frame);
    void appendCompleteCallback(const Ref<ValueFunction>& callback);

    void setCallback(const Ref<ValueFunction>& callback);

private:
    Ref<ValueFunction> _callback;
    Ref<ByteBuffer> _values;
    std::vector<Ref<ValueFunction>> _layoutCompletedCallbacks;

    void flushCompleteCallbacks();
};
} // namespace Valdi
