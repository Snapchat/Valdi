//
//  SCValdiSnapDrawingNSView.m
//  valdi-skia-apple
//
//  Created by Simon Corsin on 6/27/20.
//

#import "SCValdiSnapDrawingNSView.h"

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>

#import "snap_drawing/cpp/Layers/LayerRoot.hpp"

#import "valdi/snap_drawing/Layers/ValdiLayer.hpp"

#import "snap_drawing/cpp/Drawing/DrawLooper.hpp"

#import "valdi_core/cpp/Utils/Mutex.hpp"
#import "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#import "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"

#import "valdi/macos/Views/SCValdiSurfacePresenterView.h"
#import <objc/runtime.h>

#import "SCValdiObjCUtils.h"
#import "SCValdiCursorUpdater.h"
#import "SCValdiMacOSSurfacePresenterManager.h"
#import <QuartzCore/CAShapeLayer.h>
#import "valdi/macos/MacOSSnapDrawingRuntime.h"

@interface SCValdiSnapDrawingNSView() <NSViewLayerContentScaleDelegate, CALayerDelegate, SCValdiCursorUpdaterDelegate, SCSnapDrawingMetalSurfacePresenterManagerDelegate, SCValdiSurfacePresenterViewDelegate>

@end

@implementation SCValdiSnapDrawingNSView {
    SCValdiRuntime *_runtime;
    NSArray<NSString *> *_arguments;
    Valdi::StringBox _componentPath;

    Valdi::Ref<snap::drawing::LayerRoot> _layerRoot;
    Valdi::Ref<snap::drawing::ValdiLayer> _valdiView;

    id _windowObserver;
    id _componentContext;
    SCValdiCursorUpdater *_cursorUpdater;
}

- (instancetype)initWithValdiRuntime:(SCValdiRuntime *)runtime arguments:(NSArray<NSString *> *)arguments componentContext:(id)componentContext
{
    NSString* desktopComponentPath = @"DesktopApp@valdi_standalone_ui/src/DesktopApp";
    return [self initWithValdiRuntime:runtime arguments:arguments componentContext: componentContext componentPath:desktopComponentPath];
}

- (instancetype)initWithValdiRuntime:(SCValdiRuntime *)runtime arguments:(NSArray<NSString *> *)arguments componentContext:(id)componentContext componentPath:(NSString*)componentPath
{
    self = [self initWithFrame:NSMakeRect(0, 0, 0, 0)];

    if (self) {
        _runtime = runtime;
        _componentContext = componentContext;
        _componentPath = StringFromNSString(componentPath);

        self.wantsLayer = YES;

        self.allowedTouchTypes = NSTouchTypeMaskDirect | NSTouchTypeMaskIndirect;
        self.translatesAutoresizingMaskIntoConstraints = NO;

        _arguments = arguments;

        auto snapDrawingRuntime = [self snapDrawingRuntime];
        auto surfacePresenterManager = Valdi::makeShared<snap::drawing::MacOSSurfacePresenterManager>(self, snapDrawingRuntime->getMetalGraphicsContext());

        _layerRoot = Valdi::makeShared<snap::drawing::LayerRoot>(snapDrawingRuntime->getResources());

        snapDrawingRuntime->getDrawLooper()->addLayerRoot(_layerRoot, surfacePresenterManager, false);

        __weak SCValdiSnapDrawingNSView *weakSelf = self;
        [_runtime waitUntilReadyWithCompletion:^{
            [weakSelf _initializeValdiView];
        }];

        _cursorUpdater = [[SCValdiCursorUpdater alloc] initWithView:self];
        _cursorUpdater.delegate = self;
    }

    return self;
}

- (void)dealloc
{
    [self snapDrawingRuntime]->getDrawLooper()->removeLayerRoot(_layerRoot);
    [self _removeWindowObservers];
}

