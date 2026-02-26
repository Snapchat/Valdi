//
//  SCValdiMacOSViewManager.m
//  valdi-macos
//
//  Created by Simon Corsin on 10/13/20.
//

#import "valdi/macos/SCValdiMacOSViewManager.h"
#import "valdi/macos/SCValdiSnapDrawingNSView.h"
#import "valdi/macos/Views/SCValdiMacOSTextField.h"
#import "valdi/macos/Views/SCValdiSurfacePresenterView.h"
#import "valdi_core/cpp/Utils/StringCache.hpp"
#import "valdi/runtime/Context/ViewNodeTree.hpp"
#import "valdi/runtime/Views/PlaceholderViewMeasureDelegate.hpp"
#import "valdi_core/cpp/Interfaces/IBitmap.hpp"
#import "valdi/runtime/Attributes/BoundAttributes.hpp"
#import "valdi/runtime/Attributes/AttributesBindingContext.hpp"
#import "SCValdiObjCUtils.h"
#import "SCValdiMacOSAttributesBinder.h"
#import "snap_drawing/cpp/Touches/TouchEvent.hpp"
#import "valdi/runtime/Utils/MainThreadManager.hpp"
#import "valdi/runtime/Views/DeferredViewTransaction.hpp"
#import "valdi_core/cpp/Utils/TrackedLock.hpp"
#import "snap_drawing/cpp/Utils/BitmapFactory.hpp"

namespace ValdiMacOS {

class NSViewWrapper: public Valdi::View {
public:
    NSViewWrapper(NSView *view): _view(view) {}

    ~NSViewWrapper() override = default;

    NSView *getView() const {
        return _view;
    }

    snap::valdi_core::Platform getPlatform() const override {
        return snap::valdi_core::Platform::Ios;
    }

    bool onTouchEvent(long offsetSinceSourceMs, int touchEventType, float absoluteX, float absoluteY, float x, float y, float dx, float dy, bool isTouchOwner, const Valdi::Ref<Valdi::RefCountable> &source) override {
        NSEvent *event = NSObjectFromRefCountable(source);
        if (!event) {
            return false;
        }

        NSEventType eventType;
        switch (static_cast<snap::drawing::TouchEventType>(touchEventType)) {
            case snap::drawing::TouchEventTypeDown:
                [[fallthrough]];
            case snap::drawing::TouchEventTypePointerDown:
                eventType = NSEventTypeLeftMouseDown;
                break;
            case snap::drawing::TouchEventTypeIdle:
                [[fallthrough]];
            case snap::drawing::TouchEventTypeMoved:
                eventType = NSEventTypeLeftMouseDragged;
                break;
            case snap::drawing::TouchEventTypeUp:
                [[fallthrough]];
            case snap::drawing::TouchEventTypePointerUp:
                eventType = NSEventTypeLeftMouseUp;
                break;
            case snap::drawing::TouchEventTypeWheel:
                eventType = NSEventTypeScrollWheel;
                break;
            case snap::drawing::TouchEventTypeNone:
                return false;
        }

        NSView *nsView = getView();
        NSView *parentView = nsView.superview;

        NSPoint locationInWindow = event.locationInWindow;
        NSPoint localPoint = locationInWindow;
        if (parentView) {
            localPoint = [parentView convertPoint:locationInWindow fromView:nil];
        }

        NSView *hitView = [nsView hitTest:localPoint];

        if (!hitView) {
            return false;
        }

        NSTimeInterval timestamp = event.timestamp + static_cast<NSTimeInterval>(offsetSinceSourceMs * 1000);

        NSEvent *convertedEvent = nil;
        switch (static_cast<snap::drawing::TouchEventType>(touchEventType)) {
            case snap::drawing::TouchEventTypeDown:
                [[fallthrough]];
            case snap::drawing::TouchEventTypePointerDown:
                [[fallthrough]];
            case snap::drawing::TouchEventTypeIdle:
                [[fallthrough]];
            case snap::drawing::TouchEventTypeMoved:
                [[fallthrough]];
            case snap::drawing::TouchEventTypeUp:
                [[fallthrough]];
            case snap::drawing::TouchEventTypePointerUp:
                convertedEvent = [NSEvent mouseEventWithType:eventType
                                                    location:locationInWindow
                                               modifierFlags:0
                                                   timestamp:timestamp
                                                windowNumber:event.windowNumber
                                                     context:nil
                                                 eventNumber:event.eventNumber
                                                  clickCount:event.clickCount
                                                    pressure:event.pressure];
                break;
            case snap::drawing::TouchEventTypeWheel: {
                CGEventRef cgEvent = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitPixel, 1, 0);
                CGEventSetDoubleValueField(cgEvent, kCGScrollWheelEventDeltaAxis1, dx);
                CGEventSetDoubleValueField(cgEvent, kCGScrollWheelEventDeltaAxis2, dy);
                CGEventSetTimestamp(cgEvent, timestamp * NSEC_PER_SEC);
                convertedEvent = [NSEvent eventWithCGEvent:cgEvent];
                CFRelease(cgEvent);
            }
                break;
            case snap::drawing::TouchEventTypeNone:
                return false;
        }

