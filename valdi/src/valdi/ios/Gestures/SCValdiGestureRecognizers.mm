//
//  SCValdiGestureRecognizers.m
//  Valdi
//
//  Created by Simon Corsin on 7/12/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#import "valdi/ios/Gestures/SCValdiGestureRecognizers.h"

#import "valdi_core/UIView+ValdiObjects.h"
#import "valdi_core/SCValdiTouches.h"
#import "valdi_core/SCValdiMarshaller+CPP.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiLogger.h"

#import <UIKit/UIGestureRecognizerSubclass.h>

static BOOL SCValdiCallTouchPredicate(UIGestureRecognizer *gestureRecognizer, id<SCValdiFunction> predicate)
{
    if (!predicate) {
        return YES;
    }

    auto tapEvent = SCValdiMakeTouchEvent(gestureRecognizer.view,
                                             [gestureRecognizer locationInView:gestureRecognizer.view],
                                             gestureRecognizer.state,
                                             SCValdiGetPointerDataFromGestureRecognizer(gestureRecognizer));
    return SCValdiCallActionWithEvent(predicate, tapEvent, Valdi::ValueFunctionFlagsCallSync);
}

static void SCValdiHandleTouchEvent(UIView *view,
                                       UIEvent *uiEvent,
                                       SCValdiGestureType gestureType,
                                       UIGestureRecognizerState state,
                                       CGPoint gestureLocation,
                                       id<SCValdiFunction> action)
{
    if (!action) {
        return;
    }

    if (gestureType == SCValdiGestureTypeTap) {
        SCLogValdiInfo(@"[Valdi] tap dispatching to JS");
    }

    auto pointerLocations = SCValdiGetPointerDataFromEvent(uiEvent);
    auto tapEvent = SCValdiMakeTouchEvent(view, gestureLocation, state, pointerLocations);
    if (!tapEvent.isNull()) {
        SCValdiForwardTouchAndCallAction(view, uiEvent, gestureType, state, action, tapEvent, SCValdiGetCallFlags(state));
    }
}

@implementation SCValdiTapGestureRecognizer {
    UIEvent *_lastEvent;
    id<SCValdiFunction> _function;
    id<SCValdiFunction> _predicate;
}

- (instancetype)init
{
    self = [super initWithTarget:self action:@selector(_handleGestureRecognizer:)];
    if (self) {
        self.delegate = self;
    }
    return self;
}

- (void)reset
{
    _lastEvent = nil;
    [super reset];
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesBegan:touches withEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesEnded:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesCancelled:touches withEvent:event];
}

- (void)setFunction:(id<SCValdiFunction>)function
{
    _function = function;
}

- (void)setPredicate:(id<SCValdiFunction>)predicate
{
    _predicate = predicate;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    return SCValdiCallTouchPredicate(self, _predicate);
}

- (void)_handleGestureRecognizer:(id)sender
{
    CGPoint location = [self locationInView:self.view];

    [self triggerAtLocation:location forState:self.state];
}

- (void)triggerAtLocation:(CGPoint)location forState:(UIGestureRecognizerState)state
{
    if (state != UIGestureRecognizerStateEnded) {
        return;
    }

    SCValdiHandleTouchEvent(self.view, _lastEvent, SCValdiGestureTypeTap, state, location, _function);
}

- (BOOL)shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    // If we have a long gesture with a tap gesture, we want the tap to only trigger if the long press fails.
    if ([otherGestureRecognizer isKindOfClass:[SCValdiLongPressGestureRecognizer class]]) {
        return YES;
    }
    // If we have a double tap gesture with a tap gesture, we want the tap to only trigger if the double-tap fails.
    if ([otherGestureRecognizer isKindOfClass:[SCValdiFastDoubleTapGestureRecognizer class]]) {
        return YES;
    }
    // onTap within attributed text should take priority
    if ([otherGestureRecognizer isKindOfClass:[SCValdiAttributedTextOnTapGestureRecognizer class]]) {
        return YES;
    }

    return [super shouldRequireFailureOfGestureRecognizer:otherGestureRecognizer];
}

