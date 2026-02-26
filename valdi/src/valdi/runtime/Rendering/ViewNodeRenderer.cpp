//
//  ViewNodeRenderer.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 11/6/19.
//

#include "valdi/runtime/Rendering/ViewNodeRenderer.hpp"
#include "valdi/runtime/Attributes/Animator.hpp"
#include "valdi/runtime/Attributes/AttributesApplier.hpp"
#include "valdi/runtime/CSS/CSSAttributesManager.hpp"
#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Rendering/AnimationOptions.hpp"
#include "valdi/runtime/Rendering/RenderRequest.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

namespace Valdi {

ViewNodeRenderer::ViewNodeRenderer(ViewNodeTree& viewNodeTree, ILogger& logger, bool limitToViewportDisabled)
    : _viewNodeTree(viewNodeTree),
      _viewTransactionScope(viewNodeTree.getCurrentViewTransactionScope()),
      _mainThreadManager(*viewNodeTree.getViewManagerContext()->getMainThreadManager()),
      _attributesManager(viewNodeTree.getViewManagerContext()->getAttributesManager()),
      _viewManager(viewNodeTree.getViewManagerContext()->getViewManager()),
      _logger(logger),
      _limitToViewportDisabled(limitToViewportDisabled) {}

void ViewNodeRenderer::render(const RenderRequest& request) {
    if (!request.getVisibilityObserverCallback().isNullOrUndefined()) {
        _viewNodeTree.registerViewNodesVisibilityObserverCallback(
            request.getVisibilityObserverCallback().getFunctionRef());
    }
    if (!request.getFrameObserverCallback().isNullOrUndefined()) {
        _viewNodeTree.registerViewNodesFrameObserverCallback(request.getFrameObserverCallback().getFunctionRef());
    }

    request.visitEntries(*this);

    setCurrent(nullptr);
    setParent(nullptr);

    updateCSS();
}

void ViewNodeRenderer::visit(RenderRequestEntries::CreateElement& entry) {
    if (entry.getElementId() == 0) {
        VALDI_ERROR(_logger, "Invalid element id {}", entry.getElementId());
        return;
    }

    auto viewNode =
        Valdi::makeShared<ViewNode>(_attributesManager.getYogaConfig(), _attributesManager.getAttributeIds(), _logger);
    viewNode->setViewFactory(_viewTransactionScope, _viewNodeTree.getOrCreateViewFactory(entry.getViewClassName()));
    viewNode->setRawId(entry.getElementId());

    Ref<ViewNode> smallViewNode(std::move(viewNode));
    _viewNodeTree.addViewNode(smallViewNode);

    setCurrent(std::move(smallViewNode));
}

void ViewNodeRenderer::visit(RenderRequestEntries::DestroyElement& entry) {
    _viewNodeTree.removeViewNode(entry.getElementId());
}

void ViewNodeRenderer::visit(RenderRequestEntries::MoveElementToParent& entry) {
    if (!setParent(entry.getParentElementId(), entry)) {
        return;
    }
    if (!setCurrent(entry.getElementId(), entry)) {
        return;
    }

    _parentNode->insertChildAt(_viewTransactionScope, _currentNode, static_cast<size_t>(entry.getParentIndex()));
}

void ViewNodeRenderer::visit(RenderRequestEntries::SetRootElement& entry) {
    if (!setCurrent(entry.getElementId(), entry)) {
        return;
    }
    if (_limitToViewportDisabled) {
        _currentNode->setLimitToViewport(LimitToViewportDisabled);
    }

    _viewNodeTree.setRootViewNode(_currentNode, true);
}

void ViewNodeRenderer::visit(RenderRequestEntries::SetElementAttribute& entry) {
    if (!setCurrent(entry.getElementId(), entry)) {
        return;
    }
    if (_currentNode->getViewFactory() == nullptr) {
        return;
    }

    bool changed = false;

    if (entry.isInjectedFromParent()) {
        changed = _currentNode->setAttribute(_viewTransactionScope,
                                             entry.getAttributeId(),
                                             _viewNodeTree.getParentAttributeOwner(),
                                             entry.getAttributeValue(),
                                             _viewNodeTree.getAttachedAnimator(),
                                             true);
    } else {
        changed = _currentNode->setAttribute(_viewTransactionScope,
                                             entry.getAttributeId(),
                                             &_viewNodeTree,
                                             entry.getAttributeValue(),
                                             _viewNodeTree.getAttachedAnimator(),
                                             false);
    }

    if (changed) {
        onCurrentNodeAttributeChange();
    }
}

static constexpr double kDefaultSpringStiffness = 381.47;
static constexpr double kDefaultSpringDamping = 20.1;

void ViewNodeRenderer::visit(RenderRequestEntries::StartAnimations& entry) {
    const auto& animationsOptions = entry.getAnimationOptions();
    auto stiffness = animationsOptions.stiffness;
    auto damping = animationsOptions.damping;
    auto cancelToken = animationsOptions.cancelToken;
    if (animationsOptions.duration == 0) {
        if (stiffness == 0) {
            stiffness = kDefaultSpringStiffness;
        }
        stiffness = std::max(stiffness, 0.01);
        if (damping == 0) {
            damping = kDefaultSpringDamping;
        }
        damping = std::max(damping, 0.01);
    }
    auto nativeAnimator = _viewManager.createAnimator(animationsOptions.type,
                                                      animationsOptions.controlPoints,
                                                      animationsOptions.duration,
                                                      animationsOptions.beginFromCurrentState,
                                                      animationsOptions.crossfade,
                                                      stiffness,
                                                      damping);

    auto animatorCompletionCallback = animationsOptions.completionCallback.getFunctionRef();

    if (nativeAnimator != nullptr) {
        auto animator = Valdi::makeShared<Animator>(std::move(nativeAnimator));
        animator->appendCompletion([cancelToken,
                                    viewNodeTree = weakRef(&_viewNodeTree),
                                    completion = std::move(animatorCompletionCallback)](const auto& callContext) {
            if (auto tree = viewNodeTree.lock()) {
                tree->removePendingAnimation(cancelToken);
            }
            if (completion != nullptr) {
                (*completion)(callContext);
            }
            return Value::undefined();
        });

        updateCSS();

        // we do an initial layout pass so that all the calculated frames are up to date.
        _viewNodeTree.performUpdates();

        _viewNodeTree.attachAnimator(animator, cancelToken);
    } else if (animatorCompletionCallback != nullptr) {
        _mainThreadManager.dispatch(_viewNodeTree.getContext(),
                                    [animatorCompletionCallback]() { (*animatorCompletionCallback)(); });
    }
}

void ViewNodeRenderer::visit(RenderRequestEntries::EndAnimations& /*entry*/) {
    setCurrent(nullptr);
    setParent(nullptr);
    updateCSS();
    _viewNodeTree.performUpdatesAndFlushAnimatorIfNeeded();
}

void ViewNodeRenderer::visit(RenderRequestEntries::CancelAnimation& entry) {
    const auto token = entry.getToken();
    _viewNodeTree.cancelAnimation(token);
}

void ViewNodeRenderer::visit(RenderRequestEntries::OnLayoutComplete& entry) {
    _viewNodeTree.onNextLayout(entry.getCallback());
}

void ViewNodeRenderer::onCurrentNodeAttributeChange() {
    _currentNodeAttributeChanged = true;
}

void ViewNodeRenderer::setCurrent(Ref<ViewNode> viewNode) {
    if (_currentNodeAttributeChanged) {
        _currentNodeAttributeChanged = false;
        if (!_currentNode->cssNeedsUpdate()) {
            // Only flush if we don't need to do a CSS pass, otherwise we will
            // flush after the CSS pass.
            _currentNode->getAttributesApplier().flush(_viewTransactionScope);
        }
    }

    _currentNode = std::move(viewNode);
}

void ViewNodeRenderer::updateCSS() {
    auto root = _viewNodeTree.getRootViewNode();
    if (root != nullptr) {
        root->updateCSS(_viewTransactionScope, _viewNodeTree.getAttachedAnimator());
    }
}

bool ViewNodeRenderer::setCurrent(RawViewNodeId id, RenderRequestEntries::ElementEntryBase& entry) {
    if (_currentNode == nullptr || _currentNode->getRawId() != id) {
        setCurrent(_viewNodeTree.getViewNode(id));
        return handleViewNodeChanged(_currentNode, entry);
    } else {
        return true;
    }
}

void ViewNodeRenderer::setParent(Ref<ViewNode> viewNode) {
    _parentNode = std::move(viewNode);
}

bool ViewNodeRenderer::setParent(RawViewNodeId id, RenderRequestEntries::ElementEntryBase& entry) {
    if (_parentNode == nullptr || _parentNode->getRawId() != id) {
        setParent(_viewNodeTree.getViewNode(id));
        return handleViewNodeChanged(_parentNode, entry);
    } else {
        return true;
    }
}

bool ViewNodeRenderer::handleViewNodeChanged(const Ref<ViewNode>& viewNode,
                                             RenderRequestEntries::ElementEntryBase& entry) {
    if (viewNode != nullptr) {
        return true;
    }
    VALDI_ERROR(_logger,
                "Unable to find ViewNode while processing: {}",
                RenderRequestEntries::serializeEntry(entry, _attributesManager.getAttributeIds()).toString(false));
    return false;
}

} // namespace Valdi
