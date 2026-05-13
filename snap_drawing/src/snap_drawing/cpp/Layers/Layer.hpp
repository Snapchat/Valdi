//
//  Layer.hpp
//  valdi-skia-apple
//
//  Created by Simon Corsin on 6/27/20.
//

#pragma once

#include "Interfaces/ILayer.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include "snap_drawing/cpp/Resources.hpp"

#include "snap_drawing/cpp/Layers/Interfaces/ILayer.hpp"

#include "snap_drawing/cpp/Touches/GestureRecognizer.hpp"

#include "snap_drawing/cpp/Drawing/DrawingContext.hpp"
#include "snap_drawing/cpp/Drawing/LayerContent.hpp"
#include "snap_drawing/cpp/Drawing/LinearGradient.hpp"
#include "snap_drawing/cpp/Drawing/RadialGradient.hpp"
#include "snap_drawing/cpp/Events/EventCallback.hpp"
#include "snap_drawing/cpp/Events/EventId.hpp"
#include "snap_drawing/cpp/Utils/BorderRadius.hpp"
#include "snap_drawing/cpp/Utils/GradientWrapper.hpp"
#include "snap_drawing/cpp/Utils/LazyPath.hpp"
#include "snap_drawing/cpp/Utils/Matrix.hpp"
#include "snap_drawing/cpp/Utils/SafeContainer.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "valdi_core/cpp/Utils/PlatformResult.hpp"

#include "utils/base/NonCopyable.hpp"

namespace Valdi {
class DispatchQueue;
class ValueFunction;
class ILogger;
}; // namespace Valdi

namespace snap::drawing {

class Layer;
class ILayerRoot;
class IAnimation;
class IMaskLayer;

class BoxShadow;
class LinearGradient;
class RadialGradient;
struct AttributeContext;

class DisplayList;

struct DrawMetrics {
    int drawCacheMiss = 0;
    int matrixCacheMiss = 0;
    int visitedLayers = 0;
};

template<typename T, typename std::enable_if<std::is_convertible<T*, ILayer*>::value, int>::type = 0, typename... Args>
Ref<T> makeLayer(Args&&... args) {
    auto ref = Valdi::makeShared<T>(std::forward<Args>(args)...);
    ref->onInitialize();
    return ref;
}

class Layer : public ILayer, public snap::NonCopyable {
public:
    explicit Layer(const Ref<Resources>& resources);
    ~Layer() override;

    void onInitialize() override;
    void draw(DisplayList& displayList, DrawMetrics& metrics);

    virtual Size sizeThatFits(Size maxSize);

    void setFrame(const Rect& frame);
    const Rect& getFrame() const;

    void setBackgroundColor(Color backgroundColor);
    Color getBackgroundColor() const;

    void setBackgroundLinearGradient(std::vector<Scalar>&& locations,
                                     std::vector<Color>&& colors,
                                     LinearGradientOrientation orientation);
    void setBackgroundRadialGradient(std::vector<Scalar>&& locations, std::vector<Color>&& colors);

    void setMaskImageLinearGradient(std::vector<Scalar>&& locations,
                                    std::vector<Color>&& colors,
                                    LinearGradientOrientation orientation);
    void setMaskImageRadialGradient(std::vector<Scalar>&& locations, std::vector<Color>&& colors);

    void setTouchEnabled(bool touchEnabled);
    bool isTouchEnabled() const;

    void setBorderRadius(BorderRadius borderRadius);
    BorderRadius getBorderRadius() const;

    void setOpacity(Scalar opacity);
    Scalar getOpacity() const;

    void setBorderWidth(Scalar borderWidth);
    Scalar getBorderWidth() const;

    void setBorderColor(Color borderColor);
    Color getBorderColor() const;

    void setMaskLayer(const Ref<IMaskLayer>& maskLayer);
    const Ref<IMaskLayer>& getMaskLayer() const;

    void setClipsToBounds(bool clipToBounds);
    bool clipsToBounds() const;
    bool isVisible() const;

    virtual bool getClipsToBoundsDefaultValue() const;

    void setTouchAreaExtension(Scalar left, Scalar right, Scalar top, Scalar bottom);

    void setBoxShadow(Scalar widthOffset, Scalar heightOffset, Scalar blurAmount, Color color);

    void setTranslationX(Scalar translationX);
    Scalar getTranslationX() const;

    void setTranslationY(Scalar translationY);
    Scalar getTranslationY() const;

    void setScaleX(Scalar scaleX);
    Scalar getScaleX() const;

    void setScaleY(Scalar scaleY);
    Scalar getScaleY() const;

    void setRotation(Scalar rotation);
    Scalar getRotation() const;

    void setAccessibilityId(const Valdi::StringBox& accessibilityId);
    const Valdi::StringBox& getAccessibilityId() const;

    virtual bool hitTest(const Point& point) const;
    Ref<Layer> getLayerAtPoint(const Point& point);

    void layoutIfNeeded();

    bool needsDisplay() const;
    bool childNeedsDisplay() const;
    void setNeedsDisplay();
    void setNeedsLayout();
    void requestLayout(ILayer* layer) override;
    void setChildNeedsDisplay() override;

    void requestFocus(ILayer* layer) override;

    bool needsProcessAnimations() const;

    bool isRightToLeft() const;
    void setRightToLeft(bool isRightToLeft);

    virtual bool prepareForReuse();

    std::optional<Point> convertPointToLayer(Point point, const Valdi::Ref<Layer>& childLayer) const;