@end

@implementation SCValdiAttributedTextOnTapGestureRecognizer

- (void)reset
{
    [self setFunction:nil];
    [super reset];
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    CGPoint location = [gestureRecognizer locationInView:self.view];

    id<SCValdiFunction> function = [self.functionProvider onTapFunctionAtLocation:location];
    [self setFunction:function];
    return function != nil;
}

@end

@implementation SCValdiFastDoubleTapGestureRecognizer {
    UIEvent *_lastEvent;
    id<SCValdiFunction> _function;
    id<SCValdiFunction> _predicate;
    NSTimer *_timer;
}

- (instancetype)init
{
    self = [super initWithTarget:self action:@selector(_handleGestureRecognizer:)];
    if (self) {
        self.numberOfTapsRequired = 2;
        self.delegate = self;
    }
    return self;
}

- (void)dealloc
{
    [_timer invalidate];
    _timer = nil;
}

- (void)reset
{
    _lastEvent = nil;
    [super reset];
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesBegan:touches withEvent:event];

    UITouch *touch = [touches allObjects].firstObject;
    if (touch.tapCount >= self.numberOfTapsRequired) {
        [_timer invalidate];
        _timer = nil;
    }
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesEnded:touches withEvent:event];

    UITouch *touch = [touches allObjects].firstObject;
    if (touch.tapCount < self.numberOfTapsRequired) {
        [_timer invalidate];
        _timer = [NSTimer scheduledTimerWithTimeInterval:0.15
                                                  target:self
                                                selector:@selector(_setGestureRecognizerStateCancelled)
                                                userInfo:nil
                                                 repeats:NO];
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesCancelled:touches withEvent:event];
}

- (void)setFunction:(id<SCValdiFunction>)function
{
    _function = function;
}

- (void)setPredicate:(id<SCValdiFunction>)predicate
{
    _predicate = predicate;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    return SCValdiCallTouchPredicate(self, _predicate);
}

- (void)_handleGestureRecognizer:(id)sender
{
    CGPoint location = [self locationInView:self.view];

    [self triggerAtLocation:location forState:self.state];
}

- (void)triggerAtLocation:(CGPoint)location forState:(UIGestureRecognizerState)state
{
    if (state != UIGestureRecognizerStateEnded) {
        return;
    }

    SCValdiHandleTouchEvent(self.view, _lastEvent, SCValdiGestureTypeDoubleTap, state, location, _function);
}

- (void)_setGestureRecognizerStateCancelled
{
    self.state = UIGestureRecognizerStateCancelled;
}

@end

const NSTimeInterval kSCValdiMinLongPressDuration = 0.25;

@implementation SCValdiLongPressGestureRecognizer {
    UIEvent *_lastEvent;
    id<SCValdiFunction> _function;
    id<SCValdiFunction> _predicate;
}

- (instancetype)init
{
    self = [super initWithTarget:self action:@selector(_handleGestureRecognizer:)];
    if (self) {
        self.minimumPressDuration = kSCValdiMinLongPressDuration;
        self.delegate = self;
    }
    return self;
}

- (void)reset
{
    _lastEvent = nil;
    [super reset];
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesBegan:touches withEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesEnded:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesCancelled:touches withEvent:event];
}

- (void)setFunction:(id<SCValdiFunction>)function
{
    _function = function;
}

- (void)setPredicate:(id<SCValdiFunction>)predicate
{
    _predicate = predicate;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    return SCValdiCallTouchPredicate(self, _predicate);
}

- (void)_handleGestureRecognizer:(id)sender
{
    CGPoint location = [self locationInView:self.view];

    [self triggerAtLocation:location forState:self.state];
}

- (void)triggerAtLocation:(CGPoint)location forState:(UIGestureRecognizerState)state
{
    if (state != UIGestureRecognizerStateBegan) {
        return;
    }

    SCValdiHandleTouchEvent(self.view, _lastEvent, SCValdiGestureTypeLongPress, state, location, _function);
}