        switch (static_cast<snap::drawing::TouchEventType>(touchEventType)) {
            case snap::drawing::TouchEventTypeDown:
                [[fallthrough]];
            case snap::drawing::TouchEventTypePointerDown:
                [hitView mouseDown:convertedEvent];
                break;
            case snap::drawing::TouchEventTypeIdle:
                [[fallthrough]];
            case snap::drawing::TouchEventTypeMoved:
                [hitView mouseDragged:convertedEvent];
                break;
            case snap::drawing::TouchEventTypeUp:
                [[fallthrough]];
            case snap::drawing::TouchEventTypePointerUp:
                [hitView mouseUp:convertedEvent];
                break;
            case snap::drawing::TouchEventTypeWheel:
                [hitView scrollWheel:convertedEvent];
                break;
            case snap::drawing::TouchEventTypeNone:
                return false;
        }

        return true;
    }

    VALDI_CLASS_HEADER_IMPL(NSViewWrapper)

    static void executeSyncInMainThread(const Valdi::Function<void()> &cb) {
        if (NSThread.isMainThread) {
            cb();
        } else {
            Valdi::DropAllTrackedLocks dropAllTrackedLocks;
            dispatch_sync(dispatch_get_main_queue(), ^{
                cb();
            });
        }
    }

private:
    NSView *_view;
};

class MacOSViewTransaction : public Valdi::IViewTransaction {
public:
    MacOSViewTransaction() = default;
    ~MacOSViewTransaction() override = default;

    void flush(bool sync) override {}

    void willUpdateRootView(const Valdi::Ref<Valdi::View>& view) override {}

    void didUpdateRootView(const Valdi::Ref<Valdi::View>& view, bool layoutDidBecomeDirty) override {}

    void moveViewToTree(const Valdi::Ref<Valdi::View>& view,
                        Valdi::ViewNodeTree* viewNodeTree,
                        Valdi::ViewNode* viewNode) override {}

    void insertChildView(const Valdi::Ref<Valdi::View>& view,
                         const Valdi::Ref<Valdi::View>& childView,
                         int index,
                         const Valdi::Ref<Valdi::Animator>& animator) override {}

    void removeViewFromParent(const Valdi::Ref<Valdi::View>& view,
                              const Valdi::Ref<Valdi::Animator>& animator,
                              bool shouldClearViewNode) override {}

    void invalidateViewLayout(const Valdi::Ref<Valdi::View>& view) override {}

    void setViewFrame(const Valdi::Ref<Valdi::View>& view,
                      const Valdi::Frame& newFrame,
                      bool isRightToLeft,
                      const Valdi::Ref<Valdi::Animator>& animator) override {}

    void setViewScrollSpecs(const Valdi::Ref<Valdi::View>& view,
                            const Valdi::Point& directionDependentContentOffset,
                            const Valdi::Size& contentSize,
                            bool animated) override {}

    void setViewLoadedAsset(const Valdi::Ref<Valdi::View>& view,
                            const Valdi::Ref<Valdi::LoadedAsset>& loadedAsset,
                            bool shouldDrawFlipped) override {}

    void layoutView(const Valdi::Ref<Valdi::View>& view) override {}

    void cancelAllViewAnimations(const Valdi::Ref<Valdi::View>& view) override {}

