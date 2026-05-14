//
//  Layer.cpp
//  valdi-skia-apple
//
//  Created by Simon Corsin on 6/27/20.
//

#include "snap_drawing/cpp/Layers/Layer.hpp"
#include "Interfaces/ILayer.hpp"
#include "snap_drawing/cpp/Layers/Interfaces/ILayerRoot.hpp"
#include "snap_drawing/cpp/Layers/Mask/IMaskLayer.hpp"

#include "utils/debugging/Assert.hpp"

#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "snap_drawing/cpp/Animations/Animation.hpp"

#include "snap_drawing/cpp/Drawing/BoxShadow.hpp"
#include "snap_drawing/cpp/Drawing/DisplayList/DisplayList.hpp"
#include "snap_drawing/cpp/Drawing/LinearGradient.hpp"
#include "snap_drawing/cpp/Drawing/Mask/PaintMask.hpp"
#include "snap_drawing/cpp/Utils/GradientWrapper.hpp"

#include "include/core/SkBlendMode.h"

#include <iostream>

namespace snap::drawing {

Layer::Layer(const Ref<Resources>& resources)
    : _resources(resources),
      _frame(Rect::makeEmpty()),
      _backgroundColor(Color::transparent()),
      _translation(Size::makeEmpty()) {}

Layer::~Layer() {
    for (const auto& gestureRecognizer : _gestureRecognizers.readAccess()) {
        gestureRecognizer->setLayer(nullptr);
    }
}

void Layer::onInitialize() {}

void Layer::draw(DisplayList& displayList, DrawMetrics& metrics) {
    if (!isVisible()) {
        return;
    }

    metrics.visitedLayers++;

    auto width = _frame.width();
    auto height = _frame.height();

    if (_matrixDirty) {
        _matrixDirty = false;
        updateMatrix(width, height);

        metrics.matrixCacheMiss++;
    }

    _isDrawing = true;

    Scalar resolvedContextOpacity;
    Scalar resolvedPictureOpacity;

    if (_opacity == 1.0f || !hasOverlappingRendering()) {
        resolvedContextOpacity = 1.0f;
        resolvedPictureOpacity = _opacity;
    } else {
        resolvedContextOpacity = _opacity;
        resolvedPictureOpacity = 1.0f;
    }

    if (_layerId == kLayerIdNone && _root != nullptr) {
        _layerId = _root->allocateLayerId();
    }

    displayList.pushContext(_matrix, resolvedContextOpacity, _layerId, _needsDisplay);

    if (_needsDisplay) {
        drawBackground(width, height);
        drawContent(width, height);
        drawForeground(width, height);
    }

    MaskLayerPositioning maskPositioning = MaskLayerPositioning::BelowBackground;
    Ref<IMask> mask;
    if (_maskLayer != nullptr) {
        maskPositioning = _maskLayer->getPositioning();
        mask = _maskLayer->createMask(Rect::makeLTRB(0, 0, width, height));
    }

    Ref<IMask> maskImage;
    if (_maskImageGradient.hasGradient()) {
        auto viewRect = Rect::makeLTRB(0, 0, width, height);
        _maskImageGradient.update(viewRect);

        Paint maskPaint;
        _maskImageGradient.applyToPaint(maskPaint);
        maskPaint.setBlendMode(SkBlendMode::kDstIn);

        maskImage = Valdi::makeShared<PaintMask>(maskPaint, Path(), viewRect);
    }

    if (maskImage != nullptr) {
        displayList.appendPrepareMask(maskImage.get());
    }

    if (mask != nullptr && maskPositioning == MaskLayerPositioning::BelowBackground) {
        displayList.appendPrepareMask(mask.get());
    }

    if (!_cachedBackground.isEmpty()) {
        displayList.appendLayerContent(_cachedBackground, resolvedPictureOpacity);
    }

    if (mask != nullptr && maskPositioning == MaskLayerPositioning::AboveBackground) {
        displayList.appendPrepareMask(mask.get());
    }

    if (!_cachedContent.isEmpty()) {
        displayList.appendLayerContent(_cachedContent, resolvedPictureOpacity);
    }

    if (_clipsToBounds) {
        displayList.appendClipRound(_borderRadius, width, height);
    }

    {
        auto children = _children.readAccess();
        for (const auto& child : *children) {
            child->draw(displayList, metrics);
        }
    }

    if (mask != nullptr) {
        displayList.appendApplyMask(mask.get());
    }

    if (!_cachedForeground.isEmpty()) {
        displayList.appendLayerContent(_cachedForeground, resolvedPictureOpacity);
    }

    if (maskImage != nullptr) {
        displayList.appendApplyMask(maskImage.get());
    }

    if (_needsDisplay) {
        _needsDisplay = false;
        metrics.drawCacheMiss++;
    }

    _childNeedsDisplay = false;
    _isDrawing = false;

    displayList.popContext();
}

void Layer::onDraw(DrawingContext& drawingContext) {}

void Layer::drawContent(Scalar width, Scalar height) {
    DrawingContext drawingContext(width, height);

    onDraw(drawingContext);

    _cachedContent = drawingContext.finish();
}

void Layer::drawBackground(Scalar width, Scalar height) {
    DrawingContext drawingContext(width, height);

    if (_boxShadow != nullptr) {
        _boxShadow->draw(drawingContext, _borderRadius);
    }

    if (_gradientWrapper.hasGradient()) {
        _gradientWrapper.draw(drawingContext, _borderRadius);
    } else if (_backgroundColor != Color::transparent()) {
        Paint paint;
        paint.setColor(_backgroundColor);
        paint.setAntiAlias(true);

        drawingContext.drawPaint(paint, _borderRadius, _lazyPath);
    }

    _cachedBackground = drawingContext.finish();
}

void Layer::drawForeground(Scalar width, Scalar height) {
    DrawingContext drawingContext(width, height);

    if (_borderWidth != 0) {
        Paint paint;
        paint.setStroke(true);
        paint.setColor(_borderColor);
        paint.setStrokeWidth(_borderWidth);
        paint.setAntiAlias(true);

        drawingContext.drawPaint(paint, _borderRadius, _lazyPath);
    }

    _cachedForeground = drawingContext.finish();
}

bool Layer::hasOverlappingRendering() const {
    return getChildrenSize() > 0;
}

Size Layer::sizeThatFits(Size /*maxSize*/) {
    return Size::make(0, 0);
}

void Layer::addChild(const Valdi::Ref<Layer>& childLayer) {
    insertChild(childLayer, getChildrenSize());
}

void Layer::insertChild(const Valdi::Ref<Layer>& childLayer, size_t index) {
    SC_ASSERT(this != childLayer.get());

    if (childLayer->_hasParent) {
        bool shouldNotify = childLayer->getParent().get() != this;
        childLayer->removeFromParent(shouldNotify);
    }

    {
        auto children = _children.writeAccess();
        SC_ASSERT(index <= children->size());
        if (index == children->size()) {
            children->emplace_back(childLayer);
        } else {
            children->emplace(children->begin() + index, childLayer);
        }
    }

    childLayer->onParentChanged(Valdi::strongSmallRef(this));

    setChildNeedsDisplay();

    onChildInserted(childLayer.get(), index);

    if (childLayer->_needsLayout) {
        setNeedsLayout();
    }
    childLayer->setNeedsDisplay();
}

void Layer::onChildInserted(Layer* /*childLayer*/, size_t /*index*/) {}

void Layer::onChildRemoved(Layer* /*childLayer*/) {}

void Layer::removeChild(Layer* childLayer, bool shouldNotify) {
    auto erased = false;
    {
        auto children = _children.writeAccess();
        auto it = children->begin();
        while (it != children->end()) {
            if (it->get() == childLayer) {
                it = children->erase(it);
                erased = true;
            } else {
                it++;
            }
        }
    }

    if (erased) {
        if (shouldNotify) {
            onChildRemoved(childLayer);
        }
        setChildNeedsDisplay();
    }
}

void Layer::removeFromParent() {
    removeFromParent(true);
}

void Layer::removeFromParent(bool shouldNotify) {
    if (!_hasParent) {
        return;
    }

    auto parentLayer = Valdi::castOrNull<Layer>(getParent());
    if (parentLayer != nullptr) {
        parentLayer->removeChild(this, shouldNotify);
    }

    onParentChanged(nullptr);
}

Ref<Layer> Layer::getChild(size_t index) const {
    return (*_children.readAccess())[index];
}

size_t Layer::getChildrenSize() const {
    return _children.size();
}

bool Layer::hitTest(const Point& point) const {
    if (!isTouchEnabled() || !isVisible()) {
        return false;
    }

    if (point.x < -_touchAreaExtensionLeft) {
        return false;
    }

    if (point.y < -_touchAreaExtensionTop) {
        return false;
    }

    if (point.x > (_frame.width() + _touchAreaExtensionRight)) {
        return false;
    }

    if (point.y > (_frame.height() + _touchAreaExtensionBottom)) {
        return false;
    }

    return true;
}

Ref<Layer> Layer::getLayerAtPoint(const Point& point) {
    if (!hitTest(point)) {
        return nullptr;
    }

    // Dispatch touches to children, starting from the last child.
    auto children = _children.readAccess();
    auto i = children->size();
    while (i > 0) {
        i--;

        const auto& child = (*children)[i];
        auto childPoint = child->convertPointFromParent(point);

        auto hitChild = child->getLayerAtPoint(childPoint);
        if (hitChild != nullptr) {
            return hitChild;
        }
    }

    return Valdi::strongSmallRef(this);
}

void Layer::updateMatrix(Scalar width, Scalar height) {
    _matrix.setIdentity();

    auto scaledWidth = width;
    auto scaledHeight = height;
    auto translationX = _translation.width;
    auto translationY = _translation.height;

    // Compensate for the change of size so that it appears as if we are scaling from the center
    if (_scaleX != 1.0f) {
        scaledWidth *= _scaleX;

        translationX += (width - scaledWidth) / 2.0f;
        _matrix.setScaleX(_scaleX);
    }

    if (_scaleY != 1.0f) {
        scaledHeight *= _scaleY;

        translationY += (height - scaledHeight) / 2.0f;
        _matrix.setScaleY(_scaleY);
    }

    _matrix.setTranslateX(_frame.left + translationX);
    _matrix.setTranslateY(_frame.top + translationY);

    if (_rotation != 0.0f) {
        Scalar centerX = _matrix.getTranslateX() + scaledWidth / 2.0f;
        Scalar centerY = _matrix.getTranslateY() + scaledHeight / 2.0f;
        _matrix.postRotate(_rotation, centerX, centerY);
    }
}

void Layer::setTouchAreaExtension(Scalar left, Scalar right, Scalar top, Scalar bottom) {
    _touchAreaExtensionLeft = left;
    _touchAreaExtensionRight = right;
    _touchAreaExtensionTop = top;
    _touchAreaExtensionBottom = bottom;
}

void Layer::setBackgroundColor(Color backgroundColor) {
    if (_backgroundColor != backgroundColor) {
        _backgroundColor = backgroundColor;
        setNeedsDisplay();
    }
}

void Layer::setBackgroundLinearGradient(std::vector<Scalar>&& locations,
                                        std::vector<Color>&& colors,
                                        LinearGradientOrientation orientation) {
    if (_gradientWrapper.clearIfNeeded(GradientWrapper::GradientType::RADIAL)) {
        setNeedsDisplay();
    }

    if (colors.empty()) {
        if (_gradientWrapper.clearIfNeeded(GradientWrapper::GradientType::LINEAR)) {
            setNeedsDisplay();
        }
        return;
    }

    _gradientWrapper.setAsLinear(std::move(locations), std::move(colors), orientation);

    if (_gradientWrapper.isDirty()) {
        setNeedsDisplay();
    }
}

void Layer::setBackgroundRadialGradient(std::vector<Scalar>&& locations, std::vector<Color>&& colors) {
    if (_gradientWrapper.clearIfNeeded(GradientWrapper::GradientType::LINEAR)) {
        setNeedsDisplay();
    }

    if (colors.empty()) {
        if (_gradientWrapper.clearIfNeeded(GradientWrapper::GradientType::RADIAL)) {
            setNeedsDisplay();
        }
        return;
    }

    _gradientWrapper.setAsRadial(std::move(locations), std::move(colors));

    if (_gradientWrapper.isDirty()) {
        setNeedsDisplay();
    }
}

void Layer::setMaskImageLinearGradient(std::vector<Scalar>&& locations,
                                       std::vector<Color>&& colors,
                                       LinearGradientOrientation orientation) {
    if (_maskImageGradient.clearIfNeeded(GradientWrapper::GradientType::RADIAL)) {
        setNeedsDisplay();
    }

    if (colors.empty()) {
        if (_maskImageGradient.clearIfNeeded(GradientWrapper::GradientType::LINEAR)) {
            setNeedsDisplay();
        }
        return;
    }

    _maskImageGradient.setAsLinear(std::move(locations), std::move(colors), orientation);

    if (_maskImageGradient.isDirty()) {
        setNeedsDisplay();
    }
}

void Layer::setMaskImageRadialGradient(std::vector<Scalar>&& locations, std::vector<Color>&& colors) {
    if (_maskImageGradient.clearIfNeeded(GradientWrapper::GradientType::LINEAR)) {
        setNeedsDisplay();
    }

    if (colors.empty()) {
        if (_maskImageGradient.clearIfNeeded(GradientWrapper::GradientType::RADIAL)) {
            setNeedsDisplay();
        }
        return;
    }

    _maskImageGradient.setAsRadial(std::move(locations), std::move(colors));

    if (_maskImageGradient.isDirty()) {
        setNeedsDisplay();
    }
}

Color Layer::getBackgroundColor() const {
    return _backgroundColor;
}

void Layer::setTouchEnabled(bool touchEnabled) {
    _touchEnabled = touchEnabled;
}

bool Layer::isTouchEnabled() const {
    return _touchEnabled;
}

void Layer::setBorderRadius(BorderRadius borderRadius) {
    _borderRadius = borderRadius;
    // Path needs to be recomputed when the border radius changes
    _lazyPath.setNeedsUpdate();
    setNeedsDisplay();
}

BorderRadius Layer::getBorderRadius() const {
    return _borderRadius;
}

void Layer::setOpacity(Scalar opacity) {
    if (_opacity != opacity) {
        auto wasVisible = isVisible();
        _opacity = opacity;

        if (isVisible() != wasVisible) {
            // If we are switching from visible to non-visible or vice versa,
            // we need to redraw completely as the visibility state is used
            // to determine whether we should even emit the draw operations
            // for the layer or not
            setNeedsDisplay();
            // We also should force notify our parent since our needsDisplay/childNeedsDisplay
            // flags might be out of sync, since we are not visited when not visible.
            notifyParentSetChildNeedsDisplay();
        } else {
            setChildNeedsDisplay();
        }
    }
}

Scalar Layer::getOpacity() const {
    return _opacity;
}

bool Layer::isVisible() const {
    return _opacity > 0;
}

void Layer::setBorderWidth(Scalar borderWidth) {
    if (_borderWidth != borderWidth) {
        _borderWidth = borderWidth;
        setNeedsDisplay();
    }
}

Scalar Layer::getBorderWidth() const {
    return _borderWidth;
}

void Layer::setBorderColor(Color borderColor) {
    if (_borderColor != borderColor) {
        _borderColor = borderColor;
        setNeedsDisplay();
    }
}

Color Layer::getBorderColor() const {
    return _borderColor;
}

void Layer::setMaskLayer(const Ref<IMaskLayer>& maskLayer) {
    if (_maskLayer != maskLayer) {
        _maskLayer = maskLayer;
        setNeedsDisplay();
    }
}

const Ref<IMaskLayer>& Layer::getMaskLayer() const {
    return _maskLayer;
}

void Layer::setClipsToBounds(bool clipToBounds) {
    if (_clipsToBounds != clipToBounds) {
        _clipsToBounds = clipToBounds;
        setNeedsDisplay();
    }
}

bool Layer::clipsToBounds() const {
    return _clipsToBounds;
}

void Layer::onBoundsChanged() {}

void Layer::onLayout() {}

void Layer::onRightToLeftChanged() {}

void Layer::setFrame(const Rect& frame) {
    if (_frame != frame) {
        auto boundsChanged = (_frame.width() != frame.width()) || (_frame.height() != frame.height());
        _frame = frame;

        setVisualFrameDirty();

        if (boundsChanged) {
            setNeedsDisplay();
            onBoundsChanged();
        } else {
            setChildNeedsDisplay();
        }
    }
}

const Rect& Layer::getFrame() const {
    return _frame;
}

void Layer::setNeedsDisplay() {
    if (_isDrawing) {
        return;
    }

    if (!_needsDisplay) {
        _needsDisplay = true;

        _cachedForeground.clear();
        _cachedContent.clear();

        setChildNeedsDisplay();
    }
}

bool Layer::needsDisplay() const {
    return _needsDisplay;
}

bool Layer::childNeedsDisplay() const {
    return _childNeedsDisplay;
}

void Layer::setChildNeedsDisplay() {
    if (!_childNeedsDisplay) {
        _childNeedsDisplay = true;

        notifyParentSetChildNeedsDisplay();
    }
}

void Layer::requestFocus(ILayer* layer) {
    auto parent = getParent();
    if (parent != nullptr) {
        parent->requestFocus(layer);
    }
}

void Layer::notifyParentSetChildNeedsDisplay() {
    auto parent = _parent.lock();
    if (parent != nullptr) {
        parent->setChildNeedsDisplay();
    }
}

void Layer::setNeedsLayout() {
    requestLayout(this);
}

void Layer::requestLayout(ILayer* /*layer*/) {
    if (_needsLayout) {
        return;
    }
    _needsLayout = true;

    auto parent = _parent.lock();
    if (parent != nullptr) {
        parent->requestLayout(this);
    }
}

void Layer::layoutIfNeeded() {
    if (_needsLayout) {
        onLayout();

        for (const auto& child : _children.readAccess()) {
            child->layoutIfNeeded();
        }

        _needsLayout = false;
    }
}

void Layer::addAnimation(const String& key, const Ref<IAnimation>& animation) {
    removeAnimation(key);
    {
        (*_animations.writeAccess())[key] = animation;
    }
    scheduleProcessAnimationsIfNeeded();
}

void Layer::removeAnimation(const String& key) {
    Ref<IAnimation> animation;
    {
        auto animations = _animations.writeAccess();
        const auto& it = animations->find(key);
        if (it != animations->end()) {
            animation = std::move(it->second);
            animations->erase(it);
        }
    }

    if (animation != nullptr) {
        animation->cancel(*this);
    }
}

void Layer::removeAllAnimations() {
    while (!_animations.empty()) {
        String key;
        {
            key = _animations.readAccess()->begin()->first;
        }
        removeAnimation(key);
    }
}

bool Layer::hasAnimation(const String& key) const {
    auto animations = _animations.readAccess();
    return animations->find(key) != animations->end();
}

Ref<IAnimation> Layer::getAnimation(const String& key) const {
    auto animations = _animations.readAccess();
    auto it = animations->find(key);
    if (it == animations->end()) {
        return nullptr;
    }

    return it->second;
}

std::vector<String> Layer::getAnimationKeys() const {
    std::vector<String> out;

    auto animations = _animations.readAccess();
    out.reserve(animations->size());

    for (const auto& it : animations) {
        out.emplace_back(it.first);
    }

    return out;
}

bool Layer::needsProcessAnimations() const {
    return _enqueuedFrame.has_value();
}

void Layer::scheduleProcessAnimationsIfNeeded() {
    if (_enqueuedFrame || _animations.empty() || _root == nullptr) {
        return;
    }

    auto weakSelf = Valdi::weakRef(this);
    auto eventId = onNextFrame([weakSelf](auto /*timePoint*/, auto delta) {
        auto strongSelf = weakSelf.lock();
        if (strongSelf != nullptr) {
            strongSelf->processAnimations(delta);
        }
    });

    _enqueuedFrame = {eventId};
}

bool Layer::cancelProcessAnimations() {
    if (!_enqueuedFrame || _root == nullptr) {
        return false;
    }
    auto eventId = _enqueuedFrame.value();
    _enqueuedFrame = std::nullopt;
    return _root->cancelEvent(eventId);
}

EventId Layer::onNextFrame(EventCallback&& eventCallback) {
    if (_root == nullptr) {
        return EventId();
    }

    return _root->enqueueEvent(std::move(eventCallback), Duration(0));
}

struct AnimationToProcess {
    String key;
    Ref<IAnimation> animation;