@end

@implementation SCValdiDragGestureRecognizer {
    UIEvent *_lastEvent;
    id<SCValdiFunction> _function;
    id<SCValdiFunction> _predicate;
}

- (instancetype)init
{
    self = [super initWithTarget:self action:@selector(_handleGestureRecognizer:)];
    if (self) {
        self.delegate = self;
    }
    return self;
}

- (void)reset
{
    _lastEvent = nil;
    [super reset];
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesBegan:touches withEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesEnded:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesCancelled:touches withEvent:event];
}

- (void)setFunction:(id<SCValdiFunction>)function
{
    _function = function;
}

- (void)setPredicate:(id<SCValdiFunction>)predicate
{
    _predicate = predicate;
}

Valdi::Value SCValdiMakeDragEvent(UIPanGestureRecognizer *gestureRecognizer) {
    auto state = SCValdiMakeTouchState(gestureRecognizer.state);

    UIView *view = gestureRecognizer.view;

    SCValdiGestureLocation location = SCValdiGetGestureLocation(view, [gestureRecognizer locationInView:view]);
    CGPoint delta = [gestureRecognizer translationInView:view];
    CGPoint velocity = [gestureRecognizer velocityInView:view];

    velocity.x = [view.valdiViewNode resolveDeltaX:velocity.x directionAgnostic:YES];
    delta.x = [view.valdiViewNode resolveDeltaX:delta.x directionAgnostic:YES];

    auto pointerLocations = SCValdiGetPointerDataFromGestureRecognizer(gestureRecognizer);
    return Valdi::TouchEvents::makeDragEvent(state, location.relative.x, location.relative.y, location.absolute.x, location.absolute.y,
                                                         delta.x, delta.y, velocity.x, velocity.y, static_cast<int>(pointerLocations.size()), pointerLocations);
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    if (!_predicate) {
        return YES;
    }

    auto event = SCValdiMakeDragEvent(self);
    return SCValdiCallActionWithEvent(_predicate, event, Valdi::ValueFunctionFlagsCallSync);
}

- (void)_handleGestureRecognizer:(id)sender
{
    auto event = SCValdiMakeDragEvent(self);

    UIGestureRecognizerState state = self.state;
    SCValdiForwardTouchAndCallAction(self.view, _lastEvent, SCValdiGestureTypeDrag, state, _function, event, SCValdiGetCallFlags(state));
}

- (BOOL)shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if ([otherGestureRecognizer isKindOfClass:[SCValdiTouchGestureRecognizer class]]) {
        return NO;
    }

    return [super shouldRequireFailureOfGestureRecognizer:otherGestureRecognizer];
}

- (BOOL)shouldBeRequiredToFailByGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if ([otherGestureRecognizer isKindOfClass:[SCValdiTouchGestureRecognizer class]]) {
        return NO;
    }

    return [super shouldBeRequiredToFailByGestureRecognizer:otherGestureRecognizer];
}

@end

@interface SCValdiPinchGestureRecognizer () <UIGestureRecognizerDelegate>

@end

@implementation SCValdiPinchGestureRecognizer {
    UIEvent *_lastEvent;
    id<SCValdiFunction> _function;
    id<SCValdiFunction> _predicate;
}

- (instancetype)init
{
    self = [super initWithTarget:self action:@selector(_handleGestureRecognizer:)];
    if (self) {
        self.delegate = self;
    }
    return self;
}

- (void)reset
{
    _lastEvent = nil;
    [super reset];
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesBegan:touches withEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesEnded:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesCancelled:touches withEvent:event];
}


- (void)setFunction:(id<SCValdiFunction>)function
{
    _function = function;
}

- (void)setPredicate:(id<SCValdiFunction>)predicate
{
    _predicate = predicate;
}

