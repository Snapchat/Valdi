# Touches and Gestures

## Background
Currently, Valdi owns three primary rendering engines - iOS, Android, and SnapDrawing (Android primarily, but also desktop macOS), and each of them has a different implementation of gestures.

iOS relies on the default UIGestureRecognizer system, where we extend the existing listeners to allow us to call marshaled JS functions directly based on the events emitted from UIKit.

Android and SnapDrawing rely on our custom implementation in Kotlin and C++, respectively, where we take Android-level events and pass them onto our own TouchDispatcher system, where we locate potential touch candidates and execute any recognizers we find. We attempt to mimic iOS as the primary source of truth. 

We’ve taken this approach historically to fit with Valdi’s model of integrating seamlessly with native. 

[The `TouchEvent` forms the basis of any touch actions.](./core-events.md#touch-and-gesture-events)

## Gesture Recognizers 

In general, each of the gestures other than onTouch consist of a `on<Gesture>` function, an `<Gesture>Enabled` boolean, and a `on<Gesture>Predicate` function. The enabled and predicates determine whether a gesture should or should not run.

We first gather a list of potential candidates for gesture recognizers. When determining what element/node’s gestures should be run - the top most in the visible plane is the first one selected. If it’s enabled and the predicate (if it exists) returns true, that action will be fired, and we prevent conflicting gestures on additional elements after.

We support the following actions:

#### onTouch
Called whenever a view is touched.

This can always be run, hitting all parents/siblings in the view hierarchy - note that for a continuous gesture the initial start point of a pointer determines the `onTouch`,

We use `onTouchDelayDuration` specifically for `onTouch`, to ensure onTouches are only fired after a short time during say, a scroll event inside a ScrollView,

#### onTap
Called whenever a view is tapped, and only when we know it is not receiving a double tap or long press.

#### onDoubleTap
Called whenever a view is double (or more) tapped.

#### onLongPress
Called whenever a view is pressed and held.

We use `longPressDuration` to configure the time until the callback is triggered.

#### onDrag
Called whenever a view is dragged continuously, by one or more pointers.

This can be run simultaneously with onRotate and onPinch

#### onRotate
Called whenever a view is rotated continuously, by two or more pointers.

This can be run simultaneously with onDrag and onPinch

#### onPinch
Called whenever a view is pinched continuously, by two or more pointers.

This can be run simultaneously with onDrag and onRotate

#### Additional Configuration
`touchAreaExtension`
Used to expand the touchable surface area to recogize gestures from


## Technical Documentation

- [First party docs on the iOS responder chain](https://developer.apple.com/documentation/uikit/touches_presses_and_gestures/using_responders_and_the_responder_chain_to_handle_events?language=objc)

- [First party docs on the iOS recognizer state machine](https://developer.apple.com/documentation/uikit/touches_presses_and_gestures/implementing_a_custom_gesture_recognizer/about_the_gesture_recognizer_state_machine)

- [TouchDispatcher on Android](../../valdi/src/java/com/snap/valdi/views/touches/TouchDispatcher.kt)
