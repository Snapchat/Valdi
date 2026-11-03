//
//  SCValdiGestureRecognizers.h
//  Valdi
//
//  Created by Simon Corsin on 7/12/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#import "valdi_core/SCValdiFunction.h"

#import <UIKit/UIKit.h>

/// Realizes the private Gestures.framework (GestureFoundation) once per process by building and
/// discarding a gesture recognizer. On iOS 18+ the first UIGestureRecognizer created in the process
/// pays a large one-time framework realization; when that lands inside a Composer render pass it
/// trips the frozen-frame watchdog (COMPOSER-6174). Call this early to pay the cost off a visible
/// frame. Safe to call from any thread and any number of times; the work runs once, asynchronously
/// on the main thread.
FOUNDATION_EXTERN void SCValdiPrewarmGestureRecognizers(void);

/// Keeps a scroll view's own pan at one touch while a pinch is bound to it and
/// +[SCValdiConfiguration scrollPinchWhileDraggingEnabled] is on, so a second finger can pinch instead of
/// joining the pan. Otherwise restores the pan's original touch limit. Does nothing for other views.
FOUNDATION_EXTERN void SCValdiSetScrollPanLimitedForPinch(UIView* view, BOOL pinchBound);

@protocol SCValdiGestureRecognizer <NSObject, UIGestureRecognizerDelegate>

- (void)setFunction:(id<SCValdiFunction>)function;

- (void)setPredicate:(id<SCValdiFunction>)predicate;

@end

@interface SCValdiTapGestureRecognizer : UITapGestureRecognizer <SCValdiGestureRecognizer>

- (instancetype)init;

- (void)triggerAtLocation:(CGPoint)location forState:(UIGestureRecognizerState)state;

@end

/// Fast double tap gesture recognizer adapted from \c SCFastDoubleTapGestureRecognizer
@interface SCValdiFastDoubleTapGestureRecognizer : UITapGestureRecognizer <SCValdiGestureRecognizer>

- (instancetype)init;

- (void)triggerAtLocation:(CGPoint)location forState:(UIGestureRecognizerState)state;

@end

extern const NSTimeInterval kSCValdiMinLongPressDuration;

@interface SCValdiLongPressGestureRecognizer : UILongPressGestureRecognizer <SCValdiGestureRecognizer>

- (instancetype)init;

- (void)triggerAtLocation:(CGPoint)location forState:(UIGestureRecognizerState)state;

@end

@interface SCValdiDragGestureRecognizer : UIPanGestureRecognizer <SCValdiGestureRecognizer>

- (instancetype)init;

@end

@interface SCValdiPinchGestureRecognizer : UIPinchGestureRecognizer <SCValdiGestureRecognizer>

- (instancetype)init;

@end

@interface SCValdiRotationGestureRecognizer : UIRotationGestureRecognizer <SCValdiGestureRecognizer>

- (instancetype)init;

@end

@class SCValdiAttributedTextOnTapGestureRecognizer;
@protocol SCValdiAttributedTextOnTapGestureRecognizerFunctionProvider <NSObject>

- (id<SCValdiFunction>)onTapFunctionAtLocation:(CGPoint)location;

@end

@interface SCValdiAttributedTextOnTapGestureRecognizer : SCValdiTapGestureRecognizer

@property (weak, nonatomic) id<SCValdiAttributedTextOnTapGestureRecognizerFunctionProvider> functionProvider;
@property (nonatomic, assign) BOOL cannotBePreventedByOtherGestureRecognizers;

@end

typedef NS_ENUM(NSUInteger, SCValdiTouchGestureType) {
    SCValdiTouchGestureTypeAll,
    SCValdiTouchGestureTypeBegan,
    SCValdiTouchGestureTypeEnded
};

@interface SCValdiTouchGestureRecognizer : UIGestureRecognizer

@property (nonatomic) NSTimeInterval onTouchDelayDuration;
@property (readonly, nonatomic) BOOL isEmpty;

- (void)setFunction:(id<SCValdiFunction>)function forGestureType:(SCValdiTouchGestureType)gestureType;

@end