Valdi::Value SCValdiMakePinchEvent(UIPinchGestureRecognizer *gestureRecognizer) {
    auto state = SCValdiMakeTouchState(gestureRecognizer.state);

    UIView *view = gestureRecognizer.view;
    SCValdiGestureLocation location = SCValdiGetGestureLocation(view, [gestureRecognizer locationInView:view]);

    auto pointerLocations = SCValdiGetPointerDataFromGestureRecognizer(gestureRecognizer);
    return Valdi::TouchEvents::makePinchEvent(state,
                                                 location.relative.x,
                                                 location.relative.y,
                                                 location.absolute.x,
                                                 location.absolute.y,
                                                 gestureRecognizer.scale,
                                                 static_cast<int>(pointerLocations.size()),
                                                 pointerLocations);
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    if (!_predicate) {
        return YES;
    }

    auto event = SCValdiMakePinchEvent(self);
    return SCValdiCallActionWithEvent(_predicate, event, Valdi::ValueFunctionFlagsCallSync);
}

- (void)_handleGestureRecognizer:(id)sender
{
    auto pinchEvent = SCValdiMakePinchEvent(self);
    UIGestureRecognizerState state = self.state;
    SCValdiForwardTouchAndCallAction(self.view, _lastEvent, SCValdiGestureTypePinch, state, _function, pinchEvent, SCValdiGetCallFlags(state));
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return [otherGestureRecognizer isKindOfClass:[SCValdiDragGestureRecognizer class]] ||
           [otherGestureRecognizer isKindOfClass:[SCValdiRotationGestureRecognizer class]];
}

@end

@interface SCValdiRotationGestureRecognizer ()

@end

@implementation SCValdiRotationGestureRecognizer {
    UIEvent *_lastEvent;
    id<SCValdiFunction> _function;
    id<SCValdiFunction> _predicate;
}

- (instancetype)init
{
    self = [super initWithTarget:self action:@selector(_handleGestureRecognizer:)];
    if (self) {
        self.delegate = self;
    }
    return self;
}

Valdi::Value SCValdiMakeRotationEvent(UIRotationGestureRecognizer *gestureRecognizer)
{
    auto state = SCValdiMakeTouchState(gestureRecognizer.state);
    UIView *view = gestureRecognizer.view;
    SCValdiGestureLocation location = SCValdiGetGestureLocation(view, [gestureRecognizer locationInView:view]);
    CGFloat rotation = [view.valdiViewNode resolveDeltaX:gestureRecognizer.rotation directionAgnostic:YES];

    auto pointerLocations = SCValdiGetPointerDataFromGestureRecognizer(gestureRecognizer);
    return Valdi::TouchEvents::makeRotationEvent(state,
                                                    location.relative.x,
                                                    location.relative.y,
                                                    location.absolute.x,
                                                    location.absolute.y,
                                                    rotation,
                                                    static_cast<int>(pointerLocations.size()),
                                                    pointerLocations);
}

- (void)reset
{
    _lastEvent = nil;
    [super reset];
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesBegan:touches withEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesEnded:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesCancelled:touches withEvent:event];
}


- (void)setFunction:(id<SCValdiFunction>)function
{
    _function = function;
}

- (void)setPredicate:(id<SCValdiFunction>)predicate
{
    _predicate = predicate;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    if (!_predicate) {
        return YES;
    }

    auto event = SCValdiMakeRotationEvent(self);
    return SCValdiCallActionWithEvent(_predicate, event, Valdi::ValueFunctionFlagsCallSync);
}

- (void)_handleGestureRecognizer:(id)sender
{
    auto event = SCValdiMakeRotationEvent(self);
    UIGestureRecognizerState state = self.state;
    SCValdiForwardTouchAndCallAction(self.view, _lastEvent, SCValdiGestureTypeRotate, state, _function, event, SCValdiGetCallFlags(state));
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return [otherGestureRecognizer isKindOfClass:[SCValdiDragGestureRecognizer class]] ||
           [otherGestureRecognizer isKindOfClass:[SCValdiPinchGestureRecognizer class]];
}

@end

@interface SCValdiTouchGestureRecognizer () <UIGestureRecognizerDelegate>

