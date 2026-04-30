//
//  ViewNodeRenderer.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 11/6/19.
//

#pragma once

#include "valdi/runtime/Attributes/Animator.hpp"
#include "valdi/runtime/Context/RawViewNodeId.hpp"
#include "valdi/runtime/Rendering/RenderRequest.hpp"

#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/FlatSet.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

namespace Valdi {

class ViewNodeTree;
class RenderRequest;
class MainThreadManager;
class AttributesManager;
class ILogger;
class ViewNode;
class IViewManager;
class ViewManagerContext;
class ViewTransactionScope;

class ViewNodeRenderer {
public:
    ViewNodeRenderer(ViewNodeTree& viewNodeTree, ILogger& logger, bool limitToViewportDisabled);

    void render(const RenderRequest& request);

private:
    ViewNodeTree& _viewNodeTree;
    ViewTransactionScope& _viewTransactionScope;
    MainThreadManager& _mainThreadManager;
    AttributesManager& _attributesManager;
    IViewManager& _viewManager;
    [[maybe_unused]] ILogger& _logger;

    Ref<ViewNode> _currentNode;
    Ref<ViewNode> _parentNode;
    bool _limitToViewportDisabled;
    bool _currentNodeAttributeChanged = false;

    bool setCurrent(RawViewNodeId id, RenderRequestEntries::ElementEntryBase& entry);
    bool setParent(RawViewNodeId id, RenderRequestEntries::ElementEntryBase& entry);

    void setCurrent(Ref<ViewNode> viewNode);
    void setParent(Ref<ViewNode> viewNode);

    void onCurrentNodeAttributeChange();
    bool handleViewNodeChanged(const Ref<ViewNode>& viewNode, RenderRequestEntries::ElementEntryBase& entry);

    void updateCSS();

    friend RenderRequest;

    void removePendingCancellableAnimation(int token);

    void visit(RenderRequestEntries::CreateElement& entry);
    void visit(RenderRequestEntries::DestroyElement& entry);
    void visit(RenderRequestEntries::MoveElementToParent& entry);
    void visit(RenderRequestEntries::SetRootElement& entry);
    void visit(RenderRequestEntries::SetElementAttribute& entry);
    void visit(RenderRequestEntries::StartAnimations& entry);
    void visit(RenderRequestEntries::EndAnimations& entry);
    void visit(RenderRequestEntries::CancelAnimation& entry);
    void visit(RenderRequestEntries::OnLayoutComplete& entry);
    void visit(RenderRequestEntries::OnNextDraw& entry);
};

} // namespace Valdi