    void willEnqueueViewToPool(const Valdi::Ref<Valdi::View>& view,
                               Valdi::Function<void(Valdi::View&)> onEnqueue) override {}

    void snapshotView(const Valdi::Ref<Valdi::View>& view,
                      Valdi::Function<void(Valdi::Result<Valdi::BytesView>)> cb) override {
        cb(Valdi::BytesView());
    }

    void flushAnimator(const Valdi::Ref<Valdi::Animator>& animator,
                       const Valdi::Value& completionCallback) override {}

    void cancelAnimator(const Valdi::Ref<Valdi::Animator>& animator) override {}

    void executeInTransactionThread(Valdi::DispatchFunction executeFn) override {
        executeFn();
    }
};


Valdi::Ref<Valdi::View> toValdiView(NSView *view) {
    if (!view) {
        return nullptr;
    }
    return Valdi::makeShared<NSViewWrapper>(view);
}

NSView *fromValdiView(const Valdi::Ref<Valdi::View> &view) {
    auto wrapper = Valdi::castOrNull<NSViewWrapper>(view);
    if (wrapper == nullptr) {
        return nil;
    }
    return wrapper->getView();
}

class MacOSViewFactory: public Valdi::ViewFactory {
public:
    MacOSViewFactory(Valdi::StringBox viewClassName, Valdi::IViewManager& viewManager, Valdi::Ref<Valdi::BoundAttributes> boundAttributes)
        : Valdi::ViewFactory(std::move(viewClassName), viewManager, std::move(boundAttributes)) {}

    ~MacOSViewFactory() override = default;

    static Valdi::Ref<Valdi::View> makeViewFromClass(Class cls) {
        NSView *view = [[cls alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)];
        view.translatesAutoresizingMaskIntoConstraints = NO;
        return toValdiView(view);
    }

protected:
    Class lookupClass() {
        if (_cachedClass != nullptr) {
            return _cachedClass;
        }

        const auto& className = getViewClassName();

        NSString *viewClassName = NSStringFromString(className);
        Class cls = NSClassFromString(viewClassName);
        if (!cls) {
            return nil;
        }
        _cachedClass = cls;
        return cls;
    }

    Valdi::Ref<Valdi::View> doCreateView(Valdi::ViewNodeTree* viewNodeTree, Valdi::ViewNode* viewNode) override {
        Class cls = lookupClass();
        if (!cls) {
            return nullptr;
        }
        return MacOSViewFactory::makeViewFromClass(cls);
    }

private:
    Class _cachedClass = nullptr;
};

class MacOSMeasureDelegate: public Valdi::PlaceholderViewMeasureDelegate {
public:
    MacOSMeasureDelegate(Class cls): _cls(cls) {}

    ~MacOSMeasureDelegate() override = default;

    Valdi::Size measureView(const Valdi::Ref<Valdi::View> &view,
                               float width,
                               Valdi::MeasureMode widthMode,
                               float height,
                               Valdi::MeasureMode heightMode) final {
        Valdi::Size size;
        auto wrapper = Valdi::castOrNull<NSViewWrapper>(view);
        if (wrapper == nullptr) {
            return size;
        }
        NSViewWrapper::executeSyncInMainThread([&]() {
            NSView *nsView = wrapper->getView();
            NSSize fittingSize = [nsView fittingSize];
            size = Valdi::Size(fittingSize.width, fittingSize.height);
        });
        return size;
    }

    Valdi::Ref<Valdi::View> createPlaceholderView() final {
        Valdi::Ref<Valdi::View> out;
        NSViewWrapper::executeSyncInMainThread([&]() {
            out = MacOSViewFactory::makeViewFromClass(_cls);
        });
        return out;
    }

private:
    Class _cls;
};

STRING_CONST(textFieldClassName, "SCValdiTextField");

static Valdi::StringBox resolveClassName(const Valdi::StringBox& valdiClassName) {
    if (valdiClassName == textFieldClassName()) {
        return STRING_LITERAL("SCValdiMacOSTextField");
    }
    return Valdi::StringBox::emptyString();
}

