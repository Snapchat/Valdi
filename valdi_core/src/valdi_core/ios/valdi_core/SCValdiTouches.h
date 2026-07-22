//
//  SCValdiTouches.h
//  Valdi
//
//  Created by David Li on 06/07/24.
//  Copyright © 2024 Snap Inc. All rights reserved.
//

#import "valdi_core/SCMacros.h"
#import "valdi_core/SCValdiFunction.h"
#import "valdi_core/UIView+ValdiObjects.h"
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#ifdef __cplusplus
#import "valdi_core/SCValdiFunctionWithCPPFunction+CPP.h"
#import "valdi_core/SCValdiValueUtils.h"
#import "valdi_core/cpp/Events/TouchEvents.hpp"
#endif // __cplusplus

SC_EXTERN_C_BEGIN

typedef struct {
    CGPoint relative;
    CGPoint absolute;
} SCValdiGestureLocation;

#ifdef __cplusplus
extern "C++" {
BOOL SCValdiCallActionWithEvent(id<SCValdiFunction> action, const Valdi::Value& event, Valdi::ValueFunctionFlags flags);

BOOL SCValdiForwardTouchAndCallAction(UIView* view,
                                      UIEvent* uiEvent,
                                      SCValdiGestureType gestureType,
                                      UIGestureRecognizerState state,
                                      id<SCValdiFunction> action,
                                      const Valdi::Value& event,
                                      Valdi::ValueFunctionFlags flags);

Valdi::TouchEventState SCValdiMakeTouchState(UIGestureRecognizerState state);

SCValdiGestureLocation SCValdiGetGestureLocation(UIView* view, CGPoint gestureLocation);

Valdi::ValueFunctionFlags SCValdiGetCallFlags(UIGestureRecognizerState gestureState);

Valdi::Value SCValdiMakeTouchEvent(UIView* view,
                                   CGPoint gestureLocation,
                                   UIGestureRecognizerState gestureState,
                                   Valdi::TouchEvents::PointerLocations pointerLocations);

Valdi::TouchEvents::PointerLocations SCValdiGetPointerDataFromEvent(UIEvent* uiEvent);

Valdi::TouchEvents::PointerLocations SCValdiGetPointerDataFromGestureRecognizer(UIGestureRecognizer* gestureRecognizer);

BOOL SCValdiCallPredicateWithEvent(id<SCValdiFunction> predicate,
                                   UIView* view,
                                   const Valdi::Value& event,
                                   const char* callContext);
}
#endif // __cplusplus

BOOL SCValdiCallHitTestActionWithUIEventAndView(id<SCValdiFunction> action,
                                                CGPoint location,
                                                UIEvent* uiEvent,
                                                UIView* view);

SC_EXTERN_C_END