@end

@implementation SCValdiTouchGestureRecognizer {
    UIEvent *_lastEvent;
    id<SCValdiFunction> _beginAction;
    id<SCValdiFunction> _endAction;
    id<SCValdiFunction> _allAction;

    SCValdiTouchGestureType _gestureType;
    NSTimer * _timer;
    BOOL _allowSelfTrigger;
    BOOL _active;
}

- (instancetype)init
{
    self = [super initWithTarget:self action:@selector(_handleGestureRecognizer:)];

    if (self) {
        self.delegate = self;
        self.onTouchDelayDuration = 0;
        _allowSelfTrigger = NO;
        _active = NO;
    }

    return self;
}

- (void)_handleGestureRecognizer:(id)sender
{
    UIView *view = self.view;
    CGPoint gestureLocation = [self locationInView:view];
    UIGestureRecognizerState state = self.state;
    if (state == UIGestureRecognizerStateBegan) {
        SCValdiHandleTouchEvent(view, _lastEvent, SCValdiGestureTypeTouch, state, gestureLocation, _beginAction);
    }

    SCValdiHandleTouchEvent(view, _lastEvent, SCValdiGestureTypeTouch, state, gestureLocation, _allAction);

    if (state == UIGestureRecognizerStateCancelled || state == UIGestureRecognizerStateEnded) {
        SCValdiHandleTouchEvent(view, _lastEvent, SCValdiGestureTypeTouch, state, gestureLocation, _endAction);
    }
}

- (void)setFunction:(id<SCValdiFunction>)function forGestureType:(SCValdiTouchGestureType)gestureType
{
    switch (gestureType) {
    case SCValdiTouchGestureTypeAll:
        _allAction = function;
        break;
    case SCValdiTouchGestureTypeBegan:
        _beginAction = function;
        break;
    case SCValdiTouchGestureTypeEnded:
        _endAction = function;
        break;
    }
}

- (BOOL)isEmpty
{
    return _allAction == nil && _beginAction == nil && _endAction == nil;
}

- (void)reset
{
    _active = NO;
    _allowSelfTrigger = NO;
    [self _clearTimer];
    _lastEvent = nil;
    [super reset];
}

- (void)_clearTimer
{
    if (!_timer) {
        return;
    }
    [_timer invalidate];
    _timer = nil;
}

- (void)_onLongPress
{
    if (!_allowSelfTrigger) {
        return;
    }
    _allowSelfTrigger = YES;
    _active = YES;
    self.state = UIGestureRecognizerStateBegan;
}


- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    if (self.onTouchDelayDuration != 0) {
        _allowSelfTrigger = YES;
        [self _clearTimer];

        __weak __typeof__(self) weakSelf = self;
        _timer = [NSTimer scheduledTimerWithTimeInterval:_onTouchDelayDuration repeats:NO block:^(NSTimer * _Nonnull timer) {
            [weakSelf _onLongPress];
        }];
    } else {
        _active = YES;
        self.state = UIGestureRecognizerStateBegan;
    }
    _lastEvent = event;
    [super touchesBegan:touches withEvent:event];

}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    // prevent transitions when a delay is present until the timer sets the gesture to active
    if (!_active) {
        return;
    }
    _lastEvent = event;
    [super touchesMoved:touches withEvent:event];

    self.state = UIGestureRecognizerStateChanged;
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesEnded:touches withEvent:event];

    // Only set the gesture as .ended whean all of the remaining touches have ended.
    // If there are still touches report as changed so that the gesture will continue
    // to be considered an active gesture.
    if (touches.count == event.allTouches.count) {
        self.state = UIGestureRecognizerStateEnded;
    } else {
        self.state = UIGestureRecognizerStateChanged;
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    _lastEvent = event;
    [super touchesCancelled:touches withEvent:event];

    self.state = UIGestureRecognizerStateCancelled;
}

- (BOOL)shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return NO;
}

- (BOOL)shouldBeRequiredToFailByGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return NO;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return YES;
}

@end