    /**
     Convert the given point in the parent's coordinates to this layer's coordinates.
     */
    Point convertPointFromParent(const Point& point);

    /**
     Convert a rect from this layer's coordinates to the parent's coordinates
     */
    Rect convertRectToParent(const Rect& rect);

    /**
     Convert a point from this layer's coordinates to the parent's coordinates
     */
    Point convertPointToParent(const Point& point);

    /**
     Returns the actual frame that this layer represents, taking in account
     scaling, translation and rotation.
     */
    const Rect& getVisualFrame();

    Rect getAbsoluteVisualFrame();

    void addChild(const Valdi::Ref<Layer>& childLayer);
    void insertChild(const Valdi::Ref<Layer>& childLayer, size_t index);
    void removeFromParent();

    Valdi::Ref<Layer> getChild(size_t index) const;
    size_t getChildrenSize() const;

    Valdi::Ref<ILayer> getParent() const override;
    bool hasParent() const;

    Valdi::StringBox getDebugDescription(bool recursive) const;
    void printDebugDescription(bool recursive) const;

    void addAnimation(const String& key, const Ref<IAnimation>& animation);
    void removeAnimation(const String& key);
    void removeAllAnimations();
    bool hasAnimation(const String& key) const;
    Ref<IAnimation> getAnimation(const String& key) const;
    std::vector<String> getAnimationKeys() const;

    void addGestureRecognizer(const Valdi::Ref<GestureRecognizer>& gestureRecognizer);
    void removeGestureRecognizer(const Valdi::Ref<GestureRecognizer>& gestureRecognizer);
    size_t getGestureRecognizersSize() const;
    Valdi::Ref<GestureRecognizer> getGestureRecognizer(size_t index) const;

    void removeGestureRecognizerOfType(const std::type_info& type);
    std::optional<size_t> indexOfGestureRecognizerOfType(const std::type_info& type) const;

    const Ref<RefCountable>& getAttachedData() const;
    virtual void setAttachedData(const Ref<RefCountable>& attachedData);

    Valdi::ILogger& getLogger() const;

    ILayerRoot* getRoot() const;

    const Ref<Resources>& getResources() const;

    // Will be called automatically when inserting/removing children, this should only be called
    // explicitly by the root layer.
    void onParentChanged(const Valdi::Ref<ILayer>& parent);

protected:
    virtual void onDraw(DrawingContext& drawingContext);
    virtual void onBoundsChanged();
    virtual void onLayout();
    virtual void onRootChanged(ILayerRoot* root);
    virtual void onRightToLeftChanged();

    virtual void onChildRemoved(Layer* childLayer);
    virtual void onChildInserted(Layer* childLayer, size_t index);

    virtual std::string_view getClassName() const;

private:
    Ref<Resources> _resources;
    SafeContainer<std::vector<Valdi::Ref<Layer>>> _children;
    SafeContainer<std::vector<Valdi::Ref<GestureRecognizer>>> _gestureRecognizers;
    SafeContainer<Valdi::FlatMap<String, Ref<IAnimation>>> _animations;
    Valdi::Weak<ILayer> _parent;
    ILayerRoot* _root = nullptr;
    Ref<RefCountable> _attachedData;
    LayerId _layerId = kLayerIdNone;
    Rect _frame;
    Rect _visualFrame;
    Scalar _touchAreaExtensionLeft = 0;
    Scalar _touchAreaExtensionRight = 0;
    Scalar _touchAreaExtensionTop = 0;
    Scalar _touchAreaExtensionBottom = 0;
    Color _backgroundColor;
    Size _translation;
    Scalar _scaleX = 1;
    Scalar _scaleY = 1;
    Scalar _opacity = 1;
    Scalar _rotation = 0;
    Scalar _borderWidth = 0;
    Color _borderColor = Color::transparent();
    BorderRadius _borderRadius;
    GradientWrapper _gradientWrapper;
    GradientWrapper _maskImageGradient;
    Ref<BoxShadow> _boxShadow;
    Ref<IMaskLayer> _maskLayer;
    LayerContent _cachedBackground;
    LayerContent _cachedContent;
    LayerContent _cachedForeground;
    LazyPath _lazyPath;
    Matrix _matrix;
    bool _needsDisplay = true;
    bool _childNeedsDisplay = true;
    bool _touchEnabled = true;
    bool _clipsToBounds = false;
    bool _hasParent = false;
    bool _needsLayout = false;
    bool _hasScale = false;
    bool _isDrawing = false;
    bool _visualFrameDirty = true;
    bool _matrixDirty = true;
    bool _isRightToLeft = false;
    std::optional<EventId> _enqueuedFrame;
    Valdi::StringBox _accessibilityId;

    EventId onNextFrame(EventCallback&& eventCallback);

    Point getOffsetInParent() const;

    void removeFromParent(bool shouldNotify);
    void removeChild(Layer* childLayer, bool shouldNotify);

    void drawBackground(Scalar width, Scalar height);
    void drawContent(Scalar width, Scalar height);
    void drawForeground(Scalar width, Scalar height);

    void setVisualFrameDirty();

    void notifyParentSetChildNeedsDisplay();

    void updateMatrix(Scalar width, Scalar height);

    void scheduleProcessAnimationsIfNeeded();
    bool cancelProcessAnimations();
    void processAnimations(Duration delta);

    bool hasOverlappingRendering() const;

    void outputDebugDescription(std::string& out, int indent, bool recursive) const;
};

} // namespace snap::drawing