// Returns the class name to use for native lookup: mapped name if any, otherwise the requested name.
// Enables <custom-view> with iosClass to work on MacOS for any NSView subclass linked in the app.
static Valdi::StringBox getEffectiveClassName(const Valdi::StringBox& className) {
    auto resolved = resolveClassName(className);
    return resolved.isEmpty() ? className : resolved;
}

ViewManager::ViewManager() = default;
ViewManager::~ViewManager() = default;

Valdi::Ref<Valdi::ViewFactory> ViewManager::createViewFactory(const Valdi::StringBox& className, const Valdi::Ref<Valdi::BoundAttributes>& boundAttributes) {
    auto effectiveClassName = getEffectiveClassName(className);
    if (effectiveClassName.isEmpty()) {
        return nullptr;
    }
    NSString *nsClassName = NSStringFromString(effectiveClassName);
    Class cls = nsClassName ? NSClassFromString(nsClassName) : nil;
    if (!cls) {
        return nullptr;
    }
    return Valdi::makeShared<MacOSViewFactory>(effectiveClassName, *this, boundAttributes);
}

void ViewManager::callAction(Valdi::ViewNodeTree* viewNodeTree,
                             const Valdi::StringBox& actionName,
                             const Valdi::Ref<Valdi::ValueArray>& parameters) {

}

Valdi::PlatformType ViewManager::getPlatformType() const {
    return Valdi::PlatformTypeMacOS;
}

Valdi::RenderingBackendType ViewManager::getRenderingBackendType() const {
    return Valdi::RenderingBackendTypeNative;
}

Valdi::StringBox ViewManager::getDefaultViewClass() {
    return Valdi::StringBox();
}

Valdi::NativeAnimator ViewManager::createAnimator(snap::valdi_core::AnimationType type,
                                                     const std::vector<double>& controlPoints,
                                                     double duration,
                                                     bool beginFromCurrentState,
                                                     bool crossfade,
                                                     double stiffness,
                                                     double damping) {
    return nullptr;
}

std::vector<Valdi::StringBox> ViewManager::getClassHierarchy(const Valdi::StringBox& className) {
    std::vector<Valdi::StringBox> output;
    output.emplace_back(className);
    return output;
}

void ViewManager::bindAttributes(const Valdi::StringBox& className,
                                 Valdi::AttributesBindingContext& binder) {
    auto effectiveClassName = getEffectiveClassName(className);
    NSString *viewClassName = NSStringFromString(effectiveClassName);
    Class cls = viewClassName ? NSClassFromString(viewClassName) : nil;
    if (!cls) {
        return;
    }

    binder.setMeasureDelegate(Valdi::makeShared<MacOSMeasureDelegate>(cls));
    SCValdiMacOSAttributesBinder *attributesBinder = [[SCValdiMacOSAttributesBinder alloc] initWithCppInstance:(void *)&binder cls:cls];

    if ([cls respondsToSelector:@selector(bindAttributes:)]) {
        [cls bindAttributes:attributesBinder];
    }
}

bool ViewManager::supportsClassNameNatively(const Valdi::StringBox& className) {
    auto effectiveClassName = getEffectiveClassName(className);
    if (effectiveClassName.isEmpty()) {
        return false;
    }
    NSString *nsClassName = NSStringFromString(effectiveClassName);
    return nsClassName && NSClassFromString(nsClassName) != nil;
}

Valdi::Value ViewManager::createViewNodeWrapper(const Valdi::Ref<Valdi::ViewNode>& viewNode, bool wrapInPlatformReference) {
    return Valdi::Value();
}

Valdi::Ref<Valdi::IViewTransaction> ViewManager::createViewTransaction(
    const Valdi::Ref<Valdi::MainThreadManager>& mainThreadManager, bool shouldDefer) {
    if (!shouldDefer || mainThreadManager->currentThreadIsMainThread()) {
        static auto *kInstance = new MacOSViewTransaction();
        return Valdi::Ref(kInstance);
    } else {
        return Valdi::makeShared<Valdi::DeferredViewTransaction>(*this, *mainThreadManager);
    }
}

Valdi::Ref<Valdi::IBitmapFactory> ViewManager::getViewRasterBitmapFactory() const {
    return snap::drawing::BitmapFactory::getInstance(Valdi::ColorType::ColorTypeBGRA8888);
}

} // namespace ValdiMacOS