- (snap::drawing::Ref<ValdiMacOS::MacOSSnapDrawingRuntime>)snapDrawingRuntime
{
    auto snapDrawingRuntime = (ValdiMacOS::MacOSSnapDrawingRuntime *)_runtime.snapDrawingRuntime;
    return snap::drawing::Ref<ValdiMacOS::MacOSSnapDrawingRuntime>(snapDrawingRuntime);
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)_initializeValdiView
{
    if (_valdiView != nullptr) {
        return;
    }

    auto runtime = (Valdi::Runtime *)_runtime.nativeRuntime;
    auto viewManagerContext = (Valdi::ViewManagerContext *)_runtime.nativeViewManagerContext;
    _valdiView = Valdi::makeShared<snap::drawing::ValdiLayer>(_layerRoot->getResources(), runtime, viewManagerContext);
    _valdiView->setRightToLeft(self.userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionRightToLeft);

    Valdi::Value componentContext;
    componentContext.setMapValue("setWindowSize", Valdi::Value(Valdi::makeShared<Valdi::ValueFunctionWithCallable>([self](const Valdi::ValueFunctionCallContext &callContext) -> Valdi::Value {
        auto width = callContext.getParameterAsDouble(0);
        if (!callContext.getExceptionTracker()) {
            return Valdi::Value::undefined();
        }

        auto height = callContext.getParameterAsDouble(1);
        if (!callContext.getExceptionTracker()) {
            return Valdi::Value::undefined();
        }

        auto windowPosition = callContext.getParameterAsString(2);
        if (!callContext.getExceptionTracker()) {
            return Valdi::Value::undefined();
        }

        auto callback = callContext.getParameterAsFunction(3);
        if (!callContext.getExceptionTracker()) {
            return Valdi::Value::undefined();
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            NSSize size = NSMakeSize(width, height);
            NSWindow *window = self.window;
            [window setContentSize:size];
            const char* position = windowPosition.getCStr();
            if (strcmp(position, "center") == 0) {
                NSScreen *screen = window.screen;
                // Based on lower left, even though the API says top left.
                NSPoint topLeft = NSMakePoint((screen.frame.size.width - size.width) / 2, (screen.frame.size.height + size.height) / 2);
                [window setFrameTopLeftPoint:topLeft];
            }
            [window makeKeyAndOrderFront:nil];

            (*callback)();
        });

        return Valdi::Value();
    })));
    componentContext.setMapValue("setApplicationId", Valdi::Value(Valdi::makeShared<Valdi::ValueFunctionWithCallable>([self](const Valdi::ValueFunctionCallContext &callContext) -> Valdi::Value {
        auto applicationId = callContext.getParameterAsString(0);
        if (!callContext.getExceptionTracker()) {
            return Valdi::Value::undefined();
        }

        [_runtime setApplicationId:applicationId.getCStr()];

        return Valdi::Value();
    })));
    componentContext.setMapValue("exit", Valdi::Value(Valdi::makeShared<Valdi::ValueFunctionWithCallable>([](const Valdi::ValueFunctionCallContext &callContext) -> Valdi::Value {
        dispatch_async(dispatch_get_main_queue(), ^{
            [[NSApplication sharedApplication] terminate:nil];
        });


        return Valdi::Value();
    })));

    Valdi::ValueArrayBuilder builder;
    for (NSString *str in _arguments) {
        builder.append(Valdi::Value(StringFromNSString(str)));
    }

    componentContext.setMapValue("programArguments", Valdi::Value(builder.build()));
    componentContext.setMapValue("componentContext", ValueFromNSObject(_componentContext));
    if ([_componentContext isKindOfClass:[NSDictionary class]]) {
        NSDictionary *dict = (NSDictionary *)_componentContext;
        for (NSString *key in dict) {
            id obj = [dict objectForKey:key];
            componentContext.setMapValue(StringFromNSString(key), ValueFromNSObject(obj));
        }
    }

    _valdiView->setComponent(_componentPath, Valdi::Value(), componentContext);
    _valdiView->getValdiContext()->setUserData(ValdiObjectFromNSObject(self, NO));
    _layerRoot->setContentLayer(_valdiView, snap::drawing::ContentLayerSizingModeMatchSize);
}