    inline AnimationToProcess(const String& key, const Ref<IAnimation>& animation) : key(key), animation(animation) {}
};

void Layer::processAnimations(Duration delta) {
    _enqueuedFrame = std::nullopt;

    if (_animations.empty()) {
        return;
    }

    Valdi::SmallVector<AnimationToProcess, 4> collectedAnimations;

    // Collect all the animations to process
    {
        auto animations = _animations.readAccess();
        for (const auto& it : animations) {
            collectedAnimations.emplace_back(it.first, it.second);
        }
    }

    // Process all our animations with the delta.
    // We remove the ones that have completed
    for (const auto& animationToProcess : collectedAnimations) {
        auto completed = animationToProcess.animation->run(*this, delta);
        if (completed) {
            {
                auto animations = _animations.writeAccess();
                const auto& it = animations->find(animationToProcess.key);
                if (it != animations->end() && it->second == animationToProcess.animation) {
                    animations->erase(it);
                }
            }
            animationToProcess.animation->complete(*this);
        }
    }

    scheduleProcessAnimationsIfNeeded();
}

std::optional<Point> Layer::convertPointToLayer(Point point, const Valdi::Ref<Layer>& childLayer) const {
    thread_local std::vector<Ref<Layer>> kDescendants;

    auto& descendants = kDescendants;
    auto current = childLayer;

    for (;;) {
        if (current.get() == this) {
            break;
        }

        if (current == nullptr) {
            return std::nullopt;
        }

        descendants.emplace_back(current);
        current = Valdi::castOrNull<Layer>(current->getParent());
    }

    for (size_t i = descendants.size(); i > 0;) {
        i--;
        point = descendants[i]->convertPointFromParent(point);
    }

    descendants.clear();

    return point;
}

Point Layer::convertPointFromParent(const Point& point) {
    // TODO(simon): Take in account rotation

    if (_hasScale) {
        auto offset = getOffsetInParent();

        auto convertedPoint = point.makeOffset(-offset.x, -offset.y);

        if (_scaleX != 0.0f) {
            convertedPoint.x /= _scaleX;
        } else {
            convertedPoint.x = 0.0f;
        }

        if (_scaleY != 0.0f) {
            convertedPoint.y /= _scaleY;
        } else {
            convertedPoint.y = 0.0f;
        }

        return convertedPoint;
    } else {
        return point.makeOffset(-(_frame.left + _translation.width), -(_frame.top + _translation.height));
    }
}

const Rect& Layer::getVisualFrame() {
    if (_visualFrameDirty) {
        _visualFrameDirty = false;
        _visualFrame = convertRectToParent(Rect::makeLTRB(0, 0, _frame.width(), _frame.height()));
    }

    return _visualFrame;
}

Point Layer::getOffsetInParent() const {
    auto frameWidth = _frame.width();
    auto frameHeight = _frame.height();

    auto scaledWidth = frameWidth * _scaleX;
    auto scaledHeight = frameHeight * _scaleY;

    auto x = (_frame.left + frameWidth / 2.0f) + _translation.width - scaledWidth / 2.0f;
    auto y = (_frame.top + frameHeight / 2.0f) + _translation.height - scaledHeight / 2.0f;

    return Point::make(x, y);
}

Rect Layer::convertRectToParent(const Rect& rect) {
    // TODO(simon): Take in account rotation

    if (_hasScale) {
        auto convertedRect =
            Rect::makeXYWH(rect.left * _scaleX, rect.top * _scaleY, rect.width() * _scaleX, rect.height() * _scaleY);

        auto offset = getOffsetInParent();
        return convertedRect.makeOffset(offset.x, offset.y);
    } else {
        return rect.makeOffset(_frame.left + _translation.width, _frame.top + _translation.height);
    }
}

Point Layer::convertPointToParent(const Point& point) {
    // TODO(simon): Take in account rotation

    if (_hasScale) {
        auto convertedPoint = Point::make(point.x * _scaleX, point.y * _scaleY);

        auto offset = getOffsetInParent();
        return convertedPoint.makeOffset(offset.x, offset.y);
    } else {
        return point.makeOffset(_frame.left + _translation.width, _frame.top + _translation.height);
    }
}

void Layer::setVisualFrameDirty() {
    _visualFrameDirty = true;
    _matrixDirty = true;
}

Rect Layer::getAbsoluteVisualFrame() {
    auto frame = Rect::makeXYWH(0, 0, _frame.width(), _frame.height());
    auto current = Valdi::strongSmallRef(this);

    for (;;) {
        if (current == nullptr) {
            return frame;
        }

        frame = current->convertRectToParent(frame);

        current = Valdi::castOrNull<Layer>(current->getParent());
    }
}

void Layer::setBoxShadow(Scalar widthOffset, Scalar heightOffset, Scalar blurAmount, Color color) {
    if (color == Color::transparent()) {
        if (_boxShadow != nullptr) {
            _boxShadow = nullptr;
            setNeedsDisplay();
        }
    } else {
        if (_boxShadow == nullptr) {
            _boxShadow = Valdi::makeShared<BoxShadow>();
        }
        _boxShadow->setOffset(Size::make(widthOffset, heightOffset));
        _boxShadow->setBlurAmount(blurAmount);
        _boxShadow->setColor(color);
    }
}

Scalar Layer::getTranslationX() const {
    return _translation.width;
}

void Layer::setTranslationX(Scalar translationX) {
    if (_translation.width != translationX) {
        _translation.width = translationX;
        setChildNeedsDisplay();
        setVisualFrameDirty();
    }
}

Scalar Layer::getTranslationY() const {
    return _translation.height;
}

void Layer::setTranslationY(Scalar translationY) {
    if (_translation.height != translationY) {
        _translation.height = translationY;
        setChildNeedsDisplay();
        setVisualFrameDirty();
    }
}

Scalar Layer::getScaleX() const {
    return _scaleX;
}

void Layer::setScaleX(Scalar scaleX) {
    if (_scaleX != scaleX) {
        _scaleX = scaleX;
        _hasScale = (_scaleX != 1) || (_scaleY != 1);
        setChildNeedsDisplay();
        setVisualFrameDirty();
    }
}

Scalar Layer::getScaleY() const {
    return _scaleY;
}

void Layer::setScaleY(Scalar scaleY) {
    if (_scaleY != scaleY) {
        _scaleY = scaleY;
        _hasScale = (_scaleX != 1) || (_scaleY != 1);
        setChildNeedsDisplay();
        setVisualFrameDirty();
    }
}

Scalar Layer::getRotation() const {
    return _rotation;
}

void Layer::setRotation(Scalar rotation) {
    if (_rotation != rotation) {
        _rotation = rotation;
        setChildNeedsDisplay();
        setVisualFrameDirty();
    }
}

void Layer::onParentChanged(const Valdi::Ref<ILayer>& parent) {
    _parent = parent;
    _hasParent = parent != nullptr;

    if (_hasParent) {
        auto parentAsLayer = Valdi::castOrNull<Layer>(parent);
        if (parentAsLayer != nullptr) {
            if (parentAsLayer->_root != _root) {
                onRootChanged(parentAsLayer->_root);
            }
        } else {
            auto parentAsRoot = Valdi::castOrNull<ILayerRoot>(parent);
            if (parentAsRoot.get() != _root) {
                onRootChanged(parentAsRoot.get());
            }
        }
    } else {
        if (_root != nullptr) {
            onRootChanged(nullptr);
        }
    }
}

Valdi::Ref<ILayer> Layer::getParent() const {
    return _parent.lock();
}

bool Layer::hasParent() const {
    return _hasParent;
}

void Layer::onRootChanged(ILayerRoot* root) {
    if (root == nullptr) {
        cancelProcessAnimations();
    }

    _root = root;
    _layerId = kLayerIdNone;
    {
        for (const auto& child : _children.readAccess()) {
            child->onRootChanged(root);
        }
    }

    if (_root != nullptr) {
        scheduleProcessAnimationsIfNeeded();
    } else {
        removeAllAnimations();
        setNeedsDisplay();
    }
}

void Layer::addGestureRecognizer(const Valdi::Ref<GestureRecognizer>& gestureRecognizer) {
    auto layer = Valdi::castOrNull<Layer>(gestureRecognizer->getLayer());
    if (layer != nullptr) {
        layer->removeGestureRecognizer(gestureRecognizer);
    }

    _gestureRecognizers.writeAccess()->emplace_back(gestureRecognizer);
    gestureRecognizer->setLayer(this);
}

void Layer::removeGestureRecognizer(const Valdi::Ref<GestureRecognizer>& gestureRecognizer) {
    auto gestureRecognizers = _gestureRecognizers.writeAccess();
    auto it = gestureRecognizers->begin();
    while (it != gestureRecognizers->end()) {
        if (*it == gestureRecognizer) {
            gestureRecognizers->erase(it);
            gestureRecognizer->setLayer(nullptr);
            return;
        }
        it++;
    }
}

std::optional<size_t> Layer::indexOfGestureRecognizerOfType(const std::type_info& type) const {
    size_t i = 0;

    for (const auto& gestureRecognizer : _gestureRecognizers.readAccess()) {
        const auto* ptr = gestureRecognizer.get();
        if (typeid(*ptr) == type) {
            return {i};
        }

        i++;
    }

    return std::nullopt;
}

void Layer::removeGestureRecognizerOfType(const std::type_info& type) {
    auto index = indexOfGestureRecognizerOfType(type);
    if (index) {
        auto gestureRecognizers = _gestureRecognizers.writeAccess();
        (*gestureRecognizers)[index.value()]->setLayer(nullptr);
        gestureRecognizers->erase(gestureRecognizers->begin() + index.value());
    }
}

size_t Layer::getGestureRecognizersSize() const {
    return _gestureRecognizers.size();
}

Ref<GestureRecognizer> Layer::getGestureRecognizer(size_t index) const {
    return (*_gestureRecognizers.readAccess())[index];
}

const Ref<Valdi::RefCountable>& Layer::getAttachedData() const {
    return _attachedData;
}

void Layer::setAttachedData(const Ref<Valdi::RefCountable>& attachedData) {
    _attachedData = attachedData;
}

bool Layer::getClipsToBoundsDefaultValue() const {
    return false;
}

Valdi::StringBox Layer::getDebugDescription(bool recursive) const {
    std::string out;

    outputDebugDescription(out, 0, recursive);

    return Valdi::StringCache::getGlobal().makeString(std::move(out));
}

void Layer::printDebugDescription(bool recursive) const {
    auto debugDescription = getDebugDescription(recursive);
    std::cout << debugDescription.toStringView() << std::endl;
}

std::string_view Layer::getClassName() const {
    return "Layer";
}

void Layer::outputDebugDescription(std::string& out, int indent, bool recursive) const {
    for (int i = 0; i < indent; i++) {
        out += "  ";
    }

    out += getClassName();
    out += " (ID: " + getAccessibilityId().slowToString() + ")";
    out += " ";
    out += "x:" + std::to_string(_frame.x());
    out += ", y:" + std::to_string(_frame.y());
    out += ", w:" + std::to_string(_frame.width());
    out += ", h:" + std::to_string(_frame.height());

    if (recursive) {
        for (const auto& child : _children.readAccess()) {
            out += "\n";
            child->outputDebugDescription(out, indent + 1, recursive);
        }
    }
}

bool Layer::prepareForReuse() {
    return true;
}

ILayerRoot* Layer::getRoot() const {
    return _root;
}

Valdi::ILogger& Layer::getLogger() const {
    return _resources->getLogger();
}

const Ref<Resources>& Layer::getResources() const {
    return _resources;
}

void Layer::setRightToLeft(bool isRightToLeft) {
    if (_isRightToLeft != isRightToLeft) {
        _isRightToLeft = isRightToLeft;
        onRightToLeftChanged();
    }
}

bool Layer::isRightToLeft() const {
    return _isRightToLeft;
}

void Layer::setAccessibilityId(const Valdi::StringBox& accessibilityId) {
    if (_accessibilityId != accessibilityId) {
        _accessibilityId = accessibilityId;
    }
}

const Valdi::StringBox& Layer::getAccessibilityId() const {
    return _accessibilityId;
}

} // namespace snap::drawing
