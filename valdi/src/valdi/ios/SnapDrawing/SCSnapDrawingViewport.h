//
//  SCSnapDrawingViewport.h
//  valdi-ios
//

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>

NS_ASSUME_NONNULL_BEGIN

static inline BOOL SCSnapDrawingIsGeometryKeyPath(NSString* keyPath) {
    for (NSString* prefix in
         @[ @"position", @"bounds", @"transform", @"anchorPoint", @"sublayerTransform", @"frame" ]) {
        if ([keyPath hasPrefix:prefix]) {
            return YES;
        }
    }
    return NO;
}

/**
 * Whether `layer` or any of its ancestors has an animation that can move or resize it on screen.
 * Animations whose effect is unknown (groups, transitions, custom subclasses) count as geometric.
 */
static inline BOOL SCSnapDrawingLayerChainHasGeometryAnimation(CALayer* layer) {
    for (CALayer* current = layer; current != nil; current = current.superlayer) {
        for (NSString* key in current.animationKeys) {
            CAAnimation* animation = [current animationForKey:key];
            if (![animation isKindOfClass:[CAPropertyAnimation class]]) {
                return YES;
            }
            NSString* keyPath = ((CAPropertyAnimation*)animation).keyPath;
            if (keyPath == nil || SCSnapDrawingIsGeometryKeyPath(keyPath)) {
                return YES;
            }
        }
    }
    return NO;
}

/**
 * `windowBounds` of `windowLayer` expressed in `layer`'s coordinate space, as it will appear on the next frame.
 *
 * While an animation moves the layer, the model geometry already holds the destination and only the
 * presentation layers reflect what is on screen. Presentation layers, however, lag changes made in the
 * current transaction: a resize that lands together with an ancestor transform reset (a pinch-resized
 * sticker baking its scale on lift) would be converted through the stale transform and report a region
 * that cuts the view. With no geometry animation in flight the committed model is what the next frame
 * shows, so it is used as is.
 *
 * `presentedLayer` / `presentedWindowLayer` default to the layers' `presentationLayer` when nil.
 */
static inline CGRect SCSnapDrawingWindowRectInLayers(CALayer* layer,
                                                     CALayer* windowLayer,
                                                     CALayer* _Nullable presentedLayer,
                                                     CALayer* _Nullable presentedWindowLayer,
                                                     CGRect windowBounds) {
    if (!SCSnapDrawingLayerChainHasGeometryAnimation(layer)) {
        return [layer convertRect:windowBounds fromLayer:windowLayer];
    }
    // Resolved only here: -presentationLayer copies the layer, so the common no-animation path skips it.
    CALayer* presented = presentedLayer ?: layer.presentationLayer ?: layer;
    CALayer* presentedWindow = presentedWindowLayer ?: windowLayer.presentationLayer ?: windowLayer;
    return [presented convertRect:windowBounds fromLayer:presentedWindow];
}

static inline CGRect SCSnapDrawingWindowRectInLayer(CALayer* layer, CALayer* windowLayer, CGRect windowBounds) {
    return SCSnapDrawingWindowRectInLayers(layer, windowLayer, nil, nil, windowBounds);
}

/**
 * The region of a SnapDrawing view to render, in the view's own coordinate space: its bounds clipped to
 * the window. Always the exact on-screen region, never a union, so a view zoomed far past the screen
 * allocates at most a screen-sized drawable.
 */
static inline CGRect SCSnapDrawingVisibleRect(CGRect bounds, CGRect windowRect) {
    CGRect visible = CGRectIntersection(bounds, windowRect);
    return CGRectIsNull(visible) ? CGRectZero : visible;
}

/**
 * Whether to keep re-clamping every frame: only while the view is in a window and clipped below its bounds.
 */
static inline BOOL SCSnapDrawingShouldTrackViewport(BOOL inWindow, CGRect bounds, CGRect visibleRect) {
    return inWindow && !CGRectEqualToRect(visibleRect, bounds);
}

NS_ASSUME_NONNULL_END