- (void)_updateActiveScreenFromWindow:(NSWindow *)window
{
    NSScreen *screen = window.screen;
    NSNumber *screenNumber = screen.deviceDescription[@"NSScreenNumber"];

    if (screenNumber) {
        [self snapDrawingRuntime]->setActiveDisplay((CGDirectDisplayID)[screenNumber unsignedIntValue]);
    } else {
        [self snapDrawingRuntime]->setActiveDisplay(kCGNullDirectDisplay);
    }
    if (screen) {
        [_runtime setDisplayScale:screen.backingScaleFactor];
    }
}

- (void)_removeWindowObservers
{
    if (_windowObserver) {
        [[NSNotificationCenter defaultCenter] removeObserver:_windowObserver];
        _windowObserver = nil;
    }
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
    [super viewWillMoveToWindow:newWindow];

    [self _updateActiveScreenFromWindow:newWindow];

    [self _removeWindowObservers];

    if (newWindow) {
        __weak SCValdiSnapDrawingNSView *weakSelf = self;
        _windowObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidChangeScreenNotification object:newWindow queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification * _Nonnull note) {
            SCValdiSnapDrawingNSView *strongSelf = weakSelf;
            if (!strongSelf) {
                return;
            }

            [strongSelf _updateActiveScreenFromWindow:strongSelf.window];
        }];
    }
}

- (BOOL)layer:(CALayer *)layer shouldInheritContentsScale:(CGFloat)newScale fromWindow:(NSWindow *)window
{
    return YES;
}

- (void)layout
{
    [super layout];

    CGRect bounds = self.bounds;

    _layerRoot->setSize(snap::drawing::Size::make(bounds.size.width, bounds.size.height), self.layer.contentsScale);
    _cursorUpdater.trackingAreaSize = bounds.size;

    for (NSView *subview in self.subviews) {
        subview.frame = bounds;
        [subview layoutSubtreeIfNeeded];
    }
}

- (void)surfacePresenterView:(SCValdiSurfacePresenterView *)surfaceView willResizeDrawableWithBlock:(SCValdiSurfacePresenterViewResizeBlock)block
{
    auto drawLock = [self snapDrawingRuntime]->getDrawLooper()->getDrawLock();
    block();
}

- (snap::drawing::TouchEvent)_eventForWindowLocation:(NSPoint)windowLocation eventType:(snap::drawing::TouchEventType)eventType sourceEvent:(NSEvent *)sourceEvent
{
    auto eventTime = snap::drawing::TimePoint::now();

    NSView *rootView = self;
    while (rootView.superview) {
        rootView = rootView.superview;
    }

    NSPoint resolvedLocation = [rootView convertPoint:windowLocation toView:self];

    auto point = snap::drawing::Point::make(_layerRoot->sanitizeCoordinate((snap::drawing::Scalar)resolvedLocation.x),
                                            _layerRoot->sanitizeCoordinate((snap::drawing::Scalar)resolvedLocation.y));

    auto resolvedEventTime = _layerRoot->getFrameTimeForAbsoluteFrameTime(eventTime);

    auto direction = snap::drawing::Vector::make(0, 0);
    if (eventType == snap::drawing::TouchEventTypeWheel) {
        auto inversionMultiplier = (sourceEvent.isDirectionInvertedFromDevice ? -1 : 1);
        // TODO: When !hasPreciseScrollingDeltas we should be multiplying by line/row height.
        // This is the case when using a mouse wheel rather than trackpad gestures.
        // Currently using 10 just so it's not abysmally slow to scroll using a mouse wheel.
        auto additionalScrollingMultiplier = (sourceEvent.hasPreciseScrollingDeltas ? 1 : 10);
        direction = snap::drawing::Vector::make(sourceEvent.scrollingDeltaX * inversionMultiplier * additionalScrollingMultiplier, sourceEvent.scrollingDeltaY * inversionMultiplier * additionalScrollingMultiplier);
    }

    snap::drawing::TouchEvent::PointerLocations resolvedPointerLocations;
    resolvedPointerLocations.emplace_back(point);

    return snap::drawing::TouchEvent(eventType, point, point, direction, 1, 0, std::move(resolvedPointerLocations), resolvedEventTime, snap::drawing::Duration(), ValdiObjectFromNSObject(sourceEvent, YES));
}

- (void)_submitTouchWithEvent:(NSEvent *)event eventType:(snap::drawing::TouchEventType)eventType
{
    _layerRoot->dispatchTouchEvent([self _eventForWindowLocation:event.locationInWindow eventType:eventType sourceEvent:event]);
}

// Removed the touchesXXXWithEvent methods, since we don't care about the events triggered by touches on the
// trackpad while the user is hovering their cursor over the view. We treat the mouse events as the "touch"
// events in Valdi's semantics.

- (NSView *)hitTest:(NSPoint)point
{
    NSView *view = [super hitTest:point];
    if (view == nil) {
        return nil;
    }
    // If the hit landed on a native embedded view (e.g. NSDatePicker, NSPopUpButton),
    // return it directly so it receives real NSWindow-dispatched mouse events.
    // Synthetic event forwarding via BridgeGestureRecognizer doesn't work for controls
    // that need full AppKit event loop integration.
    if (view != self) {
        return view;
    }
    return self;
}

- (void)mouseDown:(NSEvent *)event
{
    [self _submitTouchWithEvent:event eventType:snap::drawing::TouchEventTypeDown];
}

- (void)mouseUp:(NSEvent *)event
{
    [self _submitTouchWithEvent:event eventType:snap::drawing::TouchEventTypeUp];
}

- (void)mouseDragged:(NSEvent *)event
{
    [self _submitTouchWithEvent:event eventType:snap::drawing::TouchEventTypeMoved];
}

- (void)scrollWheel:(NSEvent *)event {
    if (event.deltaX != 0 || event.deltaY != 0) {
        [self _submitTouchWithEvent:event eventType:snap::drawing::TouchEventTypeWheel];
    }
}

- (SCValdiCursorType)cursorUpdater:(SCValdiCursorUpdater *)cursorUpdater cursorTypeAtPoint:(CGPoint)point
{
    auto event = [self _eventForWindowLocation:point eventType:snap::drawing::TouchEventTypeNone sourceEvent:nil];
    auto gestureTypes = _layerRoot->getGesturesTypesForTouchEvent(event);

    if (gestureTypes.hasDrag) {
        return SCValdiCursorTypeOpenHand;
    }

    if (gestureTypes.hasTap) {
        return SCValdiCursorTypeHand;
    }

    return SCValdiCursorTypePointer;
}

- (id)createEmbeddedPresenterForView:(id)view
{
    return [SCValdiSurfacePresenterView presenterViewWithEmbeddedView:view];
}

- (id)createMetalPresenter:(inout CAMetalLayer *__autoreleasing *)metalLayer
{
    CAMetalLayer *layer = [CAMetalLayer layer];
    *metalLayer = layer;

    SCValdiSurfacePresenterView *presenterView = [SCValdiSurfacePresenterView presenterViewWithMetalLayer:layer];
    presenterView.delegate = self;

    return presenterView;
}

- (void)removePresenter:(id)presenter
{
    [(NSView *)presenter removeFromSuperview];
}

- (void)setFrame:(CGRect)frame
       transform:(CATransform3D)transform
         opacity:(CGFloat)opacity
        clipPath:(CGPathRef)clipPath
  clipHasChanged:(BOOL)clipHasChanged
forEmbeddedPresenter:(id)presenter
{
    SCValdiSurfacePresenterView *presenterView = presenter;
    [presenterView setEmbeddedViewFrame:frame transform:transform opacity:opacity clipPath:clipPath clipHasChanged:clipHasChanged];
}

- (void)setZIndex:(NSUInteger)zIndex forPresenter:(id)presenter
{
    NSView *surfaceView = presenter;
    [surfaceView removeFromSuperview];

    NSArray<NSView *> *existingSubviews = self.subviews;
    if (existingSubviews.count == zIndex) {
        [self addSubview:surfaceView];
    } else {
        [self addSubview:surfaceView positioned:NSWindowBelow relativeTo:existingSubviews[zIndex]];
    }

    surfaceView.frame = self.bounds;
    [surfaceView layoutSubtreeIfNeeded];
}

@end
